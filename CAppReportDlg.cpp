#include "pch.h"
#include "CAppReportDlg.h"
#include "resource.h"

IMPLEMENT_DYNAMIC(CAppReportDlg, CDialogEx)

CAppReportDlg::CAppReportDlg(const CString& report, const CString& title, CWnd* pParent)
    : CDialogEx(CAppReportDlg::IDD, pParent), m_report(report), m_title(title)
{
}

void CAppReportDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_EDIT_REPORT, m_editReport);
}





BOOL CAppReportDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetWindowText(m_title);
    m_editReport.SetWindowText(m_report);
    m_editReport.SetSel(-1, 0);
    m_editReport.SetSel(0, 0);

    static CFont font;
    if (!font.m_hObject)
        font.CreateFont(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, L"Consolas");
    m_editReport.SetFont(&font);

    ::HideCaret(m_editReport.GetSafeHwnd());

    return TRUE;
}

void CAppReportDlg::OnSetFocusEdit()
{
    if (m_firstFocus)
    {
        m_firstFocus = FALSE;
        m_editReport.SetSel(0, 0);
    }

    ::HideCaret(m_editReport.GetSafeHwnd());  
}


BEGIN_MESSAGE_MAP(CAppReportDlg, CDialogEx)
    ON_EN_SETFOCUS(IDC_EDIT_REPORT, &CAppReportDlg::OnSetFocusEdit)
END_MESSAGE_MAP()
