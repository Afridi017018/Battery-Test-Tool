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




// ==================== Language support ====================

enum LANG_INDEX
{
    LANG_EN = 0,
    LANG_JP = 1
};

enum TEXT_KEY
{
    TK_ERR_GDIPLUS = 0,

    TK_STATUS_FULLY_CHARGED,
    TK_DETAIL_FULLY_CHARGED,

    TK_STATUS_CHG_CALC,
    TK_STATUS_CHG_EST,
    TK_STATUS_CHG_HM,
    TK_STATUS_CHG_M,
    TK_DETAIL_CHG_RATE,

    TK_STATUS_AC,
    TK_DETAIL_AC,

    TK_STATUS_DISCH_CALC,
    TK_STATUS_DISCH_EST,
    TK_STATUS_DISCH_HM,
    TK_STATUS_DISCH_M,
    TK_DETAIL_DISCH_RATE,

    TK_ERR_BATSTATE,
    TK_ERR_BATSTATE_DETAIL,

    TK_CHART_TITLE_CHG,
    TK_CHART_TITLE_DIS,
    TK_CHART_TITLE_FMT,

    TK_LATEST_NOT_CHG,
    TK_LATEST_NOT_DIS,

    // NEW: Unknown-rate strings (appended so old indices stay valid)
    TK_DETAIL_CHG_RATE_UNKNOWN,
    TK_DETAIL_DISCH_RATE_UNKNOWN,
    TK_STATUS_CHG_UNKNOWN,
    TK_STATUS_DISCH_UNKNOWN,

    TK_COUNT
};

static const wchar_t* g_Texts[2][TK_COUNT] =
{
    // English
    {
        L"GDI+ startup failed.",                                        // TK_ERR_GDIPLUS

        L"Fully Charged (100%)",                                        // TK_STATUS_FULLY_CHARGED
        L"%lu mWh",                                                     // TK_DETAIL_FULLY_CHARGED

        L"Charging: %.0f%% | Calculating...",                           // TK_STATUS_CHG_CALC
        L"Charging: %.0f%% | Estimating...",                            // TK_STATUS_CHG_EST
        L"Charging: %.0f%% | %dh %02dm until full",                     // TK_STATUS_CHG_HM
        L"Charging: %.0f%% | %d min until full",                        // TK_STATUS_CHG_M
        L"Charge rate: %.1f W | %lu / %lu mWh",                         // TK_DETAIL_CHG_RATE

        L"AC Power: %.0f%%",                                            // TK_STATUS_AC
        L"Plugged in (not charging)",                                   // TK_DETAIL_AC

        L"Battery: %.0f%% | Calculating...",                            // TK_STATUS_DISCH_CALC
        L"Battery: %.0f%% | Estimating...",                             // TK_STATUS_DISCH_EST
        L"Battery: %.0f%% | %dh %02dm remaining",                       // TK_STATUS_DISCH_HM
        L"Battery: %.0f%% | %d min remaining",                          // TK_STATUS_DISCH_M
        L"Discharge rate: %.1f W | %lu / %lu mWh",                      // TK_DETAIL_DISCH_RATE

        L"? Cannot read battery state",                                 // TK_ERR_BATSTATE
        L"CallNtPowerInformation(SystemBatteryState) failed",           // TK_ERR_BATSTATE_DETAIL

        L"Live Charge Rate",                                            // TK_CHART_TITLE_CHG
        L"Live Discharge Rate",                                         // TK_CHART_TITLE_DIS
        L"%s (last %d s)",                                              // TK_CHART_TITLE_FMT

        L"— W (not charging)",                                          // TK_LATEST_NOT_CHG
        L"— W (not discharging)",                                       // TK_LATEST_NOT_DIS

        // NEW unknown strings
        L"Charge rate: Unknown W | %lu / %lu mWh",                      // TK_DETAIL_CHG_RATE_UNKNOWN
        L"Discharge rate: Unknown W | %lu / %lu mWh",                   // TK_DETAIL_DISCH_RATE_UNKNOWN
        L"Charging: %.0f%% | Time to full unknown",                     // TK_STATUS_CHG_UNKNOWN
        L"Battery: %.0f%% | Time remaining unknown"                     // TK_STATUS_DISCH_UNKNOWN
    },

    // Japanese
    {
        L"GDI+ の起動に失敗しました。",                                  // TK_ERR_GDIPLUS

        L"フル充電（100%）",                                              // TK_STATUS_FULLY_CHARGED
        L"%lu mWh",                                                     // TK_DETAIL_FULLY_CHARGED

        L"充電中: %.0f%% | 計算中...",                                   // TK_STATUS_CHG_CALC
        L"充電中: %.0f%% | 推定中...",                                   // TK_STATUS_CHG_EST
        L"充電中: %.0f%% | 満充電まで %d時間 %02d分",                    // TK_STATUS_CHG_HM
        L"充電中: %.0f%% | 満充電まで %d分",                             // TK_STATUS_CHG_M
        L"充電レート: %.1f W | %lu / %lu mWh",                          // TK_DETAIL_CHG_RATE

        L"AC電源: %.0f%%",                                              // TK_STATUS_AC
        L"電源に接続 (充電していません)",                                // TK_DETAIL_AC

        L"バッテリー: %.0f%% | 計算中...",                               // TK_STATUS_DISCH_CALC
        L"バッテリー: %.0f%% | 推定中...",                               // TK_STATUS_DISCH_EST
        L"バッテリー: %.0f%% | 残り %d時間 %02d分",                     // TK_STATUS_DISCH_HM
        L"バッテリー: %.0f%% | 残り %d分",                              // TK_STATUS_DISCH_M
        L"放電レート: %.1f W | %lu / %lu mWh",                          // TK_DETAIL_DISCH_RATE

        L"バッテリー状態を読み取れません",                              // TK_ERR_BATSTATE
        L"CallNtPowerInformation(SystemBatteryState) に失敗しました",    // TK_ERR_BATSTATE_DETAIL

        L"リアルタイム充電レート",                                       // TK_CHART_TITLE_CHG
        L"リアルタイム放電レート",                                       // TK_CHART_TITLE_DIS
        L"%s (直近 %d 秒)",                                             // TK_CHART_TITLE_FMT

        L"— W (充電していません)",                                      // TK_LATEST_NOT_CHG
        L"— W (放電していません)",                                      // TK_LATEST_NOT_DIS

        // NEW unknown strings (rough JP)
        L"充電レート: 不明 W | %lu / %lu mWh",                          // TK_DETAIL_CHG_RATE_UNKNOWN
        L"放電レート: 不明 W | %lu / %lu mWh",                          // TK_DETAIL_DISCH_RATE_UNKNOWN
        L"充電中: %.0f%% | 満充電までの時間は不明",                     // TK_STATUS_CHG_UNKNOWN
        L"バッテリー: %.0f%% | 残り時間は不明"                          // TK_STATUS_DISCH_UNKNOWN
    }
};






