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

CSOHResultDlg::CSOHResultDlg(CWnd* pParent)
    : CDialogEx(IDD_RESULT, pParent)
    , m_totalTimeMs(0)
    , m_showAll(false)
    , m_showChart(false)
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

END_MESSAGE_MAP()


HBRUSH CSOHResultDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

    if (pWnd->GetSafeHwnd() == m_lblLegend.GetSafeHwnd())
    {
        pDC->SetTextColor(RGB(180, 30, 30));
        pDC->SetBkMode(TRANSPARENT);
        return (HBRUSH)GetStockObject(NULL_BRUSH);
    }

    if (pWnd->GetSafeHwnd() == m_lblSummary.GetSafeHwnd())
    {
        pDC->SetTextColor(RGB(0, 0, 0));                        // black text
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
        _T("See All"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(10, 10, 120, 40),
        this,
        3001
    );

    // Chart Button
    m_btnChart.Create(
        _T("Show Chart"),
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        CRect(130, 10, 260, 40),
        this,
        3002
    );

    // ── Legend ───────────────────────────────────────────
    m_lblLegend.Create(
        _T("  ⚠  Red rows = Anomaly detected (step took too long)"),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        CRect(270, 16, rc.Width() - 10, 38),
        this
    );
    m_lblLegend.SetFont(GetFont());
    // ─────────────────────────────────────────────────────

    // List Control
    m_list.Create(WS_CHILD | WS_VISIBLE | LVS_REPORT,
        CRect(10, 50, rc.Width() - 10, rc.Height() - 80),
        this, 2001);

    m_list.InsertColumn(0, _T("Time"), LVCFMT_LEFT, 100);
    m_list.InsertColumn(1, _T("Percent"), LVCFMT_CENTER, 80);
    m_list.InsertColumn(2, _T("Duration(ms)"), LVCFMT_CENTER, 120);
    m_list.InsertColumn(3, _T("Duration(h:m:s)"), LVCFMT_CENTER, 140);
    m_list.InsertColumn(4, _T("Sum/10%(h:m:s)"), LVCFMT_CENTER, 140);
    m_list.InsertColumn(5, _T("Elapsed(h:m:s)"), LVCFMT_CENTER, 140);

    // Summary Label
    m_lblSummary.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_CENTER,
        CRect(10, rc.Height() - 60, rc.Width() - 10, rc.Height() - 10),
        this);

    m_lblSummary.SetFont(GetFont());

    LoadLogFile();
    DisplayData();

    return TRUE;
}

// ─────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────
//CString CSOHResultDlg::GetExeFolder()
//{
//    char path[MAX_PATH];
//    GetModuleFileNameA(NULL, path, MAX_PATH);
//
//    std::string exePath(path);
//    size_t pos = exePath.find_last_of("\\/");
//    std::string folder = exePath.substr(0, pos);
//
//    return CString(folder.c_str());
//}

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

        if (s.find("Percent:") != std::string::npos)
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

    // Filter latest test
    std::string matchTag = "[" + lastTestID + "]";

    m_entries.clear();
    m_totalTimeMs = 0;

    for (const auto& wline : allLines)
    {
        std::string s(wline.begin(), wline.end());
        if (s.find(matchTag) != std::string::npos &&
            s.find("Percent:") != std::string::npos)
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

    size_t d2 = line.find("Duration(h:m:s):");
    entry.durationHMS = CString(line.substr(d2 + 18).c_str());

    m_totalTimeMs += entry.durationMs;
    m_entries.push_back(entry);
}

