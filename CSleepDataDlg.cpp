// CSleepDataDlg.cpp - Fully Responsive Version (Newest log on top)
#include "pch.h"
#include "framework.h"
#include "resource.h"
#include "CSleepDataDlg.h"

#include <powrprof.h>
#pragma comment(lib, "PowrProf.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif




enum LANG_INDEX
{
    LANG_EN = 0,
    LANG_JP = 1
};

enum TEXT_KEY
{
    TK_COL_SLEEP = 0,
    TK_COL_AWAKE,
    TK_COL_PCT_BEFORE,
    TK_COL_PCT_AFTER,
    TK_UNKNOWN,
    TK_PCT_UNKNOWN,
    TK_COUNT
};


static const wchar_t* g_Texts[2][TK_COUNT] =
{
    // English
    {
        L"Sleep Time",        // TK_COL_SLEEP
        L"Awake Time",        // TK_COL_AWAKE
        L"% Before",          // TK_COL_PCT_BEFORE
        L"% After",           // TK_COL_PCT_AFTER
        L"(unknown)",         // TK_UNKNOWN
        L"(?)"                // TK_PCT_UNKNOWN
    },

    // Japanese
    {
        L"睡眠時間",       // TK_COL_SLEEP
        L"起床時間",       // TK_COL_AWAKE (shorter)
        L"前%",            // TK_COL_PCT_BEFORE (shorter to keep column width tight)
        L"後%",            // TK_COL_PCT_AFTER  (shorter to keep column width tight)
        L"(未知)",         // TK_UNKNOWN
        L"(?)"             // TK_PCT_UNKNOWN (ASCII to match data rows)
    }
};



// --------- static data (persist across openings) ---------
CSleepDataDlg::SleepSnapshot  CSleepDataDlg::s_beforeSleep{};
std::vector<CString>          CSleepDataDlg::s_logs;
HWND                          CSleepDataDlg::s_hwndOpen = nullptr;
ULONGLONG                     CSleepDataDlg::s_lastResumeTick = 0;
bool                          CSleepDataDlg::s_resumeLogged = false;
CSleepDataDlg::Cause          CSleepDataDlg::s_activeCause = CSleepDataDlg::Cause::None;
bool                          CSleepDataDlg::eng_lang;

// ---------- Fixed-width row builder ----------
CString CSleepDataDlg::MakeRow(const CString& sleepTime,
    const CString& awakeTime,
    const CString& pctBefore,
    const CString& pctAfter)
{
    CString row;
    row.Format(L"%-25s  |  %-25s  |  %-8s  |  %-8s",
        sleepTime.GetString(),
        awakeTime.GetString(),
        pctBefore.GetString(),
        pctAfter.GetString());
    return row;
}

BEGIN_MESSAGE_MAP(CSleepDataDlg, CDialogEx)
    ON_WM_SIZE()
    ON_WM_PAINT()
    ON_WM_MOUSEWHEEL()
    ON_WM_VSCROLL()
    ON_WM_HSCROLL()
    ON_WM_DESTROY()
    ON_MESSAGE(WM_SLEEPDATA_REFRESH, &CSleepDataDlg::OnRefreshMsg)
END_MESSAGE_MAP()

CSleepDataDlg::CSleepDataDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_SLEEPDATA_DIALOG, pParent)
{
}

void CSleepDataDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BOOL CSleepDataDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Enable both scrollbars
    ModifyStyle(0, WS_VSCROLL | WS_HSCROLL);

    // Create initial monospaced font
    CreateMonoFont(m_baseFontPx);

    GetClientRect(&m_client);

    // Calculate initial metrics
    {
        CClientDC dc(this);
        CFont* pOld = dc.SelectObject(&m_monoFont);
        RecalcTextMetrics(dc);
        CalculateContentWidth(dc);
        dc.SelectObject(pOld);
    }

    // Mark this window as the open viewer
    s_hwndOpen = m_hWnd;

    // Start at the top (newest log visible first)
    m_scrollPosV = 0;

    int lang = eng_lang ? LANG_EN : LANG_JP;

    UpdateScrollBars();
    Invalidate();

    // Load and set icon
    m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_ICON));
    SetIcon(m_hIcon, TRUE);   // Large icon
    SetIcon(m_hIcon, FALSE);  // Small icon


    if (eng_lang)
    {
        SetWindowTextW(L"Sleep Logs");
    }
    else
    {
        SetWindowTextW(L"睡眠ログ");
    }

    return TRUE;
}

