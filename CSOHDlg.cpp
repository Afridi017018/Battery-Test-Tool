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
CSOHDlg::CSOHDlg(CWnd* pParent)
    : CDialogEx(IDD_SOH, pParent)
    , m_lastPercent(-1), m_lastDropTime(0)
    , m_isFullyCharged(false), m_isDischarging(false)
    , m_startDischargeTime(0), m_stressRunning(false)
    , m_testStarted(false), m_targetStopPercent(-1)
    , m_startBtnHover(false), m_startBtnPressed(false)
    , m_hoveredRange(-1)
    , m_targetMinutes(0), m_hoveredTime(-1)
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
    int tbH = rbH;
    int tbTotalW = W - mx * 2;
    int tbW = (tbTotalW - rbGap * 3) / 4;
    for (int i = 0; i < (int)m_timeBtns.size(); ++i)
    {
        m_timeBtns[i].rc.SetRect(
            mx + i * (tbW + rbGap), tbY,
            mx + i * (tbW + rbGap) + tbW, tbY + tbH);
    }


    // --- start button: below time row, centered
    int sbW = min(220, W - mx * 4);
    int sbH = max(40, H / 11);
    int sbY = tbY + tbH + max(14, H / 22);
    m_rcStartBtn.SetRect((W - sbW) / 2, sbY, (W + sbW) / 2, sbY + sbH);


    // --- elapsed label: near bottom
    int elH = max(28, H / 14);
    if (m_lblElapsed.GetSafeHwnd())
        m_lblElapsed.MoveWindow(mx, H - elH - 10, W - mx * 2, elH);
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

// ── Init ──────────────────────────────────────────────────────────────────────
BOOL CSOHDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetWindowTextW(_T("Battery SOH Test"));

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
    struct { int stop; const wchar_t* lbl; } ranges[] = {
        {70,L"100 → 70%"},{50,L"100 → 50%"},{30,L"100 → 30%"},{0,L"100 → 0%"}
    };
    for (int i = 0; i < 4; ++i)
    {
        RangeBtn rb; rb.stopAt = ranges[i].stop; rb.label = ranges[i].lbl;
        m_rangeBtns.push_back(rb);
    }
    m_targetStopPercent = 0;

    // Time buttons metadata
    struct { int mins; const wchar_t* lbl; } times[] = {
        {15,L"100→15 min"},{30,L"100→30 min"},{60,L"100→60 min"},{120,L"100→120 min"}
    };
    for (int i = 0; i < 4; ++i)
    {
        TimeBtn tb; tb.minutes = times[i].mins; tb.label = times[i].lbl;
        m_timeBtns.push_back(tb);
    }
    m_targetMinutes = 0;

    RecalcLayout();

    m_instruction = _T("Select a discharge range, then press Start");
    UpdateUI(0);

    m_isFullyCharged = true;

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
        m_logFile << "Range: 100→" << m_targetStopPercent << "\n";
        m_logFile << "Start Time: "
            << st.wYear << "-" << st.wMonth << "-" << st.wDay << " "
            << st.wHour << ":" << st.wMinute << ":" << st.wSecond << "\n";
        m_logFile << "=====================================================\n";
        m_logFile.flush();
    }

    m_lastDropTime = GetTickCount64();
    m_instruction = _T("Please charge battery to 100%");
    UpdateInstruction();
    Invalidate(FALSE);
    SetTimer(1, 1000, NULL);
}

// ── Timer ─────────────────────────────────────────────────────────────────────
void CSOHDlg::OnTimer(UINT_PTR nIDEvent)
{
    UpdateBatteryStatus();
    CDialogEx::OnTimer(nIDEvent);
}

