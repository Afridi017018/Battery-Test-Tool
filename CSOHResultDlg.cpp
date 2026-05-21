#include "pch.h"
#include "CSOHResultDlg.h"
#include <fstream>
#include <sstream>
#include <shlobj.h>
#pragma comment(lib, "shell32.lib")
#include <map>
#include <vector>
#include <algorithm>

IMPLEMENT_DYNAMIC(CSOHResultDlg, CDialogEx)

CSOHResultDlg::CSOHResultDlg(CWnd* pParent, bool engLang)
    : CDialogEx(IDD_RESULT, pParent)
    , m_totalTimeMs(0)
    , m_showAll(false)
    , m_showChart(false)
    , eng_lang(engLang)             // ← store the flag
    , m_showHealth(false)
    /*, m_healthScore(0.0)*/
    , m_healthScore(0.0)
    , m_consistencyScore(0.0)
    , m_anomalyScore(0.0)
    , m_smoothnessScore(0.0)
    , m_healthScrollY(0)
    , m_healthTotalH(0)
{
}

CSOHResultDlg::~CSOHResultDlg()
{
}

void CSOHResultDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSOHResultDlg, CDialogEx)
    ON_BN_CLICKED(3001, &CSOHResultDlg::OnToggleView)
    ON_BN_CLICKED(3002, &CSOHResultDlg::OnShowChart)
    ON_WM_CTLCOLOR()
    ON_WM_PAINT()
    ON_NOTIFY(NM_CUSTOMDRAW, 2001, &CSOHResultDlg::OnCustomDrawList)
    ON_BN_CLICKED(3003, &CSOHResultDlg::OnShowHealth)
    ON_WM_MOUSEWHEEL()
    ON_WM_VSCROLL()

    //ON_NOTIFY(LVN_ENDSCROLL, 2001, &CSOHResultDlg::OnListEndScroll)

    ON_WM_SIZE()

    ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


HBRUSH CSOHResultDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

    if (pWnd->GetSafeHwnd() == m_lblLegendRed.GetSafeHwnd())
    {
        pDC->SetTextColor(RGB(180, 30, 30));
        pDC->SetBkMode(TRANSPARENT);
        return (HBRUSH)GetStockObject(NULL_BRUSH);
    }

    if (pWnd->GetSafeHwnd() == m_lblLegendOrange.GetSafeHwnd())
    {
        pDC->SetTextColor(RGB(200, 110, 0));
        pDC->SetBkMode(TRANSPARENT);
        return (HBRUSH)GetStockObject(NULL_BRUSH);
    }

    if (pWnd->GetSafeHwnd() == m_lblSummary.GetSafeHwnd())
    {
        pDC->SetTextColor(RGB(0, 0, 0));
        pDC->SetBkMode(OPAQUE);
        pDC->SetBkColor(::GetSysColor(COLOR_BTNFACE));
        return (HBRUSH)::GetSysColorBrush(COLOR_BTNFACE);
    }

    return hbr;
}


//void CSOHResultDlg::OnListEndScroll(NMHDR* pNMHDR, LRESULT* pResult)
//{
//    if (m_lblSummary.GetSafeHwnd())
//    {
//        m_lblSummary.BringWindowToTop();
//        m_lblSummary.Invalidate();
//        m_lblSummary.UpdateWindow();
//    }
//    *pResult = 0;
//}

void CSOHResultDlg::ResizeListColumns()
{
    if (!m_list.GetSafeHwnd()) return;

    CRect rcList;
    m_list.GetClientRect(&rcList);
    int totalW = rcList.Width() - 4; // small margin
    if (totalW <= 0) return;

    int colCount = m_list.GetHeaderCtrl() ? m_list.GetHeaderCtrl()->GetItemCount() : 0;
    if (colCount == 0) return;

    if (!m_showAll)
    {
        // Latest view — 6 columns: Time, Percent, Duration(ms), Duration(h:m:s), %Range(h:m:s), Elapsed(h:m:s)
        // Ratios: 14, 10, 14, 17, 17, 17  (sum ~89, rest as padding)
        int w0 = totalW * 14 / 100;
        int w1 = totalW * 10 / 100;
        int w2 = totalW * 14 / 100;
        int w3 = totalW * 17 / 100;
        int w4 = totalW * 17 / 100;
        int w5 = totalW - w0 - w1 - w2 - w3 - w4;
        m_list.SetColumnWidth(0, w0);
        m_list.SetColumnWidth(1, w1);
        m_list.SetColumnWidth(2, w2);
        m_list.SetColumnWidth(3, w3);
        m_list.SetColumnWidth(4, w4);
        m_list.SetColumnWidth(5, w5);
    }
    else
    {
        // See All view — 7 columns: TestID, Start, Time, Percent, Duration(ms), Duration(h:m:s), Elapsed(h:m:s)
        int w0 = totalW * 12 / 100;
        int w1 = totalW * 16 / 100;
        int w2 = totalW * 9 / 100;
        int w3 = totalW * 9 / 100;
        int w4 = totalW * 14 / 100;
        int w5 = totalW * 16 / 100;
        int w6 = totalW - w0 - w1 - w2 - w3 - w4 - w5;
        m_list.SetColumnWidth(0, w0);
        m_list.SetColumnWidth(1, w1);
        m_list.SetColumnWidth(2, w2);
        m_list.SetColumnWidth(3, w3);
        m_list.SetColumnWidth(4, w4);
        m_list.SetColumnWidth(5, w5);
        m_list.SetColumnWidth(6, w6);
    }
}


void CSOHResultDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);

    if (cx <= 0 || cy <= 0) return;
    if (!m_list.GetSafeHwnd()) return;   // ← guard: not ready yet

    if (m_btnToggle.GetSafeHwnd())
        m_btnToggle.MoveWindow(10, 10, 110, 30);
    if (m_btnChart.GetSafeHwnd())
        m_btnChart.MoveWindow(130, 10, 130, 30);
    if (m_btnHealth.GetSafeHwnd())
        m_btnHealth.MoveWindow(270, 10, 150, 30);

    if (m_lblLegendRed.GetSafeHwnd())
        m_lblLegendRed.MoveWindow(10, 46, 250, 20);
    if (m_lblLegendOrange.GetSafeHwnd())
        m_lblLegendOrange.MoveWindow(260, 46, cx - 270, 20);

    // List stops exactly where summary begins — zero overlap
    int summaryTop = cy - 65;
    int listH = summaryTop - 72 - 4;
    if (listH < 10) listH = 10;
    m_list.MoveWindow(10, 72, cx - 20, listH);

    if (m_lblSummary.GetSafeHwnd())
        m_lblSummary.MoveWindow(10, summaryTop, cx - 20, 55);

    ResizeListColumns();

    if (m_showChart || m_showHealth)
        Invalidate();
}