void CSleepDataDlg::OnDestroy()
{
    if (s_hwndOpen == m_hWnd) s_hwndOpen = nullptr;
    CDialogEx::OnDestroy();
}

LRESULT CSleepDataDlg::OnRefreshMsg(WPARAM, LPARAM)
{
    // Recalculate content width when new data arrives
    CClientDC dc(this);
    CFont* pOld = dc.SelectObject(&m_monoFont);
    CalculateContentWidth(dc);
    dc.SelectObject(pOld);

    // Always reset to top (to show latest log)
    m_scrollPosV = 0;
    UpdateScrollBars();
    Invalidate();
    return 0;
}

void CSleepDataDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    GetClientRect(&m_client);

    if (m_client.IsRectEmpty()) return;

    // Recalculate content width with current font
    CClientDC dc(this);
    CFont* pOld = dc.SelectObject(&m_monoFont);
    CalculateContentWidth(dc);
    dc.SelectObject(pOld);

    UpdateScrollBars();
    Invalidate();
}

void CSleepDataDlg::OnPaint()
{
    CPaintDC dc(this);

    if (m_client.IsRectEmpty())
        return;

    int lang = eng_lang ? LANG_EN : LANG_JP;


    // Double-buffer to avoid flicker
    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, m_client.Width(), m_client.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    // Fill background
    memDC.FillSolidRect(&m_client, ::GetSysColor(COLOR_WINDOW));

    // Select font
    CFont* pOldFont = memDC.SelectObject(&m_monoFont);
    memDC.SetBkMode(TRANSPARENT);
    memDC.SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));

    // Build header
    CString header;
    header.Format(L"%-25s | %-25s  | %-8s  | %-8s",
        g_Texts[lang][TK_COL_SLEEP], g_Texts[lang][TK_COL_AWAKE], g_Texts[lang][TK_COL_PCT_BEFORE], g_Texts[lang][TK_COL_PCT_AFTER]);

    const int dashLen = (int)header.GetLength();
    CString dash;
    dash.Preallocate(dashLen);
    for (int i = 0; i < dashLen; ++i) dash += L'-';

    const int leftPad = 8;
    const int topPad = 8;

    // Calculate visible lines
    const int linesPerPage = max(1, (m_client.Height() - topPad * 2) / m_lineHeight);
    const int total = TotalLines();
    const int firstLine = min(m_scrollPosV, max(0, total - 1));
    const int lastLine = min(total, firstLine + linesPerPage);

    // Horizontal scroll offset
    const int xOffset = leftPad - m_scrollPosH;
    int y = topPad;

    // Render lines
    for (int i = firstLine; i < lastLine; ++i) {
        CString text;
        if (i == 0)       text = header;
        else if (i == 1)  text = dash;
        else              text = s_logs[i - 2];

        memDC.TextOut(xOffset, y, text);
        y += m_lineHeight;
    }

    // Blit to screen
    dc.BitBlt(0, 0, m_client.Width(), m_client.Height(), &memDC, 0, 0, SRCCOPY);

    // Cleanup
    memDC.SelectObject(pOldFont);
    memDC.SelectObject(pOldBmp);
}

