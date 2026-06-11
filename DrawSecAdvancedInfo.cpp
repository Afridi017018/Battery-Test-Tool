// DrawSecAdvancedInfo.cpp

#include "pch.h"
#include "MFCUIDlg.h"

// ─────────────────────────────────────────────────────────────────
// Font scaled to card dimensions
// ─────────────────────────────────────────────────────────────────
static int AdvFont(int refW, int refH, int baseSize)
{
    float scaleW = (float)refW / 468.0f;
    float scaleH = (float)refH / 48.0f;   // header row reference height
    float scale = min(scaleW, scaleH);
    scale = max(0.55f, min(scale, 1.6f));
    return max(6, (int)(baseSize * scale));
}

// ─────────────────────────────────────────────────────────────────
// Draw one row button (icon area + label + ">" arrow)
// ─────────────────────────────────────────────────────────────────
static void DrawRowButton(
    CMFCUIDlg* dlg,
    CDC* pDC,
    CRect          rc,
    const CString& label,
    const CString& iconText,   // small unicode icon or letter
    COLORREF       iconBg,     // circle background color
    int            fontSize)
{
    if (rc.Width() < 20 || rc.Height() < 10) return;

    int RW = rc.Width();
    int RH = rc.Height();

    // Row background (white, no border — sits inside card)
    {
        CBrush br(RGB(255, 255, 255));
        CPen   pen(PS_SOLID, 1, RGB(235, 238, 245));
        CBrush* pOldB = pDC->SelectObject(&br);
        CPen* pOldP = pDC->SelectObject(&pen);
        pDC->Rectangle(rc);
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);
    }

    int padX = max(6, RW * 14 / 468);
    int midY = rc.top + RH / 2;

    // Icon circle
    int iconR = max(8, RH * 32 / 100);
    int iconCX = rc.left + padX + iconR;
    int iconCY = midY;
    {
        CBrush br(iconBg);
        CPen   pen(PS_SOLID, 1, iconBg);
        CBrush* pOldB = pDC->SelectObject(&br);
        CPen* pOldP = pDC->SelectObject(&pen);
        pDC->Ellipse(iconCX - iconR, iconCY - iconR,
            iconCX + iconR, iconCY + iconR);
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);

        // Icon text inside circle
        CRect rcIcon(iconCX - iconR, iconCY - iconR,
            iconCX + iconR, iconCY + iconR);

        LOGFONT lf = {};
        lf.lfHeight = -MulDiv(max(6, iconR * 55 / 100),
            GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
        lf.lfWeight = FW_BOLD;
        lf.lfQuality = CLEARTYPE_QUALITY;
        _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
        CFont f; f.CreateFontIndirect(&lf);
        CFont* pOld = pDC->SelectObject(&f);
        pDC->SetTextColor(RGB(255, 255, 255));
        pDC->SetBkMode(TRANSPARENT);
        pDC->DrawText(iconText, &rcIcon,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        pDC->SelectObject(pOld);
    }

    // ">" arrow on right
    int arrowW = max(10, RW * 20 / 468);
    CRect rcArrow(rc.right - arrowW - max(4, RW * 8 / 468),
        rc.top,
        rc.right - max(4, RW * 8 / 468),
        rc.bottom);

    LOGFONT lfa = {};
    lfa.lfHeight = -MulDiv(fontSize,
        GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
    lfa.lfWeight = FW_NORMAL;
    lfa.lfQuality = CLEARTYPE_QUALITY;
    _tcscpy_s(lfa.lfFaceName, _T("Segoe UI"));
    CFont fa; fa.CreateFontIndirect(&lfa);
    CFont* pOldA = pDC->SelectObject(&fa);
    pDC->SetTextColor(RGB(180, 185, 200));
    pDC->SetBkMode(TRANSPARENT);
    pDC->DrawText(_T(">"), &rcArrow,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    pDC->SelectObject(pOldA);

    // Label text
    int textX = iconCX + iconR + max(6, RW * 8 / 468);
    CRect rcLabel(textX, rc.top,
        rc.right - arrowW - max(4, RW * 8 / 468), rc.bottom);

    LOGFONT lfl = {};
    lfl.lfHeight = -MulDiv(fontSize,
        GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
    lfl.lfWeight = FW_NORMAL;
    lfl.lfQuality = CLEARTYPE_QUALITY;
    _tcscpy_s(lfl.lfFaceName, _T("Segoe UI"));
    CFont fl; fl.CreateFontIndirect(&lfl);
    CFont* pOldL = pDC->SelectObject(&fl);
    pDC->SetTextColor(CLR_DARK_TEXT);
    pDC->SetBkMode(TRANSPARENT);
    pDC->DrawText(label, &rcLabel,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    pDC->SelectObject(pOldL);

    // Bottom separator line
    CPen sep(PS_SOLID, 1, RGB(235, 238, 245));
    CPen* pOldP = pDC->SelectObject(&sep);
    pDC->MoveTo(rc.left + padX, rc.bottom - 1);
    pDC->LineTo(rc.right - padX, rc.bottom - 1);
    pDC->SelectObject(pOldP);
}

// ─────────────────────────────────────────────────────────────────
// DrawAdvancedInfo
// Header always visible at base top=736
// Expands downward when open (6 rows x rowH each)
// ─────────────────────────────────────────────────────────────────
void CMFCUIDlg::DrawAdvancedInfo(CDC* pDC, CRect rc)
{
    int W = m_clientWidth;
    int H = m_clientHeight;
    int mx = SW(16, W);

    // ── Header row height (always visible) ───────────────────────
    int hdrTop = SH(736, H);
    int hdrH = SH(48, H);
    hdrH = max(32, hdrH);
    int hdrBottom = hdrTop + hdrH;

    CRect rcHeader(mx, hdrTop, W - mx, hdrBottom);
    if (rcHeader.Width() < 40) return;

    int CW = rcHeader.Width();

    // Header card background
    DrawRoundRect(pDC, rcHeader,
        m_bAdvancedExpanded ? 0 : SW(10, W),
        CLR_CARD, CLR_BORDER);

    // If expanded, extend bottom corners square (card continues below)
    // We overdraw the bottom with a rectangle to square off bottom corners
    if (m_bAdvancedExpanded)
    {
        CBrush br(CLR_CARD);
        CPen   pen(PS_SOLID, 1, CLR_BORDER);
        CBrush* pOldB = pDC->SelectObject(&br);
        CPen* pOldP = pDC->SelectObject(&pen);
        // Draw left/right borders extending down from rounded top
        CRect rcTop(rcHeader.left, rcHeader.top + hdrH / 2,
            rcHeader.right, rcHeader.bottom + 2);
        pDC->FillRect(&rcTop, &br);
        // Redraw left and right border lines
        pDC->MoveTo(rcHeader.left, rcHeader.top + hdrH / 2);
        pDC->LineTo(rcHeader.left, rcHeader.bottom + 2);
        pDC->MoveTo(rcHeader.right, rcHeader.top + hdrH / 2);
        pDC->LineTo(rcHeader.right, rcHeader.bottom + 2);
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);
    }

    // ── "Advanced Info" title ─────────────────────────────────────
    int pad = max(6, CW * 14 / 468);
    int titleFS = AdvFont(CW, hdrH, 10);

    CRect rcTitle(rcHeader.left + pad, rcHeader.top,
        rcHeader.right - hdrH - pad, rcHeader.bottom);
    DrawTextEx(pDC, _T("Advanced Info"), rcTitle,
        CLR_TITLE, titleFS, true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // ── "+" / "-" button (right side of header) ──────────────────
    int btnSz = max(20, hdrH * 60 / 100);
    int btnX = rcHeader.right - pad - btnSz;
    int btnY = rcHeader.top + (hdrH - btnSz) / 2;

    CRect rcPlusMinus(btnX, btnY, btnX + btnSz, btnY + btnSz);

    // Circle background
    {
        CBrush br(CLR_BLUE);
        CPen   pen(PS_SOLID, 1, CLR_BLUE);
        CBrush* pOldB = pDC->SelectObject(&br);
        CPen* pOldP = pDC->SelectObject(&pen);
        pDC->Ellipse(rcPlusMinus);
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);
    }

    // "+" or "-" symbol
    CString symbol = m_bAdvancedExpanded ? _T("\u2212") : _T("+");
    DrawTextEx(pDC, symbol, rcPlusMinus,
        RGB(255, 255, 255),
        max(6, btnSz * 55 / 100), true,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Store full header as click target
    m_rcBtnAdvanced = rcHeader;

    // ── Expanded rows ─────────────────────────────────────────────
    if (!m_bAdvancedExpanded) return;

    // 6 buttons, each rowH tall
    // Use same unclamped scale as OnPaint totalHeight calculation
    double scaleY = (double)H / (double)BASE_H;
    if (scaleY < 0.4) scaleY = 0.4;
    int rowH = max(36, (int)(44.0 * scaleY));
    int bodyTop = hdrBottom;

    // Button definitions
    struct BtnDef {
        const TCHAR* label;
        const TCHAR* icon;
        COLORREF     iconBg;
    };
    BtnDef defs[7] = {
        { _T("Cpu Load Test"),        _T("C"), RGB(30,  120, 220) },
        { _T("Discharge Test"),       _T("D"), RGB(220,  60,  60) },
        { _T("Active Life Trend"),    _T("A"), RGB(40,  180,  80) },
        { _T("Standby Life Trend"),   _T("S"), RGB(160,  80, 220) },
        { _T("Running Application"),  _T("R"), RGB(220, 140,  30) },
        { _T("Detect Manipulation"),  _T("M"), RGB(80,  160, 200) },
        { _T("Check Power State"),    _T("P"), RGB(100, 100, 120) },
    };

    int rowFS = AdvFont(CW, rowH, 9);

    for (int i = 0; i < 7; i++)
    {
        int rowTop = bodyTop + i * rowH;
        int rowBottom = rowTop + rowH;

        CRect rcRow(mx, rowTop, W - mx, rowBottom);
        m_rcAdvBtn[i] = rcRow;

        // Last row gets rounded bottom corners
        bool isLast = (i == 6);

        if (isLast)
        {
            // Draw a rounded-bottom card for the last row
            CRect rcRound(mx, rowTop - 4, W - mx, rowBottom);
            DrawRoundRect(pDC, rcRound, SW(10, W), CLR_CARD, CLR_BORDER);

            // Overdraw top with flat rectangle to square the top edge
            CBrush br(CLR_CARD);
            CRect  rcFlat(mx + 1, rowTop - 4, W - mx - 1, rowTop + rowH / 3);
            pDC->FillRect(&rcFlat, &br);
        }
        else
        {
            // Left and right border lines for middle rows
            CPen sidePen(PS_SOLID, 1, CLR_BORDER);
            CPen* pOldP = pDC->SelectObject(&sidePen);
            pDC->MoveTo(mx, rowTop);
            pDC->LineTo(mx, rowBottom);
            pDC->MoveTo(W - mx, rowTop);
            pDC->LineTo(W - mx, rowBottom);
            pDC->SelectObject(pOldP);

            // Fill row bg
            CBrush br(CLR_CARD);
            CRect rcFill(mx + 1, rowTop, W - mx - 1, rowBottom);
            pDC->FillRect(&rcFill, &br);
        }

        DrawRowButton(this, pDC, rcRow,
            defs[i].label, defs[i].icon, defs[i].iconBg, rowFS);
    }
}