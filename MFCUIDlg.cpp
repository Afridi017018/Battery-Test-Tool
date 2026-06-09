#include "pch.h"
#include "framework.h"
#include "MFCUIDlg.h"
#include <vector>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Base design dimensions (what we designed against)
#define BASE_W  500
#define BASE_H  900

// Color palette
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

    SetWindowPos(nullptr, 100, 50,
        BASE_W, BASE_H,
        SWP_NOZORDER);

    return TRUE;
}

// Force repaint on every resize
void CMFCUIDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    Invalidate();
}

BOOL CMFCUIDlg::OnEraseBkgnd(CDC* pDC)
{
    return TRUE;
}

void CMFCUIDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect rcClient;
    GetClientRect(&rcClient);

    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, rcClient.Width(), rcClient.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    DrawBackground(&memDC, rcClient);
    DrawHeader(&memDC, rcClient);
    DrawBatteryOverview(&memDC, rcClient);
    DrawBasicBatteryInfo(&memDC, rcClient);

    DrawChargeRate(&memDC, rcClient);

    dc.BitBlt(0, 0, rcClient.Width(), rcClient.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

void CMFCUIDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    CDialogEx::OnLButtonUp(nFlags, point);
}

// ─── Scale Helpers ────────────────────────────────────────────────
// These convert base-design pixels into actual pixels for current window size
int CMFCUIDlg::SW(int baseW, int clientWidth)
{
    double scaleX = (double)clientWidth / BASE_W;

    if (scaleX < 0.8)
        scaleX = 0.8;
    if (scaleX > 1.5)
        scaleX = 1.5;

    return (int)(baseW * scaleX);
}

int CMFCUIDlg::SH(int baseH, int clientHeight)
{
    double scaleY = (double)clientHeight / BASE_H;

    if (scaleY < 0.8)
        scaleY = 0.8;
    if (scaleY > 1.5)
        scaleY = 1.5;

    return (int)(baseH * scaleY);
}

int CMFCUIDlg::SF(int baseFont, int clientWidth)
{
    double scale = (double)clientWidth / BASE_W;

    if (scale < 0.8)
        scale = 0.8;
    if (scale > 1.4)
        scale = 1.4;

    int size = (int)(baseFont * scale);

    int minSize = max(6, baseFont - 2);
    int maxSize = baseFont + 8;

    return min(max(size, minSize), maxSize);
}

// ─── Utility: Rounded Rectangle ──────────────────────────────────
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

// ─── Utility: Draw Text ──────────────────────────────────────────
void CMFCUIDlg::DrawTextEx(CDC* pDC, const CString& text, CRect rc,
    COLORREF color, int fontSize, bool bold, UINT format)
{
    LOGFONT lf = {};
    lf.lfHeight = -MulDiv(fontSize, GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
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

// ─── Draw Background ─────────────────────────────────────────────
void CMFCUIDlg::DrawBackground(CDC* pDC, CRect rc)
{
    CBrush br(CLR_BG);
    pDC->FillRect(&rc, &br);
}

// ─── Draw Header ─────────────────────────────────────────────────
void CMFCUIDlg::DrawHeader(CDC* pDC, CRect rc)
{
    int W = rc.Width(), H = rc.Height();
    int mx = SW(20, W);   // horizontal margin
    int y = SH(16, H);   // top padding

    // Title
    CRect rcTitle(mx, y, rc.right - mx, y + SH(32, H));
    DrawTextEx(pDC, _T("Battery Health & Monitor"), rcTitle,
        CLR_TITLE, SF(14, W), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Divider
    int divY = y + SH(36, H);
    CPen pen(PS_SOLID, 1, CLR_BORDER);
    CPen* pOld = pDC->SelectObject(&pen);
    pDC->MoveTo(mx, divY);
    pDC->LineTo(rc.right - mx, divY);
    pDC->SelectObject(pOld);

    // Subtitle
    CRect rcSub(mx, divY + SH(4, H), rc.right - mx, divY + SH(22, H));
    DrawTextEx(pDC, _T("Device ID: 123456789  |  Model: ABC-123"), rcSub,
        CLR_SUBTITLE, SF(8, W), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

