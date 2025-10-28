// YourDlg.cpp
// Live battery ETA panel + Live Charge/Discharge rate line chart (WMI + CallNtPowerInformation)
// FIXED: Double-buffering to eliminate flicker

#include "pch.h"
#include "framework.h"
#include "resource.h"
#include "CRateInfoDlg.h"

// std
#include <cmath>
#include <cstdlib>  // llabs
#include <algorithm>
using namespace Gdiplus;

// avoid Windows min/max macro conflicts
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

// =====================================================
// Toggle: true = RAW (match PowerShell), false = EMA smoothing
// =====================================================
static constexpr bool kUseRawRate = true;

// ===================== BatteryTimeEstimator =====================
bool BatteryTimeEstimator::GetBatteryState(SYSTEM_BATTERY_STATE& state)
{
    ZeroMemory(&state, sizeof(state));
    auto ret = CallNtPowerInformation(
        SystemBatteryState, nullptr, 0, &state, sizeof(state));
    return (ret == 0); // STATUS_SUCCESS == 0
}

static bool InitComSecurityOnce()
{
    // It's okay if this was already set in-process.
    HRESULT hr = CoInitializeSecurity(
        nullptr, -1, nullptr, nullptr,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE, nullptr);
    return (SUCCEEDED(hr) || hr == RPC_E_TOO_LATE);
}

bool BatteryTimeEstimator::TryReadChargeRateFromWMI(LONG& signedRate_mW)
{
    signedRate_mW = 0;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;

    const bool needUninit = (hr != RPC_E_CHANGED_MODE);
    auto scopeUninit = [needUninit]() { if (needUninit) CoUninitialize(); };

    if (!InitComSecurityOnce() && hr != RPC_E_CHANGED_MODE) {
        scopeUninit();
        return false;
    }

    IWbemLocator* pLoc = nullptr;
    hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hr)) { scopeUninit(); return false; }

    IWbemServices* pSvc = nullptr;
    hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"),
        nullptr, nullptr, 0, 0, 0, 0, &pSvc);
    if (FAILED(hr)) { pLoc->Release(); scopeUninit(); return false; }

    hr = CoSetProxyBlanket(pSvc,
        RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr, EOAC_NONE);
    if (FAILED(hr)) { pSvc->Release(); pLoc->Release(); scopeUninit(); return false; }

    // Query both ChargeRate and DischargeRate; some systems expose multiple instances
    IEnumWbemClassObject* pEnum = nullptr;
    hr = pSvc->ExecQuery(bstr_t("WQL"),
        bstr_t("SELECT ChargeRate, DischargeRate FROM BatteryStatus"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &pEnum);
    if (FAILED(hr) || !pEnum) { pSvc->Release(); pLoc->Release(); scopeUninit(); return false; }

    LONG totalCharge_mW = 0;      // WMI is unsigned; we store in LONG
    LONG totalDischarge_mW = 0;

    IWbemClassObject* pObj = nullptr;
    ULONG returned = 0;
    while (SUCCEEDED(pEnum->Next(2000, 1, &pObj, &returned)) && returned) {
        VARIANT vtC{}; VARIANT vtD{};
        if (SUCCEEDED(pObj->Get(L"ChargeRate", 0, &vtC, nullptr, nullptr))) {
            if (vtC.vt == VT_I4)  totalCharge_mW += vtC.lVal;
            if (vtC.vt == VT_UI4) totalCharge_mW += (LONG)vtC.ulVal;
        }
        if (SUCCEEDED(pObj->Get(L"DischargeRate", 0, &vtD, nullptr, nullptr))) {
            if (vtD.vt == VT_I4)  totalDischarge_mW += vtD.lVal;
            if (vtD.vt == VT_UI4) totalDischarge_mW += (LONG)vtD.ulVal;
        }
        VariantClear(&vtC);
        VariantClear(&vtD);
        pObj->Release();
    }
    pEnum->Release();

    // Determine power state / sign
    SYSTEM_POWER_STATUS ps{};
    bool onAC = (GetSystemPowerStatus(&ps) && ps.ACLineStatus == 1);

    SYSTEM_BATTERY_STATE bs{};
    bool okBat = GetBatteryState(bs);
    bool charging = okBat ? !!bs.Charging : false;

    // If charging -> +ChargeRate; if discharging -> -DischargeRate
    // If one side is zero due to driver quirk, fall back to the other
    LONG chosen = 0;
    if (charging || onAC) {
        chosen = (totalCharge_mW != 0) ? totalCharge_mW
            : (totalDischarge_mW != 0 ? +totalDischarge_mW : 0);
        signedRate_mW = chosen; // positive
    }
    else {
        chosen = (totalDischarge_mW != 0) ? totalDischarge_mW
            : (totalCharge_mW != 0 ? +totalCharge_mW : 0);
        signedRate_mW = (chosen == 0) ? 0 : -chosen; // negative when discharging
    }

    pSvc->Release();
    pLoc->Release();
    scopeUninit();
    return (signedRate_mW != 0);
}

