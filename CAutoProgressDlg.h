#pragma once
#include "afxdialogex.h"
#include "resource.h"

class CAutoProgressDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CAutoProgressDlg)

public:
    CAutoProgressDlg(CWnd* pParent = nullptr);
    virtual ~CAutoProgressDlg() {}

    enum { IDD = IDD_AUTO_PROGRESS_DLG };

    void UpdateProgress(int totalPct, int phase, const CString& phaseMsg);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    DECLARE_MESSAGE_MAP()

private:
    CProgressCtrl m_progressBar;
};