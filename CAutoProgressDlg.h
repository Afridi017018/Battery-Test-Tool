#pragma once
#include "afxdialogex.h"
#include "resource.h"

class CBatteryHelthDlg;
class CAutoProgressDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CAutoProgressDlg)

public:
    CAutoProgressDlg(CWnd* pParent = nullptr);
    virtual ~CAutoProgressDlg() {}

    enum { IDD = IDD_AUTO_PROGRESS_DLG };

    void UpdateProgress(int totalPct, int phase, const CString& phaseMsg, bool longTest);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    // FIX 1: Intercept the X (close) button so it cancels the test
    //        instead of just destroying the dialog mid-run.
    afx_msg void OnClose();

    // FIX 1: Also intercept Escape key (which calls OnCancel by default).
    virtual void OnCancel();

    DECLARE_MESSAGE_MAP()

public:
    CProgressCtrl m_progressBar;
    CBatteryHelthDlg* m_pBattDlg = nullptr;
};