// ABIDlg.h
// Full-screen "Advanced Info" dialog (resource IDD_ABI).
// Opened from CMFCUIDlg's Quick Actions "Advanced Info" button.
// CMFCUIDlg is hidden while this is shown; the Back button
// on this dialog closes it and CMFCUIDlg::OnLButtonUp un-hides itself.
#pragma once
#include "resource.h"
#include <afxwin.h>

class CMFCUIDlg;

class CABIDlg : public CDialogEx
{
public:
    // pOwner: the CMFCUIDlg that launched this dialog. Used only to
    // read its language setting (L()) and, optionally, to forward
    // row clicks back to it. Not required to be non-null.
    explicit CABIDlg(CMFCUIDlg* pOwner = nullptr, CWnd* pParentWnd = nullptr);

    enum { IDD = IDD_ABI };

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

    // ── Small utility drawing (self-contained copies, same style
    //    as CMFCUIDlg's, so this dialog doesn't depend on its
    //    parent's private helpers) ─────────────────────────────────
    void DrawRoundRect(CDC* pDC, CRect rc, int radius,
        COLORREF bg, COLORREF border);
    void DrawTextEx(CDC* pDC, const CString& text, CRect rc,
        COLORREF color, int fontSize, bool bold,
        UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CString L(const CString& en, const CString& jp);

    // Called when a row is tapped. TODO: wire each case to the real
    // test-trigger logic (whatever CMFCUIDlg::OnLButtonUp currently
    // does for m_rcAdvBtn[i]).
    void OnRowClicked(int index);

    CMFCUIDlg* m_pOwner = nullptr;

    CRect m_rcBack;          // back-arrow hit target (top-left of header)
    CRect m_rcRow[7];        // 7 advanced-info rows

    int m_clientWidth = 500;
    int m_clientHeight = 900;
};