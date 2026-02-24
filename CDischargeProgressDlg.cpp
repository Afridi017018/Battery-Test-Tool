#include "pch.h"
#include "BatteryHelth.h"
#include "CDischargeProgressDlg.h"
#include "BatteryHelthDlg.h"

IMPLEMENT_DYNAMIC(CDischargeProgressDlg, CDialogEx)

CDischargeProgressDlg::CDischargeProgressDlg(CWnd* pParent)
    : CDialogEx(IDD_DISCHARGE_PROGRESS_DLG, pParent)
{
}

CDischargeProgressDlg::~CDischargeProgressDlg()
{
}

void CDischargeProgressDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CDischargeProgressDlg, CDialogEx)
    ON_BN_CLICKED(IDC_BTN_STOP, &CDischargeProgressDlg::OnBnClickedBtnStop)
END_MESSAGE_MAP()



void CDischargeProgressDlg::UpdateProgress(int percent, const CString& text)
{
    SetDlgItemText(IDC_TXT_PROGRESS, text);

    CProgressCtrl* pBar = (CProgressCtrl*)GetDlgItem(IDC_PROGRESS_DISCHARGE);
    if (pBar)
        pBar->SetPos(percent);
}

void CDischargeProgressDlg::OnBnClickedBtnStop()
{
    CBatteryHelthDlg* pParent =
        dynamic_cast<CBatteryHelthDlg*>(GetParent());

    if (pParent)
        pParent->StopDischargeTest();
}
