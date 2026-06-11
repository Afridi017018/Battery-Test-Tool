#include "pch.h"
#include "framework.h"
#include "MFCUIDlg.h"
#include <vector>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BASE_W  500
#define BASE_H  900

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

BEGIN_MESSAGE_MAP(CMFCUIDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONUP()
    ON_WM_LBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSEWHEEL()
    ON_WM_SIZE()
END_MESSAGE_MAP()

CMFCUIDlg::CMFCUIDlg(CWnd* pParent)
    : CDialogEx(IDD_MFCUI, pParent) {
}

void CMFCUIDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BOOL CMFCUIDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    SetWindowText(_T("Battery Health & Monitor"));
    ModifyStyle(0, WS_THICKFRAME | WS_MAXIMIZEBOX);
    SetWindowPos(nullptr, 100, 50, BASE_W, BASE_H, SWP_NOZORDER);
    return TRUE;
}

void CMFCUIDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);

    m_clientWidth = cx;
    m_clientHeight = cy;

    ClampScroll();
    Invalidate();
}

BOOL CMFCUIDlg::OnEraseBkgnd(CDC* pDC)
{
    return TRUE;
}

// ─── Scroll helpers ───────────────────────────────────────────────
void CMFCUIDlg::ClampScroll()
{
    CRect rc;
    GetClientRect(&rc);
    int viewH = rc.Height();
    int maxScroll = max(0, m_totalHeight - viewH);
    if (m_scrollY < 0)         m_scrollY = 0;
    if (m_scrollY > maxScroll) m_scrollY = maxScroll;
}

// Translates a point from screen space into content space
CPoint CMFCUIDlg::ContentPoint(CPoint screenPt)
{
    return CPoint(screenPt.x, screenPt.y + m_scrollY);
}

// ─── Mouse Wheel ─────────────────────────────────────────────────
BOOL CMFCUIDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    // zDelta: +120 = scroll up, -120 = scroll down
    int step = 60;  // pixels per notch
    m_scrollY -= (zDelta / 120) * step;
    ClampScroll();
    Invalidate();
    return TRUE;
}

// ─── Click drag scroll ────────────────────────────────────────────
void CMFCUIDlg::OnLButtonDown(UINT nFlags, CPoint point)
{
    m_bDragging = true;
    m_dragStartY = point.y;
    m_dragStartScroll = m_scrollY;
    SetCapture();
    CDialogEx::OnLButtonDown(nFlags, point);
}

void CMFCUIDlg::OnMouseMove(UINT nFlags, CPoint point)
{
    if (m_bDragging)
    {
        int delta = m_dragStartY - point.y;  // drag up = scroll down
        m_scrollY = m_dragStartScroll + delta;
        ClampScroll();
        Invalidate();
    }
    CDialogEx::OnMouseMove(nFlags, point);
}

// ─── Paint ────────────────────────────────────────────────────────
void CMFCUIDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect rcClient;
    GetClientRect(&rcClient);

    // ── Step 1: Calculate total content height ───────────────────
    // Base content height + extra rows if advanced is expanded
    double scaleY = (double)m_clientHeight / (double)BASE_H;
    if (scaleY < 0.4) scaleY = 0.4;
    int rowH = max(36, (int)(44.0 * scaleY));

    int extraAdvanced = m_bAdvancedExpanded ? rowH * 7 : 0;
    int extraDataHistory = m_bDataHistoryExpanded ? rowH * 7 : 0;

    // Base bottom of DataHistory header = base y 800 + base h 48
    int baseDataHistBottom = 980 + 48;
    m_totalHeight = (int)(baseDataHistBottom * scaleY)
        + extraAdvanced
        + extraDataHistory
        + 40;

    m_totalHeight = max(m_totalHeight, rcClient.Height());

    m_totalHeight =
        max(m_totalHeight, rcClient.Height());

    ClampScroll();

    // ── Step 2: Create a tall offscreen bitmap (content space) ───
    int contentW = rcClient.Width();
    int contentH = m_totalHeight;

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, contentW, contentH);
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    // Content rect — full height
    CRect rcContent(0, 0, contentW, contentH);

    // ── Step 3: Draw everything into the tall bitmap ─────────────
    DrawBackground(&memDC, rcContent);
    DrawHeader(&memDC, rcContent);
    DrawBatteryOverview(&memDC, rcContent);
    DrawQuickActions(&memDC, rcContent);
    DrawChargeRate(&memDC, rcContent);
    DrawBasicBatteryInfo(&memDC, rcContent);
    DrawAdvancedInfo(&memDC, rcContent);

    DrawDataHistory(&memDC, rcContent);

    // ── Step 4: Blit only the visible portion to screen ──────────
    dc.BitBlt(
        0, 0,                        // dest top-left on screen
        rcClient.Width(),            // dest width
        rcClient.Height(),           // dest height
        &memDC,
        0, m_scrollY,                // source: offset by scroll
        SRCCOPY);

    memDC.SelectObject(pOldBmp);
}

