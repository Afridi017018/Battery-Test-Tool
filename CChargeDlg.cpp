#include "pch.h"
#include "BatteryHelth.h"
#include "CChargeDlg.h"
#include "afxdialogex.h"

IMPLEMENT_DYNAMIC(CChargeDlg, CDialogEx)

CChargeDlg::CChargeDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_CHARGE_DLG, pParent)
{
}

CChargeDlg::~CChargeDlg()
{
}

void CChargeDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_HISTORY, m_editHistory);
}

BEGIN_MESSAGE_MAP(CChargeDlg, CDialogEx)
    ON_EN_SETFOCUS(IDC_EDIT_HISTORY, &CChargeDlg::OnSetFocusHistory)
END_MESSAGE_MAP()

void CChargeDlg::OnSetFocusHistory()
{
    ::HideCaret(m_editHistory.GetSafeHwnd());
}

BOOL CChargeDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    m_editHistory.SetWindowTextW(m_historyText);

    m_editHistory.SetWindowTextW(m_historyText);

    ::HideCaret(m_editHistory.GetSafeHwnd());   // hide caret


    return TRUE;
}