void BatteryTimeEstimator::UpdateSmoothedRate(LONG currentRate_mW)
{
    // NOTE: currentRate_mW is signed; we smooth absolute for magnitude display
    const float rateAbs = static_cast<float>(std::llabs(static_cast<long long>(currentRate_mW)));

    if (kUseRawRate) {
        // RAW (no smoothing) — matches PowerShell 1:1
        m_smoothedRate = rateAbs;
        m_initialized = true;
        return;
    }

    // EMA smoothing (for nicer UI)
    if (!m_initialized) {
        m_smoothedRate = rateAbs;
        m_initialized = true;
    }
    else {
        m_smoothedRate = ALPHA * rateAbs + (1.0f - ALPHA) * m_smoothedRate;
    }
}

void BatteryTimeEstimator::CalculateTimes(BatteryTimeInfo& info, const SYSTEM_BATTERY_STATE& state)
{
    info.hoursRemaining = 0.0f;
    info.hoursToFull = 0.0f;

    // Prefer smoothed; fallback to instantaneous Rate if smoothing is 0
    float effectiveRate_mW = (m_smoothedRate > 1.0f) ? m_smoothedRate
        : static_cast<float>(std::llabs(static_cast<long long>(state.Rate)));

    // Very small/missing rate -> idle/unknown; skip ETA
    if (effectiveRate_mW < 100.0f) return;

    if (info.isCharging) {
        // Time to full (linear + gentle taper near full)
        ULONG missing_mWh = 0;
        if (info.maxCapacity > info.currentCapacity)
            missing_mWh = info.maxCapacity - info.currentCapacity;

        float hours = (missing_mWh > 0) ? (missing_mWh / effectiveRate_mW) : 0.0f;

        // Simple taper correction over 80%
        if (info.chargePercent > 80.0f) {
            float slow = 1.0f + (info.chargePercent - 80.0f) / 100.0f; // up to +0.2x
            hours *= slow;
        }

        // Clamp
        if (hours < 0.05f) hours = 0.05f;
        if (hours > 10.0f) hours = 10.0f;
        info.hoursToFull = hours;

    }
    else {
        // Discharge time = remaining mWh / mW
        if (info.currentCapacity > 0) {
            float hours = info.currentCapacity / effectiveRate_mW;
            if (hours < 0.05f) hours = 0.05f;
            if (hours > 24.0f) hours = 24.0f;
            info.hoursRemaining = hours;
        }
    }
}

