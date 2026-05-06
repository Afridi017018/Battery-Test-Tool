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

    m_list.Create(WS_CHILD | WS_VISIBLE | LVS_REPORT,
        CRect(10, 72, rc.Width() - 10, rc.Height() - 80),   
        this, 2001);

    m_list.InsertColumn(0, T(L"Time", L"時刻"), LVCFMT_LEFT, 100);
    m_list.InsertColumn(1, T(L"Percent", L"パーセント"), LVCFMT_CENTER, 80);
    m_list.InsertColumn(2, T(L"Duration(ms)", L"所要時間(ms)"), LVCFMT_CENTER, 120);
    m_list.InsertColumn(3, T(L"Duration(h:m:s)", L"所要時間(時:分:秒)"), LVCFMT_CENTER, 140);
    m_list.InsertColumn(4, T(L"Sum/10%(h:m:s)", L"合計/10%(時:分:秒)"), LVCFMT_CENTER, 140);
    m_list.InsertColumn(5, T(L"Elapsed(h:m:s)", L"経過時間(時:分:秒)"), LVCFMT_CENTER, 140);

    // Summary Label
    m_lblSummary.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_CENTER,
        CRect(10, rc.Height() - 60, rc.Width() - 10, rc.Height() - 10),
        this);
    m_lblSummary.SetFont(GetFont());

    LoadLogFile();
    DisplayData();

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

