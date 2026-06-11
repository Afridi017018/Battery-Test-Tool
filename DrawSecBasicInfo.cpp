// DrawSecBasicInfo.cpp

#include "pch.h"
#include "MFCUIDlg.h"

// ─────────────────────────────────────────────────────────────────
// Responsive font size for a tile:
// picks the smaller of width/height based scaling, with hard clamps
// ─────────────────────────────────────────────────────────────────
static int TileFont(int tileW, int tileH, int baseSize)
{
    // Scale relative to a "reference tile" of 140x80
    float scaleW = (float)tileW / 140.0f;
    float scaleH = (float)tileH / 80.0f;
    float scale = min(scaleW, scaleH);        // use smaller so nothing clips
    scale = max(0.55f, min(scale, 1.6f)); // clamp 55%..160%
    int sz = (int)(baseSize * scale);
    return max(6, sz);
}

// ─────────────────────────────────────────────────────────────────
// Draw one info tile completely self-contained & responsive
// ─────────────────────────────────────────────────────────────────
static void DrawInfoTile(
    CDC* pDC,
    CRect          rcTile,
    const CString& label,
    const CString& value,
    COLORREF       valueColor,
    bool           hasIcon,
    const CString& iconText,
    COLORREF       iconColor)
{
    int TW = rcTile.Width();
    int TH = rcTile.Height();

    // ── Background ───────────────────────────────────────────────
    int radius = max(4, min(TW, TH) / 10);
    {
        CBrush br(RGB(250, 251, 253));
        CPen   pen(PS_SOLID, 1, RGB(220, 225, 235));
        CBrush* pOldB = pDC->SelectObject(&br);
        CPen* pOldP = pDC->SelectObject(&pen);
        pDC->RoundRect(rcTile, CPoint(radius, radius));
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);
    }

    // ── Layout zones — all % of tile ─────────────────────────────
    int padX = max(4, TW * 8 / 100);
    int lblTop = rcTile.top + max(3, TH * 10 / 100);
    int lblBot = rcTile.top + TH * 46 / 100;
    int divY = rcTile.top + TH * 50 / 100;
    int valTop = rcTile.top + TH * 54 / 100;
    int valBot = rcTile.bottom - max(3, TH * 8 / 100);

    int lblFS = TileFont(TW, TH, 8);   // label font
    int valFS = TileFont(TW, TH, 11);  // value font
    int iconFS = TileFont(TW, TH, 9);

    // ── Icon (optional) + Label ───────────────────────────────────
    int textStartX = rcTile.left + padX;

    if (hasIcon && !iconText.IsEmpty())
    {
        // measure icon width roughly = iconFS * 1.1 pts in pixels
        int iconW = MulDiv(iconFS * 12, GetDeviceCaps(pDC->m_hDC, LOGPIXELSX), 720);
        iconW = max(10, min(iconW, TW / 4));

        CRect rcIcon(textStartX, lblTop,
            textStartX + iconW, lblBot);

        LOGFONT lf = {};
        lf.lfHeight = -MulDiv(iconFS,
            GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
        lf.lfWeight = FW_NORMAL;
        lf.lfQuality = CLEARTYPE_QUALITY;
        _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
        CFont f; f.CreateFontIndirect(&lf);
        CFont* pOld = pDC->SelectObject(&f);
        pDC->SetTextColor(iconColor);
        pDC->SetBkMode(TRANSPARENT);
        pDC->DrawText(iconText, &rcIcon,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        pDC->SelectObject(pOld);

        textStartX += iconW + max(2, TW / 20);
    }

    // Label text
    {
        CRect rcLbl(textStartX, lblTop,
            rcTile.right - max(2, TW / 20), lblBot);

        LOGFONT lf = {};
        lf.lfHeight = -MulDiv(lblFS,
            GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
        lf.lfWeight = FW_NORMAL;
        lf.lfQuality = CLEARTYPE_QUALITY;
        _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
        CFont f; f.CreateFontIndirect(&lf);
        CFont* pOld = pDC->SelectObject(&f);
        pDC->SetTextColor(RGB(90, 100, 120));
        pDC->SetBkMode(TRANSPARENT);
        // DT_END_ELLIPSIS prevents overflow
        pDC->DrawText(label, &rcLbl,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        pDC->SelectObject(pOld);
    }

    // ── Thin inner divider ────────────────────────────────────────
    {
        CPen dp(PS_SOLID, 1, RGB(230, 233, 240));
        CPen* pOld = pDC->SelectObject(&dp);
        pDC->MoveTo(rcTile.left + TW * 6 / 100, divY);
        pDC->LineTo(rcTile.right - TW * 6 / 100, divY);
        pDC->SelectObject(pOld);
    }

    // ── Value ─────────────────────────────────────────────────────
    {
        CRect rcVal(rcTile.left + padX, valTop,
            rcTile.right - max(2, TW / 20), valBot);

        LOGFONT lf = {};
        lf.lfHeight = -MulDiv(valFS,
            GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
        lf.lfWeight = FW_BOLD;
        lf.lfQuality = CLEARTYPE_QUALITY;
        _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
        CFont f; f.CreateFontIndirect(&lf);
        CFont* pOld = pDC->SelectObject(&f);
        pDC->SetTextColor(valueColor);
        pDC->SetBkMode(TRANSPARENT);
        pDC->DrawText(value, &rcVal,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        pDC->SelectObject(pOld);
    }
}

// ─────────────────────────────────────────────────────────────────
// DrawBasicBatteryInfo
// ─────────────────────────────────────────────────────────────────
void CMFCUIDlg::DrawBasicBatteryInfo(CDC* pDC, CRect rc)
{
    int W = m_clientWidth;
    int H = m_clientHeight;
    int mx = SW(16, W);

    // ── Card bounds ──────────────────────────────────────────────
    int cardTop = SH(614, H);
    int cardBottom = SH(724, H);
    CRect rcCard(mx, cardTop, W - mx, cardBottom);
    DrawRoundRect(pDC, rcCard, SW(10, W), CLR_CARD, CLR_BORDER);

    int pad = SW(14, W);

    // ── Card title ───────────────────────────────────────────────
    CRect rcTitle(
        rcCard.left + pad, rcCard.top + SH(8, H),
        rcCard.right - pad, rcCard.top + SH(28, H));
    DrawTextEx(pDC, _T("Basic Battery Info"), rcTitle,
        CLR_TITLE, SF(10, W), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Divider
    int divY = rcCard.top + SH(30, H);
    {
        CPen divPen(PS_SOLID, 1, CLR_BORDER);
        CPen* pOldP = pDC->SelectObject(&divPen);
        pDC->MoveTo(rcCard.left + SW(10, W), divY);
        pDC->LineTo(rcCard.right - SW(10, W), divY);
        pDC->SelectObject(pOldP);
    }

    // ── Three tiles ───────────────────────────────────────────────
    int innerLeft = rcCard.left + SW(10, W);
    int innerRight = rcCard.right - SW(10, W);
    int innerTop = divY + SH(6, H);
    int innerBottom = rcCard.bottom - SH(8, H);
    int totalW = innerRight - innerLeft;
    int gap = max(3, SW(6, W));
    int tileW = (totalW - gap * 2) / 3;
    int tileH = innerBottom - innerTop;

    // Guard: if tiles are too small don't draw garbage
    if (tileW < 20 || tileH < 20) return;

    // ── Tile 1: Voltage ───────────────────────────────────────────
    CRect rcT1(innerLeft,
        innerTop,
        innerLeft + tileW,
        innerBottom);
    DrawInfoTile(pDC, rcT1,
        _T("Voltage"), _T("12.5 V"),
        CLR_DARK_TEXT,
        false, _T(""), 0);

    // ── Tile 2: Temperature ───────────────────────────────────────
    CRect rcT2(innerLeft + tileW + gap,
        innerTop,
        innerLeft + tileW * 2 + gap,
        innerBottom);
    DrawInfoTile(pDC, rcT2,
        _T("Temperature"), _T("32 \u00B0C"),
        CLR_DARK_TEXT,
        true, _T("\u2193"), RGB(30, 120, 220));

    // ── Tile 3: Status (custom — red dot + "Good") ────────────────
    CRect rcT3(innerLeft + tileW * 2 + gap * 2,
        innerTop,
        innerRight,
        innerBottom);

    int TW = rcT3.Width();
    int TH = rcT3.Height();
    int radius = max(4, min(TW, TH) / 10);

    // Background
    {
        CBrush br(RGB(250, 251, 253));
        CPen   pen(PS_SOLID, 1, RGB(220, 225, 235));
        CBrush* pOldB = pDC->SelectObject(&br);
        CPen* pOldP = pDC->SelectObject(&pen);
        pDC->RoundRect(rcT3, CPoint(radius, radius));
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);
    }

    int padX3 = max(4, TW * 8 / 100);
    int lblTop3 = rcT3.top + max(3, TH * 10 / 100);
    int lblBot3 = rcT3.top + TH * 46 / 100;
    int divY3 = rcT3.top + TH * 50 / 100;
    int valTop3 = rcT3.top + TH * 54 / 100;
    int valBot3 = rcT3.bottom - max(3, TH * 8 / 100);
    int lblMidY = (lblTop3 + lblBot3) / 2;

    // Red dot — radius scales with tile
    int dotR = max(3, min(TW, TH) * 5 / 100);
    int dotCX = rcT3.left + padX3 + dotR;
    int dotCY = lblMidY;
    {
        CBrush dotBr(CLR_RED);
        CPen   dotPen(PS_SOLID, 1, CLR_RED);
        CBrush* pOldB = pDC->SelectObject(&dotBr);
        CPen* pOldP = pDC->SelectObject(&dotPen);
        pDC->Ellipse(dotCX - dotR, dotCY - dotR,
            dotCX + dotR, dotCY + dotR);
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);
    }

    // "Status" label
    int lblFS3 = TileFont(TW, TH, 8);
    {
        CRect rcLbl3(dotCX + dotR + max(2, TW / 20),
            lblTop3,
            rcT3.right - max(2, TW / 20),
            lblBot3);
        LOGFONT lf = {};
        lf.lfHeight = -MulDiv(lblFS3,
            GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
        lf.lfWeight = FW_NORMAL;
        lf.lfQuality = CLEARTYPE_QUALITY;
        _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
        CFont f; f.CreateFontIndirect(&lf);
        CFont* pOld = pDC->SelectObject(&f);
        pDC->SetTextColor(RGB(90, 100, 120));
        pDC->SetBkMode(TRANSPARENT);
        pDC->DrawText(_T("Status"), &rcLbl3,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        pDC->SelectObject(pOld);
    }

    // Inner divider
    {
        CPen dp(PS_SOLID, 1, RGB(230, 233, 240));
        CPen* pOld = pDC->SelectObject(&dp);
        pDC->MoveTo(rcT3.left + TW * 6 / 100, divY3);
        pDC->LineTo(rcT3.right - TW * 6 / 100, divY3);
        pDC->SelectObject(pOld);
    }

    // "Good" value
    int valFS3 = TileFont(TW, TH, 11);
    {
        CRect rcVal3(rcT3.left + padX3, valTop3,
            rcT3.right - max(2, TW / 20), valBot3);
        LOGFONT lf = {};
        lf.lfHeight = -MulDiv(valFS3,
            GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
        lf.lfWeight = FW_BOLD;
        lf.lfQuality = CLEARTYPE_QUALITY;
        _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
        CFont f; f.CreateFontIndirect(&lf);
        CFont* pOld = pDC->SelectObject(&f);
        pDC->SetTextColor(CLR_GREEN);
        pDC->SetBkMode(TRANSPARENT);
        pDC->DrawText(_T("Good"), &rcVal3,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        pDC->SelectObject(pOld);
    }
}