void CSOHResultDlg::DisplayData()
{
    m_list.DeleteAllItems();

    int n = (int)m_entries.size();

    std::vector<bool>       groupEnd(n, false);
    std::vector<ULONGLONG>  groupSum(n, 0);
    std::vector<int>        groupFirst(n, 0);

    if (n == 0)
    {
        // ── Summary label ───────────────────────────────────────────
        ULONGLONG totalSec = m_totalTimeMs / 1000;
        int hrs = (int)(totalSec / 3600);
        int mins = (int)((totalSec % 3600) / 60);
        int secs = (int)(totalSec % 60);
        CString summary;
        summary.Format(_T("Test ID: %s | Start: %s | Total: %02d:%02d:%02d"),
            m_testIDStr, m_startTimeStr, hrs, mins, secs);
        m_lblSummary.SetWindowText(summary);
        m_groupEnd = groupEnd;
        return;
    }

    // ── Dynamic top-down 10% grouping ───────────────────────────────
    // First percent seen becomes the top of group 1.
    // Each group spans exactly 10 percentage points downward.
    // e.g. start=100 → groups: [100..91], [90..81], [80..remainder]
    // e.g. start=95  → groups: [95..86],  [85..76], [75..remainder]

    int firstPct = m_entries[0].percent;
    int bucketFloor = firstPct - 9;   // lowest percent still in group 1
    // group 1 = [firstPct .. firstPct-9]

    int       startRow = 0;
    ULONGLONG bucketMs = 0;

    for (int i = 0; i < n; ++i)
    {
        int pct = m_entries[i].percent;

        // Has this row crossed below the current bucket floor?
        while (pct < bucketFloor && i > startRow)
        {
            // Flush the just-finished bucket (rows startRow..i-1)
            groupEnd[i - 1] = true;
            groupSum[i - 1] = bucketMs;
            for (int r = startRow; r <= i - 1; ++r)
                groupFirst[r] = startRow;

            // Advance to next bucket (floor drops by 10)
            bucketFloor -= 10;
            startRow = i;
            bucketMs = 0;
        }

        bucketMs += m_entries[i].durationMs;

        // Last row — always flush whatever bucket is open
        if (i == n - 1)
        {
            groupEnd[i] = true;
            groupSum[i] = bucketMs;
            for (int r = startRow; r <= i; ++r)
                groupFirst[r] = startRow;
        }
    }

    // ── Insert rows ─────────────────────────────────────────────────
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

        // Find the last row of this row's group
        int first = groupFirst[i];
        int last = i;
        for (int k = i; k < n; ++k) { if (groupEnd[k]) { last = k; break; } }

        // Show sum only on the vertically-centered row of the group
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

    // ── Summary label ───────────────────────────────────────────────
    ULONGLONG totalSec = m_totalTimeMs / 1000;
    int hrs = (int)(totalSec / 3600);
    int mins = (int)((totalSec % 3600) / 60);
    int secs = (int)(totalSec % 60);

    CString summary;
    summary.Format(_T("Test ID: %s | Start: %s | Total: %02d:%02d:%02d"),
        m_testIDStr, m_startTimeStr, hrs, mins, secs);
    m_lblSummary.SetWindowText(summary);

    // ── Anomaly detection ────────────────────────────────────────
    m_anomaly.assign(n, false);

    if (n > 1)
    {
        // Compute mean
        double sum = 0.0;
        for (int i = 0; i < n; ++i)
            sum += (double)m_entries[i].durationMs;
        double mean = sum / n;

        // Compute standard deviation
        double sq = 0.0;
        for (int i = 0; i < n; ++i)
        {
            double diff = (double)m_entries[i].durationMs - mean;
            sq += diff * diff;
        }
        double stddev = sqrt(sq / n);

        double threshold = mean + 1.5 * stddev;

       //// ── DEBUG: remove after checking ──
       // CString debug;
       // debug.Format(_T("mean=%.0f stddev=%.0f threshold=%.0f"), mean, stddev, threshold);
       // m_lblSummary.SetWindowText(debug);

       //// ── END DEBUG ──

        for (int i = 0; i < n; ++i)
            m_anomaly[i] = ((double)m_entries[i].durationMs > threshold);
    }

    m_groupEnd = groupEnd;  // save for custom draw separator lines
}
// ─────────────────────────────────────────────
//  Toggle (See All / Show Latest)  — unchanged
// ─────────────────────────────────────────────
//void CSOHResultDlg::OnToggleView()
//{
//    m_showAll = !m_showAll;
//
//    // If chart is visible, hide it and show list again
//    if (m_showChart)
//    {
//        m_showChart = false;
//        m_btnChart.SetWindowText(_T("Show Chart"));
//        m_list.ShowWindow(SW_SHOW);
//        Invalidate();
//    }
//
//    m_list.DeleteAllItems();
//    while (m_list.DeleteColumn(0));
//
//    if (m_showAll)
//    {
//        m_btnToggle.SetWindowText(_T("Show Latest"));
//
//        m_list.InsertColumn(0, _T("Test ID"), LVCFMT_LEFT, 100);
//        m_list.InsertColumn(1, _T("Start"), LVCFMT_LEFT, 140);
//        m_list.InsertColumn(2, _T("Time"), LVCFMT_LEFT, 80);
//        m_list.InsertColumn(3, _T("Percent"), LVCFMT_CENTER, 80);
//        m_list.InsertColumn(4, _T("Duration(ms)"), LVCFMT_CENTER, 120);
//        m_list.InsertColumn(5, _T("Duration(h:m:s)"), LVCFMT_CENTER, 140);
//
//        CString lastTestID;
//        int index = 0;
//
//        /*for (const auto& e : m_allEntries)
//        {
//            m_list.InsertItem(index, _T(""));
//
//            if (e.testID != lastTestID)
//            {
//                m_list.SetItemText(index, 0, e.testID);
//                m_list.SetItemText(index, 1, e.startTime);
//                lastTestID = e.testID;
//            }
//
//            m_list.SetItemText(index, 2, e.time);
//
//            CString pct;
//            pct.Format(_T("%d%%"), e.percent);
//            m_list.SetItemText(index, 3, pct);
//
//            CString dur;
//            dur.Format(_T("%llu"), e.durationMs);
//            m_list.SetItemText(index, 4, dur);
//
//            m_list.SetItemText(index, 5, e.durationHMS);
//
//            index++;
//        }*/
//
//
//
//        for (auto it = m_allEntries.rbegin(); it != m_allEntries.rend(); ++it)
//        {
//            const auto& e = *it;
//
//            m_list.InsertItem(index, _T(""));
//
//            if (e.testID != lastTestID)
//            {
//                m_list.SetItemText(index, 0, e.testID);
//                m_list.SetItemText(index, 1, e.startTime);
//                lastTestID = e.testID;
//            }
//
//            m_list.SetItemText(index, 2, e.time);
//
//            CString pct;
//            pct.Format(_T("%d%%"), e.percent);
//            m_list.SetItemText(index, 3, pct);
//
//            CString dur;
//            dur.Format(_T("%llu"), e.durationMs);
//            m_list.SetItemText(index, 4, dur);
//
//            m_list.SetItemText(index, 5, e.durationHMS);
//
//            index++;
//        }
//
//
//        ULONGLONG totalMs = 0;
//        for (const auto& e : m_allEntries)
//            totalMs += e.durationMs;
//
//        ULONGLONG totalSec = totalMs / 1000;
//        int hrs = (int)(totalSec / 3600);
//        int mins = (int)((totalSec % 3600) / 60);
//        int secs = (int)(totalSec % 60);
//
//        CString summary;
//        summary.Format(_T("Total Duration: %02d:%02d:%02d"), hrs, mins, secs);
//        m_lblSummary.SetWindowText(summary);
//    }
//    else
//    {
//        m_btnToggle.SetWindowText(_T("See All"));
//
//        m_list.InsertColumn(0, _T("Time"), LVCFMT_LEFT, 100);
//        m_list.InsertColumn(1, _T("Percent"), LVCFMT_CENTER, 80);
//        m_list.InsertColumn(2, _T("Duration(ms)"), LVCFMT_CENTER, 120);
//        m_list.InsertColumn(3, _T("Duration(h:m:s)"), LVCFMT_CENTER, 140);
//
//        DisplayData();
//    }
//}






