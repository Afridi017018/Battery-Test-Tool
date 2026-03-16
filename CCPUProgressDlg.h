#pragma once
#include "afxdialogex.h"

class CBatteryHelthDlg;

class CCPUProgressDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CCPUProgressDlg)

public:
    CCPUProgressDlg(CWnd* pParent = nullptr);
    virtual ~CCPUProgressDlg();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_CPULOAD_PROGRESS_DLG };
#endif

    void UpdateProgress(int percent, const CString& text);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

    DECLARE_MESSAGE_MAP()


public:
    afx_msg void OnBnClickedBtnCpuStop();
    afx_msg void OnClose();
    CProgressCtrl m_CPU_ProgressDialog;
    BOOL OnInitDialog();
};