//void CSOHResultDlg::LoadLogFile()
//{
//    CString folder = GetExeFolder();
//    CString fullPath = folder + _T("\\soh_log.txt");
//
//    std::wifstream file(fullPath);
//    if (!file.is_open())
//        return;
//
//    std::vector<std::wstring> allLines;
//    std::wstring line;
//
//    while (std::getline(file, line))
//        allLines.push_back(line);
//
//    file.close();
//
//    std::string lastTestID;
//    std::string lastStartTime;
//    std::string currentStartTime;
//
//    for (size_t i = 0; i < allLines.size(); ++i)
//    {
//        std::string s(allLines[i].begin(), allLines[i].end());
//
//        if (s.find("TEST_ID:") != std::string::npos)
//        {
//            size_t pos = s.find("TEST_ID:");
//            lastTestID = s.substr(pos + 8);
//            lastTestID.erase(0, lastTestID.find_first_not_of(" \t"));
//            lastTestID.erase(lastTestID.find_last_not_of(" \t") + 1);
//
//            if (i + 1 < allLines.size())
//            {
//                std::string next(allLines[i + 1].begin(), allLines[i + 1].end());
//                if (next.find("Start Time:") != std::string::npos)
//                {
//                    size_t tpos = next.find("Start Time:");
//                    lastStartTime = next.substr(tpos + 11);
//                    lastStartTime.erase(0, lastStartTime.find_first_not_of(" \t"));
//                    lastStartTime.erase(lastStartTime.find_last_not_of(" \t") + 1);
//                    currentStartTime = lastStartTime;
//                }
//            }
//        }
//
//        if (s.find("Percent:") != std::string::npos)
//        {
//            ParseLine(s);
//
//            size_t idStart = s.find("[");
//            size_t idEnd = s.find("]");
//            std::string testID = s.substr(idStart + 1, idEnd - idStart - 1);
//
//            SOHFullEntry full;
//            full.testID = CString(testID.c_str());
//            full.startTime = CString(currentStartTime.c_str());
//
//            auto& e = m_entries.back();
//            full.time = e.time;
//            full.percent = e.percent;
//            full.durationMs = e.durationMs;
//            full.durationHMS = e.durationHMS;
//
//            m_allEntries.push_back(full);
//        }
//    }
//
//    m_testIDStr = CString(lastTestID.c_str());
//    m_startTimeStr = CString(lastStartTime.c_str());
//
//    std::string matchTag = "[" + lastTestID + "]";
//
//    m_entries.clear();
//    m_totalTimeMs = 0;
//
//    for (const auto& wline : allLines)
//    {
//        std::string s(wline.begin(), wline.end());
//        if (s.find(matchTag) != std::string::npos &&
//            s.find("Percent:") != std::string::npos)
//        {
//            ParseLine(s);
//        }
//    }
//}

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

    m_totalTimeMs += entry.durationMs;
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
    MessageBox(debug, _T("Debug"), MB_OK);

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




    //// ── Health Score ─────────────────────────────────────────────
    //m_healthScore = 0.0;

    //if (n >= 2)
    //{
    //    // Total time and percent range
    //    double totalMs = 0.0;
    //    for (int i = 0; i < n; ++i)
    //        totalMs += (double)m_entries[i].durationMs;

    //    int firstPct2 = m_entries[0].percent;
    //    int lastPct2 = m_entries[n - 1].percent;
    //    int pctRange = firstPct2 - lastPct2;   // e.g. 99 - 0 = 99

    //    if (pctRange > 0 && totalMs > 0)
    //    {
    //        // Perfect linear rate: how many ms per 1% drop
    //        double msPerPct = totalMs / (double)pctRange;

    //        // At each step, where SHOULD we be on the perfect line?
    //        // Walk cumulative time and compare actual percent vs ideal percent
    //        double cumMs = 0.0;
    //        double sumDiff = 0.0;

    //        for (int i = 0; i < n; ++i)
    //        {
    //            cumMs += (double)m_entries[i].durationMs;

    //            // Ideal percent at this cumulative time
    //            double idealPct = (double)firstPct2 - (cumMs / msPerPct);

    //            // Actual percent
    //            double actualPct = (double)m_entries[i].percent;

    //            // Absolute difference
    //            double diff = fabs(actualPct - idealPct);
    //            sumDiff += diff;
    //        }

    //        double avgDiff = sumDiff / (double)n;

    //        // Normalize: avgDiff as % of total range
    //        // avgDiff=0 → score=100%, avgDiff=pctRange → score=0%
    //        double normalized = avgDiff / (double)pctRange;
    //        m_healthScore = max(0.0, (1.0 - normalized) * 100.0);

    //        //// Add right before health score calculation
    //        //CString debug;
    //        //debug.Format(_T("n=%d firstPct=%d lastPct=%d totalMs=%.0f"),
    //        //    n,
    //        //    m_entries[0].percent,
    //        //    m_entries[n - 1].percent,
    //        //    totalMs);
    //        //MessageBox(debug, _T("Debug"), MB_OK);
    //    }
    //}


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
        m_list.InsertColumn(4, T(L"Sum/10%(h:m:s)", L"合計/10%(時:分:秒)"), LVCFMT_CENTER, 140);
        m_list.InsertColumn(5, T(L"Elapsed(h:m:s)", L"経過時間(時:分:秒)"), LVCFMT_CENTER, 140);

        DisplayData();

        CString text;
        m_lblSummary.GetWindowText(text);
        SetSummary(text);
    }
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

    const int LEFT = 80;
    const int RIGHT = rc.Width() - 40;
    const int TOP = 95;
    const int BOTTOM = rc.Height() - 120;

    int chartW = RIGHT - LEFT;
    int chartH = BOTTOM - TOP;

    dc.FillSolidRect(&rc, RGB(30, 30, 40));
    CRect chartRect(LEFT, TOP, RIGHT, BOTTOM);
    dc.FillSolidRect(&chartRect, RGB(20, 20, 30));

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

    CFont fontSmall;
    fontSmall.CreatePointFont(70, _T("Segoe UI"));
    CFont* pOldFont = dc.SelectObject(&fontSmall);
    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(RGB(140, 140, 165));

    CPen penGrid(PS_DOT, 1, RGB(55, 55, 75));
    CPen* pOldPen = dc.SelectObject(&penGrid);

    for (int pct = 0; pct <= 100; pct += 10)
    {
        int y = BOTTOM - (int)((double)pct / 100.0 * chartH);
        dc.MoveTo(LEFT, y);
        dc.LineTo(RIGHT, y);

        CString lbl;
        lbl.Format(_T("%d%%"), pct);
        dc.TextOut(LEFT - 42, y - 7, lbl);
    }
    dc.SelectObject(pOldPen);

    CPen penAxis(PS_SOLID, 2, RGB(170, 170, 195));
    pOldPen = dc.SelectObject(&penAxis);
    dc.MoveTo(LEFT, TOP);
    dc.LineTo(LEFT, BOTTOM);
    dc.LineTo(RIGHT, BOTTOM);
    dc.SelectObject(pOldPen);

    if (n >= 2)
    {
        std::vector<POINT> poly;
        poly.reserve(n + 2);
        poly.push_back({ LEFT, BOTTOM });
        for (int i = 0; i < n; ++i)
            poly.push_back({ px[i], py[i] });
        poly.push_back({ px[n - 1], BOTTOM });

        for (int band = 0; band < chartH; ++band)
        {
            double t = (double)band / chartH;
            int    alpha = (int)(80.0 * (1.0 - t));
            if (alpha <= 0) continue;

            int y = TOP + band;

            int  xLeft = LEFT, xRight = LEFT;
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

            CPen penBand(PS_SOLID, 1, RGB(
                (BYTE)(0 + (int)(30 * t)),
                (BYTE)(100 + (int)(20 * t)),
                (BYTE)(180 - (int)(60 * t))
            ));
            CPen* pOld = dc.SelectObject(&penBand);
            dc.MoveTo(xLeft, y);
            dc.LineTo(xRight, y);
            dc.SelectObject(pOld);
        }
    }

    {
        CPen penLine(PS_SOLID, 2, RGB(0, 185, 255));
        pOldPen = dc.SelectObject(&penLine);
        for (int i = 0; i < n; ++i)
        {
            if (i == 0) dc.MoveTo(px[i], py[i]);
            else        dc.LineTo(px[i], py[i]);
        }
        dc.SelectObject(pOldPen);
    }

    for (int i = 0; i < n; ++i)
    {
        bool isSlow = (i < (int)m_anomaly.size() && m_anomaly[i]);
        bool isFast = (i < (int)m_anomalyFast.size() && m_anomalyFast[i]);

        COLORREF dotColor, rimColor;

        if (isSlow) { dotColor = RGB(220, 50, 50);  rimColor = RGB(255, 100, 100); }
        else if (isFast) { dotColor = RGB(220, 130, 0);  rimColor = RGB(255, 180, 60); }
        else { dotColor = RGB(0, 210, 255);  rimColor = RGB(180, 240, 255); }

        CBrush brushDot(dotColor);
        CBrush* pOldBrush = dc.SelectObject(&brushDot);

        CPen penDot(PS_SOLID, 1, rimColor);
        CPen* pOldPenDot = dc.SelectObject(&penDot);

        int r = (isSlow || isFast) ? 6 : 4;
        dc.Ellipse(px[i] - r, py[i] - r, px[i] + r, py[i] + r);

        dc.SelectObject(pOldPenDot);
        dc.SelectObject(pOldBrush);
    }

    {
        const int MIN_GAP = 50;
        const int ROW_A = BOTTOM + 20;
        const int ROW_B = BOTTOM + 38;

        CPen penTick(PS_SOLID, 1, RGB(140, 140, 165));
        pOldPen = dc.SelectObject(&penTick);
        for (int i = 0; i < n; ++i)
        {
            dc.MoveTo(px[i], BOTTOM);
            dc.LineTo(px[i], BOTTOM + 5);
        }
        dc.SelectObject(pOldPen);

        dc.SetTextColor(RGB(190, 190, 215));

        CFont fontXLabel;
        fontXLabel.CreatePointFont(70, _T("Segoe UI"));
        dc.SelectObject(&fontXLabel);

        int lastDrawnX = -9999;
        int staggerIdx = 0;

        for (int i = 0; i < n; ++i)
        {
            if (px[i] - lastDrawnX < MIN_GAP)
                continue;

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
                CPen penConn(PS_DOT, 1, RGB(70, 70, 90));
                CPen* pOldC = dc.SelectObject(&penConn);
                dc.MoveTo(px[i], BOTTOM + 5);
                dc.LineTo(px[i], ROW_B - 3);
                dc.SelectObject(pOldC);
            }

            CSize sz = dc.GetTextExtent(lbl);
            dc.TextOut(px[i] - sz.cx / 2, row, lbl);

            lastDrawnX = px[i];
            ++staggerIdx;
        }

        dc.SelectObject(fontSmall);
    }

    // ── Y-axis vertical label ─────────────────────
    CFont fontVert;
    LOGFONT lf = {};
    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
    lf.lfHeight = -14;
    lf.lfEscapement = 900;
    lf.lfOrientation = 900;
    fontVert.CreateFontIndirect(&lf);
    dc.SelectObject(&fontVert);
    dc.SetTextColor(RGB(140, 140, 165));
    dc.TextOut(15, BOTTOM - chartH / 2 + 40,
        T(L"Percent (%)", L"割合 (%)"));

    // ── X-axis title ─────────────────────────────
    CFont fontLabel;
    fontLabel.CreatePointFont(85, _T("Segoe UI"));
    dc.SelectObject(&fontLabel);
    dc.SetTextColor(RGB(140, 140, 165));
    dc.TextOut(LEFT + chartW / 2 - 60, BOTTOM + 60,
        T(L"Elapsed Time (seconds)", L"経過時間 (秒)"));

    // ── Chart title ──────────────────────────────
    CFont fontTitle;
    fontTitle.CreatePointFont(100, _T("Segoe UI"));
    dc.SelectObject(&fontTitle);
    dc.SetTextColor(RGB(210, 210, 240));

    CString title;
    title.Format(
        T(L"SOH \x2014 Percent vs Elapsed Time  [%s]",
            L"SOH \x2014 割合 vs 経過時間  [%s]"),
        m_testIDStr);
    CSize sz = dc.GetTextExtent(title);
    dc.TextOut(LEFT + (chartW - sz.cx) / 2, TOP - 55, title);

    // ── Legend ───────────────────────────────────
    dc.SelectObject(&fontSmall);
    dc.SetTextColor(RGB(170, 170, 200));

    int ly = TOP - 10;

    const int blockW = 10;
    const int gap = 4;
    const int spacing = 20;

    CString strSOH = T(L"SOH %", L"SOH %");
    CString strSlow = T(L"Too Slow", L"遅すぎる");
    CString strFast = T(L"Too Fast", L"速すぎる");

    CSize szSOH = dc.GetTextExtent(strSOH);
    CSize szSlow = dc.GetTextExtent(strSlow);
    CSize szFast = dc.GetTextExtent(strFast);

    int totalLegendW = (blockW + gap + szSOH.cx)
        + spacing
        + (blockW + gap + szSlow.cx)
        + spacing
        + (blockW + gap + szFast.cx);

    int lx = LEFT + (chartW - totalLegendW) / 2;

    CBrush bCyan(RGB(0, 185, 255));
    dc.FillRect(CRect(lx, ly + 1, lx + blockW, ly + blockW + 1), &bCyan);
    dc.TextOut(lx + blockW + gap, ly, strSOH);
    lx += blockW + gap + szSOH.cx + spacing;

    CBrush bRed(RGB(220, 50, 50));
    dc.FillRect(CRect(lx, ly + 1, lx + blockW, ly + blockW + 1), &bRed);
    dc.TextOut(lx + blockW + gap, ly, strSlow);
    lx += blockW + gap + szSlow.cx + spacing;

    CBrush bOrange(RGB(220, 130, 0));
    dc.FillRect(CRect(lx, ly + 1, lx + blockW, ly + blockW + 1), &bOrange);
    dc.TextOut(lx + blockW + gap, ly, strFast);

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

