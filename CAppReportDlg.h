//#pragma once
//#include <afxdialogex.h>
//#include "resource.h"
//
//class CAppReportDlg : public CDialogEx
//{
//    DECLARE_DYNAMIC(CAppReportDlg)
//public:
//    CAppReportDlg(const CString& report, const CString& title, CWnd* pParent = nullptr);
//
//    enum { IDD = IDD_BGAPP_REPORT };   
//
//protected:
//    virtual void DoDataExchange(CDataExchange* pDX);
//    virtual BOOL OnInitDialog();
//    DECLARE_MESSAGE_MAP()
//
//private:
//    CString m_report;
//    CString m_title;
//    CEdit   m_editReport;
//
//public:
//    afx_msg void OnSetFocusEdit();
//    BOOL m_firstFocus = TRUE;
//};


#pragma once
#include <afxdialogex.h>
#include "resource.h"

class CAppReportDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CAppReportDlg)
public:
    CAppReportDlg(const CString& report, const CString& title, CWnd* pParent = nullptr);
    enum { IDD = IDD_BGAPP_REPORT };
protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    DECLARE_MESSAGE_MAP()
private:
    CString m_report;
    CString m_title;
    CEdit   m_editReport;
public:
    afx_msg void OnSetFocusEdit();
    BOOL m_firstFocus = TRUE;
};