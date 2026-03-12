#include "pch.h"
#include "BatteryHelth.h"
#include "CCPUProgressDlg.h"
#include "BatteryHelthDlg.h"

IMPLEMENT_DYNAMIC(CCPUProgressDlg, CDialogEx)

CCPUProgressDlg::CCPUProgressDlg(CWnd* pParent)
    : CDialogEx(IDD_CPULOAD_PROGRESS_DLG, pParent)
{
}

CCPUProgressDlg::~CCPUProgressDlg()
{
}

void CCPUProgressDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CCPUProgressDlg, CDialogEx)

    ON_BN_CLICKED(IDC_BTN_CPU_STOP, &CCPUProgressDlg::OnBnClickedBtnCpuStop)
    ON_WM_CLOSE()
END_MESSAGE_MAP()

void CCPUProgressDlg::UpdateProgress(int percent, const CString& text)
{
    SetDlgItemText(IDC_TXT_CPU, text);

    CProgressCtrl* pBar =
        (CProgressCtrl*)GetDlgItem(IDC_PROGRESS_CPU);

    if (pBar)
        pBar->SetPos(percent);
}


void CCPUProgressDlg::OnBnClickedBtnCpuStop()
{
    CBatteryHelthDlg* pParent =
        dynamic_cast<CBatteryHelthDlg*>(GetParent());

    if (pParent)
        pParent->StopCpuLoadTest();
}


void CCPUProgressDlg::OnClose()
{
    CBatteryHelthDlg* pParent =
        dynamic_cast<CBatteryHelthDlg*>(GetParent());

    if (pParent)
        pParent->StopCpuLoadTest();
}