BOOL CSOHResultDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    CRect rc;
    GetClientRect(&rc);

    // Toggle Button
    m_btnToggle.Create(
        T(L"See All", L"全件表示"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(10, 10, 120, 40),
        this,
        3001
    );

    // Chart Button
    m_btnChart.Create(
        T(L"Show Chart", L"グラフ表示"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(130, 10, 260, 40),
        this,
        3002
    );

    // Health Score Button
    m_btnHealth.Create(
        T(L"Health Score", L"健全スコア"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(270, 10, 420, 40),
        this,
        3003
    );

    // Legend Red
    m_lblLegendRed.Create(
        T(L"  \u26a0  Red = Too Slow", L"  \u26a0  赤 = 遅すぎる"),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        CRect(10, 46, 260, 66),      // ← moved to y=46..66
        this
    );
    m_lblLegendRed.SetFont(GetFont());

    // Legend Orange
    m_lblLegendOrange.Create(
        T(L"|  Orange = Too Fast  (statistical anomaly)",
            L"|  橙 = 速すぎる（統計的異常）"),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        CRect(260, 46, rc.Width() - 10, 66),   // ← moved to y=46..66
        this
    );
    m_lblLegendOrange.SetFont(GetFont());

    m_list.Create(WS_CHILD | WS_VISIBLE | LVS_REPORT | WS_CLIPSIBLINGS,
        CRect(10, 72, rc.Width() - 10, rc.Height() - 80),
        this, 2001);

    m_list.InsertColumn(0, T(L"Time", L"時刻"), LVCFMT_LEFT, 100);
    m_list.InsertColumn(1, T(L"Percent", L"パーセント"), LVCFMT_CENTER, 80);
    m_list.InsertColumn(2, T(L"Duration(ms)", L"所要時間(ms)"), LVCFMT_CENTER, 120);
    m_list.InsertColumn(3, T(L"Duration(h:m:s)", L"所要時間(時:分:秒)"), LVCFMT_CENTER, 140);
    m_list.InsertColumn(4,
        T(L"% Range(h:m:s)",
            L"%範囲(時:分:秒)"),
        LVCFMT_CENTER,
        140);
    m_list.InsertColumn(5, T(L"Elapsed(h:m:s)", L"経過時間(時:分:秒)"), LVCFMT_CENTER, 140);

    // Summary Label
    m_lblSummary.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_CENTER | WS_CLIPSIBLINGS,
        CRect(10, rc.Height() - 60, rc.Width() - 10, rc.Height() - 10),
        this);
    m_lblSummary.SetFont(GetFont());

    LoadLogFile();
    DisplayData();

    m_lblSummary.SetWindowPos(&CWnd::wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    return TRUE;
}

CString CSOHResultDlg::GetExeFolder()
{
    wchar_t appDataPath[MAX_PATH];
    SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, appDataPath);
    std::wstring folder = std::wstring(appDataPath) + L"\\SOHTool";
    CreateDirectoryW(folder.c_str(), NULL);
    return CString(folder.c_str());
}


void CSOHResultDlg::LoadLogFile()
{
    CString folder = GetExeFolder();
    CString fullPath = folder + _T("\\soh_log.txt");

    std::wifstream file(fullPath);
    if (!file.is_open())
        return;

    std::vector<std::wstring> allLines;
    std::wstring line;

    while (std::getline(file, line))
        allLines.push_back(line);

    file.close();

    std::string lastTestID;
    std::string lastStartTime;
    std::string currentStartTime;

    for (size_t i = 0; i < allLines.size(); ++i)
    {
        std::string s(allLines[i].begin(), allLines[i].end());

        if (s.find("TEST_ID:") != std::string::npos)
        {
            size_t pos = s.find("TEST_ID:");
            lastTestID = s.substr(pos + 8);
            lastTestID.erase(0, lastTestID.find_first_not_of(" \t"));
            lastTestID.erase(lastTestID.find_last_not_of(" \t") + 1);

            if (i + 1 < allLines.size())
            {
                std::string next(allLines[i + 1].begin(), allLines[i + 1].end());
                if (next.find("Start Time:") != std::string::npos)
                {
                    size_t tpos = next.find("Start Time:");
                    lastStartTime = next.substr(tpos + 11);
                    lastStartTime.erase(0, lastStartTime.find_first_not_of(" \t"));
                    lastStartTime.erase(lastStartTime.find_last_not_of(" \t") + 1);
                    currentStartTime = lastStartTime;
                }
            }
        }

        // ← added: skip TEST STOPPED lines
        if (s.find("Percent:") != std::string::npos &&
            s.find("TEST STOPPED") == std::string::npos)
        {
            ParseLine(s);

            size_t idStart = s.find("[");
            size_t idEnd = s.find("]");
            std::string testID = s.substr(idStart + 1, idEnd - idStart - 1);

            SOHFullEntry full;
            full.testID = CString(testID.c_str());
            full.startTime = CString(currentStartTime.c_str());

            auto& e = m_entries.back();
            full.time = e.time;
            full.percent = e.percent;
            full.durationMs = e.durationMs;
            full.durationHMS = e.durationHMS;

            m_allEntries.push_back(full);
        }
    }

    m_testIDStr = CString(lastTestID.c_str());
    m_startTimeStr = CString(lastStartTime.c_str());

    std::string matchTag = "[" + lastTestID + "]";

    m_entries.clear();
    m_totalTimeMs = 0;

    for (const auto& wline : allLines)
    {
        std::string s(wline.begin(), wline.end());

        // ← added: skip TEST STOPPED lines
        if (s.find(matchTag) != std::string::npos &&
            s.find("Percent:") != std::string::npos &&
            s.find("TEST STOPPED") == std::string::npos)
        {
            ParseLine(s);
        }
    }
}

void CSOHResultDlg::ParseLine(const std::string& line)
{
    SOHEntry entry;

    size_t t1 = line.find("Time:");
    size_t t2 = line.find("| Percent:");
    entry.time = CString(line.substr(t1 + 6, t2 - (t1 + 6)).c_str());

    size_t p1 = line.find("Percent:");
    entry.percent = atoi(line.substr(p1 + 9).c_str());

    size_t d1 = line.find("Duration(ms):");
    entry.durationMs = _strtoui64(line.substr(d1 + 13).c_str(), nullptr, 10);

    {
        ULONGLONG totalSec = entry.durationMs / 1000;
        int h = (int)(totalSec / 3600);
        int m = (int)((totalSec % 3600) / 60);
        int s = (int)(totalSec % 60);
        CString hms;
        hms.Format(_T("%02d:%02d:%02d"), h, m, s);
        entry.durationHMS = hms;
    }

   /* m_totalTimeMs += entry.durationMs;*/
    m_totalTimeMs += (ULONGLONG)entry.durationMs;
    m_entries.push_back(entry);
}

void CSOHResultDlg::DisplayData()
{
    m_list.DeleteAllItems();

    int n = (int)m_entries.size();

    // Add at the START of DisplayData(), right after int n = (int)m_entries.size();
    CString debug;
    debug.Format(_T("n=%d"), n);
    for (int i = 0; i < n; ++i)
    {
        CString row;
        row.Format(_T("\n[%d] pct=%d dur=%llu"), i, m_entries[i].percent, m_entries[i].durationMs);
        debug += row;
    }
  /*  MessageBox(debug, _T("Debug"), MB_OK);*/

    std::vector<bool>       groupEnd(n, false);
    std::vector<ULONGLONG>  groupSum(n, 0);
    std::vector<int>        groupFirst(n, 0);

    if (n == 0)
    {
        ULONGLONG totalSec = m_totalTimeMs / 1000;
        int hrs = (int)(totalSec / 3600);
        int mins = (int)((totalSec % 3600) / 60);
        int secs = (int)(totalSec % 60);
        CString summary;
        summary.Format(
            T(L"Test ID: %s | Start: %s | Total: %02d:%02d:%02d",
                L"テストID: %s | 開始: %s | 合計: %02d:%02d:%02d"),
            m_testIDStr, m_startTimeStr, hrs, mins, secs);
        m_lblSummary.SetWindowText(summary);
        m_groupEnd = groupEnd;
        return;
    }

    int firstPct = m_entries[0].percent;
    int bucketFloor = firstPct - 9;

    int       startRow = 0;
    ULONGLONG bucketMs = 0;

    for (int i = 0; i < n; ++i)
    {
        int pct = m_entries[i].percent;

        while (pct < bucketFloor && i > startRow)
        {
            groupEnd[i - 1] = true;
            groupSum[i - 1] = bucketMs;
            for (int r = startRow; r <= i - 1; ++r)
                groupFirst[r] = startRow;

            bucketFloor -= 10;
            startRow = i;
            bucketMs = 0;
        }

        bucketMs += m_entries[i].durationMs;

        if (i == n - 1)
        {
            groupEnd[i] = true;
            groupSum[i] = bucketMs;
            for (int r = startRow; r <= i; ++r)
                groupFirst[r] = startRow;
        }
    }

    // ── Insert rows ──────────────────────────────────────────────────
    ULONGLONG elapsedMs = 0;
    for (int i = 0; i < n; ++i)
    {
        const auto& e = m_entries[i];
        elapsedMs += e.durationMs;

        m_list.InsertItem(i, e.time);

        CString pct;
        pct.Format(_T("%d%%"), e.percent);
        m_list.SetItemText(i, 1, pct);

        CString durMs;
        durMs.Format(_T("%llu"), e.durationMs);
        m_list.SetItemText(i, 2, durMs);

        m_list.SetItemText(i, 3, e.durationHMS);

        int first = groupFirst[i];
        int last = i;
        for (int k = i; k < n; ++k) { if (groupEnd[k]) { last = k; break; } }

        int midRow = (first + last) / 2;
        if (i == midRow)
        {
            ULONGLONG totalSec = groupSum[last] / 1000;
            int hrs = (int)(totalSec / 3600);
            int mins = (int)((totalSec % 3600) / 60);
            int secs = (int)(totalSec % 60);

            CString sumHMS;
            sumHMS.Format(_T("%02d:%02d:%02d"), hrs, mins, secs);
            m_list.SetItemText(i, 4, sumHMS);
        }

        {
            ULONGLONG eSec = elapsedMs / 1000;
            int eh = (int)(eSec / 3600);
            int em = (int)((eSec % 3600) / 60);
            int es = (int)(eSec % 60);
            CString elapsedHMS;
            elapsedHMS.Format(_T("%02d:%02d:%02d"), eh, em, es);
            m_list.SetItemText(i, 5, elapsedHMS);
        }
    }

    // ── Summary label ────────────────────────────────────────────────
    ULONGLONG totalSec = m_totalTimeMs / 1000;
    int hrs = (int)(totalSec / 3600);
    int mins = (int)((totalSec % 3600) / 60);
    int secs = (int)(totalSec % 60);

    CString summary;
    summary.Format(
        T(L"Test ID: %s | Start: %s | Total: %02d:%02d:%02d",
            L"テストID: %s | 開始: %s | 合計: %02d:%02d:%02d"),
        m_testIDStr, m_startTimeStr, hrs, mins, secs);
    m_lblSummary.SetWindowText(summary);

    // ── Anomaly detection ────────────────────────────────────────────
    m_anomaly.assign(n, false);
    m_anomalyFast.assign(n, false);

    if (n > 1)
    {
        double sum = 0.0;
        for (int i = 0; i < n; ++i)
            sum += (double)m_entries[i].durationMs;
        double mean = sum / n;

        double sq = 0.0;
        for (int i = 0; i < n; ++i)
        {
            double diff = (double)m_entries[i].durationMs - mean;
            sq += diff * diff;
        }
        double stddev = sqrt(sq / n);

        double upperThreshold = mean + 1.5 * stddev;
        double lowerThreshold = max(0.0, mean - 1.5 * stddev);

        for (int i = 0; i < n; ++i)
        {
            double dur = (double)m_entries[i].durationMs;
            m_anomaly[i] = (dur > upperThreshold);
            m_anomalyFast[i] = (dur < lowerThreshold);
        }
    }



    // ── Health Score ─────────────────────────────────────────────
    m_healthScore = 0.0;
    m_consistencyScore = 0.0;
    m_anomalyScore = 0.0;
    m_smoothnessScore = 0.0;

    if (n >= 2)
    {
        // ── Build filtered list: skip big percent jumps (gap > 1) ──
        std::vector<ULONGLONG> filtered;
        for (int i = 0; i < n - 1; ++i)
        {
            int gap = m_entries[i].percent - m_entries[i + 1].percent;
            if (gap == 1)
                filtered.push_back(m_entries[i].durationMs);
        }
        // always include second-to-last if last is a big jump
        if (filtered.empty() && n >= 2)
            filtered.push_back(m_entries[n - 2].durationMs);

        int fn = (int)filtered.size();
        if (fn >= 2)
        {
            // ── COMPONENT 1: CONSISTENCY (CV) ─────────────────────
            // "Are all step durations similar to each other?"
            // CV = stddev / mean.  CV=0 → perfect, CV=1 → chaotic
            double sum = 0.0;
            for (int i = 0; i < fn; ++i)
                sum += (double)filtered[i];
            double mean = sum / fn;

            double sq = 0.0;
            for (int i = 0; i < fn; ++i)
            {
                double d = (double)filtered[i] - mean;
                sq += d * d;
            }
            double stddev = sqrt(sq / fn);
            double cv = (mean > 0.0) ? stddev / mean : 1.0;
            m_consistencyScore = max(0.0, (1.0 - cv) * 100.0);

            // ── COMPONENT 2: ANOMALY SCORE ────────────────────────
            // "What % of steps are NOT wild outliers?"
            double upper = mean + 1.5 * stddev;
            double lower = max(0.0, mean - 1.5 * stddev);
            int anomalyCount = 0;
            for (int i = 0; i < fn; ++i)
            {
                double d = (double)filtered[i];
                if (d > upper || d < lower)
                    anomalyCount++;
            }
            m_anomalyScore = (1.0 - (double)anomalyCount / fn) * 100.0;

            // ── COMPONENT 3: SMOOTHNESS ───────────────────────────
            // "Are neighboring steps changing gradually?"
            double ratioSum = 0.0;
            for (int i = 1; i < fn; ++i)
            {
                double prev = (double)filtered[i - 1];
                double curr = (double)filtered[i];
                if (prev > 0.0)
                    ratioSum += fabs(curr - prev) / prev;
            }
            double avgRatio = ratioSum / (fn - 1);
            m_smoothnessScore = max(0.0, (1.0 - avgRatio) * 100.0);

            // ── COMPOSITE: weighted average ───────────────────────
            m_healthScore = 0.50 * m_consistencyScore
                + 0.30 * m_anomalyScore
                + 0.20 * m_smoothnessScore;
        }
    }


    m_groupEnd = groupEnd;

    ResizeListColumns();
}


void CSOHResultDlg::OnToggleView()
{
    m_showAll = !m_showAll;

    auto SetSummary = [&](const CString& text)
        {
            CRect rc;
            GetClientRect(&rc);

            if (m_lblSummary.GetSafeHwnd())
                m_lblSummary.DestroyWindow();

            m_lblSummary.Create(
                text,
                WS_CHILD | WS_VISIBLE | SS_CENTER,
                CRect(10, rc.Height() - 60, rc.Width() - 10, rc.Height() - 10),
                this
            );
            m_lblSummary.SetFont(GetFont());
        };

    if (m_showChart)
    {
        m_showChart = false;
        m_btnChart.SetWindowText(T(L"Show Chart", L"グラフ表示"));
        m_list.ShowWindow(SW_SHOW);
        m_lblLegendRed.ShowWindow(SW_SHOW);
        m_lblLegendOrange.ShowWindow(SW_SHOW);
        Invalidate();
    }

    if (m_showHealth)
    {
        m_showHealth = false;
        m_btnHealth.SetWindowText(T(L"Health Score", L"健全スコア"));
        m_list.ShowWindow(SW_SHOW);
        ShowScrollBar(SB_VERT, FALSE);
        Invalidate();
    }

    m_list.DeleteAllItems();
    while (m_list.DeleteColumn(0));

    if (m_showAll)
    {
        m_lblLegendRed.ShowWindow(SW_HIDE);
        m_lblLegendOrange.ShowWindow(SW_HIDE);

        /*m_groupEnd.clear();
        m_anomaly.clear();
        m_anomalyFast.clear();*/

        m_showHealth = false;           // ← add this
        m_btnHealth.SetWindowText(T(L"Health Score", L"健全スコア"));

        m_groupEnd.clear();

        m_btnToggle.SetWindowText(T(L"Show Latest", L"最新表示"));

        m_list.InsertColumn(0, T(L"Test ID", L"テストID"), LVCFMT_LEFT, 100);
        m_list.InsertColumn(1, T(L"Start", L"開始"), LVCFMT_LEFT, 140);
        m_list.InsertColumn(2, T(L"Time", L"時刻"), LVCFMT_LEFT, 80);
        m_list.InsertColumn(3, T(L"Percent", L"パーセント"), LVCFMT_CENTER, 80);
        m_list.InsertColumn(4, T(L"Duration(ms)", L"所要時間(ms)"), LVCFMT_CENTER, 120);
        m_list.InsertColumn(5, T(L"Duration(h:m:s)", L"所要時間(時:分:秒)"), LVCFMT_CENTER, 140);
        m_list.InsertColumn(6, T(L"Elapsed(h:m:s)", L"経過時間(時:分:秒)"), LVCFMT_CENTER, 140);

        std::map<CString, std::vector<SOHFullEntry>> grouped;
        for (const auto& e : m_allEntries)
            grouped[e.testID].push_back(e);

        std::vector<CString> order;
        for (const auto& e : m_allEntries)
            if (std::find(order.begin(), order.end(), e.testID) == order.end())
                order.push_back(e.testID);

        std::reverse(order.begin(), order.end());

        CString lastTestID;
        int index = 0;
        ULONGLONG elapsedMs = 0;

        for (const auto& testID : order)
        {
            const auto& entries = grouped[testID];
            elapsedMs = 0;

            for (const auto& e : entries)
            {
                elapsedMs += e.durationMs;
                m_list.InsertItem(index, _T(""));

                if (e.testID != lastTestID)
                {
                    m_list.SetItemText(index, 0, e.testID);
                    m_list.SetItemText(index, 1, e.startTime);
                    lastTestID = e.testID;
                }

                m_list.SetItemText(index, 2, e.time);

                CString pct;
                pct.Format(_T("%d%%"), e.percent);
                m_list.SetItemText(index, 3, pct);

                CString dur;
                dur.Format(_T("%llu"), e.durationMs);
                m_list.SetItemText(index, 4, dur);

                m_list.SetItemText(index, 5, e.durationHMS);

                {
                    ULONGLONG eSec = elapsedMs / 1000;
                    int eh = (int)(eSec / 3600);
                    int em = (int)((eSec % 3600) / 60);
                    int es = (int)(eSec % 60);
                    CString elapsedHMS;
                    elapsedHMS.Format(_T("%02d:%02d:%02d"), eh, em, es);
                    m_list.SetItemText(index, 6, elapsedHMS);
                }

                index++;
            }
        }

        ULONGLONG totalMs = 0;
        for (const auto& e : m_allEntries)
            totalMs += e.durationMs;

        ULONGLONG totalSec = totalMs / 1000;
        int hrs = (int)(totalSec / 3600);
        int mins = (int)((totalSec % 3600) / 60);
        int secs = (int)(totalSec % 60);

        CString summary;
        summary.Format(
            T(L"Total Duration: %02d:%02d:%02d",
                L"総所要時間: %02d:%02d:%02d"),
            hrs, mins, secs);
        SetSummary(summary);
    }
    else
    {
        m_lblLegendRed.ShowWindow(SW_SHOW);
        m_lblLegendOrange.ShowWindow(SW_SHOW);

        m_btnToggle.SetWindowText(T(L"See All", L"全件表示"));

        m_list.InsertColumn(0, T(L"Time", L"時刻"), LVCFMT_LEFT, 100);
        m_list.InsertColumn(1, T(L"Percent", L"パーセント"), LVCFMT_CENTER, 80);
        m_list.InsertColumn(2, T(L"Duration(ms)", L"所要時間(ms)"), LVCFMT_CENTER, 120);
        m_list.InsertColumn(3, T(L"Duration(h:m:s)", L"所要時間(時:分:秒)"), LVCFMT_CENTER, 140);
        m_list.InsertColumn(4,
            T(L"% Range(h:m:s)",
                L"%範囲(時:分:秒)"),
            LVCFMT_CENTER,
            140);
        m_list.InsertColumn(5, T(L"Elapsed(h:m:s)", L"経過時間(時:分:秒)"), LVCFMT_CENTER, 140);

        DisplayData();

        CString text;
        m_lblSummary.GetWindowText(text);
        SetSummary(text);
    }

    ResizeListColumns();
}


void CSOHResultDlg::OnShowChart()
{
    /* if (m_showAll)
     {

         MessageBox(
             T(L"Switch to 'Show Latest' view to see the chart.",
                 L"グラフを表示するには「最新表示」に切り替えてください。"),
             T(L"Chart", L"グラフ"),
             MB_ICONINFORMATION);
         return;
     }*/

    m_showChart = !m_showChart;

    // Hide health view if open
    if (m_showChart && m_showHealth)
    {
        m_showHealth = false;
        m_btnHealth.SetWindowText(T(L"Health Score", L"健全スコア"));
    }

    if (m_showChart)
    {
        m_btnChart.SetWindowText(T(L"Hide Chart", L"グラフ非表示"));
        m_list.ShowWindow(SW_HIDE);
        m_lblLegendRed.ShowWindow(SW_HIDE);
        m_lblLegendOrange.ShowWindow(SW_HIDE);
        m_lblSummary.ShowWindow(SW_HIDE);

        // Also close health view if open
        if (m_showHealth)
        {
            m_showHealth = false;
            m_btnHealth.SetWindowText(T(L"Health Score", L"健全スコア"));
        }
    }
    else
    {
        m_btnChart.SetWindowText(T(L"Show Chart", L"グラフ表示"));
        m_list.ShowWindow(SW_SHOW);
        m_lblLegendRed.ShowWindow(SW_SHOW);
        m_lblLegendOrange.ShowWindow(SW_SHOW);
        m_lblSummary.ShowWindow(SW_SHOW);
    }

    Invalidate();
}


void CSOHResultDlg::OnPaint()
{
    if (m_showHealth && !m_entries.empty())
    {
        DrawHealthScore();
        return;
    }

    if (!m_showChart || m_entries.empty())
    {
        CDialogEx::OnPaint();
        return;
    }

    CPaintDC dc(this);

    CRect rc;
    GetClientRect(&rc);

    const int LEFT = 85;
    const int RIGHT = rc.Width() - 30;
    const int TOP = 100;
    const int BOTTOM = rc.Height() - 130;

    int chartW = RIGHT - LEFT;
    int chartH = BOTTOM - TOP;

    // ── White background ──────────────────────────────────────────
    dc.FillSolidRect(&rc, RGB(255, 255, 255));

    // ── Light grey chart plot area ────────────────────────────────
    CRect chartRect(LEFT, TOP, RIGHT, BOTTOM);
    dc.FillSolidRect(&chartRect, RGB(248, 249, 251));

    // ── Chart border ──────────────────────────────────────────────
    CPen penBorder(PS_SOLID, 1, RGB(220, 222, 228));
    CPen* pOldPen = dc.SelectObject(&penBorder);
    dc.MoveTo(LEFT, TOP);    dc.LineTo(RIGHT, TOP);
    dc.LineTo(RIGHT, BOTTOM); dc.LineTo(LEFT, BOTTOM);
    dc.LineTo(LEFT, TOP);
    dc.SelectObject(pOldPen);

    int n = (int)m_entries.size();

    std::vector<double> cumulative;
    cumulative.reserve(n);
    double totalTime = 0.0;
    for (const auto& e : m_entries)
    {
        cumulative.push_back(totalTime);
        totalTime += (double)e.durationMs;
    }
    double maxTime = totalTime;

    std::vector<int> px(n), py(n);
    for (int i = 0; i < n; ++i)
    {
        px[i] = LEFT + (int)((cumulative[i] / maxTime) * chartW);
        py[i] = BOTTOM - (int)((double)m_entries[i].percent / 100.0 * chartH);
    }

    // ── Fonts ─────────────────────────────────────────────────────
    CFont fontSmall;
    fontSmall.CreatePointFont(72, _T("Segoe UI"));
    CFont* pOldFont = dc.SelectObject(&fontSmall);
    dc.SetBkMode(TRANSPARENT);

    // ── Horizontal grid lines ──────────────────────────────────────
    for (int pct = 0; pct <= 100; pct += 10)
    {
        int y = BOTTOM - (int)((double)pct / 100.0 * chartH);

        COLORREF lineCol = (pct % 50 == 0) ? RGB(190, 193, 200) : RGB(220, 222, 228);
        CPen penGrid(PS_SOLID, 1, lineCol);
        pOldPen = dc.SelectObject(&penGrid);
        dc.MoveTo(LEFT, y);
        dc.LineTo(RIGHT, y);
        dc.SelectObject(pOldPen);

        // Y-axis labels
        dc.SetTextColor(RGB(110, 115, 130));
        CString lbl;
        lbl.Format(_T("%d%%"), pct);
        CSize szL = dc.GetTextExtent(lbl);
        dc.TextOut(LEFT - szL.cx - 8, y - 7, lbl);
    }

    // ── Axes ──────────────────────────────────────────────────────
    CPen penAxis(PS_SOLID, 2, RGB(180, 183, 192));
    pOldPen = dc.SelectObject(&penAxis);
    dc.MoveTo(LEFT, TOP);
    dc.LineTo(LEFT, BOTTOM);
    dc.LineTo(RIGHT, BOTTOM);
    dc.SelectObject(pOldPen);

    // ── Area fill under the line (soft blue tint) ─────────────────
    if (n >= 2)
    {
        // Build polygon
        std::vector<POINT> poly;
        poly.reserve(n + 2);
        poly.push_back({ LEFT, BOTTOM });
        for (int i = 0; i < n; ++i)
            poly.push_back({ px[i], py[i] });
        poly.push_back({ px[n - 1], BOTTOM });

        // Draw horizontal scan lines for a translucent fill effect
        for (int band = 0; band < chartH; ++band)
        {
            double t = (double)band / chartH;
            int alpha = (int)(55.0 * (1.0 - t));
            if (alpha <= 0) continue;

            int y = TOP + band;
            int xLeft = LEFT, xRight = LEFT;
            bool found = false;

            for (int i = 0; i < (int)poly.size() - 1; ++i)
            {
                int y0 = poly[i].y, y1 = poly[i + 1].y;
                int x0 = poly[i].x, x1 = poly[i + 1].x;
                if ((y0 <= y && y < y1) || (y1 <= y && y < y0))
                {
                    double frac = (double)(y - y0) / (y1 - y0);
                    int    xi = x0 + (int)(frac * (x1 - x0));
                    if (!found) { xLeft = xRight = xi; found = true; }
                    else { xLeft = min(xLeft, xi); xRight = max(xRight, xi); }
                }
            }
            if (!found) continue;

            // Soft blue fill — blended into the white bg
            int blue = 210 - (int)(30.0 * t);
            int green = 225 - (int)(15.0 * t);
            CPen penBand(PS_SOLID, 1, RGB(190, green, blue));
            CPen* pOld = dc.SelectObject(&penBand);
            dc.MoveTo(xLeft, y);
            dc.LineTo(xRight, y);
            dc.SelectObject(pOld);
        }
    }

    // ── Main line ─────────────────────────────────────────────────
    if (n >= 2)
    {
        CPen penLine(PS_SOLID, 2, RGB(37, 99, 235));   // blue-600
        pOldPen = dc.SelectObject(&penLine);
        for (int i = 0; i < n; ++i)
        {
            if (i == 0) dc.MoveTo(px[i], py[i]);
            else        dc.LineTo(px[i], py[i]);
        }
        dc.SelectObject(pOldPen);
    }

    // ── Data point dots ───────────────────────────────────────────
    for (int i = 0; i < n; ++i)
    {
        bool isSlow = (i < (int)m_anomaly.size() && m_anomaly[i]);
        bool isFast = (i < (int)m_anomalyFast.size() && m_anomalyFast[i]);

        COLORREF dotFill, dotRim;
        if (isSlow) { dotFill = RGB(220, 50, 50); dotRim = RGB(180, 20, 20); }
        else if (isFast) { dotFill = RGB(234, 128, 0); dotRim = RGB(180, 90, 0); }
        else { dotFill = RGB(37, 99, 235); dotRim = RGB(255, 255, 255); }

        CBrush brushDot(dotFill);
        CBrush* pOldBrush = dc.SelectObject(&brushDot);
        CPen   penDot(PS_SOLID, 2, dotRim);
        CPen* pOldPenDot = dc.SelectObject(&penDot);

        int r = (isSlow || isFast) ? 6 : 4;
        dc.Ellipse(px[i] - r, py[i] - r, px[i] + r, py[i] + r);

        dc.SelectObject(pOldPenDot);
        dc.SelectObject(pOldBrush);
    }

    // ── X-axis labels ─────────────────────────────────────────────
    {
        const int MIN_GAP = 50;
        const int ROW_A = BOTTOM + 18;
        const int ROW_B = BOTTOM + 36;

        dc.SetTextColor(RGB(100, 105, 120));
        dc.SelectObject(&fontSmall);

        CPen penTick(PS_SOLID, 1, RGB(180, 183, 192));
        pOldPen = dc.SelectObject(&penTick);
        for (int i = 0; i < n; ++i)
        {
            dc.MoveTo(px[i], BOTTOM);
            dc.LineTo(px[i], BOTTOM + 4);
        }
        dc.SelectObject(pOldPen);

        int lastDrawnX = -9999;
        int staggerIdx = 0;

        for (int i = 0; i < n; ++i)
        {
            if (px[i] - lastDrawnX < MIN_GAP) continue;

            double sec = cumulative[i] / 1000.0;
            CString lbl;
            if (sec < 60.0)
                lbl.Format(_T("%.0fs"), sec);
            else
            {
                int m2 = (int)(sec / 60);
                int s2 = (int)(sec) % 60;
                lbl.Format(_T("%dm%02ds"), m2, s2);
            }

            int row = (staggerIdx % 2 == 0) ? ROW_A : ROW_B;

            if (row == ROW_B)
            {
                CPen penConn(PS_DOT, 1, RGB(200, 202, 210));
                CPen* pOldC = dc.SelectObject(&penConn);
                dc.MoveTo(px[i], BOTTOM + 4);
                dc.LineTo(px[i], ROW_B - 3);
                dc.SelectObject(pOldC);
            }

            CSize sz = dc.GetTextExtent(lbl);
            dc.TextOut(px[i] - sz.cx / 2, row, lbl);

            lastDrawnX = px[i];
            ++staggerIdx;
        }
    }

    // ── Y-axis vertical label ──────────────────────────────────────
    CFont fontVert;
    LOGFONT lf = {};
    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
    lf.lfHeight = -13;
    lf.lfEscapement = 900;
    lf.lfOrientation = 900;
    fontVert.CreateFontIndirect(&lf);
    dc.SelectObject(&fontVert);
    dc.SetTextColor(RGB(120, 125, 140));
    dc.TextOut(18, BOTTOM - chartH / 2 + 40,
        T(L"Percent (%)", L"割合 (%)"));

    // ── X-axis title ──────────────────────────────────────────────
    CFont fontLabel;
    fontLabel.CreatePointFont(82, _T("Segoe UI"));
    dc.SelectObject(&fontLabel);
    dc.SetTextColor(RGB(120, 125, 140));
    dc.TextOut(LEFT + chartW / 2 - 70, BOTTOM + 58,
        T(L"Elapsed Time (seconds)", L"経過時間 (秒)"));

    // ── Chart title ───────────────────────────────────────────────
    CFont fontTitle;
    fontTitle.CreatePointFont(105, _T("Segoe UI"));
    dc.SelectObject(&fontTitle);
    dc.SetTextColor(RGB(30, 35, 50));

    CString title;
    title.Format(
        T(L"SOH \x2014 Percent vs Elapsed Time  [%s]",
            L"SOH \x2014 割合 vs 経過時間  [%s]"),
        m_testIDStr);
    CSize szT = dc.GetTextExtent(title);
    dc.TextOut(LEFT + (chartW - szT.cx) / 2, TOP - 48, title);

    // ── Legend ────────────────────────────────────────────────────
    dc.SelectObject(&fontSmall);

    CString strSOH = T(L"SOH %", L"SOH %");
    CString strSlow = T(L"Too Slow", L"遅すぎる");
    CString strFast = T(L"Too Fast", L"速すぎる");

    CSize szSOH = dc.GetTextExtent(strSOH);
    CSize szSlow = dc.GetTextExtent(strSlow);
    CSize szFast = dc.GetTextExtent(strFast);

    const int SWATCH = 10;
    const int GAP = 5;
    const int SPACE = 22;

    int totalLegendW = (SWATCH + GAP + szSOH.cx)
        + SPACE + (SWATCH + GAP + szSlow.cx)
        + SPACE + (SWATCH + GAP + szFast.cx);

    int lx = LEFT + (chartW - totalLegendW) / 2;
    int ly = TOP - 20;

    // SOH % — blue square
    {
        CBrush b(RGB(37, 99, 235));
        dc.FillRect(CRect(lx, ly + 1, lx + SWATCH, ly + SWATCH + 1), &b);
    }
    dc.SetTextColor(RGB(60, 65, 80));
    dc.TextOut(lx + SWATCH + GAP, ly, strSOH);
    lx += SWATCH + GAP + szSOH.cx + SPACE;

    // Too Slow — red square
    {
        CBrush b(RGB(220, 50, 50));
        dc.FillRect(CRect(lx, ly + 1, lx + SWATCH, ly + SWATCH + 1), &b);
    }
    dc.SetTextColor(RGB(60, 65, 80));
    dc.TextOut(lx + SWATCH + GAP, ly, strSlow);
    lx += SWATCH + GAP + szSlow.cx + SPACE;

    // Too Fast — orange square
    {
        CBrush b(RGB(234, 128, 0));
        dc.FillRect(CRect(lx, ly + 1, lx + SWATCH, ly + SWATCH + 1), &b);
    }
    dc.SetTextColor(RGB(60, 65, 80));
    dc.TextOut(lx + SWATCH + GAP, ly, strFast);

    dc.SelectObject(pOldFont);
}


void CSOHResultDlg::OnCustomDrawList(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVCUSTOMDRAW* pCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
    *pResult = CDRF_DODEFAULT;

    switch (pCD->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        *pResult = CDRF_NOTIFYITEMDRAW;
        break;

        /*case CDDS_ITEMPREPAINT:
        {
            int row = (int)pCD->nmcd.dwItemSpec;

            if (row < (int)m_anomaly.size() && m_anomaly[row])
            {
                pCD->clrText = RGB(255, 255, 255);
                pCD->clrTextBk = RGB(180, 30, 30);
                *pResult = CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT;
            }
            else if (row < (int)m_anomalyFast.size() && m_anomalyFast[row])
            {
                pCD->clrText = RGB(255, 255, 255);
                pCD->clrTextBk = RGB(200, 110, 0);
                *pResult = CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT;
            }
            else
            {
                *pResult = CDRF_NOTIFYPOSTPAINT;
            }
            break;
        }*/

    case CDDS_ITEMPREPAINT:
    {
        int row = (int)pCD->nmcd.dwItemSpec;

        if (!m_showAll && row < (int)m_anomaly.size() && m_anomaly[row])
        {
            pCD->clrText = RGB(255, 255, 255);
            pCD->clrTextBk = RGB(180, 30, 30);
            *pResult = CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT;
        }
        else if (!m_showAll && row < (int)m_anomalyFast.size() && m_anomalyFast[row])
        {
            pCD->clrText = RGB(255, 255, 255);
            pCD->clrTextBk = RGB(200, 110, 0);
            *pResult = CDRF_NEWFONT | CDRF_NOTIFYPOSTPAINT;
        }
        else
        {
            *pResult = CDRF_NOTIFYPOSTPAINT;
        }
        break;
    }

    case CDDS_ITEMPOSTPAINT:
    {
        int row = (int)pCD->nmcd.dwItemSpec;
        if (row < (int)m_groupEnd.size() && m_groupEnd[row])
        {
            CDC* pDC = CDC::FromHandle(pCD->nmcd.hdc);
            CRect rcItem;
            m_list.GetItemRect(row, &rcItem, LVIR_BOUNDS);

            CPen  pen(PS_SOLID, 2, RGB(0, 0, 0));
            CPen* pOld = pDC->SelectObject(&pen);
            pDC->MoveTo(rcItem.left, rcItem.bottom - 1);
            pDC->LineTo(rcItem.right, rcItem.bottom - 1);
            pDC->SelectObject(pOld);
        }
        *pResult = CDRF_DODEFAULT;
        break;
    }
    }
}


void CSOHResultDlg::OnShowHealth()
{
    // Cannot show in See All mode
  /*  if (m_showAll)
    {
        MessageBox(
            T(L"Switch to 'Show Latest' view to see Health Score.",
                L"健全スコアを表示するには「最新表示」に切り替えてください。"),
            T(L"Health Score", L"健全スコア"),
            MB_ICONINFORMATION);
        return;
    }*/

    m_showHealth = !m_showHealth;

    if (m_showHealth)
    {
        m_btnHealth.SetWindowText(T(L"Hide Health", L"健全スコア非表示"));
        m_list.ShowWindow(SW_HIDE);
        m_lblLegendRed.ShowWindow(SW_HIDE);
        m_lblLegendOrange.ShowWindow(SW_HIDE);
        m_lblSummary.ShowWindow(SW_HIDE);

        // Hide chart if open
        if (m_showChart)
        {
            m_showChart = false;
            m_btnChart.SetWindowText(T(L"Show Chart", L"グラフ表示"));
        }

        // Reset scroll and show scrollbar
        m_healthScrollY = 0;
        m_healthTotalH = 0;
        ShowScrollBar(SB_VERT, TRUE);
        UpdateHealthScrollBar();
    }
    else
    {
        m_btnHealth.SetWindowText(T(L"Health Score", L"健全スコア"));
        m_list.ShowWindow(SW_SHOW);
        m_lblLegendRed.ShowWindow(SW_SHOW);
        m_lblLegendOrange.ShowWindow(SW_SHOW);
        m_lblSummary.ShowWindow(SW_SHOW);

        // Hide scrollbar when leaving health view
        ShowScrollBar(SB_VERT, FALSE);
    }

    Invalidate();
}



BOOL CSOHResultDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (m_showHealth)
    {
        int step = 40;
        m_healthScrollY -= (zDelta > 0 ? step : -step);
        m_healthScrollY = max(0, min(m_healthScrollY, m_healthTotalH));
        UpdateHealthScrollBar();

        // Only invalidate the content area below the button bar
        CRect rc;
        GetClientRect(&rc);
        CRect rcContent(0, 50, rc.Width(), rc.Height());
        InvalidateRect(&rcContent, FALSE);   // FALSE = no erase
        UpdateWindow();                       // repaint immediately, no queue
        return TRUE;
    }
    return CDialogEx::OnMouseWheel(nFlags, zDelta, pt);
}

void CSOHResultDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar)
{
    if (m_showHealth && pBar == nullptr)
    {
        CRect rc; GetClientRect(&rc);
        int pageH = rc.Height() - 55;

        switch (nSBCode)
        {
        case SB_LINEUP:        m_healthScrollY -= 20; break;
        case SB_LINEDOWN:      m_healthScrollY += 20; break;
        case SB_PAGEUP:        m_healthScrollY -= pageH; break;
        case SB_PAGEDOWN:      m_healthScrollY += pageH; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: m_healthScrollY = (int)nPos; break;
        case SB_TOP:           m_healthScrollY = 0; break;
        case SB_BOTTOM:        m_healthScrollY = m_healthTotalH; break;
        }
        m_healthScrollY = max(0, min(m_healthScrollY, m_healthTotalH));
        UpdateHealthScrollBar();

        // Only invalidate the content area below the button bar
        CRect rcContent(0, 50, rc.Width(), rc.Height());
        InvalidateRect(&rcContent, FALSE);   // FALSE = no erase
        UpdateWindow();
        return;
    }
    CDialogEx::OnVScroll(nSBCode, nPos, pBar);
}


// ── Add this helper to .cpp (declare in header as private void) ──
void CSOHResultDlg::UpdateHealthScrollBar()
{
    CRect rc; GetClientRect(&rc);
    int pageH = rc.Height() - 55;
    int totalH = m_healthTotalH + pageH;

    SCROLLINFO si = {};
    si.cbSize = sizeof(si);
    si.fMask = SIF_ALL;
    si.nMin = 0;
    si.nMax = totalH;
    si.nPage = pageH;
    si.nPos = m_healthScrollY;
    SetScrollInfo(SB_VERT, &si, TRUE);
}


