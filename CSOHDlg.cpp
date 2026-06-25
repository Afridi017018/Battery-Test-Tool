#include "pch.h"
#include "CSOHDlg.h"
#define NOMINMAX
#include <ctime>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <windows.h>
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")

IMPLEMENT_DYNAMIC(CSOHDlg, CDialogEx)

// ── Palette: clean white + single blue accent ─────────────────────────────────
static const COLORREF CLR_BG = RGB(255, 255, 255);
static const COLORREF CLR_ACCENT = RGB(30, 120, 255);   // vivid blue
static const COLORREF CLR_ACCENT_HOV = RGB(55, 140, 255);
static const COLORREF CLR_ACCENT_PRS = RGB(10, 90, 210);
static const COLORREF CLR_TEXT_DARK = RGB(20, 20, 30);
static const COLORREF CLR_TEXT_MID = RGB(90, 90, 110);
static const COLORREF CLR_TEXT_LIGHT = RGB(160, 160, 175);
static const COLORREF CLR_RULE = RGB(230, 230, 235);
static const COLORREF CLR_RANGE_IDL = RGB(245, 246, 248);
static const COLORREF CLR_RANGE_SEL = RGB(30, 120, 255);
static const COLORREF CLR_RANGE_HOV = RGB(235, 240, 255);
static const COLORREF CLR_RANGE_BDR = RGB(210, 215, 225);

// ── Constructor ───────────────────────────────────────────────────────────────
CSOHDlg::CSOHDlg(CWnd* pParent, bool engLang)
    : CDialogEx(IDD_SOH, pParent)
    , m_engLang(engLang)
    , m_lastPercent(-1), m_lastDropTime(0)
    , m_isFullyCharged(false), m_isDischarging(false)
    , m_startDischargeTime(0), m_stressRunning(false)
    , m_testStarted(false)
    , m_testComplete(false)
    , m_targetStopPercent(-1)
    , m_startBtnHover(false)
    , m_startBtnPressed(false)
    , m_hoveredRange(-1)
    , m_targetMinutes(0)
    , m_sliderDragging(false)
    , m_currentPercent(0)
{
}

CSOHDlg::~CSOHDlg()
{
    StopCpuStress();
    if (m_logFile.is_open()) m_logFile.close();
}

void CSOHDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSOHDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_TIMER()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_LBUTTONUP()
    ON_WM_CTLCOLOR()
    ON_WM_SIZE()
END_MESSAGE_MAP()

// ── Background ────────────────────────────────────────────────────────────────
BOOL CSOHDlg::OnEraseBkgnd(CDC* pDC)
{
    CRect r; GetClientRect(&r);
    pDC->FillSolidRect(&r, CLR_BG);
    return TRUE;
}

// Make child static controls white-background
HBRUSH CSOHDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    if (nCtlColor == CTLCOLOR_STATIC)
    {
        pDC->SetBkMode(TRANSPARENT);
        pDC->SetTextColor(CLR_TEXT_DARK);
        return (HBRUSH)m_brWhite.GetSafeHandle();
    }
    return CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}

