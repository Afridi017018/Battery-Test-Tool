#pragma once
#include "afxdialogex.h"

class CBatteryHelthDlg;

class CDischargeProgressDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CDischargeProgressDlg)

public:
    CDischargeProgressDlg(CWnd* pParent = nullptr);
    virtual ~CDischargeProgressDlg();

    enum { IDD = IDD_DISCHARGE_PROGRESS_DLG };

    void UpdateProgress(int percent, const CString& text);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnBnClickedBtnStop();
protected:
    afx_msg void OnClose();
};