BOOL CSleepDataDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint /*pt*/)
{
    // Check if Shift is pressed for horizontal scrolling
    if (nFlags & MK_SHIFT) {
        // Horizontal scroll
        int step = (zDelta > 0) ? -m_charWidth * 3 : m_charWidth * 3;
        m_scrollPosH = max(0, min(m_scrollPosH + step, max(0, m_contentWidth - m_client.Width())));
    }
    else {
        // Vertical scroll: 3 lines per notch
        int step = (zDelta > 0) ? -3 : 3;
        m_scrollPosV = max(0, min(m_scrollPosV + step, max(0, TotalLines() - 1)));
    }

    UpdateScrollBars();
    Invalidate();
    return TRUE;
}

void CSleepDataDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    UNREFERENCED_PARAMETER(pScrollBar);

    int pos = m_scrollPosV;
    const int page = max(1, (m_client.Height() - 16) / m_lineHeight);

    switch (nSBCode) {
    case SB_TOP:            pos = 0; break;
    case SB_BOTTOM:         pos = max(0, TotalLines() - 1); break;
    case SB_LINEUP:         pos = max(0, pos - 1); break;
    case SB_LINEDOWN:       pos = min(max(0, TotalLines() - 1), pos + 1); break;
    case SB_PAGEUP:         pos = max(0, pos - page); break;
    case SB_PAGEDOWN:       pos = min(max(0, TotalLines() - 1), pos + page); break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:     pos = nPos; break;
    default: break;
    }

    if (pos != m_scrollPosV) {
        m_scrollPosV = pos;
        UpdateScrollBars();
        Invalidate();
    }
}

void CSleepDataDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    UNREFERENCED_PARAMETER(pScrollBar);

    int pos = m_scrollPosH;
    const int page = max(1, m_client.Width() / 2);
    const int maxScroll = max(0, m_contentWidth - m_client.Width());

    switch (nSBCode) {
    case SB_LEFT:           pos = 0; break;
    case SB_RIGHT:          pos = maxScroll; break;
    case SB_LINELEFT:       pos = max(0, pos - m_charWidth); break;
    case SB_LINERIGHT:      pos = min(maxScroll, pos + m_charWidth); break;
    case SB_PAGELEFT:       pos = max(0, pos - page); break;
    case SB_PAGERIGHT:      pos = min(maxScroll, pos + page); break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:     pos = nPos; break;
    default: break;
    }

    if (pos != m_scrollPosH) {
        m_scrollPosH = pos;
        UpdateScrollBars();
        Invalidate();
    }
}

void CSleepDataDlg::CreateMonoFont(int pixelHeight)
{
    if (m_monoFont.GetSafeHandle()) {
        m_monoFont.DeleteObject();
    }

    LOGFONT lf{};
    lf.lfHeight = -pixelHeight; // negative = character height in pixels
    wcscpy_s(lf.lfFaceName, L"Consolas");
    lf.lfQuality = CLEARTYPE_QUALITY;
    m_monoFont.CreateFontIndirect(&lf);
}

void CSleepDataDlg::RecalcTextMetrics(CDC& dc)
{
    TEXTMETRIC tm{};
    dc.GetTextMetrics(&tm);
    m_lineHeight = max(1, (int)tm.tmHeight + (int)tm.tmExternalLeading + 2); // +2 for spacing
    m_charWidth = max(1, (int)tm.tmAveCharWidth);
}

void CSleepDataDlg::CalculateContentWidth(CDC& dc)
{
    int lang = eng_lang ? LANG_EN : LANG_JP;

    m_contentWidth = 0;

    // Measure header
    CString header;
    header.Format(L"%-25s  |  %-25s  |  %-8s  |  %-8s",
        g_Texts[lang][TK_COL_SLEEP], g_Texts[lang][TK_COL_AWAKE], g_Texts[lang][TK_COL_PCT_BEFORE], g_Texts[lang][TK_COL_PCT_AFTER]);

    CSize sz = dc.GetTextExtent(header);
    m_contentWidth = max(m_contentWidth, sz.cx);

    // Measure all log lines
    for (const auto& line : s_logs) {
        sz = dc.GetTextExtent(line);
        m_contentWidth = max(m_contentWidth, sz.cx);
    }

    // Add padding
    m_contentWidth += 16; // left + right padding
}

