#include "pch.h"
#include "CSOHResultDlg.h"
#include <fstream>
#include <sstream>

IMPLEMENT_DYNAMIC(CSOHResultDlg, CDialogEx)

CSOHResultDlg::CSOHResultDlg(CWnd* pParent)
    : CDialogEx(IDD_RESULT, pParent)
    , m_totalTimeMs(0)
    , m_showAll(false)
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

    // List Control
    m_list.Create(WS_CHILD | WS_VISIBLE | LVS_REPORT,
        CRect(10, 50, rc.Width() - 10, rc.Height() - 80),
        this, 2001);

    m_list.InsertColumn(0, _T("Time"), LVCFMT_LEFT, 100);
    m_list.InsertColumn(1, _T("Percent"), LVCFMT_CENTER, 80);
    m_list.InsertColumn(2, _T("Duration(ms)"), LVCFMT_CENTER, 120);
    m_list.InsertColumn(3, _T("Duration(h:m:s)"), LVCFMT_CENTER, 140);

    // Summary Label
    m_lblSummary.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_CENTER,
        CRect(10, rc.Height() - 60, rc.Width() - 10, rc.Height() - 10),
        this);

    LoadLogFile();
    DisplayData();

    return TRUE;
}

CString CSOHResultDlg::GetExeFolder()
{
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);

    std::string exePath(path);
    size_t pos = exePath.find_last_of("\\/");
    std::string folder = exePath.substr(0, pos);

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
    std::vector<SOHEntry> filtered;

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

    int index = 0;
    for (const auto& e : m_entries)
    {
        m_list.InsertItem(index, e.time);

        CString pct;
        pct.Format(_T("%d%%"), e.percent);
        m_list.SetItemText(index, 1, pct);

        CString durMs;
        durMs.Format(_T("%llu"), e.durationMs);
        m_list.SetItemText(index, 2, durMs);

        m_list.SetItemText(index, 3, e.durationHMS);

        index++;
    }

    ULONGLONG totalSec = m_totalTimeMs / 1000;

    int hrs = (int)(totalSec / 3600);
    int mins = (int)((totalSec % 3600) / 60);
    int secs = (int)(totalSec % 60);

    CString summary;
    summary.Format(
        _T("Test ID: %s | Start: %s | Total: %02d:%02d:%02d"),
        m_testIDStr, m_startTimeStr, hrs, mins, secs
    );

    m_lblSummary.SetWindowText(summary);
}

void CSOHResultDlg::OnToggleView()
{
    m_showAll = !m_showAll;

    m_list.DeleteAllItems();
    while (m_list.DeleteColumn(0));

    if (m_showAll)
    {
        m_btnToggle.SetWindowText(_T("Show Latest"));

        m_list.InsertColumn(0, _T("Test ID"), LVCFMT_LEFT, 100);
        m_list.InsertColumn(1, _T("Start"), LVCFMT_LEFT, 140);
        m_list.InsertColumn(2, _T("Time"), LVCFMT_LEFT, 80);
        m_list.InsertColumn(3, _T("Percent"), LVCFMT_CENTER, 80);
        m_list.InsertColumn(4, _T("Duration(ms)"), LVCFMT_CENTER, 120);
        m_list.InsertColumn(5, _T("Duration(h:m:s)"), LVCFMT_CENTER, 140);

        CString lastTestID;
        int index = 0;

        for (const auto& e : m_allEntries)
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

       
        ULONGLONG totalMs = 0;

        for (const auto& e : m_allEntries)
        {
            totalMs += e.durationMs;
        }

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

        DisplayData(); // back to original summary
    }
}