// ===================== BatteryTimeEstimator =====================
bool BatteryTimeEstimator::GetBatteryState(SYSTEM_BATTERY_STATE& state)
{

    ZeroMemory(&state, sizeof(state));
    auto ret = CallNtPowerInformation(
        SystemBatteryState, nullptr, 0, &state, sizeof(state));
    return (ret == 0); // STATUS_SUCCESS == 0
}

// NEW: helper to detect "unknown" or clearly bogus rates
static inline bool IsUnknownOrBogusRate(LONG rate_mW)
{
    // BATTERY_UNKNOWN_RATE is 0x80000000U (appears as -2147483648 in signed LONG)
    if (rate_mW == (LONG)0x80000000) return true;

    long long a = (long long)rate_mW;
    if (a < 0) a = -a;

    // Anything above 500 W for a laptop battery is nonsense
    if (a > 500000) return true; // 500000 mW

    return false;
}

static bool InitComSecurityOnce()
{
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

    // Query both ChargeRate and DischargeRate
    IEnumWbemClassObject* pEnum = nullptr;
    hr = pSvc->ExecQuery(bstr_t("WQL"),
        bstr_t("SELECT ChargeRate, DischargeRate FROM BatteryStatus"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr, &pEnum);
    if (FAILED(hr) || !pEnum) { pSvc->Release(); pLoc->Release(); scopeUninit(); return false; }

    LONG totalCharge_mW = 0;
    LONG totalDischarge_mW = 0;

    IWbemClassObject* pObj = nullptr;
    ULONG returned = 0;
    while (SUCCEEDED(pEnum->Next(2000, 1, &pObj, &returned)) && returned) {
        VARIANT vtC{}; VARIANT vtD{};
        if (SUCCEEDED(pObj->Get(L"ChargeRate", 0, &vtC, nullptr, nullptr))) {
            if (vtC.vt == VT_I4) {
                if (!IsUnknownOrBogusRate(vtC.lVal))
                    totalCharge_mW += vtC.lVal;
            }
            if (vtC.vt == VT_UI4) {
                LONG tmp = (LONG)vtC.ulVal;
                if (!IsUnknownOrBogusRate(tmp))
                    totalCharge_mW += tmp;
            }
        }
        if (SUCCEEDED(pObj->Get(L"DischargeRate", 0, &vtD, nullptr, nullptr))) {
            if (vtD.vt == VT_I4) {
                if (!IsUnknownOrBogusRate(vtD.lVal))
                    totalDischarge_mW += vtD.lVal;
            }
            if (vtD.vt == VT_UI4) {
                LONG tmp = (LONG)vtD.ulVal;
                if (!IsUnknownOrBogusRate(tmp))
                    totalDischarge_mW += tmp;
            }
        }
        VariantClear(&vtC);
        VariantClear(&vtD);
        pObj->Release();
    }
    pEnum->Release();

    SYSTEM_POWER_STATUS ps{};
    bool onAC = (GetSystemPowerStatus(&ps) && ps.ACLineStatus == 1);

    SYSTEM_BATTERY_STATE bs{};
    bool okBat = GetBatteryState(bs);
    bool charging = okBat ? !!bs.Charging : false;

    LONG chosen = 0;
    if (charging || onAC) {
        chosen = (totalCharge_mW != 0) ? totalCharge_mW
            : (totalDischarge_mW != 0 ? +totalDischarge_mW : 0);
        signedRate_mW = chosen;
    }
    else {
        chosen = (totalDischarge_mW != 0) ? totalDischarge_mW
            : (totalCharge_mW != 0 ? +totalCharge_mW : 0);
        signedRate_mW = (chosen == 0) ? 0 : -chosen;
    }

    pSvc->Release();
    pLoc->Release();
    scopeUninit();

    if (IsUnknownOrBogusRate(signedRate_mW))
        signedRate_mW = 0;

    return (signedRate_mW != 0);
}

void BatteryTimeEstimator::UpdateSmoothedRate(LONG currentRate_mW)
{
    if (IsUnknownOrBogusRate(currentRate_mW)) {
        // ignore bogus/unknown values and keep previous smoothing
        return;
    }

    const float rateAbs = static_cast<float>(std::llabs(static_cast<long long>(currentRate_mW)));

    if (kUseRawRate) {
        m_smoothedRate = rateAbs;
        m_initialized = true;
        return;
    }

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

    LONG stateRate = state.Rate;
    if (IsUnknownOrBogusRate(stateRate)) {
        stateRate = 0;
    }

    float effectiveRate_mW = (m_smoothedRate > 1.0f) ? m_smoothedRate
        : static_cast<float>(std::llabs(static_cast<long long>(stateRate)));

    // Compute a fresh ETA if rate is usable
    bool etaComputed = false;
    if (effectiveRate_mW >= 100.0f) {
        if (info.isCharging) {
            ULONG missing_mWh = 0;
            if (info.maxCapacity > info.currentCapacity)
                missing_mWh = info.maxCapacity - info.currentCapacity;

            float hours = (missing_mWh > 0) ? (missing_mWh / effectiveRate_mW) : 0.0f;
            if (info.chargePercent > 80.0f) {
                float slow = 1.0f + (info.chargePercent - 80.0f) / 100.0f;
                hours *= slow;
            }
            if (hours < 0.05f) hours = 0.05f;
            if (hours > 10.0f) hours = 10.0f;
            info.hoursToFull = hours;
            etaComputed = true;
        }
        else {
            if (info.currentCapacity > 0) {
                float hours = info.currentCapacity / effectiveRate_mW;
                if (hours < 0.05f) hours = 0.05f;
                if (hours > 24.0f) hours = 24.0f;
                info.hoursRemaining = hours;
                etaComputed = true;
            }
        }
    }

    // === Gate + Percent lock ==========================================
    // Mode: 0=discharging, 1=charging, 2=neutral
    static int        s_lastPct = -1;
    static int        s_lastStableMode = 2;
    static float      s_lockedToFull = 0.0f;
    static float      s_lockedRemaining = 0.0f;
    static bool       s_gateActive = false;
    static ULONGLONG  s_gateEndMs = 0;

    const bool modeCharging = info.isCharging && !info.isFullyCharged;
    const bool modeDischarging = !info.isOnAC && !info.isCharging;
    const int  curMode = modeCharging ? 1 : (modeDischarging ? 0 : 2);

    const int  pctNow = static_cast<int>(info.chargePercent);
    const ULONGLONG nowMs = GetTickCount64();

    // Start 5s gate only when entering charging/discharging from a different stable mode
    if (!s_gateActive && (curMode == 0 || curMode == 1) && curMode != s_lastStableMode) {
        s_gateActive = true;
        s_gateEndMs = nowMs + 5000;
        s_lastPct = -1; // force a capture after gate
        s_lastStableMode = curMode;

        if (curMode == 1)  info.hoursToFull = -1.0f;     // "Calculating…"
        if (curMode == 0)  info.hoursRemaining = -1.0f;  // "Calculating…"
        // keep "Calculating..." during gate
        return;
    }

    // If moved to neutral, cancel gate and clear sentinels
    if (curMode == 2) {
        s_gateActive = false;
        s_gateEndMs = 0;
        // neutral does not lock or arm
        s_lastStableMode = 2;
    }

    // While gate is active ? stay "Calculating..."
    if (s_gateActive) {
        if (nowMs < s_gateEndMs) {
            if (curMode == 1) info.hoursToFull = -1.0f;
            if (curMode == 0) info.hoursRemaining = -1.0f;
            return;
        }
        // gate ended
        s_gateActive = false;
        s_gateEndMs = 0;

        // If we still couldn't compute ETA, fall back to last lock (or 0 to avoid sticking)
        if (!etaComputed) {
            if (curMode == 1)  info.hoursToFull = (s_lockedToFull > 0.0f ? s_lockedToFull : 0.0f);
            if (curMode == 0)  info.hoursRemaining = (s_lockedRemaining > 0.0f ? s_lockedRemaining : 0.0f);
        }
    }

    // Percent lock (update only when integer % changes)
    if (curMode == 1) {
        if (s_lastPct < 0 || pctNow != s_lastPct) {
            if (etaComputed && info.hoursToFull > 0.0f)
                s_lockedToFull = info.hoursToFull;
            info.hoursToFull = s_lockedToFull; // might be 0 on first capture
            s_lastPct = pctNow;
        }
        else {
            info.hoursToFull = s_lockedToFull;
        }
    }
    else if (curMode == 0) {
        if (s_lastPct < 0 || pctNow != s_lastPct) {
            if (etaComputed && info.hoursRemaining > 0.0f)
                s_lockedRemaining = info.hoursRemaining;
            info.hoursRemaining = s_lockedRemaining;
            s_lastPct = pctNow;
        }
        else {
            info.hoursRemaining = s_lockedRemaining;
        }
    }
    else {
        // neutral: keep lastPct in sync but no locking
        s_lastPct = pctNow;
    }
}

void BatteryTimeEstimator::FormatStatusText(BatteryTimeInfo& info, int lang)
{

    /*int lang = eng_lang ? LANG_EN : LANG_JP;*/

    if (info.isFullyCharged) {
        info.statusText = g_Texts[lang][TK_STATUS_FULLY_CHARGED];
        info.detailText.Format(L"%lu mWh", info.currentCapacity);
        return;
    }

    // Sentinel for "unknown ETA"
    const float UNKNOWN_ETA_SENTINEL = -9999.0f;

    if (info.isCharging) {

        // If rate/ETA are unknown
        if (info.hoursToFull <= UNKNOWN_ETA_SENTINEL + 1.0f) {
            info.statusText.Format(g_Texts[lang][TK_STATUS_CHG_UNKNOWN], info.chargePercent);
            info.detailText.Format(g_Texts[lang][TK_DETAIL_CHG_RATE_UNKNOWN],
                info.currentCapacity, info.maxCapacity);
            return;
        }

        if (info.hoursToFull < 0.0f) {
            info.statusText.Format(g_Texts[lang][TK_STATUS_CHG_CALC], info.chargePercent);
        }
        else if (info.hoursToFull <= 0.0f) {
            info.statusText.Format(g_Texts[lang][TK_STATUS_CHG_EST], info.chargePercent);
        }
        else {
            int h = (int)info.hoursToFull;
            int m = (int)((info.hoursToFull - h) * 60.0f);
            if (h > 0)
                info.statusText.Format(g_Texts[lang][TK_STATUS_CHG_HM], info.chargePercent, h, m);
            else
                info.statusText.Format(g_Texts[lang][TK_STATUS_CHG_M], info.chargePercent, m);
        }
        info.detailText.Format(g_Texts[lang][TK_DETAIL_CHG_RATE],
            m_smoothedRate / 1000.0f, info.currentCapacity, info.maxCapacity);
        return;
    }

    if (info.isOnAC && !info.isCharging) {
        info.statusText.Format(g_Texts[lang][TK_STATUS_AC], info.chargePercent);
        info.detailText = g_Texts[lang][TK_DETAIL_AC];
        return;
    }

    // Discharging
    if (info.hoursRemaining <= UNKNOWN_ETA_SENTINEL + 1.0f) {
        info.statusText.Format(g_Texts[lang][TK_STATUS_DISCH_UNKNOWN], info.chargePercent);
        info.detailText.Format(g_Texts[lang][TK_DETAIL_DISCH_RATE_UNKNOWN],
            info.currentCapacity, info.maxCapacity);
        return;
    }

    if (info.hoursRemaining < 0.0f) {
        info.statusText.Format(g_Texts[lang][TK_STATUS_DISCH_CALC], info.chargePercent);
    }
    else if (info.hoursRemaining <= 0.0f) {
        info.statusText.Format(g_Texts[lang][TK_STATUS_DISCH_EST], info.chargePercent);
    }
    else {
        int h = (int)info.hoursRemaining;
        int m = (int)((info.hoursRemaining - h) * 60.0f);
        if (h > 0)
            info.statusText.Format(g_Texts[lang][TK_STATUS_DISCH_HM], info.chargePercent, h, m);
        else
            info.statusText.Format(g_Texts[lang][TK_STATUS_DISCH_M], info.chargePercent, m);
    }

    info.detailText.Format(g_Texts[lang][TK_DETAIL_DISCH_RATE],
        m_smoothedRate / 1000.0f, info.currentCapacity, info.maxCapacity);
}



BatteryTimeInfo BatteryTimeEstimator::GetBatteryTime(int lang)
{
    BatteryTimeInfo info;

    //int lang = eng_lang ? LANG_EN : LANG_JP;

    SYSTEM_POWER_STATUS ps{};
    if (GetSystemPowerStatus(&ps)) {
        info.isOnAC = (ps.ACLineStatus == 1);
        info.chargePercent = (ps.BatteryLifePercent <= 100)
            ? (float)ps.BatteryLifePercent
            : 0.0f;
    }

    SYSTEM_BATTERY_STATE bs{};
    if (!GetBatteryState(bs)) {
        info.statusText = g_Texts[lang][TK_ERR_BATSTATE];
        info.detailText = g_Texts[lang][TK_ERR_BATSTATE_DETAIL];
        return info;
    }

    info.isCharging = !!bs.Charging;
    info.currentCapacity = bs.RemainingCapacity; // mWh
    info.maxCapacity = bs.MaxCapacity;           // mWh
    info.currentRate_mW = bs.Rate;               // signed (+/-), may be 0
    info.designCapacity = 0;
    info.isFullyCharged = (info.chargePercent >= 99.0f && info.isOnAC);

    // Clamp unknown/bogus driver rates from SystemBatteryState
    if (IsUnknownOrBogusRate(info.currentRate_mW)) {
        info.currentRate_mW = 0;
    }

    LONG wmiRateSigned = 0;
    if (TryReadChargeRateFromWMI(wmiRateSigned) &&
        wmiRateSigned != 0 &&
        !IsUnknownOrBogusRate(wmiRateSigned)) {
        info.currentRate_mW = wmiRateSigned;
    }

    bool haveValidRate = (info.currentRate_mW != 0);

    if (haveValidRate) {
        UpdateSmoothedRate(info.currentRate_mW);
    }
    else {
        // force smoothed rate to zero if we don't actually know power
        m_smoothedRate = 0.0f;
    }

    CalculateTimes(info, bs);

    // If we don't have a valid rate at all, mark ETA as "unknown"
    if (!haveValidRate) {
        info.hoursToFull = -9999.0f;
        info.hoursRemaining = -9999.0f;
    }

    FormatStatusText(info, lang);

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

    int lang = eng_lang ? LANG_EN : LANG_JP;

    GdiplusStartupInput gpsi;
    if (GdiplusStartup(&m_gdiplusToken, &gpsi, nullptr) != Ok) {
        AfxMessageBox(g_Texts[lang][TK_ERR_GDIPLUS], MB_ICONWARNING);
    }

    UpdateBatteryInfo();
    StartBatteryMonitoring();

    // Load and set icon
    m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_ICON));
    SetIcon(m_hIcon, TRUE);   // Large icon
    SetIcon(m_hIcon, FALSE);  // Small icon

    if (eng_lang)
    {
        SetWindowTextW(L"Charge/Discharge Rate Information");
    }
    else
    {
        SetWindowTextW(L"充電/放電レート情報");
    }

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
    m_samplesChargeW[m_cursor] = chargeW;
    m_samplesDischargeW[m_cursor] = dischargeW;

    m_cursor = (m_cursor + 1) % kWindowSeconds;
    if (!m_filled && m_cursor == 0) m_filled = true;
}