int CSleepDataDlg::TotalLines() const
{
    return 2 + (int)s_logs.size(); // header + dash + logs
}

void CSleepDataDlg::UpdateScrollBars()
{
    const int total = TotalLines();
    const int vPage = max(1, (m_client.Height() - 16) / m_lineHeight);
    const int hPage = max(1, m_client.Width());

    // Vertical scrollbar
    SCROLLINFO siV{};
    siV.cbSize = sizeof(siV);
    siV.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    siV.nMin = 0;
    siV.nMax = max(0, total - 1);
    siV.nPage = vPage;
    siV.nPos = min(m_scrollPosV, siV.nMax);
    SetScrollInfo(SB_VERT, &siV, TRUE);

    // Horizontal scrollbar
    const int maxHScroll = max(0, m_contentWidth - m_client.Width());
    SCROLLINFO siH{};
    siH.cbSize = sizeof(siH);
    siH.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
    siH.nMin = 0;
    siH.nMax = m_contentWidth;
    siH.nPage = hPage;
    siH.nPos = min(m_scrollPosH, maxHScroll);

    // Only show horizontal scrollbar if needed
    if (maxHScroll > 0) {
        ShowScrollBar(SB_HORZ, TRUE);
        SetScrollInfo(SB_HORZ, &siH, TRUE);
    }
    else {
        ShowScrollBar(SB_HORZ, FALSE);
        m_scrollPosH = 0;
    }
}

// ========================== Battery queries (static) ==========================
bool CSleepDataDlg::QueryBattery_mWh(ULONGLONG& out_mWh, bool& outAC)
{
    out_mWh = 0; outAC = false;

    SYSTEM_BATTERY_STATE sbs{};
    ULONG status = CallNtPowerInformation(SystemBatteryState, nullptr, 0, &sbs, sizeof(sbs));
    if (status != 0) return false;

    if (sbs.BatteryPresent && sbs.RemainingCapacity > 0) {
        out_mWh = sbs.RemainingCapacity;
        outAC = !!sbs.AcOnLine;
        return true;
    }
    return false;
}

bool CSleepDataDlg::QueryBattery_Percent(int& outPct, bool& outAC)
{
    outPct = -1; outAC = false;

    SYSTEM_POWER_STATUS sps{};
    if (!GetSystemPowerStatus(&sps)) return false;

    outAC = (sps.ACLineStatus == 1);
    if (sps.BatteryLifePercent != 255) {
        outPct = (int)sps.BatteryLifePercent;
        return true;
    }
    return false;
}

// ========================== Formatting helper (static) ==========================
CString CSleepDataDlg::FormatSysTime(const std::chrono::system_clock::time_point& tp)
{
    auto tt = std::chrono::system_clock::to_time_t(tp);
    struct tm localTm {};
#if defined(_WIN32)
    localtime_s(&localTm, &tt);
#else
    localTm = *std::localtime(&tt);
#endif
    wchar_t buf[32]{};
    wcsftime(buf, _countof(buf), L"%Y-%m-%d %H:%M:%S", &localTm);
    return CString(buf);
}


// ========================== Tracking logic (static, no UI) ==========================
void CSleepDataDlg::TrackBeforeSleep_NoUI()
{
    s_beforeSleep = SleepSnapshot{};

    bool onACpct = false;
    int pctBefore = -1;
    QueryBattery_Percent(pctBefore, onACpct);

    ULONGLONG mWh = 0; bool onACmWh = false;
    if (QueryBattery_mWh(mWh, onACmWh)) {
        s_beforeSleep.mWh = mWh;
        s_beforeSleep.onAC = onACmWh;
        s_beforeSleep.valid = true;
    }
    else {
        s_beforeSleep.mWh = 0;
        s_beforeSleep.onAC = onACpct;
        s_beforeSleep.valid = (pctBefore >= 0);
    }

    s_beforeSleep.percent = pctBefore;
    s_beforeSleep.t = std::chrono::system_clock::now();
}