// ── Responsive layout ─────────────────────────────────────────────────────────
void CSOHDlg::RecalcLayout()
{
    CRect rc; GetClientRect(&rc);
    int W = rc.Width(), H = rc.Height();
    int mx = max(16, W / 20);          // horizontal margin ~5% of width

    // --- instruction label: top 12% of height
    int instrH = max(40, H / 8);
    if (m_lblInstruction.GetSafeHwnd())
        m_lblInstruction.MoveWindow(mx, 12, W - mx * 2, instrH);

    // --- progress bar: thin strip below instruction
    int progY = instrH + 18;
    int progH = max(6, H / 60);
    if (m_progress.GetSafeHwnd())
        m_progress.MoveWindow(mx, progY, W - mx * 2, progH);

    // --- big percent label: center zone ~30% of height
    int percY = progY + progH + 8;
    int percH = max(80, H * 30 / 100);
    if (m_lblPercent.GetSafeHwnd())
        m_lblPercent.MoveWindow(mx, percY, W - mx * 2, percH);

    // --- range buttons row: below percent (extra gap for DISCHARGE RANGE label)
    int rbY = percY + percH + 36;
    int rbH = max(34, H / 12);
    int rbGap = max(6, W / 60);
    int rbTotalW = W - mx * 2;
    int rbW = (rbTotalW - rbGap * 3) / 4;
    for (int i = 0; i < (int)m_rangeBtns.size(); ++i)
    {
        m_rangeBtns[i].rc.SetRect(
            mx + i * (rbW + rbGap), rbY,
            mx + i * (rbW + rbGap) + rbW, rbY + rbH);
    }

    //// --- start button: below range row, centered
    //int sbW = min(220, W - mx * 4);
    //int sbH = max(40, H / 11);
    //int sbY = rbY + rbH + max(14, H / 22);
    //m_rcStartBtn.SetRect((W - sbW) / 2, sbY, (W + sbW) / 2, sbY + sbH);



    // --- time buttons row: below range buttons
    int tbY = rbY + rbH + max(30, H / 14);
    int tbH = max(34, H / 12);   // slider total height (track + labels)
    m_timeSlider.rcTrack.SetRect(mx, tbY, W - mx, tbY + tbH);





    // --- start button: below time row, centered — capped to always fit
    int sbW = min(220, W - mx * 4);
    int sbH = max(34, H / 13);
    int elH = max(24, H / 16);
    int elY = H - elH - 8;
    int sbGap = max(8, H / 22);
    int sbY = tbY + tbH + sbGap;
    // If it would overflow into the elapsed label zone, clamp it up
    int sbYMax = elY - sbH - sbGap;
    if (sbY > sbYMax) sbY = sbYMax;
    // If still negative (window extremely small), just place it below time row
    if (sbY < tbY + tbH + 4) sbY = tbY + tbH + 4;
    m_rcStartBtn.SetRect((W - sbW) / 2, sbY, (W + sbW) / 2, sbY + sbH);

    // --- elapsed label: near bottom
    if (m_lblElapsed.GetSafeHwnd())
        m_lblElapsed.MoveWindow(mx, elY, W - mx * 2, elH);


}

void CSOHDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (m_lblInstruction.GetSafeHwnd())
    {
        RecalcLayout();
        Invalidate(FALSE);
    }
}



BOOL CSOHDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetWindowTextW(T(L"Battery SOH Test", L"バッテリーSOHテスト"));

    m_brWhite.CreateSolidBrush(CLR_BG);

    // Log file
    wchar_t appDataPath[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath);
    std::wstring folder = std::wstring(appDataPath) + L"\\SOHTool";
    CreateDirectoryW(folder.c_str(), NULL);
    m_logFile.open(std::wstring(folder + L"\\soh_log.txt"), std::ios::out | std::ios::app);

    time_t now = time(NULL);
    CString tmp; tmp.Format(_T("%lld"), (long long)now);
    CT2A ascii(tmp); m_testID = std::string(ascii);

    // Fonts — all Segoe UI, size relative to screen DPI
    m_fontTitle.CreatePointFont(110, _T("Segoe UI"));
    m_fontInstruction.CreatePointFont(95, _T("Segoe UI"));
    m_fontPercent.CreatePointFont(340, _T("Segoe UI Light"));
    m_fontBtn.CreatePointFont(105, _T("Segoe UI Semibold"));
    m_fontRange.CreatePointFont(88, _T("Segoe UI"));

    CRect rcClient; GetClientRect(&rcClient);
    int W = rcClient.Width(), H = rcClient.Height();
    int mx = max(16, W / 20);

    // Create child controls at placeholder rects — RecalcLayout fixes them
    m_lblInstruction.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        CRect(mx, 12, W - mx, 60), this);
    m_lblInstruction.SetFont(&m_fontInstruction);

    m_progress.Create(WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        CRect(mx, 65, W - mx, 75), this, 1001);
    m_progress.SetRange(0, 100); m_progress.SetPos(0);

    m_lblPercent.Create(_T("--"), WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        CRect(mx, 80, W - mx, 230), this);
    m_lblPercent.SetFont(&m_fontPercent);

    m_lblElapsed.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
        CRect(mx, H - 40, W - mx, H - 10), this);
    m_lblElapsed.SetFont(&m_fontInstruction);

    // Range buttons metadata (rects computed by RecalcLayout)
    // Labels are dynamic (built in DrawRangeButtons using live %), just store target
    struct { int stop; const wchar_t* lbl; } ranges[] = {
        {70,L"→ 70%"},{50,L"→ 50%"},{30,L"→ 30%"},{0,L"→ 0%"}
    };
    for (int i = 0; i < 4; ++i)
    {
        RangeBtn rb; rb.stopAt = ranges[i].stop; rb.label = ranges[i].lbl;
        m_rangeBtns.push_back(rb);
    }
    m_targetStopPercent = -1;   // nothing selected yet

    // Time buttons metadata
    m_timeSlider.minVal = 10;
    m_timeSlider.maxVal = 120;
    m_targetMinutes = 0;

    // Seed current battery percent
    SYSTEM_POWER_STATUS sps = {};
    if (GetSystemPowerStatus(&sps) && sps.BatteryLifePercent <= 100)
        m_currentPercent = sps.BatteryLifePercent;
    else
        m_currentPercent = 0;

    RecalcLayout();

    m_instruction = T(L"Select a discharge range or time, then press Start",
        L"放電範囲または時間を選択して「開始」を押してください");
    m_testStarted = true;          // temporarily allow % to show
    UpdateUI(m_currentPercent);
    m_testStarted = false;         // restore
    m_lblInstruction.SetWindowTextW(m_instruction);

    // m_isFullyCharged intentionally NOT set to true here —
    // it will be set naturally when battery reads 100% on charger.

    return TRUE;
}


