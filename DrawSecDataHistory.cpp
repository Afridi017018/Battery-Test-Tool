// DrawSecDataHistory.cpp

#include "pch.h"
#include "MFCUIDlg.h"

// ─────────────────────────────────────────────────────────────────
// Font scaled to card dimensions
// ─────────────────────────────────────────────────────────────────
static int DhFont(int refW, int refH, int baseSize)
{
    float scaleW = (float)refW / 468.0f;
    float scaleH = (float)refH / 48.0f;
    float scale = min(scaleW, scaleH);
    scale = max(0.55f, min(scale, 1.6f));
    return max(6, (int)(baseSize * scale));
}

// ─────────────────────────────────────────────────────────────────
// Draw one data history row
// ─────────────────────────────────────────────────────────────────
static void DrawDHRow(
    CMFCUIDlg* dlg,
    CDC* pDC,
    CRect          rc,
    const CString& label,
    const CString& iconText,
    COLORREF       iconBg,
    int            fontSize)
{
    if (rc.Width() < 20 || rc.Height() < 10) return;

    int RW = rc.Width();
    int RH = rc.Height();

    // Row background
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
    {
        LOGFONT lf = {};
        lf.lfHeight = -MulDiv(fontSize,
            GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
        lf.lfWeight = FW_NORMAL;
        lf.lfQuality = CLEARTYPE_QUALITY;
        _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
        CFont f; f.CreateFontIndirect(&lf);
        CFont* pOld = pDC->SelectObject(&f);
        pDC->SetTextColor(RGB(180, 185, 200));
        pDC->SetBkMode(TRANSPARENT);
        pDC->DrawText(_T(">"), &rcArrow,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        pDC->SelectObject(pOld);
    }

    // Label
    int textX = iconCX + iconR + max(6, RW * 8 / 468);
    CRect rcLabel(textX, rc.top,
        rc.right - arrowW - max(4, RW * 8 / 468), rc.bottom);
    {
        LOGFONT lf = {};
        lf.lfHeight = -MulDiv(fontSize,
            GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
        lf.lfWeight = FW_NORMAL;
        lf.lfQuality = CLEARTYPE_QUALITY;
        _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
        CFont f; f.CreateFontIndirect(&lf);
        CFont* pOld = pDC->SelectObject(&f);
        pDC->SetTextColor(CLR_DARK_TEXT);
        pDC->SetBkMode(TRANSPARENT);
        pDC->DrawText(label, &rcLabel,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        pDC->SelectObject(pOld);
    }

    // Bottom separator
    CPen sep(PS_SOLID, 1, RGB(235, 238, 245));
    CPen* pOldP = pDC->SelectObject(&sep);
    pDC->MoveTo(rc.left + padX, rc.bottom - 1);
    pDC->LineTo(rc.right - padX, rc.bottom - 1);
    pDC->SelectObject(pOldP);
}

// ─────────────────────────────────────────────────────────────────
// DrawDataHistory
// Header sits below Advanced Info
// Base position: Advanced header=736+48=784, gap=16 → top=800
// ─────────────────────────────────────────────────────────────────
void CMFCUIDlg::DrawDataHistory(CDC* pDC, CRect rc)
{
    int W = m_clientWidth;
    int H = m_clientHeight;
    int mx = SW(16, W);

    // ── Use unclamped scale (same as totalHeight calc) ────────────
    double scaleY = (double)H / (double)BASE_H;
    if (scaleY < 0.4) scaleY = 0.4;

    // ── Calculate where Advanced section ends ─────────────────────
    // Advanced header base top=736, base h=48
    int advHdrTop = (int)(860.0 * scaleY);
    int advHdrH = max(32, (int)(48.0 * scaleY));
    int advHdrBottom = advHdrTop + advHdrH;

    int rowH = max(36, (int)(44.0 * scaleY));

    // If advanced is expanded, skip past its 7 rows
    int advBodyH = m_bAdvancedExpanded ? rowH * 7 : 0;

    // Gap between sections
    int gap = max(8, (int)(16.0 * scaleY));

    // ── Header position ───────────────────────────────────────────
    int hdrTop = advHdrBottom + advBodyH + gap;
    int hdrH = max(32, (int)(48.0 * scaleY));
    int hdrBottom = hdrTop + hdrH;

    CRect rcHeader(mx, hdrTop, W - mx, hdrBottom);
    if (rcHeader.Width() < 40) return;

    int CW = rcHeader.Width();

    // ── Header background ─────────────────────────────────────────
    DrawRoundRect(pDC, rcHeader,
        m_bDataHistoryExpanded ? 0 : SW(10, W),
        CLR_CARD, CLR_BORDER);

    // Square off bottom when expanded
    if (m_bDataHistoryExpanded)
    {
        CBrush br(CLR_CARD);
        CPen   pen(PS_SOLID, 1, CLR_BORDER);
        CBrush* pOldB = pDC->SelectObject(&br);
        CPen* pOldP = pDC->SelectObject(&pen);
        CRect rcFlat(rcHeader.left, rcHeader.top + hdrH / 2,
            rcHeader.right, rcHeader.bottom + 2);
        pDC->FillRect(&rcFlat, &br);
        pDC->MoveTo(rcHeader.left, rcHeader.top + hdrH / 2);
        pDC->LineTo(rcHeader.left, rcHeader.bottom + 2);
        pDC->MoveTo(rcHeader.right, rcHeader.top + hdrH / 2);
        pDC->LineTo(rcHeader.right, rcHeader.bottom + 2);
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);
    }

    // ── Title ─────────────────────────────────────────────────────
    int pad = max(6, CW * 14 / 468);
    int titleFS = DhFont(CW, hdrH, 10);

    CRect rcTitle(rcHeader.left + pad, rcHeader.top,
        rcHeader.right - hdrH - pad, rcHeader.bottom);
    DrawTextEx(pDC, _T("Data and History"), rcTitle,
        CLR_TITLE, titleFS, true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // ── +/- button ────────────────────────────────────────────────
    int btnSz = max(20, hdrH * 60 / 100);
    int btnX = rcHeader.right - pad - btnSz;
    int btnY = rcHeader.top + (hdrH - btnSz) / 2;
    CRect rcPM(btnX, btnY, btnX + btnSz, btnY + btnSz);

    {
        CBrush br(CLR_BLUE);
        CPen   pen(PS_SOLID, 1, CLR_BLUE);
        CBrush* pOldB = pDC->SelectObject(&br);
        CPen* pOldP = pDC->SelectObject(&pen);
        pDC->Ellipse(rcPM);
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);
    }

    CString sym = m_bDataHistoryExpanded ? _T("\u2212") : _T("+");
    DrawTextEx(pDC, sym, rcPM,
        RGB(255, 255, 255),
        max(6, btnSz * 55 / 100), true,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Store click target
    m_rcBtnDataHistory = rcHeader;

    // ── Expanded rows ─────────────────────────────────────────────
    if (!m_bDataHistoryExpanded) return;

    struct RowDef {
        const TCHAR* label;
        const TCHAR* icon;
        COLORREF     iconBg;
    };
    RowDef defs[7] = {
        { _T("Charge and History"),      _T("C"), RGB(30,  120, 220) },
        { _T("Export to CSV"),           _T("E"), RGB(40,  180,  80) },
        { _T("Sleep Logs"),              _T("S"), RGB(160,  80, 220) },
        { _T("Usage History"),           _T("U"), RGB(220, 120,  40) },
        { _T("Capacity History"),        _T("H"), RGB(220,  60,  60) },
        { _T("Battery Report"),          _T("B"), RGB(80,  160, 200) },
        { _T("View Power State Logs"),   _T("P"), RGB(60,  140,  80) },
    };

    int rowFS = DhFont(CW, rowH, 9);

    for (int i = 0; i < 7; i++)
    {
        int rowTop = hdrBottom + i * rowH;
        int rowBottom = rowTop + rowH;
        CRect rcRow(mx, rowTop, W - mx, rowBottom);
        m_rcDataHistBtn[i] = rcRow;

        bool isLast = (i == 6);

        if (isLast)
        {
            // Rounded bottom corners on last row
            CRect rcRound(mx, rowTop - 4, W - mx, rowBottom);
            DrawRoundRect(pDC, rcRound, SW(10, W), CLR_CARD, CLR_BORDER);

            CBrush br(CLR_CARD);
            CRect  rcFlat(mx + 1, rowTop - 4,
                W - mx - 1, rowTop + rowH / 3);
            pDC->FillRect(&rcFlat, &br);
        }
        else
        {
            // Side borders for middle rows
            CPen sidePen(PS_SOLID, 1, CLR_BORDER);
            CPen* pOldP = pDC->SelectObject(&sidePen);
            pDC->MoveTo(mx, rowTop);
            pDC->LineTo(mx, rowBottom);
            pDC->MoveTo(W - mx, rowTop);
            pDC->LineTo(W - mx, rowBottom);
            pDC->SelectObject(pOldP);

            CBrush br(CLR_CARD);
            CRect  rcFill(mx + 1, rowTop, W - mx - 1, rowBottom);
            pDC->FillRect(&rcFill, &br);
        }

        DrawDHRow(this, pDC, rcRow,
            defs[i].label, defs[i].icon, defs[i].iconBg, rowFS);
    }
}