// ─── LButtonUp — translate point into content space ──────────────
void CMFCUIDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_bDragging)
    {
        m_bDragging = false;
        ReleaseCapture();

        // If it was a real drag (moved more than 4px), don't fire clicks
        int dragDist = abs(point.y - m_dragStartY);
        if (dragDist > 4)
        {
            CDialogEx::OnLButtonUp(nFlags, point);
            return;
        }
    }

    // Translate screen point → content point
    CPoint cp = ContentPoint(point);

    if (m_rcBtnAutoTest.PtInRect(cp))
        MessageBox(_T("Auto Test started!"), _T("Auto Test"),
            MB_OK | MB_ICONINFORMATION);
    else if (m_rcBtnViewLog.PtInRect(cp))
        MessageBox(_T("Opening View Log..."), _T("View Log"),
            MB_OK | MB_ICONINFORMATION);
    else if (m_rcBtnLanguage.PtInRect(cp))
    {
        m_bJapanese = !m_bJapanese;
        Invalidate();
    }

    if (m_rcBtnAdvanced.PtInRect(cp))
    {
        m_bAdvancedExpanded = !m_bAdvancedExpanded;

        if (!m_bAdvancedExpanded)
        {
            for (int i = 0; i < 7; i++)
                m_rcAdvBtn[i].SetRectEmpty();
        }
        Invalidate();
    }
    else if (m_bAdvancedExpanded)
    {
        const TCHAR* btnNames[] = {
            _T("Cpu Load Test"),
            _T("Discharge Test"),
            _T("Active Life Trend"),
            _T("Standby Life Trend"),
            _T("Running Application"),
            _T("Detect Manipulation"),
            _T("Check Power State")
        };
        for (int i = 0; i < 7; i++)
        {
            if (!m_rcAdvBtn[i].IsRectEmpty() &&
                m_rcAdvBtn[i].PtInRect(cp))
            {
                CString msg;
                msg.Format(_T("%s clicked!"), btnNames[i]);
                MessageBox(msg, btnNames[i], MB_OK | MB_ICONINFORMATION);
                break;
            }
        }
    }

    if (m_rcBtnDataHistory.PtInRect(cp))
    {
        m_bDataHistoryExpanded = !m_bDataHistoryExpanded;

        // Clear all sub-button rects immediately so stale
        // rects never fire when collapsed
        if (!m_bDataHistoryExpanded)
        {
            for (int i = 0; i < 7; i++)
                m_rcDataHistBtn[i].SetRectEmpty();
        }
        Invalidate();
    }
    else if (m_bDataHistoryExpanded)
    {
        // Only check sub-buttons if header was NOT clicked
        const TCHAR* dhNames[] = {
            _T("Charge and History"),
            _T("Export to CSV"),
            _T("Sleep Logs"),
            _T("Usage History"),
            _T("Capacity History"),
            _T("Battery Report"),
            _T("View Power State Logs")
        };
        for (int i = 0; i < 7; i++)
        {
            if (!m_rcDataHistBtn[i].IsRectEmpty() &&
                m_rcDataHistBtn[i].PtInRect(cp))
            {
                CString msg;
                msg.Format(_T("%s clicked!"), dhNames[i]);
                MessageBox(msg, dhNames[i], MB_OK | MB_ICONINFORMATION);
                break;
            }
        }
    }

    CDialogEx::OnLButtonUp(nFlags, point);
}