void CSOHResultDlg::OnToggleView()
{
    m_showAll = !m_showAll;

    // Helper: destroy and recreate footer to avoid ghosting
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

    // If chart is visible, hide it and show list again
    if (m_showChart)
    {
        m_showChart = false;
        m_btnChart.SetWindowText(_T("Show Chart"));
        m_list.ShowWindow(SW_SHOW);
        m_lblLegend.ShowWindow(SW_SHOW);
        Invalidate();
    }

    m_list.DeleteAllItems();
    while (m_list.DeleteColumn(0));

    if (m_showAll)
    {
        m_lblLegend.ShowWindow(SW_HIDE);

        m_groupEnd.clear();
        m_anomaly.clear();

        m_btnToggle.SetWindowText(_T("Show Latest"));

        m_list.InsertColumn(0, _T("Test ID"), LVCFMT_LEFT, 100);
        m_list.InsertColumn(1, _T("Start"), LVCFMT_LEFT, 140);
        m_list.InsertColumn(2, _T("Time"), LVCFMT_LEFT, 80);
        m_list.InsertColumn(3, _T("Percent"), LVCFMT_CENTER, 80);
        m_list.InsertColumn(4, _T("Duration(ms)"), LVCFMT_CENTER, 120);
        m_list.InsertColumn(5, _T("Duration(h:m:s)"), LVCFMT_CENTER, 140);
        m_list.InsertColumn(6, _T("Elapsed(h:m:s)"), LVCFMT_CENTER, 140);

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
        summary.Format(_T("Total Duration: %02d:%02d:%02d"), hrs, mins, secs);
        SetSummary(summary);
    }
    else
    {
        m_lblLegend.ShowWindow(SW_SHOW);

        m_btnToggle.SetWindowText(_T("See All"));

        m_list.InsertColumn(0, _T("Time"), LVCFMT_LEFT, 100);
        m_list.InsertColumn(1, _T("Percent"), LVCFMT_CENTER, 80);
        m_list.InsertColumn(2, _T("Duration(ms)"), LVCFMT_CENTER, 120);
        m_list.InsertColumn(3, _T("Duration(h:m:s)"), LVCFMT_CENTER, 140);
        m_list.InsertColumn(4, _T("Sum/10%(h:m:s)"), LVCFMT_CENTER, 140);
        m_list.InsertColumn(5, _T("Elapsed(h:m:s)"), LVCFMT_CENTER, 140);

        DisplayData();

        // Recreate footer after DisplayData() which also sets it
        CString text;
        m_lblSummary.GetWindowText(text);
        SetSummary(text);
    }
}







