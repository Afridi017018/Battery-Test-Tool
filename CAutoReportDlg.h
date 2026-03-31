#pragma once
#include <vector>
#include <string>
#include <afxwin.h>
#include <afxdlgs.h>
#include"resource.h"

struct ReportRow {
    CString period;
    double val1 = 0, val2 = 0, val3 = 0, val4 = 0;
};

class CAutoReportDlg : public CDialogEx
{
public:
    CAutoReportDlg(CWnd* pParent = nullptr);

    enum { IDD = IDD_AUTO_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    DECLARE_MESSAGE_MAP()

private:
    std::vector<ReportRow> m_usageRows, m_capacityRows, m_estimateRows;
    int m_scrollPos = 0, m_scrollMax = 0;
    CFont m_fontSection, m_fontColLabel, m_fontRow;
    CPen m_penDivider;
    CBrush m_brushStripe, m_brushBg;

    void LoadData();
    void CreateGDIObjects();
    void UpdateScrollBar();
    int DrawSection(CDC& dc, const CString& title, const std::vector<ReportRow>& rows,
        const std::vector<CString>& cols, int startY, COLORREF themeClr);

    bool FetchAllBatteryData();
    void ParseUsageTable(const std::wstring& html);
    void ParseCapacityTable(const std::wstring& html);
    void ParseEstimatesTable(const std::wstring& html);

    CString ReadFile(const CString& path);
    CString StripHtml(const CString& input);
    std::vector<CString> ExtractAllHms(const CString& text);
    double HmsToHours(const CString& hms);
    double ParseMwh(CString text);

};