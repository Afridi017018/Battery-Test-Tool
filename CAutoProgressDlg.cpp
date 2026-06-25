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

    m_pBattDlg = dynamic_cast<CBatteryHelthDlg*>(GetParent());

    //CString dbg;
    //dbg.Format(L"longTest = %d", m_pBattDlg->longTest);
    //AfxMessageBox(dbg);

    if (m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
    {
        if (m_pBattDlg->longTest)
            SetWindowText(L"バッテリーテスト");
        else
            SetWindowText(L"バッテリーテスト");
    }
    else
    {
        if (m_pBattDlg->longTest)
            SetWindowText(L"Battery Test");
        else
            SetWindowText(L"Battery Test");
    }

    m_progressBar.SetRange(0, 100);
    m_progressBar.SetPos(0);


    return TRUE;
}

void CAutoProgressDlg::UpdateProgress(int totalPct, int phase, const CString& phaseMsg, bool longTest)
{

    m_progressBar.SetPos(totalPct);

    CString overall;

    if (m_pBattDlg &&
        m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
    {
        if (m_pBattDlg->longTest)
            overall.Format(L"全体の進捗: %d%% （合計 8 分）", totalPct);
        else
            overall.Format(L"全体の進捗: %d%% （合計 2 分）", totalPct);
    }
    else
    {
        if (m_pBattDlg && m_pBattDlg->longTest)
            overall.Format(L"Overall Progress: %d%% (8 minutes total)", totalPct);
        else
            overall.Format(L"Overall Progress: %d%% (2 minutes total)", totalPct);
    }

    SetDlgItemText(IDC_AUTO_PROGRESS_LABEL, overall);

    CString phaseStr;

    bool jp =
        (m_pBattDlg &&
            m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP);

    bool longT =
        (m_pBattDlg &&
            m_pBattDlg->longTest);

    if (phase == 1)
    {
        if (jp)
        {
            if (longT)
                phaseStr.Format(L"フェーズ 1 / 2 — CPU負荷テスト (3 分): %s",
                    (LPCWSTR)phaseMsg);
            else
                phaseStr.Format(L"フェーズ 1 / 2 — CPU負荷テスト (1 分): %s",
                    (LPCWSTR)phaseMsg);
        }
        else
        {
            if (longT)
                phaseStr.Format(L"Phase 1 / 2 — CPU Load (3 min): %s",
                    (LPCWSTR)phaseMsg);
            else
                phaseStr.Format(L"Phase 1 / 2 — CPU Load (1 min): %s",
                    (LPCWSTR)phaseMsg);
        }
    }
    else
    {
        if (jp)
        {
            if (longT)
                phaseStr.Format(L"フェーズ 2 / 2 — 放電テスト (5 分): %s",
                    (LPCWSTR)phaseMsg);
            else
                phaseStr.Format(L"フェーズ 2 / 2 — 放電テスト (1 分): %s",
                    (LPCWSTR)phaseMsg);
        }
        else
        {
            if (longT)
                phaseStr.Format(L"Phase 2 / 2 — Discharge (5 min): %s",
                    (LPCWSTR)phaseMsg);
            else
                phaseStr.Format(L"Phase 2 / 2 — Discharge (1 min): %s",
                    (LPCWSTR)phaseMsg);
        }
    }

    SetDlgItemText(IDC_AUTO_PHASE_LABEL, phaseStr);
}

// ---------------------------------------------------------------------------
// FIX 1 — X button pressed
// Tell the parent dialog to cancel the auto test properly, then hide ourselves.
// We do NOT call DestroyWindow() here; the parent owns our lifetime.
// ---------------------------------------------------------------------------
void CAutoProgressDlg::OnClose()
{
    m_pBattDlg->longTest = false;

    CBatteryHelthDlg* pParent =
        dynamic_cast<CBatteryHelthDlg*>(GetParent());

    if (pParent)
        pParent->CancelAutoTest();   // sets m_autoCancelled, stops threads/timers
    else
        ShowWindow(SW_HIDE);         // fallback: just hide
    // Do NOT call CDialogEx::OnClose() — that would destroy the dialog
    // prematurely and leave the parent holding a dangling pointer.

    Invalidate();
}

// ---------------------------------------------------------------------------
// FIX 1 — Escape key
// Same treatment as the X button.
// ---------------------------------------------------------------------------
void CAutoProgressDlg::OnCancel()
{
    m_pBattDlg->longTest = false;

    CBatteryHelthDlg* pParent =
        dynamic_cast<CBatteryHelthDlg*>(GetParent());

    if (pParent)
        pParent->CancelAutoTest();
    else
        ShowWindow(SW_HIDE);
    // Do NOT call CDialogEx::OnCancel() — same reason as above.

    Invalidate();
}

BEGIN_MESSAGE_MAP(CAutoProgressDlg, CDialogEx)
    ON_WM_CLOSE()
END_MESSAGE_MAP()