// ── Battery logic ─────────────────────────────────────────────────────────────
void CSOHDlg::UpdateBatteryStatus()
{
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps)) return;

    int percent = sps.BatteryLifePercent;
    ULONGLONG now = GetTickCount64();

    if (percent == 100 && sps.ACLineStatus == 1)
    {
        m_isFullyCharged = true; UpdateInstruction();
    }

    if (m_isFullyCharged && sps.ACLineStatus == 0)
    {
        if (!m_isDischarging)
        {
            m_isDischarging = true;
            m_lastDropTime = m_startDischargeTime = now;
            StartCpuStress();
        }
        UpdateInstruction();
    }

    if (m_isDischarging && m_lastPercent != -1 && percent < m_lastPercent)
        LogData(percent, now - m_lastDropTime), m_lastDropTime = now;

    UpdateUI(percent);
    m_lastPercent = percent;

    /*if (m_isDischarging && percent <= m_targetStopPercent)
    {
        KillTimer(1); StopCpuStress();
        m_instruction = _T("Test complete — data saved.");
        UpdateInstruction(); UpdateUI(percent); Invalidate(FALSE);
        return;
    }*/

    // Stop by percent (range mode)
    if (m_isDischarging && m_targetStopPercent >= 0 && percent <= m_targetStopPercent)
    {
        KillTimer(1); StopCpuStress();
        m_instruction = _T("Test complete — data saved.");
        UpdateInstruction(); UpdateUI(percent); Invalidate(FALSE);
        return;
    }

    // Stop by time (time mode)
    if (m_isDischarging && m_targetMinutes > 0)
    {
        ULONGLONG elapsedMin = (now - m_startDischargeTime) / 60000;
        if (elapsedMin >= (ULONGLONG)m_targetMinutes)
        {
            KillTimer(1); StopCpuStress();
            m_instruction = _T("Test complete — data saved.");
            UpdateInstruction(); UpdateUI(percent); Invalidate(FALSE);
            return;
        }
    }

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
        m_instruction = _T("Test stopped: charger detected. Closing...");
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
    if (!m_isFullyCharged)
        m_instruction = _T("Charge battery to 100% to begin");
    else if (!m_isDischarging)
        m_instruction = _T("Fully charged — unplug the charger");
    else
        m_instruction = _T("Discharging... recording data");
    if (m_lblInstruction.GetSafeHwnd())
        m_lblInstruction.SetWindowTextW(m_instruction);
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
            CString e; e.Format(_T("Elapsed  %02d:%02d:%02d"),
                (int)(s / 3600), (int)((s % 3600) / 60), (int)(s % 60));
            m_lblElapsed.SetWindowTextW(e);
        }
        else m_lblElapsed.SetWindowTextW(_T(""));
    }
    if (m_lblInstruction.GetSafeHwnd())
        m_lblInstruction.SetWindowTextW(m_instruction);
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