void CSleepDataDlg::TrackAfterResume_NoUI()
{
    auto now = std::chrono::system_clock::now();

    int lang = eng_lang ? LANG_EN : LANG_JP;

    CString line;
    if (!s_beforeSleep.valid) {
        line = MakeRow(g_Texts[lang][TK_UNKNOWN], FormatSysTime(now), g_Texts[lang][TK_UNKNOWN], g_Texts[lang][TK_UNKNOWN]);
        AppendLogStatic(line);
        return;
    }

    bool onACnow = false;
    int pctAfter = -1;
    QueryBattery_Percent(pctAfter, onACnow);

    CString pctBeforeStr, pctAfterStr;
    if (s_beforeSleep.percent >= 0) pctBeforeStr.Format(L"%d%%", s_beforeSleep.percent);
    else                            pctBeforeStr = L"(?)";
    if (pctAfter >= 0)             pctAfterStr.Format(L"%d%%", pctAfter);
    else                           pctAfterStr = L"(?)";

    line = MakeRow(
        FormatSysTime(s_beforeSleep.t),
        FormatSysTime(now),
        pctBeforeStr,
        pctAfterStr
    );

    AppendLogStatic(line);
    s_beforeSleep.valid = false;
}


void CSleepDataDlg::AppendLogStatic(const CString& line)
{
    // Newest log goes on top
    s_logs.insert(s_logs.begin(), line);

    if (s_hwndOpen && ::IsWindow(s_hwndOpen)) {
        ::PostMessage(s_hwndOpen, WM_SLEEPDATA_REFRESH, 0, 0);
    }
}

// ========================== UI helpers ==========================
void CSleepDataDlg::OnBeforeSleep_UI() { TrackBeforeSleep_NoUI(); }
void CSleepDataDlg::OnAfterResume_UI()
{
    UpdateScrollBars();
    Invalidate();
}
void CSleepDataDlg::AppendLog_UI(const CString& line)
{
    AppendLogStatic(line);
    UpdateScrollBars();
    Invalidate();
}




// ========================== Public static entries ==========================
void CSleepDataDlg::HandlePowerBroadcast(UINT nPowerEvent)
{
    switch (nPowerEvent)
    {
    case PBT_APMSUSPEND:
        // Only track "before sleep" if we haven't already started tracking
        // This prevents overwriting the original sleep time when hibernate follows sleep
        if (s_activeCause == Cause::None) {
            s_activeCause = Cause::Real;
            TrackBeforeSleep_NoUI();
            s_resumeLogged = false;
            s_lastResumeTick = 0;
        }
        // If already tracking (Cause::Real or Cause::Screen), don't overwrite
        break;

    case PBT_APMRESUMECRITICAL:
        [[fallthrough]];
    case PBT_APMRESUMEAUTOMATIC:
        [[fallthrough]];
    case PBT_APMRESUMESUSPEND:
    {
        if (s_activeCause == Cause::Real) {
            const ULONGLONG now = ::GetTickCount64();
            const bool shouldLog =
                !s_resumeLogged || (s_lastResumeTick == 0) || (now - s_lastResumeTick > 1500ULL);

            if (shouldLog) {
                TrackAfterResume_NoUI();
                s_resumeLogged = true;
                s_lastResumeTick = now;
                s_activeCause = Cause::None;
            }
        }
        break;
    }

    default:
        break;
    }
}



void CSleepDataDlg::HandleDisplayState(DWORD displayState)
{
    switch (displayState)
    {
    case 0: // OFF
        if (s_activeCause == Cause::None) {
            s_activeCause = Cause::Screen;
            TrackBeforeSleep_NoUI();
        }
        break;

    case 1: // ON
        if (s_activeCause == Cause::Screen) {
            TrackAfterResume_NoUI();
            s_activeCause = Cause::None;
        }
        break;

    case 2: // DIM
    default:
        break;
    }
}

CSleepDataDlg::~CSleepDataDlg()
{
    if (m_monoFont.GetSafeHandle()) {
        m_monoFont.DeleteObject();
    }
}
