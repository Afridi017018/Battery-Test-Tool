#pragma once
#include <afxcmn.h>
#include <afxwin.h>
#include <vector>
#include <string>
#include "resource.h"

struct SOHEntry
{
    CString time;
    int percent;
    ULONGLONG durationMs;
    CString durationHMS;
};

struct SOHFullEntry
{
    CString testID;
    CString startTime;
    CString time;
    int percent;
    ULONGLONG durationMs;
    CString durationHMS;
};

class CSOHResultDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CSOHResultDlg)
public:
    CSOHResultDlg(CWnd* pParent = nullptr);
    virtual ~CSOHResultDlg();
    enum { IDD = IDD_RESULT };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    DECLARE_MESSAGE_MAP()

private:
    void LoadLogFile();
    void ParseLine(const std::string& line);
    void DisplayData();
    CString GetExeFolder();
    afx_msg void OnToggleView();
    afx_msg void OnShowChart();
    afx_msg void OnPaint();

private:
    // Latest view
    std::vector<SOHEntry>     m_entries;
    // All logs view
    std::vector<SOHFullEntry> m_allEntries;

    // UI
    CListCtrl  m_list;
    CStatic    m_lblSummary;
    CButton    m_btnToggle;
    CButton    m_btnChart;

    // State
    bool m_showAll;
    bool m_showChart;

    // Info
    CString   m_testIDStr;
    CString   m_startTimeStr;
    ULONGLONG m_totalTimeMs;
};