void BatteryTimeEstimator::FormatStatusText(BatteryTimeInfo& info)
{
    if (info.isFullyCharged) {
        info.statusText = L"Fully Charged (100%)";
        info.detailText.Format(L"%s | %lu mWh",
            kUseRawRate ? L"src=WMI sum, raw" : L"src=WMI sum, EMA",
            info.currentCapacity);
        return;
    }

    if (info.isCharging) {
        int h = (int)info.hoursToFull;
        int m = (int)((info.hoursToFull - h) * 60.0f);

        if (h > 0) {
            info.statusText.Format(L"Charging: %.0f%% | %dh %02dm until full",
                info.chargePercent, h, m);
        }
        else {
            info.statusText.Format(L"Charging: %.0f%% | %d min until full",
                info.chargePercent, m);
        }

        info.detailText.Format(L"Charge rate: %.1f W | %lu / %lu mWh | %s",
            m_smoothedRate / 1000.0f, info.currentCapacity, info.maxCapacity,
            kUseRawRate ? L"WMI raw" : L"WMI EMA");
        return;
    }

    if (info.isOnAC && !info.isCharging) {
        info.statusText.Format(L"AC Power: %.0f%%", info.chargePercent);
        info.detailText = L"Plugged in (not charging)";
        return;
    }

    // Discharging
    int h = (int)info.hoursRemaining;
    int m = (int)((info.hoursRemaining - h) * 60.0f);

    if (h > 0) {
        info.statusText.Format(L"Battery: %.0f%% | %dh %02dm remaining",
            info.chargePercent, h, m);
    }
    else {
        info.statusText.Format(L"Battery: %.0f%% | %d min remaining",
            info.chargePercent, m);
    }

    info.detailText.Format(L"Discharge rate: %.1f W | %lu / %lu mWh | %s",
        m_smoothedRate / 1000.0f, info.currentCapacity, info.maxCapacity,
        kUseRawRate ? L"WMI raw" : L"WMI EMA");
}

BatteryTimeInfo BatteryTimeEstimator::GetBatteryTime()
{
    BatteryTimeInfo info;

    // Basic AC + %
    SYSTEM_POWER_STATUS ps{};
    if (GetSystemPowerStatus(&ps)) {
        info.isOnAC = (ps.ACLineStatus == 1);
        info.chargePercent = (ps.BatteryLifePercent <= 100)
            ? (float)ps.BatteryLifePercent
            : 0.0f;
    }

    // Detailed battery state (capacity + raw Rate)
    SYSTEM_BATTERY_STATE bs{};
    if (!GetBatteryState(bs)) {
        info.statusText = L"? Cannot read battery state";
        info.detailText = L"CallNtPowerInformation(SystemBatteryState) failed";
        return info;
    }

    info.isCharging = !!bs.Charging;
    info.currentCapacity = bs.RemainingCapacity; // mWh
    info.maxCapacity = bs.MaxCapacity;           // mWh
    info.currentRate_mW = bs.Rate;               // signed (+/-), may be 0
    info.designCapacity = 0;                     
    info.isFullyCharged = (info.chargePercent >= 99.0f && info.isOnAC);

    // Prefer WMI rate if available (sum instances, correct sign)
    LONG wmiRateSigned = 0;
    if (TryReadChargeRateFromWMI(wmiRateSigned) && wmiRateSigned != 0) {
        info.currentRate_mW = wmiRateSigned;
    }

    // Smooth/Raw magnitude update
    UpdateSmoothedRate(info.currentRate_mW);

    // Compute ETAs (use absolute power)
    CalculateTimes(info, bs);

    // Compose texts
    FormatStatusText(info);

    return info;
}

// ===================== CRateInfoDlg =====================
CRateInfoDlg::CRateInfoDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_RATEINFO, pParent)
{
    m_samplesChargeW.resize(kWindowSeconds, kMissing);
    m_samplesDischargeW.resize(kWindowSeconds, kMissing);
}

void CRateInfoDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CRateInfoDlg, CDialogEx)
    ON_WM_DESTROY()
    ON_WM_TIMER()
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

BOOL CRateInfoDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Start GDI+
    GdiplusStartupInput gpsi;
    if (GdiplusStartup(&m_gdiplusToken, &gpsi, nullptr) != Ok) {
        AfxMessageBox(L"GDI+ startup failed.", MB_ICONWARNING);
    }

    // Initial read + timer
    UpdateBatteryInfo();
    StartBatteryMonitoring();

    return TRUE;
}