// ── Start test ────────────────────────────────────────────────────────────────
void CSOHDlg::StartTest()
{
    if (m_testStarted) return;
    m_testStarted = true;

    SYSTEMTIME st; GetLocalTime(&st);
    if (m_logFile.is_open())
    {
        m_logFile << "\n=====================================================\n";
        m_logFile << "                NEW SOH TEST\n";
        m_logFile << "TEST_ID: " << m_testID << "\n";
        /*m_logFile << "Range: 100→" << m_targetStopPercent << "\n";*/
        m_logFile << "Range: " << m_currentPercent << "→" << m_targetStopPercent << "\n";
        m_logFile << "Start Time: "
            << st.wYear << "-" << st.wMonth << "-" << st.wDay << " "
            << st.wHour << ":" << st.wMinute << ":" << st.wSecond << "\n";
        m_logFile << "=====================================================\n";
        m_logFile.flush();
    }

    m_lastDropTime = GetTickCount64();
    m_instruction = T(L"Please charge battery to 100%",
        L"バッテリーを100%まで充電してください");
    UpdateInstruction();
    Invalidate(FALSE);
    SetTimer(1, 1000, NULL);
}

void CSOHDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1)
    {
        UpdateBatteryStatus();
        UpdateInstruction();
    }
    else if (nIDEvent == 2)   // post-complete charger watch
    {
        SYSTEM_POWER_STATUS sps;
        if (GetSystemPowerStatus(&sps) && sps.ACLineStatus == 1)
        {
            KillTimer(2);
            PostMessage(WM_CLOSE);
        }
    }
    CDialogEx::OnTimer(nIDEvent);
}

// ── Battery logic ─────────────────────────────────────────────────────────────
void CSOHDlg::UpdateBatteryStatus()
{
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps)) return;

    int percent = sps.BatteryLifePercent;
    if (percent < 0 || percent > 100) return;
    ULONGLONG now = GetTickCount64();

    m_currentPercent = percent;

    // ── Range mode: needs charger unplugged, selected target < current % ──
    // ── Time mode:  just needs charger unplugged ──────────────────────────

    bool usingTimeMode = (m_targetMinutes > 0 && m_targetStopPercent < 0);
    bool usingRangeMode = (!usingTimeMode && m_targetStopPercent >= 0);

    // Determine fully charged: only relevant to range mode for user hint
    if (percent == 100 && sps.ACLineStatus == 1)
        m_isFullyCharged = true;

    // Start discharging when charger unplugged after test started
    if (m_testStarted && !m_isDischarging && sps.ACLineStatus == 0)
    {
        // For range mode: current % must be above target
        bool canStart = usingTimeMode || (usingRangeMode && percent > m_targetStopPercent);
        if (canStart)
        {
            m_isDischarging = true;
            m_lastDropTime = m_startDischargeTime = now;
            StartCpuStress();
        }
    }

    // Update instruction hint
    if (m_testStarted && !m_isDischarging)
    {
        if (sps.ACLineStatus == 1)
        {
            if (usingRangeMode && percent <= m_targetStopPercent)
                m_instruction = T(L"Current charge is already at or below target!",
                    L"現在の充電量はすでに目標以下です！");
            else
                m_instruction = T(L"Unplug the charger to begin discharging",
                    L"充電器を抜いて放電を開始してください");
        }
    }

    // Log per-percent drop
    if (m_isDischarging && m_lastPercent != -1 && percent < m_lastPercent)
    {
        LogData(percent, now - m_lastDropTime);
        m_lastDropTime = now;
    }

    UpdateUI(percent);
    m_lastPercent = percent;



    // Stop by percent (range mode)
    if (m_isDischarging && usingRangeMode && percent <= m_targetStopPercent)
    {
        KillTimer(1); StopCpuStress();
        m_testComplete = true;   // ← add
        m_instruction = T(L"Test complete — data saved.",
            L"テスト完了 — データを保存しました。");
        UpdateInstruction(); UpdateUI(percent); Invalidate(FALSE);
        SetTimer(2, 1000, NULL); // ← add: watch for charger plug-in
        return;
    }

    // Stop by time (time mode)
    if (m_isDischarging && usingTimeMode)
    {
        ULONGLONG elapsedMin = (now - m_startDischargeTime) / 60000;
        if (elapsedMin >= (ULONGLONG)m_targetMinutes)
        {
            KillTimer(1); StopCpuStress();
            m_testComplete = true;   // ← add
            m_instruction = _T("Test complete — data saved.");
            UpdateInstruction(); UpdateUI(percent); Invalidate(FALSE);
            SetTimer(2, 1000, NULL); // ← add: watch for charger plug-in
            return;
        }
    }

    // Charger plugged back in mid-test
    if (m_isDischarging && sps.ACLineStatus == 1)
    {
        if (m_logFile.is_open())
        {
            SYSTEMTIME st; GetLocalTime(&st);
            m_logFile << "[" << m_testID << "] "
                << "TEST STOPPED: Charger plugged in at "
                << st.wHour << ":" << st.wMinute << ":" << st.wSecond
                << " | Percent: " << percent << std::endl;
            m_logFile.flush();
        }
        KillTimer(1); StopCpuStress();
        m_instruction = T(L"Test stopped: charger detected. Closing...",
            L"テスト停止：充電器を検出しました。終了中...");
        UpdateInstruction(); UpdateUI(percent);
        PostMessage(WM_CLOSE);
    }
}

