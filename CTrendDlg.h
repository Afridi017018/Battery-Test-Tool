#pragma once
#include "resource.h"
#include <vector>
#include <gdiplus.h>

class CTrendDlg : public CDialogEx
{
public:
    CTrendDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_TREND_DIALOG };
#endif

    // Optional: pass custom data; otherwise HTML loader / defaults are used
    void SetData(const std::vector<CString>& periods,
        const std::vector<float>& fullHours,
        const std::vector<float>& designHours);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

    ULONG_PTR m_gdiplusToken{};
    std::vector<CString> m_periods;
    std::vector<float>   m_full, m_design;

    // === NEW: data sourcing ===
    void EnsureDefaultData();                   // fallback hardcoded dataset
    bool LoadFromBatteryReport(const CString&); // parse battery-report.html

    // === drawing ===
    DECLARE_MESSAGE_MAP()
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg void OnDestroy();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
public:
    afx_msg void OnBnClickedButton2();
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	bool eng_lang;

};