void CRateInfoDlg::OnDestroy()
{
    KillTimer(TIMER_BATTERY_UPDATE);

    if (m_gdiplusToken) {
        GdiplusShutdown(m_gdiplusToken);
        m_gdiplusToken = 0;
    }
    CDialogEx::OnDestroy();
}

void CRateInfoDlg::StartBatteryMonitoring()
{
    // 1s refresh to match your PowerShell watch loop
    SetTimer(TIMER_BATTERY_UPDATE, 1000, nullptr);
}

void CRateInfoDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == TIMER_BATTERY_UPDATE) {
        UpdateBatteryInfo();
    }
    CDialogEx::OnTimer(nIDEvent);
}

void CRateInfoDlg::PushSamples(float chargeW, float dischargeW)
{
    // Store W or NaN (gap) for both series
    m_samplesChargeW[m_cursor] = chargeW;
    m_samplesDischargeW[m_cursor] = dischargeW;

    m_cursor = (m_cursor + 1) % kWindowSeconds;
    if (!m_filled && m_cursor == 0) m_filled = true;
}

void CRateInfoDlg::UpdateBatteryInfo()
{
    m_last = m_estimator.GetBatteryTime();

    // Decide samples based on the signed rate:
    // >0 ? charging; <0 ? discharging; 0 ? idle
    float chargeW = kMissing, dischargeW = kMissing;
    if (m_last.currentRate_mW > 0) {
        chargeW = (float)m_last.currentRate_mW / 1000.0f; // W
    }
    else if (m_last.currentRate_mW < 0) {
        dischargeW = std::fabs((float)m_last.currentRate_mW) / 1000.0f; // W
    }
    // push both; one will be NaN => hidden
    PushSamples(chargeW, dischargeW);

    Invalidate(FALSE); // trigger repaint (will use double-buffer)
}

BOOL CRateInfoDlg::OnEraseBkgnd(CDC* pDC)
{
    // Prevent background erase to reduce flicker
    return TRUE;
}

void CRateInfoDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect rcClient;
    GetClientRect(&rcClient);

    // Create memory DC for double-buffering
    CDC memDC;
    memDC.CreateCompatibleDC(&dc);

    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, rcClient.Width(), rcClient.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    // Draw everything to memory DC
    Graphics g(memDC.m_hDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // Clear background
    SolidBrush bgBrush(Color(255, 240, 240, 240));
    g.FillRectangle(&bgBrush, 0, 0, rcClient.Width(), rcClient.Height());

    const float margin = 12.0f;
    float x = (float)rcClient.left + margin;
    float y = (float)rcClient.top + margin;
    float w = (float)rcClient.Width() - 2.0f * margin;

    // Card height
    float hCard = 110.0f;
    DrawBatteryCard(g, x, y, w, hCard);

    // Chart rect below card
    float gap = 10.0f;
    float yChart = y + hCard + gap;
    float hChart = std::max(140.0f, (float)rcClient.Height() - (hCard + 3 * margin));
    DrawLivePowerChart(g, x, yChart, w, hChart);

    // Copy memory DC to screen DC (single blit - no flicker)
    dc.BitBlt(0, 0, rcClient.Width(), rcClient.Height(), &memDC, 0, 0, SRCCOPY);

    // Cleanup
    memDC.SelectObject(pOldBmp);
}