// ─── Scale Helpers ────────────────────────────────────────────────
int CMFCUIDlg::SW(int baseW, int clientWidth)
{
    double scaleX = (double)clientWidth / BASE_W;
    if (scaleX < 0.8) scaleX = 0.8;
    if (scaleX > 1.5) scaleX = 1.5;
    return (int)(baseW * scaleX);
}

int CMFCUIDlg::SH(int baseH, int clientHeight)
{
    double scaleY = (double)clientHeight / BASE_H;
    if (scaleY < 0.8) scaleY = 0.8;
    if (scaleY > 1.5) scaleY = 1.5;
    return (int)(baseH * scaleY);
}

int CMFCUIDlg::SF(int baseFont, int clientWidth)
{
    double scale = (double)clientWidth / BASE_W;
    if (scale < 0.8) scale = 0.8;
    if (scale > 1.4) scale = 1.4;
    int size = (int)(baseFont * scale);
    int minSize = max(6, baseFont - 2);
    int maxSize = baseFont + 8;
    return min(max(size, minSize), maxSize);
}

// ─── Utilities ────────────────────────────────────────────────────
void CMFCUIDlg::DrawRoundRect(CDC* pDC, CRect rc, int radius,
    COLORREF bg, COLORREF border)
{
    CBrush brush(bg);
    CPen   pen(PS_SOLID, 1, border);
    CBrush* pOldB = pDC->SelectObject(&brush);
    CPen* pOldP = pDC->SelectObject(&pen);
    pDC->RoundRect(rc, CPoint(radius, radius));
    pDC->SelectObject(pOldB);
    pDC->SelectObject(pOldP);
}

void CMFCUIDlg::DrawTextEx(CDC* pDC, const CString& text, CRect rc,
    COLORREF color, int fontSize, bool bold, UINT format)
{
    LOGFONT lf = {};
    lf.lfHeight = -MulDiv(fontSize,
        GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfQuality = CLEARTYPE_QUALITY;
    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
    CFont font;
    font.CreateFontIndirect(&lf);
    CFont* pOld = pDC->SelectObject(&font);
    pDC->SetTextColor(color);
    pDC->SetBkMode(TRANSPARENT);
    pDC->DrawText(text, &rc, format);
    pDC->SelectObject(pOld);
}

void CMFCUIDlg::DrawBackground(CDC* pDC, CRect rc)
{
    int W = m_clientWidth;
    int H = m_clientHeight;

    CBrush br(CLR_BG);
    pDC->FillRect(&rc, &br);
}

void CMFCUIDlg::DrawHeader(CDC* pDC, CRect rc)
{
    int W = m_clientWidth;
    int H = m_clientHeight;
    int mx = SW(20, W);
    int y = SH(16, H);

    CRect rcTitle(mx, y, rc.right - mx, y + SH(32, H));
    DrawTextEx(pDC, _T("Battery Health & Monitor"), rcTitle,
        CLR_TITLE, SF(14, W), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int divY = y + SH(36, H);
    CPen pen(PS_SOLID, 1, CLR_BORDER);
    CPen* pOld = pDC->SelectObject(&pen);
    pDC->MoveTo(mx, divY);
    pDC->LineTo(rc.right - mx, divY);
    pDC->SelectObject(pOld);

    CRect rcSub(mx, divY + SH(4, H), rc.right - mx, divY + SH(22, H));
    DrawTextEx(pDC, _T("Device ID: 123456789  |  Model: ABC-123"), rcSub,
        CLR_SUBTITLE, SF(8, W), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}