void CSOHDlg::LogData(int percent, ULONGLONG duration)
{
    if (!m_logFile.is_open()) return;

    ULONGLONG totalSec = duration / 1000;
    int hrs = static_cast<int>(totalSec / 3600);
    int mins = static_cast<int>((totalSec % 3600) / 60);
    int secs = static_cast<int>(totalSec % 60);

    SYSTEMTIME st; GetLocalTime(&st);

    m_logFile << "[" << m_testID << "] "
        << "Time: "
        << st.wHour << ":" << st.wMinute << ":" << st.wSecond
        << " | Percent: " << percent
        << " | Duration(ms): " << duration
        << " | Duration(h:m:s): " << hrs << ":" << mins << ":" << secs
        << std::endl;
    m_logFile.flush();
}



void CSOHDlg::UpdateInstruction()
{
    bool usingTimeMode = (m_targetMinutes > 0 && m_targetStopPercent < 0);
    bool usingRangeMode = (!usingTimeMode && m_targetStopPercent >= 0);

    if (!m_testStarted)
    {
        m_instruction = T(L"Select a discharge range or time, then press Start",
            L"放電範囲または時間を選択して「開始」を押してください");
    }
    else if (!m_isDischarging)
    {
        SYSTEM_POWER_STATUS sps = {};
        GetSystemPowerStatus(&sps);
        if (sps.ACLineStatus == 1)
        {
            if (usingRangeMode && m_currentPercent <= m_targetStopPercent)
                m_instruction = T(L"Current charge is already at or below target — pick a different range",
                    L"現在の充電量はすでに目標以下です — 別の範囲を選択してください");
            else
                m_instruction = T(L"Unplug the charger to begin discharging",
                    L"充電器を抜いて放電を開始してください");
        }
        else
        {
            m_instruction = T(L"Unplug the charger to begin discharging",
                L"充電器を抜いて放電を開始してください");
        }
    }
    else
    {
        // Actively discharging
        if (usingRangeMode)
        {
            CString s;
            s.Format(T(L"Discharging… recording data  |  target: %d%%",
                L"放電中… データ記録中  |  目標: %d%%"),
                m_targetStopPercent);
            m_instruction = s;
        }
        else if (usingTimeMode)
        {
            ULONGLONG elapsedMin = (GetTickCount64() - m_startDischargeTime) / 60000;
            CString s;
            s.Format(T(L"Discharging… %llu / %d min elapsed",
                L"放電中… %llu / %d 分経過"),
                elapsedMin, m_targetMinutes);
            m_instruction = s;
        }
        else
        {
            m_instruction = T(L"Discharging… recording data",
                L"放電中… データ記録中");
        }
    }


    //if (m_lblInstruction.GetSafeHwnd())
    //    m_lblInstruction.SetWindowTextW(m_instruction);
    if (m_lblInstruction.GetSafeHwnd())
    {
        CString cur;
        m_lblInstruction.GetWindowTextW(cur);
        if (cur != m_instruction)                  // ← only set when changed
            m_lblInstruction.SetWindowTextW(m_instruction);
    }
}