void CRateInfoDlg::UpdateBatteryInfo()
{
    int lang = eng_lang ? LANG_EN : LANG_JP;

    m_last = m_estimator.GetBatteryTime(lang);

    float chargeW = kMissing, dischargeW = kMissing;
    if (m_last.currentRate_mW > 0) {
        chargeW = (float)m_last.currentRate_mW / 1000.0f; // W
    }
    else if (m_last.currentRate_mW < 0) {
        dischargeW = std::fabs((float)m_last.currentRate_mW) / 1000.0f; // W
    }

    PushSamples(chargeW, dischargeW);
    Invalidate(FALSE);
}

BOOL CRateInfoDlg::OnEraseBkgnd(CDC* pDC)
{
    return TRUE;
}

void CRateInfoDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect rcClient;
    GetClientRect(&rcClient);

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);

    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, rcClient.Width(), rcClient.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    Graphics g(memDC.m_hDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    SolidBrush bgBrush(Color(255, 240, 240, 240));
    g.FillRectangle(&bgBrush, 0, 0, rcClient.Width(), rcClient.Height());

    const float margin = 12.0f;
    float x = (float)rcClient.left + margin;
    float y = (float)rcClient.top + margin;
    float w = (float)rcClient.Width() - 2.0f * margin;

    float hCard = 110.0f;
    DrawBatteryCard(g, x, y, w, hCard);

    float gap = 10.0f;
    float yChart = y + hCard + gap;
    float hChart = std::max(140.0f, (float)rcClient.Height() - (hCard + 3 * margin));
    DrawLivePowerChart(g, x, yChart, w, hChart);

    dc.BitBlt(0, 0, rcClient.Width(), rcClient.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

void CRateInfoDlg::DrawBatteryCard(Graphics& g, float x, float y, float w, float h)
{
    SolidBrush bg(Color(255, 255, 255, 255));
    g.FillRectangle(&bg, x, y, w, h);

    Pen border(Color(255, 210, 210, 210), 1.5f);
    g.DrawRectangle(&border, x, y, w, h);

    Gdiplus::FontFamily fam(L"Segoe UI");
    Gdiplus::Font fTitle(&fam, 11.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font fDetail(&fam, 9.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);

    SolidBrush textDark(Color(255, 40, 40, 40));
    SolidBrush textMid(Color(255, 100, 100, 100));

    g.DrawString(m_last.statusText.GetString(), -1, &fTitle,
        Gdiplus::PointF(x + 12.f, y + 10.f), &textDark);

    g.DrawString(m_last.detailText.GetString(), -1, &fDetail,
        Gdiplus::PointF(x + 12.f, y + 34.f), &textMid);

    float barY = y + 64.f;
    float barH = 20.f;
    float barW = w - 24.f;
    float barX = x + 12.f;

    SolidBrush barBg(Color(255, 235, 235, 235));
    g.FillRectangle(&barBg, barX, barY, barW, barH);

    Color fillColor;
    if (m_last.isCharging) fillColor = Color(255, 0, 180, 0);
    else if (m_last.chargePercent < 20.0f) fillColor = Color(255, 220, 20, 60);
    else if (m_last.chargePercent < 50.0f) fillColor = Color(255, 255, 165, 0);
    else fillColor = Color(255, 0, 122, 204);

    float fillW = barW * (m_last.chargePercent / 100.0f);
    fillW = std::max(0.0f, std::min(fillW, barW));

    SolidBrush barFill(fillColor);
    g.FillRectangle(&barFill, barX, barY, fillW, barH);

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
    const bool showCharge = (m_last.currentRate_mW > 0) || m_last.isCharging;
    int lang = eng_lang ? LANG_EN : LANG_JP;

    const std::vector<float>& series = showCharge ? m_samplesChargeW : m_samplesDischargeW;
    const WCHAR* chartTitle = showCharge
        ? g_Texts[lang][TK_CHART_TITLE_CHG]
        : g_Texts[lang][TK_CHART_TITLE_DIS];

    // Background
    SolidBrush bg(Color(255, 252, 252, 252));
    g.FillRectangle(&bg, x, y, w, h);

    Pen border(Color(255, 210, 210, 210), 1.5f);
    g.DrawRectangle(&border, x, y, w, h);

    const float padL = 46.f, padR = 12.f, padT = 40.f, padB = 24.f;
    const float px = x + padL;
    const float py = y + padT;
    const float pw = w - padL - padR;
    const float ph = h - padT - padB;

    // Fonts
    Gdiplus::FontFamily fam(L"Segoe UI");
    Gdiplus::Font fTitle(&fam, 11.f, FontStyleBold, UnitPoint);
    Gdiplus::Font fAxis(&fam, 9.f, FontStyleRegular, UnitPoint);
    Gdiplus::Font fBadge(&fam, 11.f, FontStyleBold, UnitPoint);

    SolidBrush textDark(Color(255, 40, 40, 40));
    SolidBrush textMid(Color(255, 110, 110, 110));
    SolidBrush overlayText(Color(180, 80, 80, 80));

    // Title
    CString title;
    title.Format(g_Texts[lang][TK_CHART_TITLE_FMT], chartTitle, kWindowSeconds);
    g.DrawString(title, -1, &fTitle, PointF(x + 12.f, y + 6.f), &textDark);

    // Find max value
    float maxW = 1.0f;
    const int N = kWindowSeconds;
    for (int i = 0; i < N; ++i) {
        float v = series[i];
        if (!isNaNf(v))
            maxW = std::max(maxW, v);
    }

    maxW *= 1.15f;
    maxW = std::max(1.0f, std::ceil(maxW * 2.0f) / 2.0f);

    // Grid + Y labels
    Pen grid(Color(255, 230, 230, 230), 1.0f);
    const int gridLines = 5;

    for (int i = 0; i <= gridLines; ++i) {
        float yy = py + ph * (1.0f - (float)i / gridLines);
        g.DrawLine(&grid, px, yy, px + pw, yy);

        float val = maxW * (float)i / gridLines;
        CString lbl;
        lbl.Format(L"%.1f W", val);
        g.DrawString(lbl, -1, &fAxis, PointF(x + 6.f, yy - 9.f), &textMid);
    }

    // X labels
    {
        const int step = N / 4;
        CString l0, l1, l2, l3, l4;
        l0.Format(L"%ds", 0);
        l1.Format(L"%ds", step);
        l2.Format(L"%ds", 2 * step);
        l3.Format(L"%ds", 3 * step);
        l4.Format(L"%ds", N);

        const float yLab = py + ph + 4.f;
        g.DrawString(l0, -1, &fAxis, PointF(px - 6.f, yLab), &textMid);
        g.DrawString(l1, -1, &fAxis, PointF(px + pw * 0.25f - 12.f, yLab), &textMid);
        g.DrawString(l2, -1, &fAxis, PointF(px + pw * 0.50f - 12.f, yLab), &textMid);
        g.DrawString(l3, -1, &fAxis, PointF(px + pw * 0.75f - 12.f, yLab), &textMid);
        g.DrawString(l4, -1, &fAxis, PointF(px + pw - 22.f, yLab), &textMid);
    }

    // Axes
    Pen axis(Color(255, 180, 180, 180), 1.2f);
    g.DrawLine(&axis, px, py, px, py + ph);
    g.DrawLine(&axis, px, py + ph, px + pw, py + ph);

    // Mapping helpers
    auto idxToSample = [&](int k)->float {
        int base = m_filled ? m_cursor : 0;
        int realIdx = m_filled ? ((base + k) % N) : k;
        if (!m_filled && k >= m_cursor)
            return kMissing;
        return series[realIdx];
        };

    auto mapX = [&](int k)->float {
        return px + (float)k * (pw / (float)(N - 1));
        };

    auto mapY = [&](float watts)->float {
        float t = (maxW <= 0.0f) ? 0.0f : (watts / maxW);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return py + ph * (1.0f - t);
        };

    // Line
    Pen linePen(showCharge
        ? Color(255, 0, 180, 0)
        : Color(255, 0, 122, 204), 2.0f);

    bool hasPrev = false;
    PointF prev{};
    for (int k = 0; k < N; ++k) {
        float sample = idxToSample(k);
        if (isNaNf(sample)) {
            hasPrev = false;
            continue;
        }
        PointF cur(mapX(k), mapY(sample));
        if (hasPrev)
            g.DrawLine(&linePen, prev, cur);
        prev = cur;
        hasPrev = true;
    }

    // Latest value
    float latest = kMissing;
    if (m_filled || m_cursor > 0) {
        int newestIdx = (m_cursor + N - 1) % N;
        latest = series[newestIdx];
    }

    CString latestTxt;

    if (isNaNf(latest)) {
        latestTxt = showCharge
            ? g_Texts[lang][TK_LATEST_NOT_CHG]
            : g_Texts[lang][TK_LATEST_NOT_DIS];

        // Centered overlay text
        RectF rc;
        g.MeasureString(latestTxt, -1, &fBadge, PointF(0, 0), &rc);

        float cx = px + (pw - rc.Width) * 0.5f;
        float cy = py + (ph - rc.Height) * 0.5f;

        g.DrawString(latestTxt, -1, &fBadge, PointF(cx, cy), &overlayText);
    }
    else {
        latestTxt.Format(L"%.2f W", latest);

        // Top-right badge
        g.DrawString(
            latestTxt,
            -1,
            &fBadge,
            PointF(px + pw - 110.f, py + 4.f),
            &textDark
        );
    }
}


//void CRateInfoDlg::DrawLivePowerChart(Graphics& g, float x, float y, float w, float h)
//{
//    const bool showCharge = (m_last.currentRate_mW > 0) || m_last.isCharging;
//
//    int lang = eng_lang ? LANG_EN : LANG_JP;
//
//    const std::vector<float>& series = showCharge ? m_samplesChargeW : m_samplesDischargeW;
//    const WCHAR* chartTitle = showCharge ? g_Texts[lang][TK_CHART_TITLE_CHG] : g_Texts[lang][TK_CHART_TITLE_DIS];
//
//    SolidBrush bg(Color(255, 252, 252, 252));
//    g.FillRectangle(&bg, x, y, w, h);
//
//    Pen border(Color(255, 210, 210, 210), 1.5f);
//    g.DrawRectangle(&border, x, y, w, h);
//
//    const float padL = 46.f, padR = 12.f, padT = 40.f, padB = 24.f;
//    const float px = x + padL, py = y + padT, pw = w - padL - padR, ph = h - padT - padB;
//
//    Gdiplus::FontFamily fam(L"Segoe UI");
//    Gdiplus::Font fTitle(&fam, 11.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
//    Gdiplus::Font fAxis(&fam, 9.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
//    SolidBrush textDark(Color(255, 40, 40, 40));
//    SolidBrush textMid(Color(255, 110, 110, 110));
//
//    CString title; title.Format(g_Texts[lang][TK_CHART_TITLE_FMT], chartTitle, kWindowSeconds);
//    g.DrawString(title, -1, &fTitle, Gdiplus::PointF(x + 12.f, y + 6.f), &textDark);
//
//    float maxW = 1.0f;
//    const int N = kWindowSeconds;
//    for (int i = 0; i < N; ++i) {
//        float v = series[i];
//        if (!isNaNf(v)) maxW = std::max(maxW, v);
//    }
//    maxW *= 1.15f;
//    maxW = std::max(1.0f, std::ceil(maxW * 2.0f) / 2.0f);
//
//    Pen grid(Color(255, 230, 230, 230), 1.0f);
//    const int gridLines = 5;
//    for (int i = 0; i <= gridLines; ++i) {
//        float yy = py + ph * (1.0f - (float)i / gridLines);
//        g.DrawLine(&grid, px, yy, px + pw, yy);
//
//        float val = maxW * (float)i / gridLines;
//        CString lbl; lbl.Format(L"%.1f W", val);
//        g.DrawString(lbl, -1, &fAxis, Gdiplus::PointF(x + 6.f, yy - 9.f), &textMid);
//    }
//
//    {
//        const int step = N / 4;
//        CString l0, l1, l2, l3, l4;
//        l0.Format(L"%ds", 0);
//        l1.Format(L"%ds", step);
//        l2.Format(L"%ds", 2 * step);
//        l3.Format(L"%ds", 3 * step);
//        l4.Format(L"%ds", N);
//
//        const float yLab = py + ph + 4.f;
//        g.DrawString(l0, -1, &fAxis, Gdiplus::PointF(px - 6.f, yLab), &textMid);
//        g.DrawString(l1, -1, &fAxis, Gdiplus::PointF(px + pw * 0.25f - 12.f, yLab), &textMid);
//        g.DrawString(l2, -1, &fAxis, Gdiplus::PointF(px + pw * 0.50f - 12.f, yLab), &textMid);
//        g.DrawString(l3, -1, &fAxis, Gdiplus::PointF(px + pw * 0.75f - 12.f, yLab), &textMid);
//        g.DrawString(l4, -1, &fAxis, Gdiplus::PointF(px + pw - 22.f, yLab), &textMid);
//    }
//
//    Pen axis(Color(255, 180, 180, 180), 1.2f);
//    g.DrawLine(&axis, px, py, px, py + ph);
//    g.DrawLine(&axis, px, py + ph, px + pw, py + ph);
//
//    auto idxToSample = [&](int k)->float {
//        int base = m_filled ? m_cursor : 0;
//        int realIdx = m_filled ? ((base + k) % N) : k;
//        if (!m_filled && k >= m_cursor) return kMissing;
//        return series[realIdx];
//        };
//
//    auto mapX = [&](int k)->float {
//        return px + (float)k * (pw / (float)(N - 1));
//        };
//
//    auto mapY = [&](float watts)->float {
//        float t = (maxW <= 0.0f) ? 0.0f : (watts / maxW);
//        if (t < 0.0f) t = 0.0f; if (t > 1.0f) t = 1.0f;
//        return py + ph * (1.0f - t);
//        };
//
//    Pen linePen(showCharge ? Color(255, 0, 180, 0) : Color(255, 0, 122, 204), 2.0f);
//
//    bool hasPrev = false;
//    PointF prev{};
//    for (int k = 0; k < N; ++k) {
//        float sample = idxToSample(k);
//        if (isNaNf(sample)) { hasPrev = false; continue; }
//        PointF cur(mapX(k), mapY(sample));
//        if (hasPrev) g.DrawLine(&linePen, prev, cur);
//        prev = cur; hasPrev = true;
//    }
//
//    float latest = kMissing;
//    if (m_filled || m_cursor > 0) {
//        int newestIdx = (m_cursor + N - 1) % N;
//        latest = series[newestIdx];
//    }
//    CString latestTxt;
//    if (isNaNf(latest)) latestTxt = showCharge ? g_Texts[lang][TK_LATEST_NOT_CHG] : g_Texts[lang][TK_LATEST_NOT_DIS];
//    else latestTxt.Format(L"%.2f W", latest);
//
//    Gdiplus::Font fBadge(&fam, 9.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
//    SolidBrush badgeText(Color(255, 40, 40, 40));
//    g.DrawString(latestTxt, -1, &fBadge,
//        Gdiplus::PointF(px + pw - 110.f, py + 4.f), &badgeText);
//
//}
