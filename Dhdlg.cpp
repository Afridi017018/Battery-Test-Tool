// DHDlg.cpp
#include "pch.h"
#include "DHDlg.h"
#include "MFCUIDlg.h"        // CLR_* defines
#include "BatteryHelthDlg.h" // CBatteryHelthDlg::OnBnClickedBtn*()

BEGIN_MESSAGE_MAP(CDHDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_LBUTTONUP()
    ON_WM_SIZE()
END_MESSAGE_MAP()

CDHDlg::CDHDlg(CMFCUIDlg* pOwner, CWnd* pParentWnd)
    : CDialogEx(IDD, pParentWnd), m_pOwner(pOwner)
{
}

void CDHDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BOOL CDHDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Sit exactly on top of the owner so the transition looks seamless
    if (m_pOwner && ::IsWindow(m_pOwner->GetSafeHwnd()))
    {
        CRect rcOwner;
        m_pOwner->GetWindowRect(&rcOwner);
        SetWindowPos(nullptr, rcOwner.left, rcOwner.top,
            rcOwner.Width(), rcOwner.Height(),
            SWP_NOZORDER | SWP_NOACTIVATE);
    }

    CRect rcClient;
    GetClientRect(&rcClient);
    m_clientWidth = max(1, rcClient.Width());
    m_clientHeight = max(1, rcClient.Height());

    return TRUE;
}

void CDHDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    m_clientWidth = max(1, cx);
    m_clientHeight = max(1, cy);
    Invalidate();
}

BOOL CDHDlg::OnEraseBkgnd(CDC* pDC)
{
    return TRUE;  // full repaint in OnPaint
}

void CDHDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect rcClient;
    GetClientRect(&rcClient);

    // Double-buffer
    CDC memDC;
    memDC.CreateCompatibleDC(&dc);
    CBitmap bmp;
    bmp.CreateCompatibleBitmap(&dc, rcClient.Width(), rcClient.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);

    DrawBackground(&memDC, rcClient);
    DrawHeader(&memDC, rcClient);
    DrawRows(&memDC, rcClient);

    dc.BitBlt(0, 0, rcClient.Width(), rcClient.Height(),
        &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

void CDHDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    // Back button
    if (m_rcBack.PtInRect(point))
    {
        EndDialog(IDCANCEL);
        return;
    }

    // Row buttons
    for (int i = 0; i < 7; i++)
    {
        if (m_rcRow[i].PtInRect(point))
        {
            OnRowClicked(i);
            return;
        }
    }

    CDialogEx::OnLButtonUp(nFlags, point);
}

void CDHDlg::OnRowClicked(int index)
{
    if (!m_pOwner || !m_pOwner->m_pBattDlg)
    {
        MessageBox(_T("Battery dialog not linked."), _T("Error"),
            MB_OK | MB_ICONWARNING);
        return;
    }

    CBatteryHelthDlg* pBatt = m_pOwner->m_pBattDlg;

    switch (index)
    {
    case 0: pBatt->OnBnClickedBtnHistory();   break; // Charge History
    case 1: pBatt->OnBnClickedBtnUploadpdf(); break; // Export to CSV
    case 2: pBatt->OnBnClickedBtnSleep();     break; // Sleep Logs
    case 3: pBatt->OnBnClickedBtnUsage();     break; // Usage History
    case 4: pBatt->OnBnClickedBtnCaphis();    break; // Capacity History
    case 5: pBatt->OnBnClickedBtnBreport();   break; // Battery Report
    case 6: pBatt->OnBnClickedResult();       break; // View Power State Logs
    }
}

CString CDHDlg::L(const CString& en, const CString& jp)
{
    if (m_pOwner) return m_pOwner->L(en, jp);
    return en;
}

// ─────────────────────────────────────────────────────────────────
void CDHDlg::DrawBackground(CDC* pDC, CRect rc)
{
    CBrush br(CLR_BG);
    pDC->FillRect(rc, &br);
}