// ============================================================
//  REPLACE the existing DrawHealthScore() in CSOHResultDlg.cpp
//  Everything else (logic, calculations) is UNCHANGED.
// ============================================================

// ============================================================
//  REPLACE DrawHealthScore() in CSOHResultDlg.cpp
//  Fixes:
//    1. All content starts below the button bar (y=55)
//    2. Left panel (scores) on top, chart below — vertical stack
//    3. Scroll support via WM_MOUSEWHEEL + manual offset
//    4. No text hidden behind buttons
// ============================================================

// ── Add these private members to CSOHResultDlg.h ────────────
//   int m_healthScrollY;   // current scroll offset (pixels, >= 0)
//   int m_healthTotalH;    // total content height needed
//
// ── Add to constructor init list ────────────────────────────
//   , m_healthScrollY(0)
//   , m_healthTotalH(0)
//
// ── Add to message map ───────────────────────────────────────
//   ON_WM_MOUSEWHEEL()
//   ON_WM_VSCROLL()
//
// ── Add handler declarations to header ───────────────────────
//   afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
//   afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pBar);
// ============================================================


// ── Paste these two handlers anywhere in the .cpp ────────────

BOOL CSOHResultDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    if (m_showHealth)
    {
        int step = 40;
        m_healthScrollY -= (zDelta > 0 ? step : -step);
        m_healthScrollY = max(0, min(m_healthScrollY, m_healthTotalH));
        UpdateHealthScrollBar();
        Invalidate();
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
        Invalidate();
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

// ── Call this in OnShowHealth() when m_showHealth becomes TRUE ──
//   m_healthScrollY = 0;
//   ShowScrollBar(SB_VERT, TRUE);
//   UpdateHealthScrollBar();
//
// ── And when m_showHealth becomes FALSE ─────────────────────────
//   ShowScrollBar(SB_VERT, FALSE);


// ============================================================
//  THE MAIN DrawHealthScore() FUNCTION
// ============================================================

void CSOHResultDlg::DrawHealthScore()
{
    CPaintDC dc(this);

    CRect rc;
    GetClientRect(&rc);

    // ── Background ───────────────────────────────────────────────
    dc.FillSolidRect(&rc, RGB(20, 20, 30));

    // ── Button bar safe zone ─────────────────────────────────────
    // Buttons occupy y=0..44. We paint a separator and start below.
    dc.FillSolidRect(CRect(0, 0, rc.Width(), 50), RGB(30, 30, 42));
    CPen penSep(PS_SOLID, 1, RGB(55, 55, 75));
    CPen* pOldPen = dc.SelectObject(&penSep);
    dc.MoveTo(0, 49); dc.LineTo(rc.Width(), 49);
    dc.SelectObject(pOldPen);

    const int BUTTONS_H = 50;           // first pixel we can draw into
    const int SCROLL_Y = m_healthScrollY;
    const int CONTENT_X0 = 18;
    const int CONTENT_X1 = rc.Width() - 20;
    const int CONTENT_W = CONTENT_X1 - CONTENT_X0;

    // All content drawn with this origin; subtract SCROLL_Y for position
    // Helper: translate a content-space y to screen y
    auto Y = [&](int contentY) -> int {
        return BUTTONS_H + contentY - SCROLL_Y;
        };

    int n = (int)m_entries.size();
    double score = m_healthScore;

    // ── Fonts ────────────────────────────────────────────────────
    CFont fontTiny, fontSmall, fontSub, fontMed, fontBig;
    fontTiny.CreatePointFont(72, _T("Segoe UI"));
    fontSmall.CreatePointFont(82, _T("Segoe UI"));
    fontSub.CreatePointFont(92, _T("Segoe UI"));
    fontMed.CreatePointFont(105, _T("Segoe UI"));
    fontBig.CreatePointFont(240, _T("Segoe UI"));
    dc.SetBkMode(TRANSPARENT);

    // ── Score colour ─────────────────────────────────────────────
    COLORREF scoreColor;
    CString  scoreLabel;
    if (score >= 80.0) { scoreColor = RGB(50, 220, 100); scoreLabel = T(L"Healthy", L"健全"); }
    else if (score >= 60.0) { scoreColor = RGB(220, 190, 0);  scoreLabel = T(L"Moderate", L"普通"); }
    else { scoreColor = RGB(220, 60, 60);  scoreLabel = T(L"Degraded", L"劣化"); }

    // ════════════════════════════════════════════════════════════
    //  SECTION 1 — TITLE  (content y = 0)
    // ════════════════════════════════════════════════════════════
    int curY = 12;  // content-space cursor

    dc.SelectObject(&fontMed);
    dc.SetTextColor(RGB(200, 200, 230));
    CString title;
    title.Format(
        T(L"Battery Discharge Health Score  [%s]",
            L"放電曲線健全スコア  [%s]"),
        m_testIDStr);
    CSize szTitle = dc.GetTextExtent(title);
    dc.TextOut(CONTENT_X0 + (CONTENT_W - szTitle.cx) / 2, Y(curY), title);
    curY += szTitle.cy + 10;

    // thin rule
    {
        CPen penRule(PS_SOLID, 1, RGB(50, 50, 70));
        pOldPen = dc.SelectObject(&penRule);
        dc.MoveTo(CONTENT_X0, Y(curY)); dc.LineTo(CONTENT_X1, Y(curY));
        dc.SelectObject(pOldPen);
    }
    curY += 14;

    // ════════════════════════════════════════════════════════════
    //  SECTION 2 — CIRCLE + STATUS  (horizontal: circle left, blurb right)
    // ════════════════════════════════════════════════════════════
    const int CIRCLE_R = 72;
    const int CIRCLE_CX = CONTENT_X0 + CIRCLE_R + 10;
    const int CIRCLE_CY_content = curY + CIRCLE_R;

    // Shadow
    {
        CBrush bShadow(RGB(10, 10, 18));
        CPen   pNull(PS_NULL, 0, RGB(0, 0, 0));
        dc.SelectObject(&bShadow);
        dc.SelectObject(&pNull);
        dc.Ellipse(CIRCLE_CX - CIRCLE_R + 4, Y(CIRCLE_CY_content - CIRCLE_R + 4),
            CIRCLE_CX + CIRCLE_R + 4, Y(CIRCLE_CY_content + CIRCLE_R + 4));
    }
    // Ring + fill
    {
        CPen   pRing(PS_SOLID, 6, scoreColor);
        CBrush bFill(RGB(25, 25, 38));
        dc.SelectObject(&pRing);
        dc.SelectObject(&bFill);
        dc.Ellipse(CIRCLE_CX - CIRCLE_R, Y(CIRCLE_CY_content - CIRCLE_R),
            CIRCLE_CX + CIRCLE_R, Y(CIRCLE_CY_content + CIRCLE_R));
    }
    // Big % inside circle
    dc.SelectObject(&fontBig);
    dc.SetTextColor(scoreColor);
    {
        CString strPct; strPct.Format(_T("%.0f"), score);
        CSize sz = dc.GetTextExtent(strPct);
        dc.TextOut(CIRCLE_CX - sz.cx / 2, Y(CIRCLE_CY_content - sz.cy / 2 - 6), strPct);
    }
    // "%" small superscript
    dc.SelectObject(&fontSub);
    dc.SetTextColor(scoreColor);
    dc.TextOut(CIRCLE_CX + CIRCLE_R - 20, Y(CIRCLE_CY_content - CIRCLE_R + 10), _T("%"));

    // Status label below circle
    dc.SelectObject(&fontMed);
    dc.SetTextColor(scoreColor);
    {
        CSize sz = dc.GetTextExtent(scoreLabel);
        dc.TextOut(CIRCLE_CX - sz.cx / 2, Y(CIRCLE_CY_content + CIRCLE_R + 6), scoreLabel);
    }

    // ── Right of circle: plain-English what-does-this-mean ───────
    int blurbX = CIRCLE_CX + CIRCLE_R + 24;
    int blurbW = CONTENT_X1 - blurbX;
    int blurbTopY = CIRCLE_CY_content - CIRCLE_R;

    dc.SelectObject(&fontSub);
    dc.SetTextColor(RGB(185, 185, 215));

    CString whatIsIt = T(
        L"What is the Health Score?",
        L"健全スコアとは？");
    dc.TextOut(blurbX, Y(blurbTopY), whatIsIt);

    dc.SelectObject(&fontSmall);
    dc.SetTextColor(RGB(120, 120, 160));

    // Multi-line explanation — manually split to fit blurbW
    struct Line { CString text; };
    std::vector<CString> explainLines;
 
    if (eng_lang) {
        explainLines.push_back(L"Measures how stable and predictable your battery's");
        explainLines.push_back(L"discharge curve is. A healthy battery drains each 1%");
        explainLines.push_back(L"in roughly the same time \u2014 no sudden slow or fast steps.");
        explainLines.push_back(L"");
        explainLines.push_back(L"80\u201399%  \u2192  Consistent discharge (good battery)");
        explainLines.push_back(L"60\u201379%   \u2192  Some variation (monitor it)");
        explainLines.push_back(L"0\u201359%    \u2192  Irregular discharge (degraded)");
    }
    else {
        explainLines.push_back(L"\u30d0\u30c3\u30c6\u30ea\u30fc\u306e\u653e\u96fb\u66f2\u7dda\u304c\u3069\u308c\u3060\u3051\u5b89\u5b9a\u30fb\u4e88\u6e2c\u53ef\u80fd\u304b\u3092");
        explainLines.push_back(L"\u6e2c\u5b9a\u3057\u307e\u3059\u3002\u5065\u5168\u306a\u30d0\u30c3\u30c6\u30ea\u30fc\u306f\u54041%\u3092\u307b\u307c\u540c\u3058\u6642\u9593\u3067");
        explainLines.push_back(L"\u6d88\u8cbb\u3057\u3001\u7a81\u7136\u306e\u9045\u5ef6\u3084\u6025\u901f\u6d88\u8017\u304c\u3042\u308a\u307e\u305b\u3093\u3002");
        explainLines.push_back(L"");
        explainLines.push_back(L"80\uff5e100%  \u2192  \u4e00\u8cab\u3057\u305f\u653e\u96fb\uff08\u826f\u597d\uff09");
        explainLines.push_back(L"60\uff5e79%   \u2192  \u82e5\u5e72\u306e\u3070\u3089\u3064\u304d\uff08\u8981\u76e3\u8996\uff09");
        explainLines.push_back(L"0\uff5e59%    \u2192  \u4e0d\u898f\u5247\u306a\u653e\u96fb\uff08\u52a3\u5316\uff09");
    }

    int lineH = 16;
    int ly = blurbTopY + 22;
    for (const auto& ln : explainLines)
    {
        if (!ln.IsEmpty())
            dc.TextOut(blurbX, Y(ly), ln);
        ly += lineH;
    }

    // advance curY past the circle block
    curY = CIRCLE_CY_content + CIRCLE_R + 30;

    //// ════════════════════════════════════════════════════════════
    ////  SECTION 3 — SUB-SCORE CARDS  (3 horizontal cards)
    //// ════════════════════════════════════════════════════════════
    //{
    //    CPen penRule(PS_SOLID, 1, RGB(50, 50, 70));
    //    pOldPen = dc.SelectObject(&penRule);
    //    dc.MoveTo(CONTENT_X0, Y(curY)); dc.LineTo(CONTENT_X1, Y(curY));
    //    dc.SelectObject(pOldPen);
    //}
    //curY += 10;

    //dc.SelectObject(&fontSub);
    //dc.SetTextColor(RGB(160, 160, 200));
    //CString secLabel = T(L"Score Breakdown", L"スコア内訳");
    //dc.TextOut(CONTENT_X0, Y(curY), secLabel);
    //curY += 22;

    //auto CardColor = [](double v) -> COLORREF {
    //    if (v >= 80.0) return RGB(50, 220, 100);
    //    if (v >= 60.0) return RGB(220, 190, 0);
    //    return RGB(220, 60, 60);
    //    };

    //struct SubCard {
    //    CString label;
    //    CString weight;
    //    CString meaning;
    //    CString detail;
    //    double  value;
    //};

    //SubCard cards[3] = {
    //    {
    //        T(L"Consistency", L"一貫性"),
    //        T(L"Weight: 50%", L"重み: 50%"),
    //        T(L"Are all 1% steps taking similar time?",
    //          L"各1%ステップの所要時間は均一ですか？"),
    //        T(L"Measures the coefficient of variation (CV) of step durations.\n"
    //          L"CV near 0 = all steps equal. High CV = chaotic discharge.",
    //          L"ステップ時間の変動係数(CV)を測定。CV≈0で均一、高いほど不規則。"),
    //        m_consistencyScore
    //    },
    //    {
    //        T(L"Anomaly-Free", L"異常なし"),
    //        T(L"Weight: 30%", L"重み: 30%"),
    //        T(L"What % of steps are NOT outliers?",
    //          L"外れ値でないステップの割合は？"),
    //        T(L"Steps >1.5\x03c3 above or below the mean are flagged.\n"
    //          L"100% = no outliers. Lower = more anomalous steps.",
    //          L"平均±1.5\x03c3 を超えるステップを外れ値とし検出。100%=外れ値なし。"),
    //        m_anomalyScore
    //    },
    //    {
    //        T(L"Smoothness", L"滑らかさ"),
    //        T(L"Weight: 20%", L"重み: 20%"),
    //        T(L"Do consecutive steps change gradually?",
    //          L"連続ステップは徐々に変化していますか？"),
    //        T(L"Measures avg ratio of change between neighbours.\n"
    //          L"Sudden jumps lower this score.",
    //          L"隣接ステップ間の変化率の平均を測定。急激な変化でスコア低下。"),
    //        m_consistencyScore  // placeholder — each card uses its own value below
    //    }
    //};
    //// Fix the smoothness value (the struct literal above has a placeholder)
    //cards[2].value = m_smoothnessScore;

    //const int CARD_GAP = 10;
    //const int NUM_CARDS = 3;
    //int cardW = (CONTENT_W - CARD_GAP * (NUM_CARDS - 1)) / NUM_CARDS;
    //const int CARD_H = 92;

    //for (int c = 0; c < NUM_CARDS; ++c)
    //{
    //    const SubCard& card = cards[c];
    //    COLORREF cc = CardColor(card.value);

    //    int cx0 = CONTENT_X0 + c * (cardW + CARD_GAP);
    //    int cx1 = cx0 + cardW;
    //    int cy0 = curY;
    //    int cy1 = curY + CARD_H;

    //    // Card background
    //    dc.FillSolidRect(CRect(cx0, Y(cy0), cx1, Y(cy1)), RGB(28, 28, 44));

    //    // Top colour bar
    //    dc.FillSolidRect(CRect(cx0, Y(cy0), cx1, Y(cy0 + 4)), cc);

    //    // Score (large, right-aligned)
    //    dc.SelectObject(&fontMed);
    //    dc.SetTextColor(cc);
    //    CString scoreStr; scoreStr.Format(_T("%.0f%%"), card.value);
    //    CSize szSc = dc.GetTextExtent(scoreStr);
    //    dc.TextOut(cx1 - szSc.cx - 8, Y(cy0 + 6), scoreStr);

    //    // Label
    //    dc.SelectObject(&fontSmall);
    //    dc.SetTextColor(RGB(210, 210, 235));
    //    dc.TextOut(cx0 + 8, Y(cy0 + 6), card.label);

    //    // Weight tag
    //    dc.SelectObject(&fontTiny);
    //    dc.SetTextColor(RGB(110, 110, 150));
    //    dc.TextOut(cx0 + 8, Y(cy0 + 24), card.weight);

    //    // Meaning
    //    dc.SelectObject(&fontTiny);
    //    dc.SetTextColor(RGB(150, 150, 185));
    //    dc.TextOut(cx0 + 8, Y(cy0 + 42), card.meaning);

    //    // Detail (second line, dimmer)
    //    dc.SetTextColor(RGB(90, 90, 125));
    //    // split detail at \n if present
    //    CString det = card.detail;
    //    int nl = det.Find(L'\n');
    //    if (nl >= 0)
    //    {
    //        dc.TextOut(cx0 + 8, Y(cy0 + 58), det.Left(nl));
    //        dc.TextOut(cx0 + 8, Y(cy0 + 72), det.Mid(nl + 1));
    //    }
    //    else
    //    {
    //        dc.TextOut(cx0 + 8, Y(cy0 + 58), det);
    //    }
    //}
    //curY += CARD_H + 8;

    //// ── Formula ───────────────────────────────────────────────────
    //dc.SelectObject(&fontTiny);
    //dc.SetTextColor(RGB(75, 75, 110));
    //CString formula = T(
    //    L"Overall = Consistency\xd7""0.5  +  Anomaly-Free\xd7""0.3  +  Smoothness\xd7""0.2",
    //    L"総合 = 一貫性\xd7""0.5  +  異常なし\xd7""0.3  +  滑らかさ\xd7""0.2");
    //CSize szF = dc.GetTextExtent(formula);
    //dc.TextOut(CONTENT_X0 + (CONTENT_W - szF.cx) / 2, Y(curY), formula);
    //curY += 22;

// ════════════════════════════════════════════════════════════
    //  SECTION 3 — SUB-SCORE CARDS  (3 horizontal cards)
    //  REPLACE this entire section in DrawHealthScore()
    // ════════════════════════════════════════════════════════════
    {
        CPen penRule(PS_SOLID, 1, RGB(50, 50, 70));
        pOldPen = dc.SelectObject(&penRule);
        dc.MoveTo(CONTENT_X0, Y(curY)); dc.LineTo(CONTENT_X1, Y(curY));
        dc.SelectObject(pOldPen);
    }
    curY += 10;

    dc.SelectObject(&fontSub);
    dc.SetTextColor(RGB(160, 160, 200));
    CString secLabel = T(L"Score Breakdown", L"\u30b9\u30b3\u30a2\u5185\u8a33");
    dc.TextOut(CONTENT_X0, Y(curY), secLabel);
    curY += 22;

    auto CardColor = [](double v) -> COLORREF {
        if (v >= 80.0) return RGB(50, 220, 100);
        if (v >= 60.0) return RGB(220, 190, 0);
        return RGB(220, 60, 60);
        };

    struct SubCard {
        CString label;
        CString weight;
        CString meaning;
        CString detail1;   // split into two separate lines instead of \n
        CString detail2;
        double  value;
    };

    SubCard cards[3] = {
        {
            T(L"Consistency",   L"\u4e00\u8cab\u6027"),
            T(L"Weight: 50%",   L"\u91cd\u307f: 50%"),
            T(L"Are all 1% steps taking similar time?",
              L"\u404b1%\u30b9\u30c6\u30c3\u30d7\u306e\u6642\u9593\u306f\u5747\u4e00\uff1f"),
            T(L"CV of step durations (stddev/mean).",
              L"\u30b9\u30c6\u30c3\u30d7\u6642\u9593\u306eCV\u3092\u6e2c\u5b9a\u3002"),
            T(L"CV near 0 = uniform.  High CV = chaotic.",
              L"CV\u22480\u3067\u5747\u4e00\u3001\u9ad8\u3044\u3068\u4e0d\u898f\u5247\u3002"),
            m_consistencyScore
        },
        {
            T(L"Anomaly-Free",  L"\u7570\u5e38\u306a\u3057"),
            T(L"Weight: 30%",   L"\u91cd\u307f: 30%"),
            T(L"What % of steps are NOT outliers?",
              L"\u5916\u308c\u5024\u3067\u306a\u3044\u30b9\u30c6\u30c3\u30d7\u306e\u5272\u5408\uff1f"),
            T(L"Steps beyond mean \xb1 1.5\u03c3 are flagged.",
              L"\u5e73\u5747\xb11.5\u03c3\u8d85\u3048\u308b\u30b9\u30c6\u30c3\u30d7\u3092\u691c\u51fa\u3002"),
            T(L"100% = no outliers found.",
              L"100%=\u5916\u308c\u5024\u306a\u3057\u3002"),
            m_anomalyScore
        },
        {
            T(L"Smoothness",    L"\u6ed1\u3089\u304b\u3055"),
            T(L"Weight: 20%",   L"\u91cd\u307f: 20%"),
            T(L"Do consecutive steps change gradually?",
              L"\u9023\u7d9a\u30b9\u30c6\u30c3\u30d7\u306f\u5f90\u3005\u306b\u5909\u5316\uff1f"),
            T(L"Avg change ratio between neighbours.",
              L"\u96a3\u63a5\u30b9\u30c6\u30c3\u30d7\u9593\u306e\u5909\u5316\u7387\u5e73\u5747\u3002"),
            T(L"Sudden jumps lower this score.",
              L"\u6025\u6fc0\u306a\u5909\u5316\u3067\u30b9\u30b3\u30a2\u4f4e\u4e0b\u3002"),
            m_smoothnessScore
        }
    };

    const int CARD_GAP = 8;
    const int NUM_CARDS = 3;
    const int CARD_H = 145;   // taller to fit all text rows
    const int TOP_BAR = 5;     // colour accent bar height
    const int PAD = 8;     // inner horizontal padding

    int cardW = (CONTENT_W - CARD_GAP * (NUM_CARDS - 1)) / NUM_CARDS;

    for (int c = 0; c < NUM_CARDS; ++c)
    {
        const SubCard& card = cards[c];
        COLORREF cc = CardColor(card.value);

        int cx0 = CONTENT_X0 + c * (cardW + CARD_GAP);
        int cx1 = cx0 + cardW;
        int cy0 = curY;

        // ── Card background ───────────────────────────────────
        dc.FillSolidRect(CRect(cx0, Y(cy0), cx1, Y(cy0 + CARD_H)), RGB(28, 28, 44));

        // ── Top colour accent bar ─────────────────────────────
        dc.FillSolidRect(CRect(cx0, Y(cy0), cx1, Y(cy0 + TOP_BAR)), cc);

        // ── Row 1: label (left) + score (right) ───────────────
        int row1Y = cy0 + TOP_BAR + 5;
        dc.SelectObject(&fontSmall);
        dc.SetTextColor(RGB(210, 210, 235));
        dc.TextOut(cx0 + PAD, Y(row1Y), card.label);

        dc.SelectObject(&fontMed);
        dc.SetTextColor(cc);
        CString scoreStr; scoreStr.Format(_T("%.0f%%"), card.value);
        CSize szSc = dc.GetTextExtent(scoreStr);
        dc.TextOut(cx1 - szSc.cx - PAD, Y(row1Y), scoreStr);

        // ── Row 2: weight tag ─────────────────────────────────
        int row2Y = row1Y + 18;
        dc.SelectObject(&fontTiny);
        dc.SetTextColor(RGB(100, 100, 140));
        dc.TextOut(cx0 + PAD, Y(row2Y), card.weight);

        // ── Thin divider ──────────────────────────────────────
        int divY = row2Y + 16;
        {
            CPen penDiv(PS_SOLID, 1, RGB(45, 45, 65));
            pOldPen = dc.SelectObject(&penDiv);
            dc.MoveTo(cx0 + PAD, Y(divY));
            dc.LineTo(cx1 - PAD, Y(divY));
            dc.SelectObject(pOldPen);
        }

        // ── Row 3: meaning (question) ─────────────────────────
        int row3Y = divY + 5;
        dc.SelectObject(&fontTiny);
        dc.SetTextColor(RGB(150, 150, 190));

        // Use DrawText with clipping so text never overflows the card
        CRect rcMeaning(cx0 + PAD, Y(row3Y), cx1 - PAD, Y(row3Y + 40));
        dc.DrawText(card.meaning, &rcMeaning,
            DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);


        // ── Row 4 + 5: detail lines ───────────────────────────
        int row4Y = row3Y + 30;
        dc.SetTextColor(RGB(85, 85, 120));

        CRect rcD1(cx0 + PAD, Y(row4Y), cx1 - PAD, Y(row4Y + 28));
        dc.DrawText(card.detail1, &rcD1,
            DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);

        CRect rcD2(cx0 + PAD, Y(row4Y + 30), cx1 - PAD, Y(row4Y + 58));
        dc.DrawText(card.detail2, &rcD2,
            DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
    }
    curY += CARD_H + 8;

    // ── Formula ───────────────────────────────────────────────────
    dc.SelectObject(&fontTiny);
    dc.SetTextColor(RGB(75, 75, 110));
    CString formula = T(
        L"Overall = Consistency\xd7""0.5  +  Anomaly-Free\xd7""0.3  +  Smoothness\xd7""0.2",
        L"\u7dcf\u5408 = \u4e00\u8cab\u6027\xd7""0.5  +  \u7570\u5e38\u306a\u3057\xd7""0.3  +  \u6ed1\u3089\u304b\u3055\xd7""0.2");
    CSize szF = dc.GetTextExtent(formula);
    dc.TextOut(CONTENT_X0 + (CONTENT_W - szF.cx) / 2, Y(curY), formula);
    curY += 22;

    // ════════════════════════════════════════════════════════════
    //  SECTION 4 — STEP-DURATION BAR CHART  (below everything)
    // ════════════════════════════════════════════════════════════
    {
        CPen penRule(PS_SOLID, 1, RGB(50, 50, 70));
        pOldPen = dc.SelectObject(&penRule);
        dc.MoveTo(CONTENT_X0, Y(curY)); dc.LineTo(CONTENT_X1, Y(curY));
        dc.SelectObject(pOldPen);
    }
    curY += 10;

    // Section heading
    dc.SelectObject(&fontSub);
    dc.SetTextColor(RGB(160, 160, 200));
    dc.TextOut(CONTENT_X0, Y(curY),
        T(L"Step Duration Chart", L"ステップ所要時間チャート"));
    curY += 18;

    // One-liner explanation
    dc.SelectObject(&fontTiny);
    dc.SetTextColor(RGB(100, 100, 140));
    dc.TextOut(CONTENT_X0, Y(curY),
        T(L"Each bar = time taken for the battery to drop 1%.  "
            L"Taller bar = slower step.  Red = too slow.  Orange = too fast.  Yellow dashed = average.",
            L"各バー = バッテリーが1%低下するのに要した時間。"
            L"高いバー = 遅いステップ。赤=遅すぎ、橙=速すぎ、黄破線=平均。"));
    curY += 18;

    // Legend swatches
    auto DrawSwatch = [&](int lx, int ly_content, COLORREF col, const CString& txt, bool dashed = false) -> int
        {
            if (dashed)
            {
                CPen pd(PS_DASH, 1, col);
                pOldPen = dc.SelectObject(&pd);
                dc.MoveTo(lx, Y(ly_content + 5));
                dc.LineTo(lx + 18, Y(ly_content + 5));
                dc.SelectObject(pOldPen);
            }
            else
            {
                CRect sw(lx, Y(ly_content + 1), lx + 10, Y(ly_content + 10));
                CBrush sb(col);
                dc.FillRect(&sw, &sb);
            }
            dc.SetTextColor(RGB(130, 130, 165));
            dc.TextOut(lx + 22, Y(ly_content), txt);
            CSize sz = dc.GetTextExtent(txt);
            return lx + 22 + sz.cx + 16;
        };

    dc.SelectObject(&fontTiny);
    int lx = CONTENT_X0;
    lx = DrawSwatch(lx, curY, RGB(0, 185, 255), T(L"Normal", L"通常"));
    lx = DrawSwatch(lx, curY, RGB(220, 60, 60), T(L"Too Slow", L"遅すぎ"));
    lx = DrawSwatch(lx, curY, RGB(220, 130, 0), T(L"Too Fast", L"速すぎ"));
    DrawSwatch(lx, curY, RGB(255, 230, 80), T(L"Average", L"平均"), true);
    curY += 18;

    // ── Chart area ───────────────────────────────────────────────
    const int CHART_H = 180;
    const int CHART_PAD_L = 52;   // room for y-axis labels
    const int CHART_PAD_R = 8;
    const int CHART_PAD_B = 30;   // room for x-axis labels

    int cleft = CONTENT_X0 + CHART_PAD_L;
    int cright = CONTENT_X1 - CHART_PAD_R;
    int ctop = curY;
    int cbottom = curY + CHART_H;
    int cw = cright - cleft;
    int ch = CHART_H;

    dc.FillSolidRect(CRect(cleft, Y(ctop), cright, Y(cbottom)), RGB(15, 15, 25));

    if (n >= 2)
    {
        // ── Stats ──────────────────────────────────────────────
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
            if (m_entries[i].durationMs > maxD)
                maxD = m_entries[i].durationMs;

        // ── Y gridlines + labels ───────────────────────────────
        auto FmtMs = [](ULONGLONG ms) -> CString {
            ULONGLONG s = ms / 1000;
            if (s < 60) { CString r; r.Format(_T("%llus"), s); return r; }
            int m2 = (int)(s / 60), s2 = (int)(s % 60);
            CString r; r.Format(_T("%dm%ds"), m2, s2); return r;
            };

        dc.SelectObject(&fontTiny);
        for (int g = 0; g <= 4; ++g)
        {
            double frac = (double)g / 4.0;
            int    gy = ctop + (int)(frac * ch);
            ULONGLONG lMs = (ULONGLONG)((1.0 - frac) * (double)maxD);

            CPen pg(PS_DOT, 1, RGB(38, 38, 55));
            pOldPen = dc.SelectObject(&pg);
            dc.MoveTo(cleft, Y(gy)); dc.LineTo(cright, Y(gy));
            dc.SelectObject(pOldPen);

            CString lbl = FmtMs(lMs);
            CSize szL = dc.GetTextExtent(lbl);
            dc.SetTextColor(RGB(90, 90, 125));
            dc.TextOut(cleft - szL.cx - 4, Y(gy) - 7, lbl);
        }

        // ── Average dashed line ────────────────────────────────
        int avgY = ctop + (int)((1.0 - meanD / (double)maxD) * ch);
        {
            CPen pa(PS_DASH, 1, RGB(255, 230, 80));
            pOldPen = dc.SelectObject(&pa);
            dc.MoveTo(cleft, Y(avgY));
            dc.LineTo(cright, Y(avgY));
            dc.SelectObject(pOldPen);
        }
        dc.SelectObject(&fontTiny);
        dc.SetTextColor(RGB(210, 190, 50));
        dc.TextOut(cright + 2, Y(avgY) - 7, T(L"Avg", L"平均"));

        // ── Bars ──────────────────────────────────────────────
        int barW = max(2, cw / n - 1);

        for (int i = 0; i < n; ++i)
        {
            double dur = (double)m_entries[i].durationMs;
            int barH = (int)(dur / (double)maxD * ch);
            if (barH < 1) barH = 1;

            int bx = cleft + (int)((double)i / n * cw);
            int by = cbottom - barH;

            bool isSlow = (dur > upperT);
            bool isFast = (dur < lowerT && lowerT > 0.0);

            COLORREF barCol = isSlow ? RGB(220, 60, 60)
                : isFast ? RGB(220, 130, 0)
                : RGB(0, 185, 255);

            CRect barRc(bx, Y(by), bx + barW, Y(cbottom));
            CBrush bb(barCol);
            dc.FillRect(&barRc, &bb);

            // top highlight
            COLORREF topCol = RGB(min(255, GetRValue(barCol) + 40),
                min(255, GetGValue(barCol) + 40),
                min(255, GetBValue(barCol) + 40));
            CRect topRc(bx, Y(by), bx + barW, Y(by) + 2);
            CBrush bt(topCol);
            dc.FillRect(&topRc, &bt);
        }

        // ── X-axis percent labels ──────────────────────────────
        dc.SelectObject(&fontTiny);
        dc.SetTextColor(RGB(90, 90, 130));

        const int MIN_GAP = 36;
        int lastLX = -9999;

        for (int i = 0; i < n; ++i)
        {
            int bx = cleft + (int)((double)i / n * cw) + barW / 2;
            if (bx - lastLX < MIN_GAP) continue;

            CPen pt(PS_SOLID, 1, RGB(60, 60, 80));
            pOldPen = dc.SelectObject(&pt);
            dc.MoveTo(bx, Y(cbottom));
            dc.LineTo(bx, Y(cbottom) + 4);
            dc.SelectObject(pOldPen);

            CString pl; pl.Format(_T("%d%%"), m_entries[i].percent);
            CSize szP = dc.GetTextExtent(pl);
            dc.TextOut(bx - szP.cx / 2, Y(cbottom) + 5, pl);
            lastLX = bx;
        }

        // X-axis title
        dc.SetTextColor(RGB(85, 85, 125));
        CString xTitle = T(L"Battery % at each measurement",
            L"各測定時のバッテリー残量 (%)");
        CSize szXT = dc.GetTextExtent(xTitle);
        dc.TextOut(cleft + (cw - szXT.cx) / 2, Y(cbottom) + 18, xTitle);
    }

    // ── Axes lines ─────────────────────────────────────────────
    {
        CPen pAxis(PS_SOLID, 1, RGB(75, 75, 100));
        pOldPen = dc.SelectObject(&pAxis);
        dc.MoveTo(cleft, Y(ctop));
        dc.LineTo(cleft, Y(cbottom));
        dc.LineTo(cright, Y(cbottom));
        dc.SelectObject(pOldPen);
    }

    curY = cbottom + CHART_PAD_B + 10;

    // ════════════════════════════════════════════════════════════
    //  Track total content height for scroll
    // ════════════════════════════════════════════════════════════
    int viewH = rc.Height() - BUTTONS_H;
    m_healthTotalH = max(0, curY - viewH);
    // update scrollbar range silently
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
}
