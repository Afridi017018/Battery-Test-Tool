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
    DDX_Control(pDX, IDC_PROGRESS_DISCHARGE, m_Discharge_ProgressDialog);
}

BEGIN_MESSAGE_MAP(CDischargeProgressDlg, CDialogEx)
    ON_WM_CLOSE()
END_MESSAGE_MAP()


BOOL CDischargeProgressDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // CPU progress
    m_Discharge_ProgressDialog.SetRange(0, 100);
    m_Discharge_ProgressDialog.SetPos(0);
    m_Discharge_ProgressDialog.SetStep(1);
    m_Discharge_ProgressDialog.ModifyStyle(0, PBS_SMOOTH);
    ::SetWindowTheme(m_Discharge_ProgressDialog.GetSafeHwnd(), L"", L"");
    m_Discharge_ProgressDialog.SetBarColor(RGB(0, 122, 204));
    m_Discharge_ProgressDialog.SetBkColor(RGB(220, 220, 220));


    return true;

}


void CDischargeProgressDlg::UpdateProgress(int percent, const CString& text)
{
    SetDlgItemText(IDC_TXT_PROGRESS, text);

    CProgressCtrl* pBar = (CProgressCtrl*)GetDlgItem(IDC_PROGRESS_DISCHARGE);
    if (pBar)
        pBar->SetPos(percent);
}

//void CDischargeProgressDlg::OnBnClickedBtnStop()
//{
//    CBatteryHelthDlg* pParent =
//        dynamic_cast<CBatteryHelthDlg*>(GetParent());
//
//    if (pParent)
//        pParent->StopDischargeTest();
//}

void CDischargeProgressDlg::OnClose()
{
    int result = MessageBox(
        L"Stop discharge test?\n\nThe running discharge measurement will be terminated.",
        L"Battery Health Monitor",
        MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);

    if (result == IDYES)
    {
        CBatteryHelthDlg* pParent =
            dynamic_cast<CBatteryHelthDlg*>(GetParent());

        if (pParent)
            pParent->StopDischargeTest();

        CDialogEx::OnClose();
    }
}