// ============================================================
//  THE MAIN DrawHealthScore() FUNCTION
// ============================================================

//void CSOHResultDlg::DrawHealthScore()
//{
//    CPaintDC paintDC(this);
//    CRect rc;
//    GetClientRect(&rc);
//
//    CDC memDC;
//    memDC.CreateCompatibleDC(&paintDC);
//    CBitmap memBmp;
//    memBmp.CreateCompatibleBitmap(&paintDC, rc.Width(), rc.Height());
//    CBitmap* pOldBmp = memDC.SelectObject(&memBmp);
//    CDC& dc = memDC;
//
//  /*  CRect rc;
//    GetClientRect(&rc);*/
//
//   
//
//    // ── Background ───────────────────────────────────────────────
//    dc.FillSolidRect(&rc, RGB(20, 20, 30));
//
//    // ── Button bar safe zone ─────────────────────────────────────
//    // Buttons occupy y=0..44. We paint a separator and start below.
//    dc.FillSolidRect(CRect(0, 0, rc.Width(), 50), RGB(30, 30, 42));
//    CPen penSep(PS_SOLID, 1, RGB(55, 55, 75));
//    CPen* pOldPen = dc.SelectObject(&penSep);
//    dc.MoveTo(0, 49); dc.LineTo(rc.Width(), 49);
//    dc.SelectObject(pOldPen);
//
//    const int BUTTONS_H = 50;           // first pixel we can draw into
//    const int SCROLL_Y = m_healthScrollY;
//    const int CONTENT_X0 = 18;
//    const int CONTENT_X1 = rc.Width() - 20;
//    const int CONTENT_W = CONTENT_X1 - CONTENT_X0;
//
//    // All content drawn with this origin; subtract SCROLL_Y for position
//    // Helper: translate a content-space y to screen y
//    auto Y = [&](int contentY) -> int {
//        return BUTTONS_H + contentY - SCROLL_Y;
//        };
//
//    int n = (int)m_entries.size();
//    double score = m_healthScore;
//
//    //// ── Fonts ────────────────────────────────────────────────────
//    //CFont fontTiny, fontSmall, fontSub, fontMed, fontBig;
//    //fontTiny.CreatePointFont(72, _T("Segoe UI"));
//    //fontSmall.CreatePointFont(82, _T("Segoe UI"));
//    //fontSub.CreatePointFont(92, _T("Segoe UI"));
//    //fontMed.CreatePointFont(105, _T("Segoe UI"));
//    //fontBig.CreatePointFont(240, _T("Segoe UI"));
//    //dc.SetBkMode(TRANSPARENT);
//
//   // ── Fonts ────────────────────────────────────────────────────
//    CFont fontTiny, fontSmall, fontSub, fontMed, fontBig;
//    CFont fontTinyB, fontSmallB, fontSubB, fontMedB;
//
//    CDC* pScreenDC = GetDC();
//    int dpi = GetDeviceCaps(pScreenDC->m_hDC, LOGPIXELSY);
//    ReleaseDC(pScreenDC);
//
//    auto MakeFont = [&](CFont& f, int ptx10, bool bold) {
//        LOGFONT lf = {};
//        lf.lfHeight = -::MulDiv(ptx10, dpi, 720);
//        lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
//        lf.lfCharSet = DEFAULT_CHARSET;
//        lf.lfQuality = CLEARTYPE_QUALITY;
//        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
//        _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
//        f.CreateFontIndirect(&lf);
//        };
//
//    MakeFont(fontTiny, 72, false);
//    MakeFont(fontSmall, 82, false);
//    MakeFont(fontSub, 92, false);
//    MakeFont(fontMed, 105, false);
//    MakeFont(fontBig, 240, false);
//    MakeFont(fontTinyB, 72, true);
//    MakeFont(fontSmallB, 82, true);
//    MakeFont(fontSubB, 92, true);
//    MakeFont(fontMedB, 105, true);
//
//    dc.SetBkMode(TRANSPARENT);
//
//    // ── Score colour ─────────────────────────────────────────────
//    COLORREF scoreColor;
//    CString  scoreLabel;
//    if (score >= 80.0) { scoreColor = RGB(50, 220, 100); scoreLabel = T(L"Healthy", L"健全"); }
//    else if (score >= 60.0) { scoreColor = RGB(220, 190, 0);  scoreLabel = T(L"Moderate", L"普通"); }
//    else { scoreColor = RGB(220, 60, 60);  scoreLabel = T(L"Degraded", L"劣化"); }
//
//    // ════════════════════════════════════════════════════════════
//    //  SECTION 1 — TITLE  (content y = 0)
//    // ════════════════════════════════════════════════════════════
//    int curY = 12;  // content-space cursor
//
//    dc.SelectObject(&fontMedB);
//    dc.SetTextColor(RGB(200, 200, 230));
//    CString title;
//    title.Format(
//        T(L"Battery Discharge Score  [ID-%s]",
//            L"バッテリー放電スコア [ID-%s]"),
//        m_testIDStr);
//    CSize szTitle = dc.GetTextExtent(title);
//    dc.TextOut(CONTENT_X0 + (CONTENT_W - szTitle.cx) / 2, Y(curY), title);
//    curY += szTitle.cy + 10;
//
//    // thin rule
//    {
//        CPen penRule(PS_SOLID, 1, RGB(50, 50, 70));
//        pOldPen = dc.SelectObject(&penRule);
//        dc.MoveTo(CONTENT_X0, Y(curY)); dc.LineTo(CONTENT_X1, Y(curY));
//        dc.SelectObject(pOldPen);
//    }
//    curY += 14;
//
//    // ════════════════════════════════════════════════════════════
//    //  SECTION 2 — CIRCLE + STATUS  (horizontal: circle left, blurb right)
//    // ════════════════════════════════════════════════════════════
//    const int CIRCLE_R = 72;
//    const int CIRCLE_CX = CONTENT_X0 + CIRCLE_R + 10;
//    const int CIRCLE_CY_content = curY + CIRCLE_R;
//
//    // Shadow
//    {
//        CBrush bShadow(RGB(10, 10, 18));
//        CPen   pNull(PS_NULL, 0, RGB(0, 0, 0));
//        dc.SelectObject(&bShadow);
//        dc.SelectObject(&pNull);
//        dc.Ellipse(CIRCLE_CX - CIRCLE_R + 4, Y(CIRCLE_CY_content - CIRCLE_R + 4),
//            CIRCLE_CX + CIRCLE_R + 4, Y(CIRCLE_CY_content + CIRCLE_R + 4));
//    }
//    // Ring + fill
//    {
//        CPen   pRing(PS_SOLID, 6, scoreColor);
//        CBrush bFill(RGB(25, 25, 38));
//        dc.SelectObject(&pRing);
//        dc.SelectObject(&bFill);
//        dc.Ellipse(CIRCLE_CX - CIRCLE_R, Y(CIRCLE_CY_content - CIRCLE_R),
//            CIRCLE_CX + CIRCLE_R, Y(CIRCLE_CY_content + CIRCLE_R));
//    }
//    // Big % inside circle
//    dc.SelectObject(&fontBig);
//    dc.SetTextColor(scoreColor);
//    {
//        CString strPct; strPct.Format(_T("%.0f"), score);
//        CSize sz = dc.GetTextExtent(strPct);
//        dc.TextOut(CIRCLE_CX - sz.cx / 2, Y(CIRCLE_CY_content - sz.cy / 2 - 6), strPct);
//    }
//
//    // "%" small — placed inside circle, top-right of the score number
//    {
//        dc.SelectObject(&fontBig);
//        CString strPct; strPct.Format(_T("%.0f"), score);
//        CSize szBig = dc.GetTextExtent(strPct);
//        dc.SelectObject(&fontSub);
//        dc.SetTextColor(scoreColor);
//        int numLeft = CIRCLE_CX - szBig.cx / 2;
//        int pctX = numLeft + szBig.cx + 2;
//        int pctY = CIRCLE_CY_content - szBig.cy / 2 - 6;
//        dc.TextOut(pctX, Y(pctY), _T("%"));
//    }
//
//    // Status label below circle
//    dc.SelectObject(&fontMed);
//    dc.SetTextColor(scoreColor);
//    {
//        CSize sz = dc.GetTextExtent(scoreLabel);
//        dc.TextOut(CIRCLE_CX - sz.cx / 2, Y(CIRCLE_CY_content + CIRCLE_R + 6), scoreLabel);
//    }
//
//    // ── Right of circle: plain-English what-does-this-mean ───────
//    int blurbX = CIRCLE_CX + CIRCLE_R + 24;
//    int blurbW = CONTENT_X1 - blurbX;
//    int blurbTopY = CIRCLE_CY_content - CIRCLE_R;
//
//    dc.SelectObject(&fontSubB);
//    dc.SetTextColor(RGB(185, 185, 215));
//
//    CString whatIsIt = T(
//        L"What does Discharge Score mean?",
//        L"放電スコアとは何を意味するか？");
//    dc.TextOut(blurbX, Y(blurbTopY), whatIsIt);
//
//    dc.SelectObject(&fontSmall);
//    dc.SetTextColor(RGB(120, 120, 160));
//
//    // Multi-line explanation — manually split to fit blurbW
//    struct Line { CString text; };
//    std::vector<CString> explainLines;
//
//    if (eng_lang) {
//        explainLines.push_back(L"Measures how stable and predictable your battery's");
//        explainLines.push_back(L"discharge curve is. A healthy battery drains each 1%");
//        explainLines.push_back(L"in roughly the same time \u2014 no sudden slow or fast steps.");
//        explainLines.push_back(L"");
//        explainLines.push_back(L"80\u201399%  \u2192  Consistent discharge (good battery)");
//        explainLines.push_back(L"60\u201379%   \u2192  Some variation (monitor it)");
//        explainLines.push_back(L"0\u201359%    \u2192  Irregular discharge (degraded)");
//    }
//    else {
//        explainLines.push_back(L"\u30d0\u30c3\u30c6\u30ea\u30fc\u306e\u653e\u96fb\u66f2\u7dda\u304c\u3069\u308c\u3060\u3051\u5b89\u5b9a\u30fb\u4e88\u6e2c\u53ef\u80fd\u304b\u3092");
//        explainLines.push_back(L"\u6e2c\u5b9a\u3057\u307e\u3059\u3002\u5065\u5168\u306a\u30d0\u30c3\u30c6\u30ea\u30fc\u306f\u54041%\u3092\u307b\u307c\u540c\u3058\u6642\u9593\u3067");
//        explainLines.push_back(L"\u6d88\u8cbb\u3057\u3001\u7a81\u7136\u306e\u9045\u5ef6\u3084\u6025\u901f\u6d88\u8017\u304c\u3042\u308a\u307e\u305b\u3093\u3002");
//        explainLines.push_back(L"");
//        explainLines.push_back(L"80\uff5e100%  \u2192  \u4e00\u8cab\u3057\u305f\u653e\u96fb\uff08\u826f\u597d\uff09");
//        explainLines.push_back(L"60\uff5e79%   \u2192  \u82e5\u5e72\u306e\u3070\u3089\u3064\u304d\uff08\u8981\u76e3\u8996\uff09");
//        explainLines.push_back(L"0\uff5e59%    \u2192  \u4e0d\u898f\u5247\u306a\u653e\u96fb\uff08\u52a3\u5316\uff09");
//    }
//
//   /* int lineH = 16;
//    int ly = blurbTopY + 22;*/
//
//    int lineH = 19;
//    int ly = blurbTopY + 24;
//
//    for (const auto& ln : explainLines)
//    {
//        if (!ln.IsEmpty())
//            dc.TextOut(blurbX, Y(ly), ln);
//        ly += lineH;
//    }
//
//    // advance curY past the circle block
//    curY = CIRCLE_CY_content + CIRCLE_R + 30;
//
//
//
//// ════════════════════════════════════════════════════════════
//    //  SECTION 3 — SUB-SCORE CARDS  (3 horizontal cards)
//    //  REPLACE this entire section in DrawHealthScore()
//    // ════════════════════════════════════════════════════════════
//    {
//        CPen penRule(PS_SOLID, 1, RGB(50, 50, 70));
//        pOldPen = dc.SelectObject(&penRule);
//        dc.MoveTo(CONTENT_X0, Y(curY)); dc.LineTo(CONTENT_X1, Y(curY));
//        dc.SelectObject(pOldPen);
//    }
//    curY += 10;
//
//    /*dc.SelectObject(&fontSub);
//    dc.SetTextColor(RGB(160, 160, 200));
//    CString secLabel = T(L"Score Breakdown", L"\u30b9\u30b3\u30a2\u5185\u8a33");
//    dc.TextOut(CONTENT_X0, Y(curY), secLabel);
//    curY += 22;*/
//
//    dc.SelectObject(&fontSubB);
//    dc.SetTextColor(RGB(160, 160, 200));
//    CString secLabel = T(L"Score Breakdown", L"\u30b9\u30b3\u30a2\u5185\u8a33");
//    dc.TextOut(CONTENT_X0, Y(curY), secLabel);
//    curY += 24;
//
//    auto CardColor = [](double v) -> COLORREF {
//        if (v >= 80.0) return RGB(50, 220, 100);
//        if (v >= 60.0) return RGB(220, 190, 0);
//        return RGB(220, 60, 60);
//        };
//
//    struct SubCard {
//        CString label;
//        CString weight;
//        CString meaning;
//        CString detail1;   // split into two separate lines instead of \n
//        CString detail2;
//        double  value;
//    };
//
//    SubCard cards[3] = {
//        {
//            T(L"Consistency",   L"\u4e00\u8cab\u6027"),
//            T(L"Weight: 50%",   L"\u91cd\u307f: 50%"),
//            T(L"Are all 1% steps taking similar time?",
//              L"\u404b1%\u30b9\u30c6\u30c3\u30d7\u306e\u6642\u9593\u306f\u5747\u4e00\uff1f"),
//            T(L"CV of step durations (stddev/mean).",
//              L"\u30b9\u30c6\u30c3\u30d7\u6642\u9593\u306eCV\u3092\u6e2c\u5b9a\u3002"),
//            T(L"CV near 0 = uniform.  High CV = chaotic.",
//              L"CV\u22480\u3067\u5747\u4e00\u3001\u9ad8\u3044\u3068\u4e0d\u898f\u5247\u3002"),
//            m_consistencyScore
//        },
//        {
//            T(L"Anomaly-Free",  L"\u7570\u5e38\u306a\u3057"),
//            T(L"Weight: 30%",   L"\u91cd\u307f: 30%"),
//            T(L"What % of steps are NOT outliers?",
//              L"\u5916\u308c\u5024\u3067\u306a\u3044\u30b9\u30c6\u30c3\u30d7\u306e\u5272\u5408\uff1f"),
//            T(L"Steps beyond mean \xb1 1.5\u03c3 are flagged.",
//              L"\u5e73\u5747\xb11.5\u03c3\u8d85\u3048\u308b\u30b9\u30c6\u30c3\u30d7\u3092\u691c\u51fa\u3002"),
//            T(L"100% = no outliers found.",
//              L"100%=\u5916\u308c\u5024\u306a\u3057\u3002"),
//            m_anomalyScore
//        },
//        {
//            T(L"Smoothness",    L"\u6ed1\u3089\u304b\u3055"),
//            T(L"Weight: 20%",   L"\u91cd\u307f: 20%"),
//            T(L"Do consecutive steps change gradually?",
//              L"\u9023\u7d9a\u30b9\u30c6\u30c3\u30d7\u306f\u5f90\u3005\u306b\u5909\u5316\uff1f"),
//            T(L"Avg change ratio between neighbours.",
//              L"\u96a3\u63a5\u30b9\u30c6\u30c3\u30d7\u9593\u306e\u5909\u5316\u7387\u5e73\u5747\u3002"),
//            T(L"Sudden jumps lower this score.",
//              L"\u6025\u6fc0\u306a\u5909\u5316\u3067\u30b9\u30b3\u30a2\u4f4e\u4e0b\u3002"),
//            m_smoothnessScore
//        }
//    };
//
//    const int CARD_GAP = 8;
//    const int NUM_CARDS = 3;
//    const int CARD_H = 145;   // taller to fit all text rows
//    const int TOP_BAR = 5;     // colour accent bar height
//    const int PAD = 8;     // inner horizontal padding
//
//    int cardW = (CONTENT_W - CARD_GAP * (NUM_CARDS - 1)) / NUM_CARDS;
//
//    for (int c = 0; c < NUM_CARDS; ++c)
//    {
//        const SubCard& card = cards[c];
//        COLORREF cc = CardColor(card.value);
//
//        int cx0 = CONTENT_X0 + c * (cardW + CARD_GAP);
//        int cx1 = cx0 + cardW;
//        int cy0 = curY;
//
//        // ── Card background ───────────────────────────────────
//        dc.FillSolidRect(CRect(cx0, Y(cy0), cx1, Y(cy0 + CARD_H)), RGB(28, 28, 44));
//
//        // ── Top colour accent bar ─────────────────────────────
//        dc.FillSolidRect(CRect(cx0, Y(cy0), cx1, Y(cy0 + TOP_BAR)), cc);
//
//        // ── Row 1: label (left) + score (right) ───────────────
//        /*int row1Y = cy0 + TOP_BAR + 5;
//        dc.SelectObject(&fontSmall);
//        dc.SetTextColor(RGB(210, 210, 235));
//        dc.TextOut(cx0 + PAD, Y(row1Y), card.label);*/
//
//        int row1Y = cy0 + TOP_BAR + 6;
//        dc.SelectObject(&fontSmallB);
//        dc.SetTextColor(RGB(210, 210, 235));
//        dc.TextOut(cx0 + PAD, Y(row1Y), card.label);
//
//
//        dc.SelectObject(&fontMed);
//        dc.SetTextColor(cc);
//        CString scoreStr; scoreStr.Format(_T("%.0f%%"), card.value);
//        CSize szSc = dc.GetTextExtent(scoreStr);
//        dc.TextOut(cx1 - szSc.cx - PAD, Y(row1Y), scoreStr);
//
//        // ── Row 2: weight tag ─────────────────────────────────
//        int row2Y = row1Y + 18;
//        dc.SelectObject(&fontTiny);
//        dc.SetTextColor(RGB(100, 100, 140));
//        dc.TextOut(cx0 + PAD, Y(row2Y), card.weight);
//
//        // ── Thin divider ──────────────────────────────────────
//        int divY = row2Y + 16;
//        {
//            CPen penDiv(PS_SOLID, 1, RGB(45, 45, 65));
//            pOldPen = dc.SelectObject(&penDiv);
//            dc.MoveTo(cx0 + PAD, Y(divY));
//            dc.LineTo(cx1 - PAD, Y(divY));
//            dc.SelectObject(pOldPen);
//        }
//
//        // ── Row 3: meaning (question) ─────────────────────────
//        int row3Y = divY + 5;
//        dc.SelectObject(&fontTiny);
//        dc.SetTextColor(RGB(150, 150, 190));
//
//        // Use DrawText with clipping so text never overflows the card
//        CRect rcMeaning(cx0 + PAD, Y(row3Y), cx1 - PAD, Y(row3Y + 40));
//        dc.DrawText(card.meaning, &rcMeaning,
//            DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
//
//
//        // ── Row 4 + 5: detail lines ───────────────────────────
//        int row4Y = row3Y + 30;
//        dc.SetTextColor(RGB(85, 85, 120));
//
//        CRect rcD1(cx0 + PAD, Y(row4Y), cx1 - PAD, Y(row4Y + 28));
//        dc.DrawText(card.detail1, &rcD1,
//            DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
//
//        CRect rcD2(cx0 + PAD, Y(row4Y + 30), cx1 - PAD, Y(row4Y + 58));
//        dc.DrawText(card.detail2, &rcD2,
//            DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
//    }
//    curY += CARD_H + 8;
//
//    // ── Formula ───────────────────────────────────────────────────
//    dc.SelectObject(&fontTiny);
//    dc.SetTextColor(RGB(75, 75, 110));
//    CString formula = T(
//        L"Overall = Consistency\xd7""0.5  +  Anomaly-Free\xd7""0.3  +  Smoothness\xd7""0.2",
//        L"\u7dcf\u5408 = \u4e00\u8cab\u6027\xd7""0.5  +  \u7570\u5e38\u306a\u3057\xd7""0.3  +  \u6ed1\u3089\u304b\u3055\xd7""0.2");
//    CSize szF = dc.GetTextExtent(formula);
//    dc.TextOut(CONTENT_X0 + (CONTENT_W - szF.cx) / 2, Y(curY), formula);
//    curY += 22;
//
//    // ════════════════════════════════════════════════════════════
//    //  SECTION 4 — STEP-DURATION BAR CHART  (below everything)
//    // ════════════════════════════════════════════════════════════
//    {
//        CPen penRule(PS_SOLID, 1, RGB(50, 50, 70));
//        pOldPen = dc.SelectObject(&penRule);
//        dc.MoveTo(CONTENT_X0, Y(curY)); dc.LineTo(CONTENT_X1, Y(curY));
//        dc.SelectObject(pOldPen);
//    }
//    curY += 10;
//
//    // Section heading
//   /* dc.SelectObject(&fontSub);
//    dc.SetTextColor(RGB(160, 160, 200));
//    dc.TextOut(CONTENT_X0, Y(curY),
//        T(L"Step Duration Chart", L"ステップ所要時間チャート"));
//    curY += 18;*/
//
//    dc.SelectObject(&fontSubB);
//    dc.SetTextColor(RGB(160, 160, 200));
//    dc.TextOut(CONTENT_X0, Y(curY),
//        T(L"Step Duration Chart", L"ステップ所要時間チャート"));
//    curY += 22;
//
//    // One-liner explanation
//    dc.SelectObject(&fontTiny);
//    dc.SetTextColor(RGB(100, 100, 140));
//    dc.TextOut(CONTENT_X0, Y(curY),
//        T(L"Each bar = time taken for the battery to drop 1%.  "
//            L"Taller bar = slower step.  Red = too slow.  Orange = too fast.  Yellow dashed = average.",
//            L"各バー = バッテリーが1%低下するのに要した時間。"
//            L"高いバー = 遅いステップ。赤=遅すぎ、橙=速すぎ、黄破線=平均。"));
//    curY += 18;
//
//    // Legend swatches
//    auto DrawSwatch = [&](int lx, int ly_content, COLORREF col, const CString& txt, bool dashed = false) -> int
//        {
//            if (dashed)
//            {
//                CPen pd(PS_DASH, 1, col);
//                pOldPen = dc.SelectObject(&pd);
//                dc.MoveTo(lx, Y(ly_content + 5));
//                dc.LineTo(lx + 18, Y(ly_content + 5));
//                dc.SelectObject(pOldPen);
//            }
//            else
//            {
//                CRect sw(lx, Y(ly_content + 1), lx + 10, Y(ly_content + 10));
//                CBrush sb(col);
//                dc.FillRect(&sw, &sb);
//            }
//            dc.SetTextColor(RGB(130, 130, 165));
//            dc.TextOut(lx + 22, Y(ly_content), txt);
//            CSize sz = dc.GetTextExtent(txt);
//            return lx + 22 + sz.cx + 16;
//        };
//
//    dc.SelectObject(&fontTiny);
//    int lx = CONTENT_X0;
//    lx = DrawSwatch(lx, curY, RGB(0, 185, 255), T(L"Normal", L"通常"));
//    lx = DrawSwatch(lx, curY, RGB(220, 60, 60), T(L"Too Slow", L"遅すぎ"));
//    lx = DrawSwatch(lx, curY, RGB(220, 130, 0), T(L"Too Fast", L"速すぎ"));
//    DrawSwatch(lx, curY, RGB(255, 230, 80), T(L"Average", L"平均"), true);
//    curY += 18;
//
//    // ── Chart area ───────────────────────────────────────────────
//    const int CHART_H = 180;
//    const int CHART_PAD_L = 52;   // room for y-axis labels
//    const int CHART_PAD_R = 8;
//    const int CHART_PAD_B = 30;   // room for x-axis labels
//
//    int cleft = CONTENT_X0 + CHART_PAD_L;
//    int cright = CONTENT_X1 - CHART_PAD_R;
//    int ctop = curY;
//    int cbottom = curY + CHART_H;
//    int cw = cright - cleft;
//    int ch = CHART_H;
//
//    dc.FillSolidRect(CRect(cleft, Y(ctop), cright, Y(cbottom)), RGB(15, 15, 25));
//
//    if (n >= 2)
//    {
//        // ── Stats ──────────────────────────────────────────────
//        double sumD = 0.0;
//        for (int i = 0; i < n; ++i) sumD += (double)m_entries[i].durationMs;
//        double meanD = sumD / n;
//
//        double sqD = 0.0;
//        for (int i = 0; i < n; ++i) {
//            double d = (double)m_entries[i].durationMs - meanD;
//            sqD += d * d;
//        }
//        double stddevD = sqrt(sqD / n);
//        double upperT = meanD + 1.5 * stddevD;
//        double lowerT = max(0.0, meanD - 1.5 * stddevD);
//
//        ULONGLONG maxD = 1;
//        for (int i = 0; i < n; ++i)
//            if (m_entries[i].durationMs > maxD)
//                maxD = m_entries[i].durationMs;
//
//        // ── Y gridlines + labels ───────────────────────────────
//        auto FmtMs = [](ULONGLONG ms) -> CString {
//            ULONGLONG s = ms / 1000;
//            if (s < 60) { CString r; r.Format(_T("%llus"), s); return r; }
//            int m2 = (int)(s / 60), s2 = (int)(s % 60);
//            CString r; r.Format(_T("%dm%ds"), m2, s2); return r;
//            };
//
//        dc.SelectObject(&fontTiny);
//        for (int g = 0; g <= 4; ++g)
//        {
//            double frac = (double)g / 4.0;
//            int    gy = ctop + (int)(frac * ch);
//            ULONGLONG lMs = (ULONGLONG)((1.0 - frac) * (double)maxD);
//
//            CPen pg(PS_DOT, 1, RGB(38, 38, 55));
//            pOldPen = dc.SelectObject(&pg);
//            dc.MoveTo(cleft, Y(gy)); dc.LineTo(cright, Y(gy));
//            dc.SelectObject(pOldPen);
//
//            CString lbl = FmtMs(lMs);
//            CSize szL = dc.GetTextExtent(lbl);
//            dc.SetTextColor(RGB(90, 90, 125));
//            dc.TextOut(cleft - szL.cx - 4, Y(gy) - 7, lbl);
//        }
//
//        // ── Average dashed line ────────────────────────────────
//        int avgY = ctop + (int)((1.0 - meanD / (double)maxD) * ch);
//        {
//            CPen pa(PS_DASH, 1, RGB(255, 230, 80));
//            pOldPen = dc.SelectObject(&pa);
//            dc.MoveTo(cleft, Y(avgY));
//            dc.LineTo(cright, Y(avgY));
//            dc.SelectObject(pOldPen);
//        }
//        dc.SelectObject(&fontTiny);
//        dc.SetTextColor(RGB(210, 190, 50));
//        dc.TextOut(cright + 2, Y(avgY) - 7, T(L"Avg", L"平均"));
//
//        // ── Bars ──────────────────────────────────────────────
//        int barW = max(2, cw / n - 1);
//
//        for (int i = 0; i < n; ++i)
//        {
//            double dur = (double)m_entries[i].durationMs;
//            int barH = (int)(dur / (double)maxD * ch);
//            if (barH < 1) barH = 1;
//
//            int bx = cleft + (int)((double)i / n * cw);
//            int by = cbottom - barH;
//
//            bool isSlow = (dur > upperT);
//            bool isFast = (dur < lowerT && lowerT > 0.0);
//
//            COLORREF barCol = isSlow ? RGB(220, 60, 60)
//                : isFast ? RGB(220, 130, 0)
//                : RGB(0, 185, 255);
//
//            CRect barRc(bx, Y(by), bx + barW, Y(cbottom));
//            CBrush bb(barCol);
//            dc.FillRect(&barRc, &bb);
//
//            // top highlight
//            COLORREF topCol = RGB(min(255, GetRValue(barCol) + 40),
//                min(255, GetGValue(barCol) + 40),
//                min(255, GetBValue(barCol) + 40));
//            CRect topRc(bx, Y(by), bx + barW, Y(by) + 2);
//            CBrush bt(topCol);
//            dc.FillRect(&topRc, &bt);
//        }
//
//        // ── X-axis percent labels ──────────────────────────────
//        dc.SelectObject(&fontTiny);
//        dc.SetTextColor(RGB(90, 90, 130));
//
//        const int MIN_GAP = 36;
//        int lastLX = -9999;
//
//        for (int i = 0; i < n; ++i)
//        {
//            int bx = cleft + (int)((double)i / n * cw) + barW / 2;
//            if (bx - lastLX < MIN_GAP) continue;
//
//            CPen pt(PS_SOLID, 1, RGB(60, 60, 80));
//            pOldPen = dc.SelectObject(&pt);
//            dc.MoveTo(bx, Y(cbottom));
//            dc.LineTo(bx, Y(cbottom) + 4);
//            dc.SelectObject(pOldPen);
//
//            CString pl; pl.Format(_T("%d%%"), m_entries[i].percent);
//            CSize szP = dc.GetTextExtent(pl);
//            dc.TextOut(bx - szP.cx / 2, Y(cbottom) + 5, pl);
//            lastLX = bx;
//        }
//
//        // X-axis title
//        dc.SetTextColor(RGB(85, 85, 125));
//        CString xTitle = T(L"Battery % at each measurement",
//            L"各測定時のバッテリー残量 (%)");
//        CSize szXT = dc.GetTextExtent(xTitle);
//        dc.TextOut(cleft + (cw - szXT.cx) / 2, Y(cbottom) + 18, xTitle);
//    }
//
//    // ── Axes lines ─────────────────────────────────────────────
//    {
//        CPen pAxis(PS_SOLID, 1, RGB(75, 75, 100));
//        pOldPen = dc.SelectObject(&pAxis);
//        dc.MoveTo(cleft, Y(ctop));
//        dc.LineTo(cleft, Y(cbottom));
//        dc.LineTo(cright, Y(cbottom));
//        dc.SelectObject(pOldPen);
//    }
//
//    curY = cbottom + CHART_PAD_B + 10;
//
//    // ════════════════════════════════════════════════════════════
//    //  Track total content height for scroll
//    // ════════════════════════════════════════════════════════════
//    int viewH = rc.Height() - BUTTONS_H;
//    m_healthTotalH = max(0, curY - viewH);
//    // update scrollbar range silently
//    {
//        SCROLLINFO si = {};
//        si.cbSize = sizeof(si);
//        si.fMask = SIF_ALL;
//        si.nMin = 0;
//        si.nMax = curY;
//        si.nPage = viewH;
//        si.nPos = m_healthScrollY;
//        SetScrollInfo(SB_VERT, &si, FALSE);
//    }
//
//    paintDC.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
//    memDC.SelectObject(pOldBmp);
//
//}


