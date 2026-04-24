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
    ON_WM_PAINT()

    ON_NOTIFY(NM_CUSTOMDRAW, 2001, &CSOHResultDlg::OnCustomDrawList)

END_MESSAGE_MAP()

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

    // List Control
    m_list.Create(WS_CHILD | WS_VISIBLE | LVS_REPORT,
        CRect(10, 50, rc.Width() - 10, rc.Height() - 80),
        this, 2001);

    m_list.InsertColumn(0, _T("Time"), LVCFMT_LEFT, 100);
    m_list.InsertColumn(1, _T("Percent"), LVCFMT_CENTER, 80);
    m_list.InsertColumn(2, _T("Duration(ms)"), LVCFMT_CENTER, 120);
    m_list.InsertColumn(3, _T("Duration(h:m:s)"), LVCFMT_CENTER, 140);

    // After inserting your existing 4 columns, add:
    m_list.InsertColumn(4, _T("Sum/10%(h:m:s)"), LVCFMT_CENTER, 140);

    // Summary Label
    m_lblSummary.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_CENTER,
        CRect(10, rc.Height() - 60, rc.Width() - 10, rc.Height() - 10),
        this);

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
    for (int i = 0; i < n; ++i)
    {
        const auto& e = m_entries[i];

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

    // If chart is visible, hide it and show list again
    if (m_showChart)
    {
        m_showChart = false;
        m_btnChart.SetWindowText(_T("Show Chart"));
        m_list.ShowWindow(SW_SHOW);
        Invalidate();
    }

    m_list.DeleteAllItems();
    while (m_list.DeleteColumn(0));

    if (m_showAll)
    {
        m_groupEnd.clear();

        m_btnToggle.SetWindowText(_T("Show Latest"));

        m_list.InsertColumn(0, _T("Test ID"), LVCFMT_LEFT, 100);
        m_list.InsertColumn(1, _T("Start"), LVCFMT_LEFT, 140);
        m_list.InsertColumn(2, _T("Time"), LVCFMT_LEFT, 80);
        m_list.InsertColumn(3, _T("Percent"), LVCFMT_CENTER, 80);
        m_list.InsertColumn(4, _T("Duration(ms)"), LVCFMT_CENTER, 120);
        m_list.InsertColumn(5, _T("Duration(h:m:s)"), LVCFMT_CENTER, 140);

        // ─────────────────────────────────────────────
        // Group entries by TestID
        // ─────────────────────────────────────────────
        std::map<CString, std::vector<SOHFullEntry>> grouped;

        for (const auto& e : m_allEntries)
        {
            grouped[e.testID].push_back(e);
        }

        // Maintain appearance order of Test IDs
        std::vector<CString> order;
        for (const auto& e : m_allEntries)
        {
            if (std::find(order.begin(), order.end(), e.testID) == order.end())
                order.push_back(e.testID);
        }

        // Reverse ONLY test order (latest first)
        std::reverse(order.begin(), order.end());

        CString lastTestID;
        int index = 0;

        // ─────────────────────────────────────────────
        // Display grouped data
        // ─────────────────────────────────────────────
        for (const auto& testID : order)
        {
            const auto& entries = grouped[testID];

            for (const auto& e : entries) // keep original order inside test
            {
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

                index++;
            }
        }

        // ─────────────────────────────────────────────
        // Summary (unchanged)
        // ─────────────────────────────────────────────
        ULONGLONG totalMs = 0;
        for (const auto& e : m_allEntries)
            totalMs += e.durationMs;

        ULONGLONG totalSec = totalMs / 1000;
        int hrs = (int)(totalSec / 3600);
        int mins = (int)((totalSec % 3600) / 60);
        int secs = (int)(totalSec % 60);

        CString summary;
        summary.Format(_T("Total Duration: %02d:%02d:%02d"), hrs, mins, secs);
        m_lblSummary.SetWindowText(summary);
    }
    else
    {
        m_btnToggle.SetWindowText(_T("See All"));

        m_list.InsertColumn(0, _T("Time"), LVCFMT_LEFT, 100);
        m_list.InsertColumn(1, _T("Percent"), LVCFMT_CENTER, 80);
        m_list.InsertColumn(2, _T("Duration(ms)"), LVCFMT_CENTER, 120);
        m_list.InsertColumn(3, _T("Duration(h:m:s)"), LVCFMT_CENTER, 140);
        m_list.InsertColumn(4, _T("Sum/10%(h:m:s)"), LVCFMT_CENTER, 140);  // ← add this

        DisplayData();
    }
}







