// DrawSecBasicInfo.cpp

#include "pch.h"
#include "MFCUIDlg.h"
#include "BatteryHelthDlg.h"

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
    /*int cardTop = SH(614, H);
    int cardBottom = SH(840, H);*/

    int cardTop = SH(308, H);
    int cardBottom = SH(534, H);

    CRect rcCard(mx, cardTop, W - mx, cardBottom);
    DrawRoundRect(pDC, rcCard, SW(10, W), CLR_CARD, CLR_BORDER);

    int pad = SW(14, W);

    // ── Card title ───────────────────────────────────────────────
    CRect rcTitle(
        rcCard.left + pad, rcCard.top + SH(8, H),
        rcCard.right - pad, rcCard.top + SH(28, H));
    DrawTextEx(pDC,
        L(_T("Basic Battery Info"),
            _T("基本バッテリー情報")),
        rcTitle,
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
    int rows = 2;
    int cols = 2;

    int vGap = gap;

    int tileW = (totalW - gap * (cols - 1)) / cols;
    int tileH = (innerBottom - innerTop - vGap) / rows;

   

    CRect rcT1(
        innerLeft,
        innerTop,
        innerLeft + tileW,
        innerTop + tileH);

    CRect rcT2(
        innerLeft + tileW + gap,
        innerTop,
        innerRight,
        innerTop + tileH);

    CRect rcT3(
        innerLeft,
        innerTop + tileH + vGap,
        innerLeft + tileW,
        innerBottom);

    CRect rcT4(
        innerLeft + tileW + gap,
        innerTop + tileH + vGap,
        innerRight,
        innerBottom);

    /*DrawInfoTile(pDC, rcT1,
        _T("Name"), _T("Battery A"),
        CLR_DARK_TEXT,
        false, _T(""), 0);

    DrawInfoTile(pDC, rcT2,
        _T("Battery Id"), _T("BAT-001"),
        CLR_DARK_TEXT,
        false, _T(""), 0);

    DrawInfoTile(pDC, rcT3,
        _T("Time Remaining"), _T("02:35"),
        CLR_DARK_TEXT,
        false, _T(""), 0);*/

    // read real data safely
    CString battName = _T("Unknown");
    CString timeLeft = _T("--:--");
    CString health = _T("Unknown");
    CString battId = _T("Unknown");
	CString voltage = _T("Unknown");
	CString temperature = _T("Unknown");

    if (m_pBattDlg)   // always check pointer first!
    {
        // grab real values from BatteryHelthDlg
        battId = m_pBattDlg->m_fullManufacturer;
        /*healthStr = m_pBattDlg->m_cpuLoadResult;*/
        battName = m_pBattDlg->battName;
        // charging state example
        bool charging = m_pBattDlg->IsCharging();
        timeLeft = m_pBattDlg->remain;

        voltage = m_pBattDlg->out;
        temperature = m_pBattDlg->t;
        health = m_pBattDlg->healthStr;
    }

    DrawInfoTile(pDC, rcT1,
        L(_T("Name"), _T("名称")),
        battName,
        CLR_DARK_TEXT,
        false, _T(""), 0);

    DrawInfoTile(pDC, rcT2,
        L(_T("Battery Id"), _T("バッテリーID")),
        battId,
        CLR_DARK_TEXT,
        false, _T(""), 0);

    DrawInfoTile(pDC, rcT3,
        L(_T("Voltage"), _T("電圧")),
        voltage,
        CLR_DARK_TEXT,
        false, _T(""), 0);

    DrawInfoTile(pDC, rcT4,
        L(_T("Time Remaining"), _T("残り時間")),
        timeLeft,
        CLR_DARK_TEXT,
        false, _T(""), 0);
}