// ============================================================
//  DrawHealthScore() — v4:
//  - "Discharge Score" title
//  - Test ID top-right in hero
//  - Larger fonts throughout
//  - More spacing between sub-score rows
//  - Bigger bar chart
//  - Chart legend colours match bars exactly
//  Same logic/content/scroll. Replace only this function body.
// ============================================================

void CSOHResultDlg::DrawHealthScore()
{
    CPaintDC paintDC(this);
    CRect rc;
    GetClientRect(&rc);

    CDC memDC;
    memDC.CreateCompatibleDC(&paintDC);
    CBitmap memBmp;
    memBmp.CreateCompatibleBitmap(&paintDC, rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&memBmp);
    CDC& dc = memDC;

    dc.FillSolidRect(&rc, RGB(255, 255, 255));

    // ── Button bar ───────────────────────────────────────────────
    dc.FillSolidRect(CRect(0, 0, rc.Width(), 50), RGB(248, 248, 248));
    {
        CPen p(PS_SOLID, 1, RGB(218, 218, 218));
        CPen* po = dc.SelectObject(&p);
        dc.MoveTo(0, 49); dc.LineTo(rc.Width(), 49);
        dc.SelectObject(po);
    }

    const int BUTTONS_H = 50;
    const int PX = 28;
    const int CONTENT_X0 = PX;
    const int CONTENT_X1 = rc.Width() - PX;
    const int CONTENT_W = CONTENT_X1 - CONTENT_X0;

    auto Y = [&](int cy) -> int { return BUTTONS_H + cy - m_healthScrollY; };

    int n = (int)m_entries.size();
    double score = m_healthScore;

    // ── Fonts ────────────────────────────────────────────────────
    CFont fTiny, fSmall, fSub, fMed, fLarge, fHero, fTitle;
    CFont fSmallB, fSubB, fMedB, fLargeB;

    CDC* pScrDC = GetDC();
    int dpi = GetDeviceCaps(pScrDC->m_hDC, LOGPIXELSY);
    ReleaseDC(pScrDC);

    auto Mk = [&](CFont& f, int ptx10, bool bold) {
        LOGFONT lf = {};
        lf.lfHeight = -::MulDiv(ptx10, dpi, 720);
        lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = CLEARTYPE_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_SWISS;
        _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
        f.CreateFontIndirect(&lf);
        };
    Mk(fTiny, 82, false);   // was 72 — bumped up
    Mk(fSmall, 92, false);   // was 82
    Mk(fSub, 100, false);   // was 92
    Mk(fMed, 115, false);   // was 105
    Mk(fLarge, 150, false);   // was 130
    Mk(fHero, 360, false);
    Mk(fTitle, 130, false);
    Mk(fSmallB, 92, true);
    Mk(fSubB, 100, true);
    Mk(fMedB, 115, true);
    Mk(fLargeB, 150, true);

    dc.SetBkMode(TRANSPARENT);

    // ── Score colour ─────────────────────────────────────────────
    COLORREF scoreCol;
    CString  scoreLabel;
    if (score >= 80.0) { scoreCol = RGB(22, 140, 65);  scoreLabel = T(L"Healthy", L"健全"); }
    else if (score >= 60.0) { scoreCol = RGB(168, 118, 0);  scoreLabel = T(L"Moderate", L"普通"); }
    else { scoreCol = RGB(185, 38, 38);  scoreLabel = T(L"Degraded", L"劣化"); }

    COLORREF scoreTint;
    if (score >= 80.0) scoreTint = RGB(236, 250, 241);
    else if (score >= 60.0) scoreTint = RGB(252, 246, 225);
    else                    scoreTint = RGB(252, 236, 236);

    int curY = 14;

    // ════════════════════════════════════════════════════════════
    //  PAGE TITLE
    // ════════════════════════════════════════════════════════════
    dc.SelectObject(&fMedB);
    dc.SetTextColor(RGB(35, 35, 35));
    CString pageTitle = T(L"Discharge Score", L"放電スコア");
    dc.TextOut(CONTENT_X0, Y(curY), pageTitle);
    CSize szPT = dc.GetTextExtent(pageTitle);
    curY += szPT.cy + 10;

    // Divider under title
    {
        CPen pr(PS_SOLID, 1, RGB(225, 225, 225));
        CPen* po = dc.SelectObject(&pr);
        dc.MoveTo(CONTENT_X0, Y(curY)); dc.LineTo(CONTENT_X1, Y(curY));
        dc.SelectObject(po);
    }
    curY += 12;

    // ════════════════════════════════════════════════════════════
    //  HERO BLOCK — tinted banner
    // ════════════════════════════════════════════════════════════
    const int HERO_H = 96;

    dc.FillSolidRect(CRect(CONTENT_X0, Y(curY), CONTENT_X1, Y(curY + HERO_H)), scoreTint);
    {
        CPen pb(PS_SOLID, 1, scoreCol);
        CBrush nb; nb.CreateStockObject(NULL_BRUSH);
        CPen* po = dc.SelectObject(&pb);
        CBrush* bo = dc.SelectObject(&nb);
        dc.Rectangle(CONTENT_X0, Y(curY), CONTENT_X1, Y(curY + HERO_H));
        dc.SelectObject(bo); dc.SelectObject(po);
    }

    // Test ID — TOP RIGHT of hero box
    dc.SelectObject(&fSmall);
    dc.SetTextColor(scoreCol);
    CString idStr; idStr.Format(T(L"Test ID: %s", L"テストID: %s"), m_testIDStr);
    CSize szID = dc.GetTextExtent(idStr);
    dc.TextOut(CONTENT_X1 - szID.cx - 10, Y(curY + 8), idStr);

    // Big score number — left
    dc.SelectObject(&fHero);
    dc.SetTextColor(scoreCol);
    CString strScore; strScore.Format(_T("%.0f"), score);
    CSize szHero = dc.GetTextExtent(strScore);
    int heroNumX = CONTENT_X0 + 18;
    dc.TextOut(heroNumX, Y(curY - 6), strScore);

    // "%" small
    dc.SelectObject(&fLarge);
    dc.SetTextColor(scoreCol);
    dc.TextOut(heroNumX + szHero.cx + 3, Y(curY + 8), _T("%"));

    // Status label — below number, left-aligned under it
    dc.SelectObject(&fMedB);
    dc.SetTextColor(scoreCol);
    CSize szLbl = dc.GetTextExtent(scoreLabel);
    dc.TextOut(heroNumX + (szHero.cx - szLbl.cx) / 2 + 4, Y(curY + HERO_H - 26), scoreLabel);

    // Horizontal gauge bar — right side
    const int GAUGE_X0 = heroNumX + szHero.cx + 60;
    const int GAUGE_X1 = CONTENT_X1 - 14;
    const int GAUGE_W = GAUGE_X1 - GAUGE_X0;
    const int GAUGE_MID = curY + HERO_H / 2 - 4;
    const int GAUGE_H = 14;
    const int GAUGE_R = GAUGE_H / 2;

    // Track
    {
        CBrush bt(RGB(205, 205, 205));
        CPen   pn(PS_NULL, 0, RGB(0, 0, 0));
        dc.SelectObject(&bt); dc.SelectObject(&pn);
        dc.RoundRect(GAUGE_X0, Y(GAUGE_MID), GAUGE_X1, Y(GAUGE_MID + GAUGE_H),
            GAUGE_R * 2, GAUGE_R * 2);
    }
    // Fill
    int fillW = (int)((score / 100.0) * GAUGE_W);
    if (fillW > 2)
    {
        CBrush bf(scoreCol);
        CPen   pn(PS_NULL, 0, RGB(0, 0, 0));
        dc.SelectObject(&bf); dc.SelectObject(&pn);
        dc.RoundRect(GAUGE_X0, Y(GAUGE_MID), GAUGE_X0 + fillW, Y(GAUGE_MID + GAUGE_H),
            GAUGE_R * 2, GAUGE_R * 2);
    }
    // Tick marks at 60, 80
    for (int thresh : {60, 80})
    {
        int tx = GAUGE_X0 + (int)(thresh / 100.0 * GAUGE_W);
        CPen pt(PS_SOLID, 1, RGB(170, 170, 170));
        CPen* po = dc.SelectObject(&pt);
        dc.MoveTo(tx, Y(GAUGE_MID - 5));
        dc.LineTo(tx, Y(GAUGE_MID + GAUGE_H + 5));
        dc.SelectObject(po);

        dc.SelectObject(&fSmall);
        dc.SetTextColor(RGB(150, 150, 150));
        CString tl; tl.Format(_T("%d"), thresh);
        CSize szTl = dc.GetTextExtent(tl);
        dc.TextOut(tx - szTl.cx / 2, Y(GAUGE_MID + GAUGE_H + 6), tl);
    }
    // Gauge label
    dc.SelectObject(&fSmall);
    dc.SetTextColor(RGB(145, 145, 145));
    dc.TextOut(GAUGE_X0, Y(GAUGE_MID - 18),
        T(L"Overall score  (0 – 100)", L"総合スコア (0 – 100)"));

    curY += HERO_H + 18;

    // ── Score range legend ───────────────────────────────────────
    {
        struct RangeItem { COLORREF col; CString range; CString label; };
        RangeItem ranges[3] = {
            { RGB(22,140,65),
              T(L"80 \u2013 100", L"80 \u2013 100"),
              T(L"Healthy \u2014 consistent, stable discharge",
                L"\u5065\u5168 \u2014 \u4e00\u8cab\u3057\u305f\u5b89\u5b9a\u653e\u96fb") },
            { RGB(168,118,0),
              T(L"60 \u2013 79",  L"60 \u2013 79"),
              T(L"Moderate \u2014 some variation, worth monitoring",
                L"\u666e\u901a \u2014 \u82e5\u5e72\u306e\u3070\u3089\u3064\u304d\u3001\u8981\u76e3\u8996") },
            { RGB(185,38,38),
              T(L"0 \u2013 59",   L"0 \u2013 59"),
              T(L"Degraded \u2014 irregular discharge, battery may need replacement",
                L"\u52a3\u5316 \u2014 \u4e0d\u898f\u5247\u306a\u653e\u96fb\u3001\u4ea4\u63db\u8981\u691c\u8a0e") },
        };

        dc.SelectObject(&fSmall);
        dc.SetTextColor(RGB(170, 170, 170));
        dc.TextOut(CONTENT_X0, Y(curY), T(L"Score ranges", L"\u30b9\u30b3\u30a2\u7bc4\u56f2"));
        curY += 20;

        const int RH = 26, RGAP = 4, DOT = 10;
        for (int i = 0; i < 3; ++i)
        {
            COLORREF cc = ranges[i].col;
            int ry = curY + i * (RH + RGAP);

            // coloured dot
            CBrush bd(cc); CPen pn(PS_NULL, 0, cc);
            dc.SelectObject(&bd); dc.SelectObject(&pn);
            dc.Ellipse(CONTENT_X0, Y(ry + (RH - DOT) / 2),
                CONTENT_X0 + DOT, Y(ry + (RH + DOT) / 2));

            // range number bold + coloured
            dc.SelectObject(&fSmallB);
            dc.SetTextColor(cc);
            dc.TextOut(CONTENT_X0 + DOT + 8, Y(ry + 6), ranges[i].range);
            CSize szR = dc.GetTextExtent(ranges[i].range);

            // colon separator
            dc.SelectObject(&fSmall);
            dc.SetTextColor(RGB(195, 195, 195));
            int sepX = CONTENT_X0 + DOT + 8 + szR.cx + 5;
            dc.TextOut(sepX, Y(ry + 6), _T(":"));
            CSize szC = dc.GetTextExtent(_T(":"));

            // description
            dc.SetTextColor(RGB(100, 100, 100));
            dc.TextOut(sepX + szC.cx + 5, Y(ry + 6), ranges[i].label);
        }
        curY += 3 * (RH + RGAP) + 8;

        // divider
        CPen pr(PS_SOLID, 1, RGB(228, 228, 228));
        CPen* po = dc.SelectObject(&pr);
        dc.MoveTo(CONTENT_X0, Y(curY)); dc.LineTo(CONTENT_X1, Y(curY));
        dc.SelectObject(po);
        curY += 12;
    }

    // ════════════════════════════════════════════════════════════
    //  SCORE BREAKDOWN — horizontal rows, more spacing
    // ════════════════════════════════════════════════════════════
    dc.SelectObject(&fSubB);
    dc.SetTextColor(RGB(50, 50, 50));
    dc.TextOut(CONTENT_X0, Y(curY), T(L"Score breakdown", L"スコア内訳"));
    curY += 26;

    auto SubCol = [](double v) -> COLORREF {
        if (v >= 80.0) return RGB(22, 140, 65);
        if (v >= 60.0) return RGB(168, 118, 0);
        return RGB(185, 38, 38);
        };

    struct SubRow { CString name, weight, desc; double val; };
    SubRow rows[3] = {
        {
            T(L"Consistency",   L"一貫性"),
            T(L"50%",           L"50%"),
            T(L"Uniformity of step durations  (CV of durations, lower = better)",
              L"ステップ所要時間の均一性 (CV、低いほど良い)"),
            m_consistencyScore
        },
        {
            T(L"Anomaly-free",  L"異常なし"),
            T(L"30%",           L"30%"),
            T(L"Steps within mean \xb1 1.5\u03c3  (higher = fewer outliers)",
              L"平均 \xb1 1.5\u03c3 内のステップ割合 (高いほど良い)"),
            m_anomalyScore
        },
        {
            T(L"Smoothness",    L"滑らかさ"),
            T(L"20%",           L"20%"),
            T(L"Gradual change between consecutive steps  (lower ratio = smoother)",
              L"連続ステップの変化が緩やか (変化率が低いほど良い)"),
            m_smoothnessScore
        }
    };

    const int ROW_H = 48;    // taller rows
    const int ROW_GAP = 10;    // more gap between rows
    const int BAR_W = 120;

    for (int i = 0; i < 3; ++i)
    {
        COLORREF cc = SubCol(rows[i].val);
        int ry0 = curY + i * (ROW_H + ROW_GAP);
        int ry1 = ry0 + ROW_H;

        // Background + border
        dc.FillSolidRect(CRect(CONTENT_X0, Y(ry0), CONTENT_X1, Y(ry1)), RGB(251, 251, 251));
        {
            CPen pb(PS_SOLID, 1, RGB(222, 222, 222));
            CBrush nb; nb.CreateStockObject(NULL_BRUSH);
            CPen* po = dc.SelectObject(&pb);
            CBrush* bo = dc.SelectObject(&nb);
            dc.Rectangle(CONTENT_X0, Y(ry0), CONTENT_X1, Y(ry1));
            dc.SelectObject(bo); dc.SelectObject(po);
        }
        // Left colour stripe
        dc.FillSolidRect(CRect(CONTENT_X0, Y(ry0), CONTENT_X0 + 5, Y(ry1)), cc);

        const int IP = 14;

        // Name
        dc.SelectObject(&fSmallB);
        dc.SetTextColor(RGB(35, 35, 35));
        dc.TextOut(CONTENT_X0 + IP, Y(ry0 + 8), rows[i].name);

        // Weight badge
        dc.SelectObject(&fSmall);
        CSize szName; dc.SelectObject(&fSmallB); szName = dc.GetTextExtent(rows[i].name);
        dc.SelectObject(&fSmall);
        dc.SetTextColor(cc);
        CString wBadge; wBadge.Format(T(L"[%s]", L"[%s]"), rows[i].weight);
        dc.TextOut(CONTENT_X0 + IP + szName.cx + 8, Y(ry0 + 11), wBadge);

        // Description
        dc.SetTextColor(RGB(125, 125, 125));
        dc.TextOut(CONTENT_X0 + IP, Y(ry0 + 28), rows[i].desc);

        // Score value
        int scoreX = CONTENT_X1 - BAR_W - 62;
        dc.SelectObject(&fSmallB);
        dc.SetTextColor(cc);
        CString sv; sv.Format(_T("%.0f%%"), rows[i].val);
        CSize szSv = dc.GetTextExtent(sv);
        dc.TextOut(scoreX - szSv.cx - 4, Y(ry0 + (ROW_H - szSv.cy) / 2 + 1), sv);

        // Mini bar
        int barX0 = CONTENT_X1 - BAR_W - 8;
        int barX1 = CONTENT_X1 - 8;
        int barMidY = ry0 + ROW_H / 2 - 3;
        dc.FillSolidRect(CRect(barX0, Y(barMidY), barX1, Y(barMidY + 6)), RGB(215, 215, 215));
        int mfW = (int)((rows[i].val / 100.0) * (barX1 - barX0));
        if (mfW > 1)
            dc.FillSolidRect(CRect(barX0, Y(barMidY), barX0 + mfW, Y(barMidY + 6)), cc);
    }
    curY += 3 * (ROW_H + ROW_GAP) + 4;

    // Formula line
    dc.SelectObject(&fSmall);
    dc.SetTextColor(RGB(185, 185, 185));
    CString formula = T(
        L"Overall = Consistency \xd7 0.5  +  Anomaly-free \xd7 0.3  +  Smoothness \xd7 0.2",
        L"総合 = 一貫性 \xd7 0.5  +  異常なし \xd7 0.3  +  滑らかさ \xd7 0.2");
    CSize szF = dc.GetTextExtent(formula);
    dc.TextOut(CONTENT_X0 + (CONTENT_W - szF.cx) / 2, Y(curY), formula);
    curY += 24;

    // Divider
    {
        CPen pr(PS_SOLID, 1, RGB(228, 228, 228));
        CPen* po = dc.SelectObject(&pr);
        dc.MoveTo(CONTENT_X0, Y(curY)); dc.LineTo(CONTENT_X1, Y(curY));
        dc.SelectObject(po);
    }
    curY += 16;

    // ════════════════════════════════════════════════════════════
    //  BAR CHART — exact colours used in bars, matched in legend
    // ════════════════════════════════════════════════════════════
    const COLORREF COL_NORMAL = RGB(60, 140, 210);
    const COLORREF COL_SLOW = RGB(195, 48, 48);
    const COLORREF COL_FAST = RGB(200, 112, 0);
    const COLORREF COL_AVG = RGB(155, 125, 0);

    // Section heading
    dc.SelectObject(&fSubB);
    dc.SetTextColor(RGB(50, 50, 50));
    dc.TextOut(CONTENT_X0, Y(curY), T(L"Step duration chart", L"ステップ所要時間チャート"));
    curY += 8;

    // Caption
    dc.SelectObject(&fSmall);
    dc.SetTextColor(RGB(160, 160, 160));
    dc.TextOut(CONTENT_X0, Y(curY + 16),
        T(L"Each bar = time for battery to drop 1%",
            L"各バー = バッテリーが1%低下する時間"));
    curY += 36;

    // Legend row — swatches with exact bar colours
    {
        struct LegItem { COLORREF col; CString label; bool dashed; };
        LegItem items[] = {
            { COL_NORMAL, T(L"Normal",   L"通常"),   false },
            { COL_SLOW,   T(L"Too slow", L"遅すぎ"), false },
            { COL_FAST,   T(L"Too fast", L"速すぎ"), false },
            { COL_AVG,    T(L"Average",  L"平均"),   true  },
        };

        dc.SelectObject(&fSmall);
        int lx = CONTENT_X0;
        for (auto& it : items)
        {
            if (it.dashed)
            {
                CPen pd(PS_DASH, 1, it.col);
                CPen* po = dc.SelectObject(&pd);
                dc.MoveTo(lx, Y(curY + 7));
                dc.LineTo(lx + 20, Y(curY + 7));
                dc.SelectObject(po);
            }
            else
            {
                dc.FillSolidRect(CRect(lx, Y(curY + 2), lx + 12, Y(curY + 13)), it.col);
            }
            dc.SetTextColor(RGB(85, 85, 85));
            dc.TextOut(lx + 24, Y(curY), it.label);
            CSize szL = dc.GetTextExtent(it.label);
            lx += 24 + szL.cx + 18;
        }
        curY += 22;
    }

    // Chart area — taller than v3
    const int CHART_H = 200;
    const int CHART_PAD_L = 58;
    const int CHART_PAD_B = 30;

    int cleft = CONTENT_X0 + CHART_PAD_L;
    int cright = CONTENT_X1;
    int ctop = curY;
    int cbottom = curY + CHART_H;
    int cw = cright - cleft;
    int ch = CHART_H;

    dc.FillSolidRect(CRect(cleft, Y(ctop), cright, Y(cbottom)), RGB(250, 250, 250));

    if (n >= 2)
    {
        double sumD = 0.0;
        for (int i = 0; i < n; ++i) sumD += (double)m_entries[i].durationMs;
        double meanD = sumD / n;
        double sqD = 0.0;
        for (int i = 0; i < n; ++i) {
            double d = (double)m_entries[i].durationMs - meanD;
            sqD += d * d;
        }
        double stddevD = sqrt(sqD / n);
        double upperT = meanD + 1.5 * stddevD;
        double lowerT = max(0.0, meanD - 1.5 * stddevD);

        ULONGLONG maxD = 1;
        for (int i = 0; i < n; ++i)
            if (m_entries[i].durationMs > maxD) maxD = m_entries[i].durationMs;

        auto FmtMs = [](ULONGLONG ms) -> CString {
            ULONGLONG s = ms / 1000;
            if (s < 60) { CString r; r.Format(_T("%llus"), s); return r; }
            CString r; r.Format(_T("%dm%ds"), (int)(s / 60), (int)(s % 60)); return r;
            };

        // Y gridlines + labels
        dc.SelectObject(&fSmall);
        for (int g = 0; g <= 4; ++g)
        {
            double    frac = (double)g / 4.0;
            int       gy = ctop + (int)(frac * ch);
            ULONGLONG lMs = (ULONGLONG)((1.0 - frac) * (double)maxD);

            CPen pg(PS_DOT, 1, RGB(215, 215, 215));
            CPen* po = dc.SelectObject(&pg);
            dc.MoveTo(cleft, Y(gy)); dc.LineTo(cright, Y(gy));
            dc.SelectObject(po);

            CString lbl = FmtMs(lMs);
            CSize   szL = dc.GetTextExtent(lbl);
            dc.SetTextColor(RGB(155, 155, 155));
            dc.TextOut(cleft - szL.cx - 5, Y(gy) - 8, lbl);
        }

        // Mean dashed line — COL_AVG
        int avgY = ctop + (int)((1.0 - meanD / (double)maxD) * ch);
        {
            CPen pa(PS_DASH, 1, COL_AVG);
            CPen* po = dc.SelectObject(&pa);
            dc.MoveTo(cleft, Y(avgY)); dc.LineTo(cright, Y(avgY));
            dc.SelectObject(po);
        }
        dc.SelectObject(&fSmall);
        dc.SetTextColor(COL_AVG);
        dc.TextOut(cright + 3, Y(avgY) - 8, T(L"Avg", L"平均"));

        // Bars — using named colour constants
        int barW = max(2, cw / n - 1);
        for (int i = 0; i < n; ++i)
        {
            double dur = (double)m_entries[i].durationMs;
            int    barH = (int)(dur / (double)maxD * ch);
            if (barH < 1) barH = 1;

            int bx = cleft + (int)((double)i / n * cw);
            int by = cbottom - barH;

            bool isSlow = (dur > upperT);
            bool isFast = (dur < lowerT && lowerT > 0.0);

            COLORREF bc = isSlow ? COL_SLOW
                : isFast ? COL_FAST
                : COL_NORMAL;

            CBrush bb(bc);
            dc.FillRect(CRect(bx, Y(by), bx + barW, Y(cbottom)), &bb);
        }

        // X percent labels
        dc.SelectObject(&fSmall);
        dc.SetTextColor(RGB(150, 150, 150));
        const int MIN_GAP = 40;
        int lastLX = -9999;
        int barW2 = max(2, cw / n - 1);
        for (int i = 0; i < n; ++i)
        {
            int bx = cleft + (int)((double)i / n * cw) + barW2 / 2;
            if (bx - lastLX < MIN_GAP) continue;

            CPen pt(PS_SOLID, 1, RGB(205, 205, 205));
            CPen* po = dc.SelectObject(&pt);
            dc.MoveTo(bx, Y(cbottom)); dc.LineTo(bx, Y(cbottom) + 4);
            dc.SelectObject(po);

            CString pl; pl.Format(_T("%d%%"), m_entries[i].percent);
            CSize   szP = dc.GetTextExtent(pl);
            dc.TextOut(bx - szP.cx / 2, Y(cbottom) + 5, pl);
            lastLX = bx;
        }
    }

    // Axes
    {
        CPen pa(PS_SOLID, 1, RGB(195, 195, 195));
        CPen* po = dc.SelectObject(&pa);
        dc.MoveTo(cleft, Y(ctop));
        dc.LineTo(cleft, Y(cbottom));
        dc.LineTo(cright, Y(cbottom));
        dc.SelectObject(po);
    }

    curY = cbottom + CHART_PAD_B + 10;

    // ── Scroll tracking ──────────────────────────────────────────
    int viewH = rc.Height() - BUTTONS_H;
    m_healthTotalH = max(0, curY - viewH);
    {
        SCROLLINFO si = {};
        si.cbSize = sizeof(si);
        si.fMask = SIF_ALL;
        si.nMin = 0;
        si.nMax = curY;
        si.nPage = viewH;
        si.nPos = m_healthScrollY;
        SetScrollInfo(SB_VERT, &si, FALSE);
    }

    paintDC.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}



BOOL CSOHResultDlg::OnEraseBkgnd(CDC* pDC)
{
    if (m_showHealth || m_showChart)
        return TRUE;   
    return CDialogEx::OnEraseBkgnd(pDC);
}