// ─────────────────────────────────────────────
//  Chart Button
// ─────────────────────────────────────────────
void CSOHResultDlg::OnShowChart()
{
    // Chart only works for latest-test view
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
    }
    else
    {
        m_btnChart.SetWindowText(_T("Show Chart"));
        m_list.ShowWindow(SW_SHOW);
    }

    Invalidate();
}

// ─────────────────────────────────────────────
//  OnPaint — draws the chart when m_showChart
// ─────────────────────────────────────────────
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
    const int BOTTOM = rc.Height() - 110; // more space for rotated text

    int chartW = RIGHT - LEFT;
    int chartH = BOTTOM - TOP;

    // Background
    dc.FillSolidRect(&rc, RGB(30, 30, 40));
    CRect chartRect(LEFT, TOP, RIGHT, BOTTOM);
    dc.FillSolidRect(&chartRect, RGB(20, 20, 30));

    int n = (int)m_entries.size();

    // ─────────────────────────────────────────────
    // Build cumulative duration
    // ─────────────────────────────────────────────
    std::vector<double> cumulative;
    cumulative.reserve(n);

    double totalTime = 0.0;
    for (const auto& e : m_entries)
    {
        totalTime += (double)e.durationMs;
        cumulative.push_back(totalTime);
    }

    double maxTime = cumulative.back();

    // ── Y Grid ───────────────────────────────────
    CPen penGrid(PS_DOT, 1, RGB(60, 60, 80));
    CPen* pOldPen = dc.SelectObject(&penGrid);

    CFont fontSmall;
    fontSmall.CreatePointFont(70, _T("Segoe UI"));
    CFont* pOldFont = dc.SelectObject(&fontSmall);

    dc.SetBkMode(TRANSPARENT);
    dc.SetTextColor(RGB(160, 160, 180));

    for (int pct = 0; pct <= 100; pct += 10)
    {
        int y = BOTTOM - (int)((double)pct / 100.0 * chartH);
        dc.MoveTo(LEFT, y);
        dc.LineTo(RIGHT, y);

        CString lbl;
        lbl.Format(_T("%d%%"), pct);
        dc.TextOut(LEFT - 40, y - 7, lbl);
    }

    dc.SelectObject(pOldPen);

    // ── Axes ─────────────────────────────────────
    CPen penAxis(PS_SOLID, 2, RGB(180, 180, 200));
    pOldPen = dc.SelectObject(&penAxis);

    dc.MoveTo(LEFT, TOP);
    dc.LineTo(LEFT, BOTTOM);
    dc.LineTo(RIGHT, BOTTOM);

    dc.SelectObject(pOldPen);

    // ── Filled area ─────────────────────────────
    if (n >= 2)
    {
        std::vector<POINT> poly;
        poly.reserve(n + 2);

        poly.push_back({ LEFT, BOTTOM });

        for (int i = 0; i < n; ++i)
        {
            int x = LEFT + (int)((cumulative[i] / maxTime) * chartW);
            int y = BOTTOM - (int)((double)m_entries[i].percent / 100.0 * chartH);
            poly.push_back({ x, y });
        }

        poly.push_back({ LEFT + chartW, BOTTOM });

        CBrush brushFill(RGB(0, 80, 160));
        CBrush* pOldBrush = dc.SelectObject(&brushFill);

        CPen penNone(PS_NULL, 0, RGB(0, 0, 0));
        CPen* pOldPen2 = dc.SelectObject(&penNone);

        dc.Polygon(poly.data(), (int)poly.size());

        dc.SelectObject(pOldPen2);
        dc.SelectObject(pOldBrush);
    }

    // ── Line ────────────────────────────────────
    CPen penLine(PS_SOLID, 2, RGB(0, 180, 255));
    pOldPen = dc.SelectObject(&penLine);

    for (int i = 0; i < n; ++i)
    {
        int x = LEFT + (int)((cumulative[i] / maxTime) * chartW);
        int y = BOTTOM - (int)((double)m_entries[i].percent / 100.0 * chartH);

        if (i == 0)
            dc.MoveTo(x, y);
        else
            dc.LineTo(x, y);
    }

    dc.SelectObject(pOldPen);

    // ── Dots ────────────────────────────────────
    CBrush brushDot(RGB(0, 220, 255));
    for (int i = 0; i < n; ++i)
    {
        int x = LEFT + (int)((cumulative[i] / maxTime) * chartW);
        int y = BOTTOM - (int)((double)m_entries[i].percent / 100.0 * chartH);

        CRect dotRc(x - 4, y - 4, x + 4, y + 4);
        dc.FillRect(&dotRc, &brushDot);
    }

    // ─────────────────────────────────────────────
    // ✅ X-axis: ALL durations with rotation
    // ─────────────────────────────────────────────
    dc.SetTextColor(RGB(200, 200, 220));

    CFont fontRot;
    LOGFONT lfRot = {};
    _tcscpy_s(lfRot.lfFaceName, _T("Segoe UI"));
    lfRot.lfHeight = -12;
    lfRot.lfEscapement = 450;   // 45°
    lfRot.lfOrientation = 450;

    fontRot.CreateFontIndirect(&lfRot);
    CFont* oldFont = dc.SelectObject(&fontRot);

    for (int i = 0; i < n; ++i)
    {
        int x = LEFT + (int)((cumulative[i] / maxTime) * chartW);

        // tick
        dc.MoveTo(x, BOTTOM);
        dc.LineTo(x, BOTTOM + 5);

        double sec = m_entries[i].durationMs / 1000.0;

        CString lbl;
        lbl.Format(_T("%.0fs"), sec);

        dc.TextOut(x - 5, BOTTOM + 25, lbl);
    }

    dc.SelectObject(oldFont);

    // ── Axis labels ─────────────────────────────
    CFont fontLabel;
    fontLabel.CreatePointFont(85, _T("Segoe UI"));
    dc.SelectObject(&fontLabel);

    // Y-axis
    CFont fontVert;
    LOGFONT lf = {};
    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
    lf.lfHeight = -14;
    lf.lfEscapement = 900;
    lf.lfOrientation = 900;

    fontVert.CreateFontIndirect(&lf);
    dc.SelectObject(&fontVert);

    dc.TextOut(15, BOTTOM - chartH / 2 + 30, _T("Percent (%)"));

    // X-axis title
    dc.SelectObject(&fontLabel);
    dc.TextOut(LEFT + chartW / 2 - 70, BOTTOM + 70, _T("Step Duration (seconds)"));

    // Title
    CFont fontTitle;
    fontTitle.CreatePointFont(95, _T("Segoe UI"));
    dc.SelectObject(&fontTitle);

    dc.SetTextColor(RGB(220, 220, 255));

    CString title;
    title.Format(_T("SOH — Duration per Step [%s]"), m_testIDStr);

    CSize sz = dc.GetTextExtent(title);
    dc.TextOut(LEFT + (chartW - sz.cx) / 2, TOP - 22, title);

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

    case CDDS_ITEMPREPAINT:
        *pResult = CDRF_NOTIFYSUBITEMDRAW | CDRF_NOTIFYPOSTPAINT;
        break;

    case CDDS_ITEMPOSTPAINT:
    {
        int row = (int)pCD->nmcd.dwItemSpec;
        if (row < (int)m_groupEnd.size() && m_groupEnd[row])
        {
            // Draw a separator line at the bottom of this row
            CDC* pDC = CDC::FromHandle(pCD->nmcd.hdc);
            CRect rcItem;
            m_list.GetItemRect(row, &rcItem, LVIR_BOUNDS);

           /* CPen pen(PS_SOLID, 2, RGB(100, 160, 220));*/
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