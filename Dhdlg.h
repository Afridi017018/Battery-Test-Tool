// DHDlg.h
// Full-screen "Data and History" dialog (resource IDD_DH).
// Opened from CMFCUIDlg's Quick Actions "Data and History" button.
// CMFCUIDlg is hidden while this is shown; the Back button
// closes it and CMFCUIDlg shows itself again.
#pragma once
#include "resource.h"
#include <afxwin.h>

class CMFCUIDlg;

class CDHDlg : public CDialogEx
{
public:
    explicit CDHDlg(CMFCUIDlg* pOwner = nullptr, CWnd* pParentWnd = nullptr);

    enum { IDD = IDD_DH };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnSize(UINT nType, int cx, int cy);

    DECLARE_MESSAGE_MAP()

private:
    // ── Drawing ──────────────────────────────────────────────────
    void DrawBackground(CDC* pDC, CRect rc);
    void DrawHeader(CDC* pDC, CRect rc);
    void DrawRows(CDC* pDC, CRect rc);
    void DrawRowButton(CDC* pDC, CRect rc, const CString& label,
        const CString& iconText, COLORREF iconBg, int fontSize);

    // ── Utility (self-contained, same style as ABIDlg / CMFCUIDlg) ─
    void DrawRoundRect(CDC* pDC, CRect rc, int radius,
        COLORREF bg, COLORREF border);
    void DrawTextEx(CDC* pDC, const CString& text, CRect rc,
        COLORREF color, int fontSize, bool bold,
        UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CString L(const CString& en, const CString& jp);

    // Dispatches to the real CBatteryHelthDlg handlers
    void OnRowClicked(int index);

    CMFCUIDlg* m_pOwner = nullptr;

    CRect m_rcBack;       // back-arrow hit target
    CRect m_rcRow[7];     // 7 data-history rows

    int m_clientWidth = 500;
    int m_clientHeight = 900;
};