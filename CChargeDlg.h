#pragma once
#include "afxdialogex.h"

class CChargeDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CChargeDlg)

public:
    CChargeDlg(CWnd* pParent = nullptr);
    virtual ~CChargeDlg();

    enum { IDD = IDD_CHARGE_DLG };

    CString m_historyText;   

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()

public:
    CEdit m_editHistory;
};