// ─────────────────────────────────────────────────────────────────
// Header: "<" back button  +  "Data and History" title
// ─────────────────────────────────────────────────────────────────
void CDHDlg::DrawHeader(CDC* pDC, CRect rc)
{
    int W = m_clientWidth;
    int mx = max(12, W * 16 / 500);
    int hdrH = max(40, m_clientHeight * 56 / 900);

    // White header bar
    CRect rcHeader(0, 0, W, hdrH);
    CBrush br(CLR_CARD);
    pDC->FillRect(rcHeader, &br);

    // Bottom border line
    CPen pen(PS_SOLID, 1, CLR_BORDER);
    CPen* pOld = pDC->SelectObject(&pen);
    pDC->MoveTo(0, hdrH - 1);
    pDC->LineTo(W, hdrH - 1);
    pDC->SelectObject(pOld);

    // Back circle button (grey background, "<" text)
    int btnSz = max(24, hdrH * 56 / 100);
    int btnX = mx;
    int btnY = (hdrH - btnSz) / 2;
    m_rcBack = CRect(btnX, btnY, btnX + btnSz, btnY + btnSz);

    {
        CBrush cbr(CLR_BG);
        CPen   cpen(PS_SOLID, 1, CLR_BORDER);
        CBrush* pOldB = pDC->SelectObject(&cbr);
        CPen* pOldP = pDC->SelectObject(&cpen);
        pDC->Ellipse(m_rcBack);
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);
    }
    DrawTextEx(pDC, _T("<"), m_rcBack, CLR_DARK_TEXT,
        max(8, btnSz * 50 / 100), true,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Title
    CRect rcTitle(m_rcBack.right + mx / 2, 0, W - mx, hdrH);
    DrawTextEx(pDC,
        L(_T("Data and History"), _T("データと履歴")),
        rcTitle, CLR_TITLE,
        max(10, hdrH * 32 / 100), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}

// ─────────────────────────────────────────────────────────────────
// 7 full-width rows filling the body
// ─────────────────────────────────────────────────────────────────
void CDHDlg::DrawRows(CDC* pDC, CRect rc)
{
    int W = m_clientWidth;
    int H = m_clientHeight;
    int mx = max(12, W * 16 / 500);
    int hdrH = max(40, H * 56 / 900);

    int bodyTop = hdrH + max(8, H * 10 / 900);
    int bodyBottom = H - max(8, H * 10 / 900);
    int bodyH = max(7, bodyBottom - bodyTop);
    int rowH = max(40, bodyH / 7);

    struct BtnDef { CString label; CString icon; COLORREF iconBg; };
    BtnDef defs[7] = {
        { L(_T("Charge History"),       _T("充電履歴")),         _T("C"), RGB(220, 140,  30) },
        { L(_T("Export to CSV"),        _T("CSV出力")),          _T("E"), RGB(40, 180,  80) },
        { L(_T("Sleep Logs"),           _T("スリープログ")),      _T("S"), RGB(30, 120, 220) },
        { L(_T("Usage History"),        _T("使用履歴")),         _T("U"), RGB(160,  80, 220) },
        { L(_T("Capacity History"),     _T("容量履歴")),         _T("H"), RGB(80, 160, 200) },
        { L(_T("Battery Report"),       _T("バッテリーレポート")),_T("B"), RGB(220,  60,  60) },
        { L(_T("View Power State Logs"),_T("電源状態ログ表示")), _T("V"), RGB(100, 100, 120) },
    };

    int fontSize = max(8, (int)(9.0f * min((float)W / 468.0f,
        (float)rowH / 44.0f)));

    // Rounded card behind all rows
    CRect rcCard(mx, bodyTop, W - mx, bodyTop + rowH * 7);
    DrawRoundRect(pDC, rcCard, max(8, W * 10 / 468), CLR_CARD, CLR_BORDER);

    for (int i = 0; i < 7; i++)
    {
        int top = bodyTop + i * rowH;
        CRect rcRow(mx, top, W - mx, top + rowH);
        m_rcRow[i] = rcRow;
        DrawRowButton(pDC, rcRow,
            defs[i].label, defs[i].icon, defs[i].iconBg,
            fontSize);
    }
}

void CDHDlg::DrawRowButton(CDC* pDC, CRect rc,
    const CString& label, const CString& iconText,
    COLORREF iconBg, int fontSize)
{
    if (rc.Width() < 20 || rc.Height() < 10) return;

    int RW = rc.Width();
    int RH = rc.Height();
    int padX = max(6, RW * 14 / 468);
    int midY = rc.top + RH / 2;

    // Coloured icon circle
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
        DrawTextEx(pDC, iconText, rcIcon, RGB(255, 255, 255),
            max(6, iconR * 55 / 100), true,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // ">" arrow on right
    int arrowW = max(10, RW * 20 / 468);
    CRect rcArrow(rc.right - arrowW - max(4, RW * 8 / 468), rc.top,
        rc.right - max(4, RW * 8 / 468), rc.bottom);
    DrawTextEx(pDC, _T(">"), rcArrow, CLR_MID_TEXT, fontSize, false,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // Label
    int   textX = iconCX + iconR + max(6, RW * 8 / 468);
    CRect rcLabel(textX, rc.top,
        rc.right - arrowW - max(4, RW * 8 / 468), rc.bottom);
    DrawTextEx(pDC, label, rcLabel, CLR_DARK_TEXT, fontSize, false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    // Separator line
    CPen sep(PS_SOLID, 1, CLR_BORDER);
    CPen* pOldP = pDC->SelectObject(&sep);
    pDC->MoveTo(rc.left + padX, rc.bottom - 1);
    pDC->LineTo(rc.right - padX, rc.bottom - 1);
    pDC->SelectObject(pOldP);
}

// ─────────────────────────────────────────────────────────────────
void CDHDlg::DrawRoundRect(CDC* pDC, CRect rc, int radius,
    COLORREF bg, COLORREF border)
{
    CBrush br(bg);
    CPen   pen(PS_SOLID, 1, border);
    CBrush* pOldB = pDC->SelectObject(&br);
    CPen* pOldP = pDC->SelectObject(&pen);
    pDC->RoundRect(rc, CPoint(radius, radius));
    pDC->SelectObject(pOldB);
    pDC->SelectObject(pOldP);
}

void CDHDlg::DrawTextEx(CDC* pDC, const CString& text, CRect rc,
    COLORREF color, int fontSize, bool bold, UINT format)
{
    LOGFONT lf = {};
    lf.lfHeight = -MulDiv(fontSize,
        GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfQuality = CLEARTYPE_QUALITY;
    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
    CFont f; f.CreateFontIndirect(&lf);
    CFont* pOld = pDC->SelectObject(&f);
    pDC->SetTextColor(color);
    pDC->SetBkMode(TRANSPARENT);
    pDC->DrawText(text, &rc, format);
    pDC->SelectObject(pOld);
}