void CSOHDlg::DrawTimeButtons(CDC* dc)
{
    dc->SetBkMode(TRANSPARENT);
    CFont* of = dc->SelectObject(&m_fontRange);

    for (auto& tb : m_timeBtns)
    {
        bool sel = (tb.minutes == m_targetMinutes);
        bool hov = (!m_testStarted && m_hoveredTime == tb.minutes);
        bool dim = m_testStarted;

        COLORREF fill = dim ? CLR_RANGE_IDL : sel ? CLR_RANGE_SEL : hov ? CLR_RANGE_HOV : CLR_RANGE_IDL;
        COLORREF bdr = dim ? CLR_RANGE_BDR : sel ? CLR_ACCENT : hov ? CLR_ACCENT : CLR_RANGE_BDR;
        COLORREF txtc = dim ? CLR_TEXT_LIGHT : sel ? RGB(255, 255, 255) : CLR_TEXT_DARK;

        GdiFillRoundRect(dc, tb.rc, 8, fill, bdr, sel && !dim ? 2 : 1);
        dc->SetTextColor(txtc);
        dc->DrawText(tb.label, const_cast<CRect*>(&tb.rc),
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }
    dc->SelectObject(of);
}

// ── Start button ──────────────────────────────────────────────────────────────
void CSOHDlg::DrawStartButton(CDC* dc)
{
    if (m_testStarted) return;

    COLORREF fill = m_startBtnPressed ? CLR_ACCENT_PRS
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
    dc->SetTextColor(RGB(255, 255, 255));
    CFont* of = dc->SelectObject(&m_fontBtn);
    dc->DrawText(_T("Start Test"), const_cast<CRect*>(&m_rcStartBtn),
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    dc->SelectObject(of);
}

// ── Paint ─────────────────────────────────────────────────────────────────────
void CSOHDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);
    int W = rc.Width(), H = rc.Height();
    int mx = max(16, W / 20);

    dc.FillSolidRect(&rc, CLR_BG);

    // Thin rule below instruction area
    CRect instrRc; m_lblInstruction.GetWindowRect(&instrRc);
    ScreenToClient(&instrRc);
    CPen rule(PS_SOLID, 1, CLR_RULE);
    CPen* op = dc.SelectObject(&rule);
    dc.MoveTo(mx, instrRc.bottom + 6);
    dc.LineTo(W - mx, instrRc.bottom + 6);

    // "RANGE" label — only before test starts
    if (!m_testStarted && !m_rangeBtns.empty())
    {
        int labelTop = m_rangeBtns[0].rc.top - 30;
        int labelBot = m_rangeBtns[0].rc.top - 6;
        dc.SetBkMode(TRANSPARENT);
        dc.SetTextColor(CLR_TEXT_LIGHT);
        CFont* of = dc.SelectObject(&m_fontRange);
        CRect rcLbl(mx, labelTop, W - mx, labelBot);
        dc.DrawText(_T("DISCHARGE RANGE"), &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        dc.SelectObject(of);
    }

    dc.SelectObject(op);

    DrawRangeButtons(&dc);

    // "DISCHARGE TIME" label — only before test starts
    if (!m_testStarted && !m_timeBtns.empty())
    {
        int labelTop = m_timeBtns[0].rc.top - 30;
        int labelBot = m_timeBtns[0].rc.top - 6;
        dc.SetBkMode(TRANSPARENT);
        dc.SetTextColor(CLR_TEXT_LIGHT);
        CFont* of = dc.SelectObject(&m_fontRange);
        CRect rcLbl(mx, labelTop, W - mx, labelBot);
        dc.DrawText(_T("DISCHARGE TIME"), &rcLbl, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        dc.SelectObject(of);
    }

    DrawTimeButtons(&dc);
    DrawStartButton(&dc);
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

    int nt = -1;
    for (auto& tb : m_timeBtns) if (tb.rc.PtInRect(pt)) { nt = tb.minutes; break; }
    if (nt != m_hoveredTime) { m_hoveredTime = nt; rep = true; }

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
                m_targetStopPercent = rb.stopAt;
                m_targetMinutes = 0;   // clear time selection
                Invalidate(FALSE); break;
            }

    if (!m_testStarted)
        for (auto& tb : m_timeBtns)
            if (tb.rc.PtInRect(pt))
            {
                m_targetMinutes = tb.minutes;
                m_targetStopPercent = -1;  // clear range selection
                Invalidate(FALSE); break;
            }

    CDialogEx::OnLButtonDown(nFlags, pt);
}

void CSOHDlg::OnLButtonUp(UINT nFlags, CPoint pt)
{
    if (m_startBtnPressed)
    {
        m_startBtnPressed = false;
        if (m_rcStartBtn.PtInRect(pt) && !m_testStarted) StartTest();
        Invalidate(FALSE);
    }
    CDialogEx::OnLButtonUp(nFlags, pt);
}

// ── Close / Destroy ───────────────────────────────────────────────────────────
void CSOHDlg::OnClose()
{
    KillTimer(1); StopCpuStress();
    if (m_logFile.is_open()) m_logFile.close();
    CDialogEx::OnClose();
}

void CSOHDlg::OnDestroy()
{
    KillTimer(1); StopCpuStress();
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