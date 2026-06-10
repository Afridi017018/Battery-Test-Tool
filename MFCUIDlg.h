#pragma once
#include "resource.h"
#include <afxwin.h>
#include <vector>


// ─── Base design dimensions ───────────────────────────────────────
#define BASE_W  500
#define BASE_H  900

// ─── Color palette ────────────────────────────────────────────────
#define CLR_BG        RGB(235, 238, 245)
#define CLR_CARD      RGB(255, 255, 255)
#define CLR_BORDER    RGB(220, 225, 235)
#define CLR_TITLE     RGB(22,  50,  92)
#define CLR_SUBTITLE  RGB(120, 130, 150)
#define CLR_GREEN     RGB(40,  180,  80)
#define CLR_YELLOW    RGB(255, 200,  30)
#define CLR_BLUE      RGB(30,  120, 220)
#define CLR_RED       RGB(220,  60,  60)
#define CLR_DARK_TEXT RGB(30,   40,  60)
#define CLR_MID_TEXT  RGB(90,  100, 120)

class CMFCUIDlg : public CDialogEx
{
public:
    CMFCUIDlg(CWnd* pParent = nullptr);
    enum { IDD = IDD_MFCUI };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnSize(UINT nType, int cx, int cy);

    DECLARE_MESSAGE_MAP()

private:
    // Drawing sections
    void DrawBackground(CDC* pDC, CRect rc);
    void DrawHeader(CDC* pDC, CRect rc);
    void DrawBatteryOverview(CDC* pDC, CRect rc);
    void DrawChargeRate(CDC* pDC, CRect rc);
    void DrawBasicBatteryInfo(CDC* pDC, CRect rc);
    void DrawQuickActions(CDC* pDC, CRect rc);
    void DrawQAButton(CDC* pDC, CRect rcBtn, const CString& text,
        COLORREF bgColor, COLORREF textColor,
        COLORREF borderColor, int radius);

    // Utilities
    void DrawRoundRect(CDC* pDC, CRect rc, int radius, COLORREF bg, COLORREF border);
    void DrawTextEx(CDC* pDC, const CString& text, CRect rc, COLORREF color,
        int fontSize, bool bold, UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    void DrawCircularGauge(CDC* pDC, CPoint center, int radius, float percent);

    // Scale helpers
    int SW(int baseWidth, int clientWidth);   // scale X dimension
    int SH(int baseHeight, int clientHeight); // scale Y dimension
    int SF(int baseFontSize, int clientWidth);// scale font size

    // Hit-test rects for click detection
    CRect m_rcBtnAutoTest;
    CRect m_rcBtnViewLog;
    CRect m_rcBtnLanguage;

    bool m_bJapanese = false;

    void DrawAdvancedInfo(CDC* pDC, CRect rc);

    // Advanced info state
    bool   m_bAdvancedExpanded = false;

    // Hit rects
    CRect  m_rcBtnAdvanced;      // the header click area (whole header row)
    CRect  m_rcAdvBtn[6];        // the 6 expandable buttons



};
