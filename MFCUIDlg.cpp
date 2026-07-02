#include "pch.h"
#include "framework.h"
#include "MFCUIDlg.h"
#include "ABIDlg.h"
#include "DHDlg.h"
#include <vector>
#include <cmath>
#include <algorithm>

#include "BatteryHelthDlg.h"   // ← add this

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define BASE_W  750
#define BASE_H  830

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
    ON_WM_TIMER()
    ON_BN_CLICKED(IDC_BUTTON1, &CMFCUIDlg::OnBnClickedButton1)
END_MESSAGE_MAP()

CMFCUIDlg::CMFCUIDlg(CWnd* pParent)
    : CDialogEx(IDD_MFCUI, pParent)
{
    m_samplesChargeW.resize(kWindowSeconds, kMissing);
    m_samplesDischargeW.resize(kWindowSeconds, kMissing);
}

void CMFCUIDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}




//BOOL CMFCUIDlg::OnInitDialog()
//{
//    if (m_pBattDlg && !m_pBattDlg->HasBattery())
//    {
//       /* AfxMessageBox(
//            (m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
//            ? L"バッテリーが検出されません。"
//            : L"No battery detected.");*/
//
//        MessageBox(
//            (m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
//            ? L"バッテリーが検出されません。"
//            : L"No battery detected.",
//            (m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
//            ? L"エラー"
//            : L"Error",
//            MB_OK | MB_ICONERROR);
//
//        EndDialog(IDCANCEL);      // Close this dialog
//        AfxGetMainWnd()->PostMessage(WM_CLOSE); // Close the application
//
//        return FALSE;
//    }
//
//    CDialogEx::OnInitDialog();
//
//    HICON hIcon = AfxGetApp()->LoadIcon(IDR_ICON);
//
//    SetIcon(hIcon, TRUE);
//    SetIcon(hIcon, FALSE);
//
//    SetWindowText(
//        (m_pBattDlg &&
//            m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
//        ? _T("バッテリーテストツール")
//        : _T("Battery Test Tool"));
//
//    ModifyStyle(0, WS_THICKFRAME);
//
//    SetWindowPos(nullptr, 100, 50, BASE_W, BASE_H, SWP_NOZORDER);
//
//    SetTimer(1, 1000, NULL);
//
//    Invalidate();
//    UpdateWindow();
//
//    return TRUE;
//}

BOOL CMFCUIDlg::OnInitDialog()
{
    if (m_pBattDlg && !m_pBattDlg->HasBattery())
    {
        /* AfxMessageBox(
             (m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
             ? L"バッテリーが検出されません。"
             : L"No battery detected.");*/

        MessageBox(
            (m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
            ? L"バッテリーが検出されません。"
            : L"No battery detected.",
            (m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
            ? L"エラー"
            : L"Error",
            MB_OK | MB_ICONERROR);

        EndDialog(IDCANCEL);      // Close this dialog
        AfxGetMainWnd()->PostMessage(WM_CLOSE); // Close the application

        return FALSE;
    }

    CDialogEx::OnInitDialog();

    HICON hIcon = AfxGetApp()->LoadIcon(IDR_ICON);

    SetIcon(hIcon, TRUE);
    SetIcon(hIcon, FALSE);

    SetWindowText(
        (m_pBattDlg &&
            m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
        ? _T("バッテリーテストツール")
        : _T("Battery Test Tool"));

    // ModifyStyle(0, WS_THICKFRAME);   // REMOVED — disables user resizing

    SetWindowPos(nullptr, 100, 50, BASE_W, BASE_H, SWP_NOZORDER);

    SetTimer(1, 1000, NULL);

    Invalidate();
    UpdateWindow();

    return TRUE;
}

void CMFCUIDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1)
    {
        UpdateChargeRate();
        Invalidate(FALSE);
    }

    CDialogEx::OnTimer(nIDEvent);
}

void CMFCUIDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);

    m_clientWidth = cx;
    m_clientHeight = cy;

    ClampScroll();
    Invalidate();
}