void CSOHDlg::UpdateUI(int percent)
{
    int c = percent < 0 ? 0 : min(percent, 100);
    if (m_progress.GetSafeHwnd())   m_progress.SetPos(c);
    if (m_lblPercent.GetSafeHwnd())
    {
        CString pct;
        if (!m_testStarted) pct = _T("--");
        else pct.Format(_T("%d%%"), c);
        m_lblPercent.SetWindowTextW(pct);
    }
    if (m_lblElapsed.GetSafeHwnd())
    {
        if (m_isDischarging && m_startDischargeTime)
        {
            ULONGLONG s = (GetTickCount64() - m_startDischargeTime) / 1000;
            CString e; e.Format(T(L"Elapsed  %02d:%02d:%02d",
                L"経過時間  %02d:%02d:%02d"),
                (int)(s / 3600), (int)((s % 3600) / 60), (int)(s % 60));
            m_lblElapsed.SetWindowTextW(e);
        }
        else m_lblElapsed.SetWindowTextW(_T(""));
    }
  /*  if (m_lblInstruction.GetSafeHwnd())
        m_lblInstruction.SetWindowTextW(m_instruction);*/
}

// ── GDI helpers ───────────────────────────────────────────────────────────────
static void GdiFillRoundRect(CDC* dc, const CRect& r, int rad, COLORREF fill,
    COLORREF bdr = (COLORREF)-1, int bw = 0)
{
    CBrush br(fill); CBrush* ob = dc->SelectObject(&br);
    CPen nullPen(PS_NULL, 0, RGB(0, 0, 0));
    CPen bdrPen(PS_SOLID, bw, bdr);
    CPen* op = dc->SelectObject(bdr != (COLORREF)-1 && bw > 0 ? &bdrPen : &nullPen);
    dc->RoundRect(r.left, r.top, r.right, r.bottom, rad, rad);
    dc->SelectObject(ob); dc->SelectObject(op);
}

