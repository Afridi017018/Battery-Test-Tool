#include "pch.h"
#include "CAutoProgressDlg.h"
#include "BatteryHelthDlg.h"   // needed so we can call CancelAutoTest() on the parent

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
        phaseStr.Format(L"Phase 1 / 2 — CPU Load (3 min): %s", (LPCWSTR)phaseMsg);
    else
        phaseStr.Format(L"Phase 2 / 2 — Discharge (5 min): %s", (LPCWSTR)phaseMsg);

    SetDlgItemText(IDC_AUTO_PHASE_LABEL, phaseStr);
}

// ---------------------------------------------------------------------------
// FIX 1 — X button pressed
// Tell the parent dialog to cancel the auto test properly, then hide ourselves.
// We do NOT call DestroyWindow() here; the parent owns our lifetime.
// ---------------------------------------------------------------------------
void CAutoProgressDlg::OnClose()
{
    CBatteryHelthDlg* pParent =
        dynamic_cast<CBatteryHelthDlg*>(GetParent());

    if (pParent)
        pParent->CancelAutoTest();   // sets m_autoCancelled, stops threads/timers
    else
        ShowWindow(SW_HIDE);         // fallback: just hide
    // Do NOT call CDialogEx::OnClose() — that would destroy the dialog
    // prematurely and leave the parent holding a dangling pointer.
}

// ---------------------------------------------------------------------------
// FIX 1 — Escape key
// Same treatment as the X button.
// ---------------------------------------------------------------------------
void CAutoProgressDlg::OnCancel()
{
    CBatteryHelthDlg* pParent =
        dynamic_cast<CBatteryHelthDlg*>(GetParent());

    if (pParent)
        pParent->CancelAutoTest();
    else
        ShowWindow(SW_HIDE);
    // Do NOT call CDialogEx::OnCancel() — same reason as above.
}

BEGIN_MESSAGE_MAP(CAutoProgressDlg, CDialogEx)
    ON_WM_CLOSE()
END_MESSAGE_MAP()