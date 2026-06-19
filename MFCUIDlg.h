#pragma once
#include "resource.h"
#include <afxwin.h>
#include <limits>
#include "CRateInfoDlg.h"
#include <vector>


class CBatteryHelthDlg;
class CABIDlg;          // new full-screen Advanced Info dialog (IDD_ABI)


// ─── Base design dimensions ───────────────────────────────────────
constexpr int BASE_W = 500;
constexpr int BASE_H = 900;

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
    /* void SetBatteryDlg(CBatteryHelthDlg* pDlg) { m_pBattDlg = pDlg; }*/
    enum { IDD = IDD_MFCUI };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnSize(UINT nType, int cx, int cy);

    DECLARE_MESSAGE_MAP()

private:
    // ── Drawing sections ─────────────────────────────────────────
    void DrawBackground(CDC* pDC, CRect rc);
    void DrawHeader(CDC* pDC, CRect rc);
    void DrawBatteryOverview(CDC* pDC, CRect rc);
    void DrawChargeRate(CDC* pDC, CRect rc);
    void DrawBasicBatteryInfo(CDC* pDC, CRect rc);
    void DrawQuickActions(CDC* pDC, CRect rc);
    void DrawAdvancedInfo(CDC* pDC, CRect rc);

    // ── Utility drawing ──────────────────────────────────────────
    void DrawQAButton(CDC* pDC, CRect rcBtn, const CString& text,
        COLORREF bgColor, COLORREF textColor,
        COLORREF borderColor, int radius);
    void DrawRoundRect(CDC* pDC, CRect rc, int radius,
        COLORREF bg, COLORREF border);
    void DrawTextEx(CDC* pDC, const CString& text, CRect rc,
        COLORREF color, int fontSize, bool bold,
        UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    /*void DrawCircularGauge(CDC* pDC, CPoint center,
        int radius, float percent);*/

        /*void DrawCircularGauge(CDC* pDC, CPoint center,
            int radius, const CString& percentText);*/

            /* void DrawCircularGauge(CDC* pDC, CPoint center,
                 int radius, const CString& percentText, bool isCharging);*/

    void DrawCircularGauge(
        CDC* pDC,
        CPoint center,
        int radius,
        const CString& percentText,
        bool isCharging,
        bool isHealth);

    // ── Scale helpers ─────────────────────────────────────────────
    int SW(int baseWidth, int clientWidth);
    int SH(int baseHeight, int clientHeight);
    int SF(int baseFontSize, int clientWidth);

    // ── Scroll helpers ────────────────────────────────────────────
    void   ClampScroll();
    CPoint ContentPoint(CPoint screenPt);

    // ── Quick Actions hit rects ───────────────────────────────────
    CRect m_rcBtnAutoTest;
    CRect m_rcBtnViewLog;
    CRect m_rcBtnLanguage;
    CRect m_rcBtnAdvancedQA;   // "Advanced Info" button inside Quick Actions
    // (was wrongly sharing m_rcBtnAdvanced below)

// ── Advanced Info hit rects ───────────────────────────────────
    CRect m_rcBtnAdvanced;
    CRect m_rcAdvBtn[7];

    // ── State ─────────────────────────────────────────────────────
    bool m_bJapanese = false;
    bool m_bAdvancedExpanded = false;

    // ── Client size ───────────────────────────────────────────────
    int m_clientWidth = BASE_W;
    int m_clientHeight = BASE_H;

    // ── Scroll state ──────────────────────────────────────────────
    int  m_scrollY = 0;
    int  m_totalHeight = 900;
    bool m_bDragging = false;
    int  m_dragStartY = 0;
    int  m_dragStartScroll = 0;


    void DrawDataHistory(CDC* pDC, CRect rc);

    bool  m_bDataHistoryExpanded = false;
    CRect m_rcBtnDataHistory;
    CRect m_rcDataHistBtn[7];

public:
    void SetBatteryDlg(CBatteryHelthDlg* p) { m_pBattDlg = p; }
    CBatteryHelthDlg* m_pBattDlg = nullptr;

public:
    bool m_bDialogBusy = false;
    void ClearDialogBusy() { m_bDialogBusy = false; }

    afx_msg void OnTimer(UINT_PTR nIDEvent);

public:

    BatteryTimeEstimator m_estimator;
    BatteryTimeInfo      m_last;

    static constexpr int kWindowSeconds = 120;
    static constexpr float kMissing =
        std::numeric_limits<float>::quiet_NaN();

    std::vector<float> m_samplesChargeW;
    std::vector<float> m_samplesDischargeW;

    int  m_cursor = 0;
    bool m_filled = false;

    void UpdateChargeRate();


    CString L(const CString& en, const CString& jp);
};