void CRateInfoDlg::DrawBatteryCard(Graphics& g, float x, float y, float w, float h)
{
    // Background
    SolidBrush bg(Color(255, 255, 255, 255));
    g.FillRectangle(&bg, x, y, w, h);

    // Border
    Pen border(Color(255, 210, 210, 210), 1.5f);
    g.DrawRectangle(&border, x, y, w, h);

    // Fonts
    Gdiplus::FontFamily fam(L"Segoe UI");
    Gdiplus::Font fTitle(&fam, 11.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font fDetail(&fam, 9.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);

    // Brushes
    SolidBrush textDark(Color(255, 40, 40, 40));
    SolidBrush textMid(Color(255, 100, 100, 100));

    // Status
    g.DrawString(m_last.statusText.GetString(), -1, &fTitle,
        Gdiplus::PointF(x + 12.f, y + 10.f), &textDark);

    // Detail
    g.DrawString(m_last.detailText.GetString(), -1, &fDetail,
        Gdiplus::PointF(x + 12.f, y + 34.f), &textMid);

    // Battery bar
    float barY = y + 64.f;
    float barH = 20.f;
    float barW = w - 24.f;
    float barX = x + 12.f;

    SolidBrush barBg(Color(255, 235, 235, 235));
    g.FillRectangle(&barBg, barX, barY, barW, barH);

    // Fill color by state
    Color fillColor;
    if (m_last.isCharging) fillColor = Color(255, 0, 180, 0);
    else if (m_last.chargePercent < 20.0f) fillColor = Color(255, 220, 20, 60);
    else if (m_last.chargePercent < 50.0f) fillColor = Color(255, 255, 165, 0);
    else fillColor = Color(255, 0, 122, 204);

    float fillW = barW * (m_last.chargePercent / 100.0f);
    fillW = std::max(0.0f, std::min(fillW, barW));

    SolidBrush barFill(fillColor);
    g.FillRectangle(&barFill, barX, barY, fillW, barH);

    // Percentage centered
    CString pct; pct.Format(L"%.0f%%", m_last.chargePercent);

    Gdiplus::RectF r(barX, barY, barW, barH);
    Gdiplus::StringFormat fmt;
    fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
    fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    SolidBrush white(Color(255, 255, 255, 255));
    g.DrawString(pct.GetString(), -1, &fTitle, r, &fmt, &white);
}

static inline bool isNaNf(float v) { return std::isnan(v) != 0; }

void CRateInfoDlg::DrawLivePowerChart(Graphics& g, float x, float y, float w, float h)
{
    // Decide which series to display: charge OR discharge
    const bool showCharge = (m_last.currentRate_mW > 0) || m_last.isCharging;

    const std::vector<float>& series = showCharge ? m_samplesChargeW : m_samplesDischargeW;
    const WCHAR* chartTitle = showCharge ? L"Live Charge Rate" : L"Live Discharge Rate";

    // Panel background
    SolidBrush bg(Color(255, 252, 252, 252));
    g.FillRectangle(&bg, x, y, w, h);

    // Border
    Pen border(Color(255, 210, 210, 210), 1.5f);
    g.DrawRectangle(&border, x, y, w, h);

    // Inner plot area with padding for axes (extra room under the title)
    const float padL = 46.f, padR = 12.f, padT = 40.f, padB = 24.f; // <-- increased padT
    const float px = x + padL, py = y + padT, pw = w - padL - padR, ph = h - padT - padB;

    // Title & axis labels
    Gdiplus::FontFamily fam(L"Segoe UI");
    Gdiplus::Font fTitle(&fam, 11.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font fAxis(&fam, 9.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    SolidBrush textDark(Color(255, 40, 40, 40));
    SolidBrush textMid(Color(255, 110, 110, 110));

    CString title; title.Format(L"%s (last %d s)", chartTitle, kWindowSeconds);
    g.DrawString(title, -1, &fTitle, Gdiplus::PointF(x + 12.f, y + 6.f), &textDark);

    // Determine Y max (auto-scale). Min 1 W
    float maxW = 1.0f;
    const int N = kWindowSeconds;
    for (int i = 0; i < N; ++i) {
        float v = series[i];
        if (!isNaNf(v)) maxW = std::max(maxW, v);
    }
    // Add headroom
    maxW *= 1.15f;
    // Round up a bit for nicer grid (to nearest 0.5)
    maxW = std::max(1.0f, std::ceil(maxW * 2.0f) / 2.0f);

    // Grid lines (5 horizontal)
    Pen grid(Color(255, 230, 230, 230), 1.0f);
    const int gridLines = 5;
    for (int i = 0; i <= gridLines; ++i) {
        float yy = py + ph * (1.0f - (float)i / gridLines);
        g.DrawLine(&grid, px, yy, px + pw, yy);

        float val = maxW * (float)i / gridLines;
        CString lbl; lbl.Format(L"%.1f W", val);
        g.DrawString(lbl, -1, &fAxis, Gdiplus::PointF(x + 6.f, yy - 9.f), &textMid);
    }

    // X-axis labels: 0s, 30s, 60s, 90s, 120s (left ? right)
    {
        const int step = N / 4; // 120 -> 30
        CString l0, l1, l2, l3, l4;
        l0.Format(L"%ds", 0);
        l1.Format(L"%ds", step);
        l2.Format(L"%ds", 2 * step);
        l3.Format(L"%ds", 3 * step);
        l4.Format(L"%ds", N);

        const float yLab = py + ph + 4.f;
        g.DrawString(l0, -1, &fAxis, Gdiplus::PointF(px - 6.f, yLab), &textMid);
        g.DrawString(l1, -1, &fAxis, Gdiplus::PointF(px + pw * 0.25f - 12.f, yLab), &textMid);
        g.DrawString(l2, -1, &fAxis, Gdiplus::PointF(px + pw * 0.50f - 12.f, yLab), &textMid);
        g.DrawString(l3, -1, &fAxis, Gdiplus::PointF(px + pw * 0.75f - 12.f, yLab), &textMid);
        g.DrawString(l4, -1, &fAxis, Gdiplus::PointF(px + pw - 22.f, yLab), &textMid);
    }

    // Axes
    Pen axis(Color(255, 180, 180, 180), 1.2f);
    g.DrawLine(&axis, px, py, px, py + ph);
    g.DrawLine(&axis, px, py + ph, px + pw, py + ph);

    // Build and draw polyline (skip NaN gaps)
    auto idxToSample = [&](int k)->float {
        // k: 0..N-1 where 0 = oldest, N-1 = newest
        int base = m_filled ? m_cursor : 0; // when filled, m_cursor points to *next* write slot
        int realIdx = m_filled ? ((base + k) % N) : k;
        if (!m_filled && k >= m_cursor) return kMissing; // not yet written
        return series[realIdx];
        };

    auto mapX = [&](int k)->float {
        return px + (float)k * (pw / (float)(N - 1));
        };

    auto mapY = [&](float watts)->float {
        float t = (maxW <= 0.0f) ? 0.0f : (watts / maxW);
        if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
        return py + ph * (1.0f - t);
        };

    // Color per mode: green = charge, blue = discharge
    Pen linePen(showCharge ? Color(255, 0, 180, 0) : Color(255, 0, 122, 204), 2.0f);

    bool hasPrev = false;
    PointF prev{};
    for (int k = 0; k < N; ++k) {
        float sample = idxToSample(k);
        if (isNaNf(sample)) { hasPrev = false; continue; }
        PointF cur(mapX(k), mapY(sample));
        if (hasPrev) g.DrawLine(&linePen, prev, cur);
        prev = cur; hasPrev = true;
    }

    // Latest value badge (right-top)
    float latest = kMissing;
    if (m_filled || m_cursor > 0) {
        int newestIdx = (m_cursor + N - 1) % N;
        latest = series[newestIdx];
    }
    CString latestTxt;
    if (isNaNf(latest)) latestTxt = showCharge ? L"— W (not charging)" : L"— W (not discharging)";
    else latestTxt.Format(L"%.2f W", latest);

    Gdiplus::Font fBadge(&fam, 9.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    SolidBrush badgeText(Color(255, 40, 40, 40));
    g.DrawString(latestTxt, -1, &fBadge,
        Gdiplus::PointF(px + pw - 110.f, py + 4.f), &badgeText);
}