void CMFCUIDlg::UpdateChargeRate()
{
    int lang = 0;

    m_last = m_estimator.GetBatteryTime(lang);

    float chargeW = kMissing;
    float dischargeW = kMissing;

    if (m_last.currentRate_mW > 0)
    {
        chargeW =
            (float)m_last.currentRate_mW / 1000.0f;
    }
    else if (m_last.currentRate_mW < 0)
    {
        dischargeW =
            fabs((float)m_last.currentRate_mW) / 1000.0f;
    }

    m_samplesChargeW[m_cursor] = chargeW;
    m_samplesDischargeW[m_cursor] = dischargeW;

    m_cursor = (m_cursor + 1) % kWindowSeconds;

    if (!m_filled && m_cursor == 0)
        m_filled = true;
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
    // DrawAdvancedInfo and DrawDataHistory are now in their own dialogs
    // (IDD_ABI / IDD_DH) — no extra height needed here.
    m_totalHeight = rcClient.Height();   // never scrollable

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
    // DrawAdvancedInfo moved to IDD_ABI (CABIDlg)
    // DrawAdvancedInfo(&memDC, rcContent);

    // DrawDataHistory moved to IDD_DH (CDHDlg)
    // DrawDataHistory(&memDC, rcContent);

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
    if (m_bDialogBusy)
    {
        if (m_bDragging)
        {
            m_bDragging = false;
            ReleaseCapture();
        }
        return;
    }

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

   
    if (m_rcBtnSeminarTest.PtInRect(cp))
    {
        if (m_pBattDlg)
            m_pBattDlg->OnBnClickedAuto();
    }
    else if (m_rcBtnAutoTest.PtInRect(cp))
    {
        if (m_pBattDlg)
            m_pBattDlg->OnBnClickedBtnAutoLong();
    }
    /* MessageBox(_T("Auto Test started!"), _T("Auto Test"),
         MB_OK | MB_ICONINFORMATION);*/

         /* else if (m_rcBtnLanguage.PtInRect(cp))
          {
              m_bJapanese = !m_bJapanese;
              Invalidate();
          }*/

    else if (m_rcBtnLanguage.PtInRect(cp))
    {
        if (m_pBattDlg)
        {
            if (m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::EN)
                m_pBattDlg->m_lang = CBatteryHelthDlg::Lang::JP;
            else
                m_pBattDlg->m_lang = CBatteryHelthDlg::Lang::EN;

            m_bJapanese =
                (m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP);

            // ADD THIS
            SetWindowText(
                (m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
                ? _T("バッテリーテストツール")
                : _T("Battery Test Tool"));
        }

        else
        {
            m_bJapanese = !m_bJapanese;
        }

        Invalidate();
    }


    if (m_rcBtnAdvancedQA.PtInRect(cp))
    {
        ShowWindow(SW_HIDE);
        CABIDlg dlg(this);
        dlg.DoModal();
        ShowWindow(SW_SHOW);
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
        CString btnNames[] =
        {
            L(_T("Cpu Load Test"), _T("CPU負荷テスト")),
            L(_T("Discharge Test"), _T("放電テスト")),
            L(_T("Active Life Trend"), _T("アクティブ寿命推移")),
            L(_T("Standby Life Trend"), _T("スタンバイ寿命推移")),
            L(_T("Running Application"), _T("実行中アプリ")),
            L(_T("Detect Manipulation"), _T("改ざん検出")),
            L(_T("Check Power State"), _T("電源状態確認"))
        };

        for (int i = 0; i < 7; i++)
        {
            if (!m_rcAdvBtn[i].IsRectEmpty() &&
                m_rcAdvBtn[i].PtInRect(cp))
            {
                if (!m_pBattDlg)
                {
                    MessageBox(_T("Battery dialog not linked."), _T("Error"), MB_OK | MB_ICONWARNING);
                    break;
                }

                switch (i)
                {
                case 0:
                    m_bDialogBusy = true;
                    m_pBattDlg->OnBnClickedBtnCpuload();
                    m_bDialogBusy = false;   // reset when dialog closes
                    return;
                case 1: // Discharge Test
                    m_pBattDlg->OnBnClickedBtnDischarge();
                    break;
                case 2: // Active Life Trend
                    m_pBattDlg->OnBnClickedBtnActive();
                    break;
                case 3: // Standby Life Trend
                    m_pBattDlg->OnBnClickedBtnStandby();
                    break;
                case 4: // Running Application
                    m_pBattDlg->OnBnClickedBtnBgapp();
                    break;
                case 5: // Detect Manipulation
                    m_pBattDlg->OnBnClickedBtnManipulatioin();
                    break;
                case 6: // Check Power State
                    m_pBattDlg->OnBnClickedSoh();
                    break;
                }
                break;
            }
        }
    }

    // Quick Actions "Data and History" button → open full-screen CDHDlg
    if (m_rcBtnDataHistoryQA.PtInRect(cp))
    {
        ShowWindow(SW_HIDE);
        CDHDlg dlg(this);
        dlg.DoModal();
        ShowWindow(SW_SHOW);
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
        CString dhNames[] =
        {
            L(_T("Charge History"), _T("充電履歴")),
            L(_T("Export to CSV"), _T("CSV出力")),
            L(_T("Sleep Logs"), _T("スリープログ")),
            L(_T("Usage History"), _T("使用履歴")),
            L(_T("Capacity History"), _T("容量履歴")),
            L(_T("Battery Report"), _T("バッテリーレポート")),
            L(_T("View Power State Logs"), _T("電源状態ログ表示"))
        };
        for (int i = 0; i < 7; i++)
        {
            if (!m_rcDataHistBtn[i].IsRectEmpty() &&
                m_rcDataHistBtn[i].PtInRect(cp))
            {
                if (!m_pBattDlg)
                {
                    MessageBox(_T("Battery dialog not linked."), _T("Error"), MB_OK | MB_ICONWARNING);
                    break;
                }

                switch (i)
                {
                case 0: // Charge and History
                    m_pBattDlg->OnBnClickedBtnHistory();
                    break;
                case 1: // Export to CSV
                    m_pBattDlg->OnBnClickedBtnUploadpdf();
                    break;
                case 2: // Sleep Logs
                    m_pBattDlg->OnBnClickedBtnSleep();
                    break;
                case 3: // Usage History
                    m_pBattDlg->OnBnClickedBtnUsage();
                    break;
                case 4: // Capacity History
                    m_pBattDlg->OnBnClickedBtnCaphis();
                    break;
                case 5: // Battery Report
                    m_pBattDlg->OnBnClickedBtnBreport();
                    break;
                case 6: // View Power State Logs
                    m_pBattDlg->OnBnClickedResult();
                    break;
                }
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
    CString strTitle =
        (m_pBattDlg && m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
        ? _T("バッテリー状態および性能モニター")
        : _T("Battery Health and Performance Monitor");

    DrawTextEx(pDC, strTitle, rcTitle,
        CLR_TITLE, SF(14, W), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    int divY = y + SH(36, H);
    CPen pen(PS_SOLID, 1, CLR_BORDER);
    CPen* pOld = pDC->SelectObject(&pen);
    pDC->MoveTo(mx, divY);
    pDC->LineTo(rc.right - mx, divY);
    pDC->SelectObject(pOld);

    /*CRect rcSub(mx, divY + SH(4, H), rc.right - mx, divY + SH(22, H));
    DrawTextEx(pDC, _T("Device ID: 123456789  |  Model: ABC-123"), rcSub,
        CLR_SUBTITLE, SF(8, W), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);*/

  /*  CRect rcSub(mx, divY + SH(4, H), rc.right - mx, divY + SH(22, H));

    CString strDeviceId;
    if (m_pBattDlg)
        strDeviceId = m_pBattDlg->deviceId;

    DrawTextEx(pDC, strDeviceId, rcSub,
        CLR_SUBTITLE, SF(8, W), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);*/

}


CString CMFCUIDlg::L(const CString& en, const CString& jp)
{
    return (m_pBattDlg &&
        m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
        ? jp
        : en;
}
void CMFCUIDlg::OnBnClickedButton1()
{
    if (!m_pBattDlg)
        return;

    // Count clicks
    m_langButtonClickCount++;

    if (m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
    {
        m_pBattDlg->m_lang = CBatteryHelthDlg::Lang::EN;
    }
    else
    {
        m_pBattDlg->m_lang = CBatteryHelthDlg::Lang::JP;
    }

    // Hide after 2 clicks
    if (m_langButtonClickCount >= 2)
    {
        GetDlgItem(IDC_BUTTON1)->ShowWindow(SW_HIDE);
    }

    m_pBattDlg->GetStaticBatteryInfo();


    Invalidate();
    UpdateWindow();
}
