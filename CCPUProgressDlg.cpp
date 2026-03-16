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
    DDX_Control(pDX, IDC_PROGRESS_CPU, m_CPU_ProgressDialog);
}

BEGIN_MESSAGE_MAP(CCPUProgressDlg, CDialogEx)

    ON_BN_CLICKED(IDC_BTN_CPU_STOP, &CCPUProgressDlg::OnBnClickedBtnCpuStop)
    ON_WM_CLOSE()
END_MESSAGE_MAP()


BOOL CCPUProgressDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // CPU progress
    m_CPU_ProgressDialog.SetRange(0, 100);
    m_CPU_ProgressDialog.SetPos(0);
    m_CPU_ProgressDialog.SetStep(1);
    m_CPU_ProgressDialog.ModifyStyle(0, PBS_SMOOTH);
    ::SetWindowTheme(m_CPU_ProgressDialog.GetSafeHwnd(), L"", L"");
    m_CPU_ProgressDialog.SetBarColor(RGB(0, 122, 204));
    m_CPU_ProgressDialog.SetBkColor(RGB(220, 220, 220));


    return true;

}

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
    int result = MessageBox(
        L"Stop CPU load test?\n\nThe running CPU stress test will be terminated.",
        L"Battery Health Monitor",
        MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2);

    if (result == IDYES)
    {
        CBatteryHelthDlg* pParent =
            dynamic_cast<CBatteryHelthDlg*>(GetParent());

        if (pParent)
            pParent->StopCpuLoadTest();

        CDialogEx::OnClose();
    }
}