// ─────────────────────────────────────────────
//  Chart Button
// ─────────────────────────────────────────────
void CSOHResultDlg::OnShowChart()
{
    if (m_showAll)
    {
        MessageBox(_T("Switch to 'Show Latest' view to see the chart."),
            _T("Chart"), MB_ICONINFORMATION);
        return;
    }

    m_showChart = !m_showChart;

    if (m_showChart)
    {
        m_btnChart.SetWindowText(_T("Hide Chart"));
        m_list.ShowWindow(SW_HIDE);
        m_lblLegend.ShowWindow(SW_HIDE);   // ← ADD
        m_lblSummary.ShowWindow(SW_HIDE);  // ← ADD
    }
    else
    {
        m_btnChart.SetWindowText(_T("Show Chart"));
        m_list.ShowWindow(SW_SHOW);
        m_lblLegend.ShowWindow(SW_SHOW);   // ← ADD
        m_lblSummary.ShowWindow(SW_SHOW);  // ← ADD
    }

    Invalidate();
}

// ─────────────────────────────────────────────
//  OnPaint — draws the chart when m_showChart
// ─────────────────────────────────────────────
//void CSOHResultDlg::OnPaint()
//{
//    if (!m_showChart || m_entries.empty())
//    {
//        CDialogEx::OnPaint();
//        return;
//    }
//
//    CPaintDC dc(this);
//
//    CRect rc;
//    GetClientRect(&rc);
//
//    const int LEFT = 80;
//    const int RIGHT = rc.Width() - 40;
//    const int TOP = 65;
//    const int BOTTOM = rc.Height() - 120;
//
//    int chartW = RIGHT - LEFT;
//    int chartH = BOTTOM - TOP;
//
//    // ── Background ───────────────────────────────
//    dc.FillSolidRect(&rc, RGB(30, 30, 40));
//    CRect chartRect(LEFT, TOP, RIGHT, BOTTOM);
//    dc.FillSolidRect(&chartRect, RGB(20, 20, 30));
//
//    int n = (int)m_entries.size();
//
//    // ── Build cumulative duration ────────────────
//    std::vector<double> cumulative;
//    cumulative.reserve(n);
//    double totalTime = 0.0;
//    for (const auto& e : m_entries)
//    {
//        totalTime += (double)e.durationMs;
//        cumulative.push_back(totalTime);
//    }
//    double maxTime = cumulative.back();
//
//    // ── Pre-compute pixel X/Y for every point ────
//    std::vector<int> px(n), py(n);
//    for (int i = 0; i < n; ++i)
//    {
//        px[i] = LEFT + (int)((cumulative[i] / maxTime) * chartW);
//        py[i] = BOTTOM - (int)((double)m_entries[i].percent / 100.0 * chartH);
//    }
//
//    // ── Y Grid ──────────────────────────────────
//    CFont fontSmall;
//    fontSmall.CreatePointFont(70, _T("Segoe UI"));
//    CFont* pOldFont = dc.SelectObject(&fontSmall);
//    dc.SetBkMode(TRANSPARENT);
//    dc.SetTextColor(RGB(140, 140, 165));
//
//    CPen penGrid(PS_DOT, 1, RGB(55, 55, 75));
//    CPen* pOldPen = dc.SelectObject(&penGrid);
//
//    for (int pct = 0; pct <= 100; pct += 10)
//    {
//        int y = BOTTOM - (int)((double)pct / 100.0 * chartH);
//        dc.MoveTo(LEFT, y);
//        dc.LineTo(RIGHT, y);
//
//        CString lbl;
//        lbl.Format(_T("%d%%"), pct);
//        dc.TextOut(LEFT - 42, y - 7, lbl);
//    }
//    dc.SelectObject(pOldPen);
//
//    // ── Axes ─────────────────────────────────────
//    CPen penAxis(PS_SOLID, 2, RGB(170, 170, 195));
//    pOldPen = dc.SelectObject(&penAxis);
//    dc.MoveTo(LEFT, TOP);
//    dc.LineTo(LEFT, BOTTOM);
//    dc.LineTo(RIGHT, BOTTOM);
//    dc.SelectObject(pOldPen);
//
//    // ── Gradient-style filled area (two-pass) ────
//    if (n >= 2)
//    {
//        // Build polygon
//        std::vector<POINT> poly;
//        poly.reserve(n + 2);
//        poly.push_back({ LEFT, BOTTOM });
//        for (int i = 0; i < n; ++i)
//            poly.push_back({ px[i], py[i] });
//        poly.push_back({ px[n - 1], BOTTOM });
//
//        // Simulate gradient: draw horizontal bands top-to-bottom
//        // from semi-transparent blue to nearly invisible
//        for (int band = 0; band < chartH; ++band)
//        {
//            double t = (double)band / chartH;           // 0 = top, 1 = bottom
//            int alpha = (int)(80.0 * (1.0 - t));        // 80 → 0
//            if (alpha <= 0) continue;
//
//            int y = TOP + band;
//
//            // Intersect horizontal line y with the polygon edges
//            int xLeft = LEFT, xRight = LEFT;
//            bool found = false;
//            for (int i = 0; i < (int)poly.size() - 1; ++i)
//            {
//                int y0 = poly[i].y, y1 = poly[i + 1].y;
//                int x0 = poly[i].x, x1 = poly[i + 1].x;
//                if ((y0 <= y && y < y1) || (y1 <= y && y < y0))
//                {
//                    double frac = (double)(y - y0) / (y1 - y0);
//                    int xi = x0 + (int)(frac * (x1 - x0));
//                    if (!found) { xLeft = xRight = xi; found = true; }
//                    else { xLeft = min(xLeft, xi); xRight = max(xRight, xi); }
//                }
//            }
//            if (!found) continue;
//
//            CPen penBand(PS_SOLID, 1, RGB(
//                (BYTE)(0 + (int)(30 * t)),
//                (BYTE)(100 + (int)(20 * t)),
//                (BYTE)(180 - (int)(60 * t))
//            ));
//            CPen* pOld = dc.SelectObject(&penBand);
//            dc.MoveTo(xLeft, y);
//            dc.LineTo(xRight, y);
//            dc.SelectObject(pOld);
//        }
//    }
//
//    // ── Line ─────────────────────────────────────
//    {
//        CPen penLine(PS_SOLID, 2, RGB(0, 185, 255));
//        pOldPen = dc.SelectObject(&penLine);
//        for (int i = 0; i < n; ++i)
//        {
//            if (i == 0) dc.MoveTo(px[i], py[i]);
//            else        dc.LineTo(px[i], py[i]);
//        }
//        dc.SelectObject(pOldPen);
//    }
//
//    // ── Dots (normal = cyan, anomaly = red) ──────
//    for (int i = 0; i < n; ++i)
//    {
//        bool isAnomaly = (i < (int)m_anomaly.size() && m_anomaly[i]);
//        COLORREF dotColor = isAnomaly ? RGB(220, 50, 50) : RGB(0, 210, 255);
//
//        CBrush brushDot(dotColor);
//        CBrush* pOldBrush = dc.SelectObject(&brushDot);
//
//        CPen penDot(PS_SOLID, 1, isAnomaly ? RGB(255, 100, 100) : RGB(180, 240, 255));
//        CPen* pOldPenDot = dc.SelectObject(&penDot);
//
//        // Slightly larger dot for anomalies
//        int r = isAnomaly ? 6 : 4;
//        dc.Ellipse(px[i] - r, py[i] - r, px[i] + r, py[i] + r);
//
//        dc.SelectObject(pOldPenDot);
//        dc.SelectObject(pOldBrush);
//    }
//
//    // ── X-axis labels — smart skip + stagger ─────
//    // Decide which labels to show so none overlap.
//    // Minimum gap between label centers: 50px.
//    // Alternate stagger: even-index visible label -> row A (BOTTOM+20),
//    //                    odd-index visible label  -> row B (BOTTOM+35).
//    {
//        const int MIN_GAP = 50;   // pixels between label centres
//        const int ROW_A = BOTTOM + 20;
//        const int ROW_B = BOTTOM + 38;
//
//        // Tick marks for every point
//        CPen penTick(PS_SOLID, 1, RGB(140, 140, 165));
//        pOldPen = dc.SelectObject(&penTick);
//        for (int i = 0; i < n; ++i)
//        {
//            dc.MoveTo(px[i], BOTTOM);
//            dc.LineTo(px[i], BOTTOM + 5);
//        }
//        dc.SelectObject(pOldPen);
//
//        dc.SetTextColor(RGB(190, 190, 215));
//
//        CFont fontXLabel;
//        fontXLabel.CreatePointFont(70, _T("Segoe UI"));
//        dc.SelectObject(&fontXLabel);
//
//        int lastDrawnX = -9999;
//        int staggerIdx = 0;   // counts only the labels we actually draw
//
//        for (int i = 0; i < n; ++i)
//        {
//            if (px[i] - lastDrawnX < MIN_GAP)
//                continue;   // too close to previous label — skip
//
//            double sec = m_entries[i].durationMs / 1000.0;
//            CString lbl;
//            if (sec < 60.0)
//                lbl.Format(_T("%.0fs"), sec);
//            else
//            {
//                int m2 = (int)(sec / 60);
//                int s2 = (int)(sec) % 60;
//                lbl.Format(_T("%dm%02ds"), m2, s2);
//            }
//
//            int row = (staggerIdx % 2 == 0) ? ROW_A : ROW_B;
//
//            // Draw a thin connector from tick to label row
//            if (row == ROW_B)
//            {
//                CPen penConn(PS_DOT, 1, RGB(70, 70, 90));
//                CPen* pOldC = dc.SelectObject(&penConn);
//                dc.MoveTo(px[i], BOTTOM + 5);
//                dc.LineTo(px[i], ROW_B - 3);
//                dc.SelectObject(pOldC);
//            }
//
//            CSize sz = dc.GetTextExtent(lbl);
//            dc.TextOut(px[i] - sz.cx / 2, row, lbl);
//
//            lastDrawnX = px[i];
//            ++staggerIdx;
//        }
//
//        dc.SelectObject(fontSmall);
//    }
//
//    // ── Axis labels ──────────────────────────────
//    CFont fontLabel;
//    fontLabel.CreatePointFont(85, _T("Segoe UI"));
//
//    // Y-axis vertical text
//    CFont fontVert;
//    LOGFONT lf = {};
//    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
//    lf.lfHeight = -14;
//    lf.lfEscapement = 900;
//    lf.lfOrientation = 900;
//    fontVert.CreateFontIndirect(&lf);
//    dc.SelectObject(&fontVert);
//    dc.SetTextColor(RGB(140, 140, 165));
//    dc.TextOut(15, BOTTOM - chartH / 2 + 40, _T("Percent (%)"));
//
//    // X-axis title
//    dc.SelectObject(&fontLabel);
//    dc.TextOut(LEFT + chartW / 2 - 80, BOTTOM + 60, _T("Step Duration (seconds)"));
//
//    // ── Chart title ──────────────────────────────
//    CFont fontTitle;
//    fontTitle.CreatePointFont(100, _T("Segoe UI"));
//    dc.SelectObject(&fontTitle);
//    dc.SetTextColor(RGB(210, 210, 240));
//
//    CString title;
//    title.Format(_T("SOH \x2014 Duration per Step  [%s]"), m_testIDStr);
//    CSize sz = dc.GetTextExtent(title);
//    dc.TextOut(LEFT + (chartW - sz.cx) / 2, TOP - 28, title);
//
//    // ── Legend (top-right) ───────────────────────
//    dc.SelectObject(&fontSmall);
//    dc.SetTextColor(RGB(170, 170, 200));
//
//    int lx = RIGHT - 140, ly = TOP - 25;
//
//    CBrush bCyan(RGB(0, 185, 255));
//    dc.FillRect(CRect(lx, ly + 1, lx + 10, ly + 11), &bCyan);
//    dc.TextOut(lx + 13, ly, _T("SOH %"));
//
//    CBrush bRed(RGB(220, 50, 50));
//    dc.FillRect(CRect(lx + 65, ly + 1, lx + 75, ly + 11), &bRed);
//    dc.TextOut(lx + 78, ly, _T("Anomaly"));
//
//    dc.SelectObject(pOldFont);
//}