// ── Range buttons ─────────────────────────────────────────────────────────────
void CSOHDlg::DrawRangeButtons(CDC* dc)
{
    dc->SetBkMode(TRANSPARENT);
    CFont* of = dc->SelectObject(&m_fontRange);

    for (auto& rb : m_rangeBtns)
    {
        bool sel = (rb.stopAt == m_targetStopPercent);
        bool hov = (!m_testStarted && m_hoveredRange == rb.stopAt);
        bool dim = m_testStarted;

        COLORREF fill = dim ? CLR_RANGE_IDL : sel ? CLR_RANGE_SEL : hov ? CLR_RANGE_HOV : CLR_RANGE_IDL;
        COLORREF bdr = dim ? CLR_RANGE_BDR : sel ? CLR_ACCENT : hov ? CLR_ACCENT : CLR_RANGE_BDR;
        COLORREF txtc = dim ? CLR_TEXT_LIGHT : sel ? RGB(255, 255, 255) : CLR_TEXT_DARK;

        GdiFillRoundRect(dc, rb.rc, 8, fill, bdr, sel && !dim ? 2 : 1);
        dc->SetTextColor(txtc);
        dc->DrawText(rb.label, const_cast<CRect*>(&rb.rc),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    dc->SelectObject(of);
}

// Remove the entire DrawTimeButtons function and replace with:

void CSOHDlg::DrawTimeSlider(CDC* dc)
{
    if (m_testStarted)
    {
        // Dimmed readout only
        dc->SetBkMode(TRANSPARENT);
        dc->SetTextColor(CLR_TEXT_LIGHT);
        CFont* of = dc->SelectObject(&m_fontRange);
        CString lbl;
        if (m_targetMinutes > 0)
            lbl.Format(_T("%d min"), m_targetMinutes);
        else
            lbl = _T("—");
        dc->DrawText(lbl, &m_timeSlider.rcTrack, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        dc->SelectObject(of);
        return;
    }

    const CRect& rT = m_timeSlider.rcTrack;
    int trackH = 4;
    int thumbR = 9;
    int trackY = rT.top + (rT.Height() / 2) - trackH / 2;  // vertically centered in upper half
    int trackL = rT.left + thumbR + 2;
    int trackR = rT.right - thumbR - 2;
    int trackW = trackR - trackL;

    // --- track background
    CRect rcTrackBg(trackL, trackY, trackR, trackY + trackH);
    GdiFillRoundRect(dc, rcTrackBg, 2, CLR_RANGE_BDR);

    // --- filled portion
    int thumbX = trackL;
    if (m_targetMinutes > 0)
    {
        float t = float(m_targetMinutes - m_timeSlider.minVal)
            / float(m_timeSlider.maxVal - m_timeSlider.minVal);
        thumbX = trackL + int(t * trackW);
        CRect rcFill(trackL, trackY, thumbX, trackY + trackH);
        GdiFillRoundRect(dc, rcFill, 2, CLR_ACCENT);
    }

    // --- tick marks (10, 30, 60, 90, 120)
    const int ticks[] = { 10, 30, 60, 90, 120 };
    dc->SetTextColor(CLR_TEXT_LIGHT);
    CFont* of = dc->SelectObject(&m_fontRange);
    for (int v : ticks)
    {
        float t = float(v - m_timeSlider.minVal) / float(m_timeSlider.maxVal - m_timeSlider.minVal);
        int   tx = trackL + int(t * trackW);
        // tick line
        CPen tick(PS_SOLID, 1, CLR_RANGE_BDR);
        CPen* op = dc->SelectObject(&tick);
        dc->MoveTo(tx, trackY - 3);
        dc->LineTo(tx, trackY + trackH + 3);
        dc->SelectObject(op);
        // tick label
        CString s; s.Format(_T("%d"), v);
        CRect rcLbl(tx - 18, trackY + trackH + 5, tx + 18, trackY + trackH + 22);
        dc->DrawText(s, &rcLbl, DT_CENTER | DT_TOP | DT_SINGLELINE);
    }
    dc->SelectObject(of);

    // --- thumb
    if (m_targetMinutes > 0)
    {
        float t = float(m_targetMinutes - m_timeSlider.minVal)
            / float(m_timeSlider.maxVal - m_timeSlider.minVal);
        thumbX = trackL + int(t * trackW);
    }
    else
    {
        thumbX = trackL;  // parked at left if nothing selected
    }

    bool hov = m_timeSlider.hovering || m_sliderDragging;
    COLORREF thumbFill = (m_targetMinutes > 0)
        ? (hov ? CLR_ACCENT_HOV : CLR_ACCENT)
        : CLR_RANGE_BDR;

    CRect rcThumb(thumbX - thumbR, trackY + trackH / 2 - thumbR,
        thumbX + thumbR, trackY + trackH / 2 + thumbR);
    GdiFillRoundRect(dc, rcThumb, thumbR, thumbFill);
    // white dot center
    if (m_targetMinutes > 0)
    {
        CRect rcDot(thumbX - 3, trackY + trackH / 2 - 3,
            thumbX + 3, trackY + trackH / 2 + 3);
        GdiFillRoundRect(dc, rcDot, 3, RGB(255, 255, 255));
    }

    // --- value label above thumb
    if (m_targetMinutes > 0)
    {
        dc->SetBkMode(TRANSPARENT);
        dc->SetTextColor(CLR_ACCENT);
        CFont* of2 = dc->SelectObject(&m_fontBtn);
        CString val; val.Format(T(L"%d min", L"%d 分"), m_targetMinutes);
        CRect rcVal(thumbX - 40, rT.top, thumbX + 40, trackY - 4);
        dc->DrawText(val, &rcVal, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        dc->SelectObject(of2);
    }
    else
    {
        dc->SetBkMode(TRANSPARENT);
        dc->SetTextColor(CLR_TEXT_LIGHT);
        CFont* of2 = dc->SelectObject(&m_fontRange);
        CRect rcHint(rT.left, rT.top, rT.right, trackY - 2);
        dc->DrawText(T(L"Drag to set duration", L"ドラッグして時間を設定"), &rcHint, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        dc->SelectObject(of2);
    }
}

// ── Start button ──────────────────────────────────────────────────────────────
void CSOHDlg::DrawStartButton(CDC* dc)
{
    if (m_testStarted) return;

    bool canStart = (m_targetStopPercent >= 0 || m_targetMinutes > 0);

    COLORREF fill = !canStart ? CLR_RANGE_IDL
        : m_startBtnPressed ? CLR_ACCENT_PRS
        : m_startBtnHover ? CLR_ACCENT_HOV
        : CLR_ACCENT;

    GdiFillRoundRect(dc, m_rcStartBtn, 10, fill);

    // Subtle shadow line at bottom
    CPen sh(PS_SOLID, 1, RGB(180, 200, 255));
    CPen* op = dc->SelectObject(&sh);
    dc->MoveTo(m_rcStartBtn.left + 12, m_rcStartBtn.bottom + 2);
    dc->LineTo(m_rcStartBtn.right - 12, m_rcStartBtn.bottom + 2);
    dc->SelectObject(op);

    dc->SetBkMode(TRANSPARENT);
    /*dc->SetTextColor(RGB(255, 255, 255));*/
    dc->SetTextColor(canStart ? RGB(255, 255, 255) : CLR_TEXT_LIGHT);
    CFont* of = dc->SelectObject(&m_fontBtn);
    dc->DrawText(T(L"Start Test", L"テスト開始"), const_cast<CRect*>(&m_rcStartBtn),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    dc->SelectObject(of);
}

// ── Paint ─────────────────────────────────────────────────────────────────────
// ── Paint (double-buffered to eliminate flicker) ───────────────────────────
void CSOHDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);
    int W = rc.Width(), H = rc.Height();

    // Create offscreen DC + bitmap
    CDC     memDC;
    CBitmap bmp;
    memDC.CreateCompatibleDC(&dc);
    bmp.CreateCompatibleBitmap(&dc, W, H);
    CBitmap* pOld = memDC.SelectObject(&bmp);

    // Paint everything into memDC
    PaintBuffer(&memDC, W, H);

    // Blit the finished frame to screen in one shot — no flicker
    dc.BitBlt(0, 0, W, H, &memDC, 0, 0, SRCCOPY);

    memDC.SelectObject(pOld);
}

void CSOHDlg::PaintBuffer(CDC* dc, int W, int H)
{
    int mx = max(16, W / 20);

    dc->FillSolidRect(CRect(0, 0, W, H), CLR_BG);

    // Thin rule below instruction area
    CRect instrRc; m_lblInstruction.GetWindowRect(&instrRc);
    ScreenToClient(&instrRc);
    CPen rule(PS_SOLID, 1, CLR_RULE);
    CPen* op = dc->SelectObject(&rule);
    dc->MoveTo(mx, instrRc.bottom + 6);
    dc->LineTo(W - mx, instrRc.bottom + 6);

    // "DISCHARGE RANGE" label
    if (!m_testStarted && !m_rangeBtns.empty())
    {
        int labelTop = m_rangeBtns[0].rc.top - 30;
        int labelBot = m_rangeBtns[0].rc.top - 6;
        dc->SetBkMode(TRANSPARENT);
        dc->SetTextColor(CLR_TEXT_LIGHT);
        CFont* of = dc->SelectObject(&m_fontRange);
        CRect rcLbl(mx, labelTop, W - mx, labelBot);
        dc->DrawText(T(L"DISCHARGE RANGE", L"放電範囲"), &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        dc->SelectObject(of);
    }

    dc->SelectObject(op);

    DrawRangeButtons(dc);

    // "DISCHARGE TIME" label
    if (!m_testStarted)
    {
        int labelTop = m_timeSlider.rcTrack.top - 30;
        int labelBot = m_timeSlider.rcTrack.top - 6;
        dc->SetBkMode(TRANSPARENT);
        dc->SetTextColor(CLR_TEXT_LIGHT);
        CFont* of = dc->SelectObject(&m_fontRange);
        CRect rcLbl(mx, labelTop, W - mx, labelBot);
        dc->DrawText(T(L"DISCHARGE TIME", L"放電時間"), &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        dc->SelectObject(of);
    }

    DrawTimeSlider(dc);
    DrawStartButton(dc);
}

// ── Mouse ─────────────────────────────────────────────────────────────────────
void CSOHDlg::OnMouseMove(UINT nFlags, CPoint pt)
{
    bool rep = false;
    bool nh = m_rcStartBtn.PtInRect(pt) && !m_testStarted;
    if (nh != m_startBtnHover) { m_startBtnHover = nh; rep = true; }

    int nr = -1;
    for (auto& rb : m_rangeBtns) if (rb.rc.PtInRect(pt)) { nr = rb.stopAt; break; }
    if (nr != m_hoveredRange) { m_hoveredRange = nr; rep = true; }

    bool slHov = m_timeSlider.rcTrack.PtInRect(pt) && !m_testStarted;
    if (slHov != m_timeSlider.hovering) { m_timeSlider.hovering = slHov; rep = true; }

    if (m_sliderDragging && !m_testStarted)
    {
        int trackL = m_timeSlider.rcTrack.left + 11;
        int trackR = m_timeSlider.rcTrack.right - 11;
        int trackW = trackR - trackL;
        float ratio = float(pt.x - trackL) / float(trackW);
        ratio = max(0.f, min(1.f, ratio));
        int val = m_timeSlider.minVal + int(ratio * (m_timeSlider.maxVal - m_timeSlider.minVal) + 0.5f);
        // snap to nearest 5
        val = ((val + 2) / 5) * 5;
        val = max(m_timeSlider.minVal, min(m_timeSlider.maxVal, val));
        if (val != m_targetMinutes) { m_targetMinutes = val; m_targetStopPercent = -1; rep = true; }
    }

    if (rep) Invalidate(FALSE);
    CDialogEx::OnMouseMove(nFlags, pt);
}

void CSOHDlg::OnLButtonDown(UINT nFlags, CPoint pt)
{
    if (m_rcStartBtn.PtInRect(pt) && !m_testStarted)
    {
        m_startBtnPressed = true; Invalidate(FALSE);
    }


    if (!m_testStarted)
        for (auto& rb : m_rangeBtns)
            if (rb.rc.PtInRect(pt))
            {
                if (rb.stopAt >= m_currentPercent) break;   // invalid: ignore
                m_targetStopPercent = rb.stopAt;
                m_targetMinutes = 0;
                Invalidate(FALSE); break;
            }

    if (!m_testStarted && m_timeSlider.rcTrack.PtInRect(pt))
    {
        m_sliderDragging = true;
        SetCapture();
        // immediately update value from click position
        int trackL = m_timeSlider.rcTrack.left + 11;
        int trackR = m_timeSlider.rcTrack.right - 11;
        int trackW = trackR - trackL;
        float ratio = float(pt.x - trackL) / float(trackW);
        ratio = max(0.f, min(1.f, ratio));
        int val = m_timeSlider.minVal + int(ratio * (m_timeSlider.maxVal - m_timeSlider.minVal) + 0.5f);
        val = ((val + 2) / 5) * 5;
        val = max(m_timeSlider.minVal, min(m_timeSlider.maxVal, val));
        m_targetMinutes = val;
        m_targetStopPercent = -1;
        Invalidate(FALSE);
    }

    CDialogEx::OnLButtonDown(nFlags, pt);
}

void CSOHDlg::OnLButtonUp(UINT nFlags, CPoint pt)
{
    if (m_startBtnPressed)
    {
        m_startBtnPressed = false;
        /*if (m_rcStartBtn.PtInRect(pt) && !m_testStarted) StartTest();*/
        bool canStart = (m_targetStopPercent >= 0 || m_targetMinutes > 0);
        if (m_rcStartBtn.PtInRect(pt) && !m_testStarted && canStart) StartTest();
        Invalidate(FALSE);
    }

    if (m_sliderDragging) { m_sliderDragging = false; ReleaseCapture(); Invalidate(FALSE); }
    CDialogEx::OnLButtonUp(nFlags, pt);
}

// ── Close / Destroy ───────────────────────────────────────────────────────────
void CSOHDlg::OnClose()
{
    KillTimer(1); KillTimer(2); StopCpuStress();   // ← add KillTimer(2)
    if (m_logFile.is_open()) m_logFile.close();
    CDialogEx::OnClose();
}

void CSOHDlg::OnDestroy()
{
    KillTimer(1); KillTimer(2); StopCpuStress();   // ← add KillTimer(2)
    if (m_logFile.is_open()) m_logFile.close();
    CDialogEx::OnDestroy();
}

// ── CPU stress (unchanged) ────────────────────────────────────────────────────
void CSOHDlg::StartCpuStress()
{
    if (m_stressRunning.load(std::memory_order_acquire)) return;
    unsigned int cores = std::thread::hardware_concurrency();
    if (!cores) cores = 1;
    m_stressRunning.store(true, std::memory_order_release);
    m_stressThreads.clear();
    m_stressThreads.reserve(cores);
    for (unsigned int i = 0; i < cores; ++i)
    {
        std::thread t([this]() {
            ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_IDLE);
            volatile double acc = 1.0;
            while (m_stressRunning.load(std::memory_order_relaxed))
                for (int j = 0; j < 100000; ++j) {
                    acc += std::sqrt(acc + j + 1.0000001);
                    if (acc > 1e15) acc = 1.0;
                }
            });
        DWORD_PTR mask = (DWORD_PTR)1 << (i % (sizeof(DWORD_PTR) * 8));
        SetThreadAffinityMask(t.native_handle(), mask);
        m_stressThreads.emplace_back(std::move(t));
    }
}

void CSOHDlg::StopCpuStress()
{
    if (!m_stressRunning.load(std::memory_order_acquire)) return;
    m_stressRunning.store(false, std::memory_order_release);
    for (auto& t : m_stressThreads)
        if (t.joinable()) try { t.join(); }
    catch (...) {}
    m_stressThreads.clear();
}