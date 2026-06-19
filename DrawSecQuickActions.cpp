// DrawSecQuickActions.cpp

#include "pch.h"
#include "MFCUIDlg.h"
#include "BatteryHelthDlg.h"

// ─────────────────────────────────────────────────────────────────
// Scale font relative to button dimensions
// ─────────────────────────────────────────────────────────────────
static int BtnFont(int btnW, int btnH, int baseSize)
{
    float scaleW = (float)btnW / 140.0f;
    float scaleH = (float)btnH / 38.0f;
    float scale = min(scaleW, scaleH);
    scale = max(0.5f, min(scale, 1.6f));
    return max(6, (int)(baseSize * scale));
}

// ─────────────────────────────────────────────────────────────────
// Draw a single styled button
// ─────────────────────────────────────────────────────────────────
void CMFCUIDlg::DrawQAButton(
    CDC* pDC,
    CRect          rcBtn,
    const CString& text,
    COLORREF       bgColor,
    COLORREF       textColor,
    COLORREF       borderColor,
    int            radius)
{
    if (rcBtn.Width() < 10 || rcBtn.Height() < 8) return;

    // Background + border
    {
        CBrush br(bgColor);
        CPen   pen(PS_SOLID, 1, borderColor);
        CBrush* pOldB = pDC->SelectObject(&br);
        CPen* pOldP = pDC->SelectObject(&pen);
        pDC->RoundRect(rcBtn, CPoint(radius, radius));
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);
    }

    // Text centred
    int fs = BtnFont(rcBtn.Width(), rcBtn.Height(), 9);
    DrawTextEx(pDC, text, rcBtn, textColor, fs, true,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

// ─────────────────────────────────────────────────────────────────
// DrawQuickActions
// Card sits below Basic Battery Info (base: top=634, height=90)
// ─────────────────────────────────────────────────────────────────
void CMFCUIDlg::DrawQuickActions(CDC* pDC, CRect rc)
{
    int W = m_clientWidth;
    int H = m_clientHeight;
    int mx = SW(16, W);

    // ── Card bounds ──────────────────────────────────────────────
    int cardTop = SH(308, H);
    int cardBottom = SH(398, H);
    CRect rcCard(mx, cardTop, W - mx, cardBottom);

    if (rcCard.Width() < 60 || rcCard.Height() < 30) return;

    DrawRoundRect(pDC, rcCard, SW(10, W), CLR_CARD, CLR_BORDER);

    int CW = rcCard.Width();
    int CH = rcCard.Height();
    int pad = max(6, CW * 14 / 468);

    // ── Card title ───────────────────────────────────────────────
    int titleH = max(14, CH * 22 / 90);
    CRect rcTitle(
        rcCard.left + pad, rcCard.top + CH * 8 / 90,
        rcCard.right - pad, rcCard.top + CH * 8 / 90 + titleH);
    DrawTextEx(pDC,
        L(_T("Quick Actions"),
            _T("クイックアクション")),
        rcTitle,
        CLR_TITLE,
        max(6, (int)(min(CW, CH) * 10 / 90)),
        true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Divider
    int divY = rcCard.top + CH * 32 / 90;
    {
        CPen dp(PS_SOLID, 1, CLR_BORDER);
        CPen* pOld = pDC->SelectObject(&dp);
        pDC->MoveTo(rcCard.left + CW * 10 / 468, divY);
        pDC->LineTo(rcCard.right - CW * 10 / 468, divY);
        pDC->SelectObject(pOld);
    }

    // ── Button row ───────────────────────────────────────────────
    int btnAreaTop = divY + CH * 8 / 90;
    int btnAreaBottom = rcCard.bottom - CH * 8 / 90;
    int btnH = btnAreaBottom - btnAreaTop;

    if (btnH < 8) return;

    int innerLeft = rcCard.left + CW * 10 / 468;
    int innerRight = rcCard.right - CW * 10 / 468;
    /*int totalBtnW = innerRight - innerLeft;
    int gapW = max(4, CW * 8 / 468);
    int btnW = (totalBtnW - gapW * 2) / 3;*/

    /* int totalBtnW = innerRight - innerLeft;
     int gapW = max(4, CW * 8 / 468);
     int btnW = (totalBtnW - gapW) / 2;*/

    int totalBtnW = innerRight - innerLeft;
    int gapW = max(4, CW * 8 / 468);
    int btnW = (totalBtnW - gapW * 2) / 3;

    if (btnW < 10) return;

    int btnRadius = max(4, min(btnW, btnH) / 5);

    // ── Button 1: Auto Test (solid blue) ─────────────────────────
    m_rcBtnAutoTest = CRect(
        innerLeft,
        btnAreaTop,
        innerLeft + btnW,
        btnAreaBottom);

    DrawQAButton(pDC, m_rcBtnAutoTest,
        L(_T("Auto Test"),
            _T("自動テスト")),
        RGB(255, 255, 255),
        CLR_DARK_TEXT,
        CLR_BORDER,
        btnRadius);



    //// ── Button 3: Language toggle (EN ⇔ JP) ──────────────────────
    //m_rcBtnLanguage = CRect(
    //    innerLeft + btnW + gapW,
    //    btnAreaTop,
    //    innerRight,
    //    btnAreaBottom);

    //{
    //    COLORREF btnBg = RGB(255, 255, 255);   // always white
    //    COLORREF btnBorder = CLR_BORDER;            // always grey
    //    COLORREF txtColor = CLR_DARK_TEXT;         // always dark
    //    COLORREF dotColor = CLR_RED;               // always red dot
    //    COLORREF arrowClr = CLR_MID_TEXT;          // always grey arrow

    //    CBrush br(btnBg);
    //    CPen   pen(PS_SOLID, 1, btnBorder);
    //    CBrush* pOldB = pDC->SelectObject(&br);
    //    CPen* pOldP = pDC->SelectObject(&pen);
    //    pDC->RoundRect(m_rcBtnLanguage, CPoint(btnRadius, btnRadius));
    //    pDC->SelectObject(pOldB);
    //    pDC->SelectObject(pOldP);

    //    int bW = m_rcBtnLanguage.Width();
    //    int bH = m_rcBtnLanguage.Height();

    //    // Dot
    //    int dotR = max(3, min(bW, bH) * 5 / 100);
    //    int dotCX = m_rcBtnLanguage.left + bW * 14 / 100;
    //    int dotCY = m_rcBtnLanguage.top + bH / 2;
    //    {
    //        CBrush dotBr(dotColor);
    //        CPen   dotPen(PS_SOLID, 1, dotColor);
    //        CBrush* pOldB2 = pDC->SelectObject(&dotBr);
    //        CPen* pOldP2 = pDC->SelectObject(&dotPen);
    //        pDC->Ellipse(dotCX - dotR, dotCY - dotR,
    //            dotCX + dotR, dotCY + dotR);
    //        pDC->SelectObject(pOldB2);
    //        pDC->SelectObject(pOldP2);
    //    }

    //    // Label
    //    /*CString langLabel = m_bJapanese ? _T("\u65E5\u672C\u8A9E")
    //        : _T("English");*/

    //    CString langLabel =
    //        (m_pBattDlg && m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::EN)
    //        ? _T("日本語")
    //        : _T("English");

    //    int arrowW = max(8, bW * 10 / 100);
    //    int textX = dotCX + dotR + max(3, bW * 4 / 100);

    //    CRect rcLangTxt(textX,
    //        m_rcBtnLanguage.top,
    //        m_rcBtnLanguage.right - arrowW - max(2, bW * 3 / 100),
    //        m_rcBtnLanguage.bottom);
    //    DrawTextEx(pDC, langLabel, rcLangTxt,
    //        txtColor, BtnFont(bW, bH, 9), false,
    //        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    //    // ">" arrow
    //    CRect rcArrow(m_rcBtnLanguage.right - arrowW - max(2, bW * 3 / 100),
    //        m_rcBtnLanguage.top,
    //        m_rcBtnLanguage.right - max(2, bW * 3 / 100),
    //        m_rcBtnLanguage.bottom);
    //    DrawTextEx(pDC, _T(">"), rcArrow,
    //        arrowClr, BtnFont(bW, bH, 9), false,
    //        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    //}

    m_rcBtnLanguage.SetRectEmpty();

    // Advanced Info  (opens the full-screen CABIDlg — see OnLButtonUp)
    m_rcBtnAdvancedQA = CRect(
        innerLeft + btnW + gapW,
        btnAreaTop,
        innerLeft + btnW * 2 + gapW,
        btnAreaBottom);

    DrawQAButton(
        pDC,
        m_rcBtnAdvancedQA,
        L(_T("Advanced Info"),
            _T("詳細情報")),
        RGB(255, 255, 255),
        CLR_DARK_TEXT,
        CLR_BORDER,
        btnRadius);


    // Data & History
    m_rcBtnDataHistory = CRect(
        innerLeft + (btnW + gapW) * 2,
        btnAreaTop,
        innerLeft + (btnW + gapW) * 2 + btnW,
        btnAreaBottom);

    DrawQAButton(
        pDC,
        m_rcBtnDataHistory,
        L(_T("Data and History"),
            _T("データと履歴")),
        RGB(255, 255, 255),
        CLR_DARK_TEXT,
        CLR_BORDER,
        btnRadius);


}