void CSOHResultDlg::OnCustomDrawList(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVCUSTOMDRAW* pCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
    *pResult = CDRF_DODEFAULT;

    switch (pCD->nmcd.dwDrawStage)
    {
    case CDDS_PREPAINT:
        *pResult = CDRF_NOTIFYITEMDRAW;
        break;

    case CDDS_ITEMPREPAINT:
    {
        int row = (int)pCD->nmcd.dwItemSpec;

        // Color anomaly rows red
        if (row < (int)m_anomaly.size() && m_anomaly[row])
        {
            pCD->clrText = RGB(255, 255, 255);   // white text
            pCD->clrTextBk = RGB(180, 30, 30);     // red background
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

            CPen pen(PS_SOLID, 2, RGB(0, 0, 0));
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

//void CSOHResultDlg::OnCustomDrawList(NMHDR* pNMHDR, LRESULT* pResult)
//{
//    NMLVCUSTOMDRAW* pCD = reinterpret_cast<NMLVCUSTOMDRAW*>(pNMHDR);
//    *pResult = CDRF_DODEFAULT;
//
//    switch (pCD->nmcd.dwDrawStage)
//    {
//    case CDDS_PREPAINT:
//        *pResult = CDRF_NOTIFYITEMDRAW;
//        break;
//
//    case CDDS_ITEMPREPAINT:
//        *pResult = CDRF_NOTIFYSUBITEMDRAW | CDRF_NOTIFYPOSTPAINT;
//        break;
//
//    case CDDS_ITEMPOSTPAINT:
//    {
//        int row = (int)pCD->nmcd.dwItemSpec;
//        if (row < (int)m_groupEnd.size() && m_groupEnd[row])
//        {
//            // Draw a separator line at the bottom of this row
//            CDC* pDC = CDC::FromHandle(pCD->nmcd.hdc);
//            CRect rcItem;
//            m_list.GetItemRect(row, &rcItem, LVIR_BOUNDS);
//
//           /* CPen pen(PS_SOLID, 2, RGB(100, 160, 220));*/
//            CPen pen(PS_SOLID, 2, RGB(0, 0, 0));
//            CPen* pOld = pDC->SelectObject(&pen);
//            pDC->MoveTo(rcItem.left, rcItem.bottom - 1);
//            pDC->LineTo(rcItem.right, rcItem.bottom - 1);
//            pDC->SelectObject(pOld);
//        }
//        *pResult = CDRF_DODEFAULT;
//        break;
//    }
//    }
//}



void CSOHResultDlg::OnPaint()
{
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
    const int TOP = 65;
    const int BOTTOM = rc.Height() - 120;

    int chartW = RIGHT - LEFT;
    int chartH = BOTTOM - TOP;

    // ── Background ───────────────────────────────
    dc.FillSolidRect(&rc, RGB(30, 30, 40));
    CRect chartRect(LEFT, TOP, RIGHT, BOTTOM);
    dc.FillSolidRect(&chartRect, RGB(20, 20, 30));

    int n = (int)m_entries.size();

   
    // ── Build cumulative duration ────────────────
// cumulative[i] = elapsed time BEFORE entry i (so point 0 starts at 0 = Y-axis)
    std::vector<double> cumulative;
    cumulative.reserve(n);
    double totalTime = 0.0;
    for (const auto& e : m_entries)
    {
        cumulative.push_back(totalTime);          // push BEFORE adding — starts at 0
        totalTime += (double)e.durationMs;
    }
    double maxTime = totalTime;                   // total is accumulated after the loop

    // ── Pre-compute pixel X/Y for every point ────
    std::vector<int> px(n), py(n);
    for (int i = 0; i < n; ++i)
    {
        px[i] = LEFT + (int)((cumulative[i] / maxTime) * chartW);
        py[i] = BOTTOM - (int)((double)m_entries[i].percent / 100.0 * chartH);
    }
    // ── Y Grid ──────────────────────────────────
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

    // ── Axes ─────────────────────────────────────
    CPen penAxis(PS_SOLID, 2, RGB(170, 170, 195));
    pOldPen = dc.SelectObject(&penAxis);
    dc.MoveTo(LEFT, TOP);
    dc.LineTo(LEFT, BOTTOM);
    dc.LineTo(RIGHT, BOTTOM);
    dc.SelectObject(pOldPen);

    // ── Gradient-style filled area (two-pass) ────
    if (n >= 2)
    {
        // Build polygon
        std::vector<POINT> poly;
        poly.reserve(n + 2);
        poly.push_back({ LEFT, BOTTOM });
        for (int i = 0; i < n; ++i)
            poly.push_back({ px[i], py[i] });
        poly.push_back({ px[n - 1], BOTTOM });

        for (int band = 0; band < chartH; ++band)
        {
            double t = (double)band / chartH;
            int alpha = (int)(80.0 * (1.0 - t));
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
                    int xi = x0 + (int)(frac * (x1 - x0));
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

    // ── Line ─────────────────────────────────────
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

    // ── Dots (normal = cyan, anomaly = red) ──────
    for (int i = 0; i < n; ++i)
    {
        bool isAnomaly = (i < (int)m_anomaly.size() && m_anomaly[i]);
        COLORREF dotColor = isAnomaly ? RGB(220, 50, 50) : RGB(0, 210, 255);

        CBrush brushDot(dotColor);
        CBrush* pOldBrush = dc.SelectObject(&brushDot);

        CPen penDot(PS_SOLID, 1, isAnomaly ? RGB(255, 100, 100) : RGB(180, 240, 255));
        CPen* pOldPenDot = dc.SelectObject(&penDot);

        int r = isAnomaly ? 6 : 4;
        dc.Ellipse(px[i] - r, py[i] - r, px[i] + r, py[i] + r);

        dc.SelectObject(pOldPenDot);
        dc.SelectObject(pOldBrush);
    }

    // ── X-axis labels — smart skip + stagger ─────
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

            // ── CHANGED: use cumulative elapsed time, not per-step duration ──
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

    // ── Axis labels ──────────────────────────────
    CFont fontLabel;
    fontLabel.CreatePointFont(85, _T("Segoe UI"));

    CFont fontVert;
    LOGFONT lf = {};
    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
    lf.lfHeight = -14;
    lf.lfEscapement = 900;
    lf.lfOrientation = 900;
    fontVert.CreateFontIndirect(&lf);
    dc.SelectObject(&fontVert);
    dc.SetTextColor(RGB(140, 140, 165));
    dc.TextOut(15, BOTTOM - chartH / 2 + 40, _T("Percent (%)"));

    // ── CHANGED: X-axis title ──
    dc.SelectObject(&fontLabel);
    dc.TextOut(LEFT + chartW / 2 - 60, BOTTOM + 60, _T("Elapsed Time (seconds)"));

    // ── Chart title ──────────────────────────────
    CFont fontTitle;
    fontTitle.CreatePointFont(100, _T("Segoe UI"));
    dc.SelectObject(&fontTitle);
    dc.SetTextColor(RGB(210, 210, 240));

    CString title;
    title.Format(_T("SOH \x2014 Percent vs Elapsed Time  [%s]"), m_testIDStr);
    CSize sz = dc.GetTextExtent(title);
    dc.TextOut(LEFT + (chartW - sz.cx) / 2, TOP - 28, title);

    // ── Legend (top-right) ───────────────────────
    dc.SelectObject(&fontSmall);
    dc.SetTextColor(RGB(170, 170, 200));

    int lx = RIGHT - 140, ly = TOP - 25;

    CBrush bCyan(RGB(0, 185, 255));
    dc.FillRect(CRect(lx, ly + 1, lx + 10, ly + 11), &bCyan);
    dc.TextOut(lx + 13, ly, _T("SOH %"));

    CBrush bRed(RGB(220, 50, 50));
    dc.FillRect(CRect(lx + 65, ly + 1, lx + 75, ly + 11), &bRed);
    dc.TextOut(lx + 78, ly, _T("Anomaly"));

    dc.SelectObject(pOldFont);
}