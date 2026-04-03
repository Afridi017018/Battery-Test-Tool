#include "pch.h"
#include "CAutoProgressDlg.h"

IMPLEMENT_DYNAMIC(CAutoProgressDlg, CDialogEx)

CAutoProgressDlg::CAutoProgressDlg(CWnd* pParent)
    : CDialogEx(IDD_AUTO_PROGRESS_DLG, pParent)
{
}

void CAutoProgressDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_AUTO_PROGRESS_BAR, m_progressBar);
}

BOOL CAutoProgressDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    m_progressBar.SetRange(0, 100);
    m_progressBar.SetPos(0);
    return TRUE;
}

void CAutoProgressDlg::UpdateProgress(int totalPct, int phase, const CString& phaseMsg)
{
    m_progressBar.SetPos(totalPct);

    // Top label: overall percent
    CString overall;
    overall.Format(L"Overall Progress: %d%%  (8 minutes total)", totalPct);
    SetDlgItemText(IDC_AUTO_PROGRESS_LABEL, overall);

    // Bottom label: which phase
    CString phaseStr;
    if (phase == 1)
        phaseStr.Format(L"Phase 1 / 2 — CPU Load (3 min): %s", phaseMsg);
    else
        phaseStr.Format(L"Phase 2 / 2 — Discharge (5 min): %s", phaseMsg);

    SetDlgItemText(IDC_AUTO_PHASE_LABEL, phaseStr);
}

BEGIN_MESSAGE_MAP(CAutoProgressDlg, CDialogEx)
END_MESSAGE_MAP()