// DrawSecOverview.cpp

#include "pch.h"
#include "MFCUIDlg.h"

#include <cmath>
#include <vector>
#include <algorithm>
#include "BatteryHelthDlg.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─────────────────────────────────────────────────────────────────
// Scale font relative to actual card/panel dimensions
// ─────────────────────────────────────────────────────────────────
static int OvFont(int refW, int refH, int baseSize)
{
    float scaleW = (float)refW / 468.0f;
    float scaleH = (float)refH / 220.0f;  // reference card height
    float scale = min(scaleW, scaleH);
    scale = max(0.55f, min(scale, 1.6f));
    return max(6, (int)(baseSize * scale));
}

//// ─────────────────────────────────────────────────────────────────
//// Draw Circular Gauge — fully responsive
//// ─────────────────────────────────────────────────────────────────
//void CMFCUIDlg::DrawCircularGauge(
//    CDC* pDC,
//    CPoint center,
//    int    radius,
//    const CString& percentText,
//    bool   isCharging)
//{
//    if (radius < 8) return;
//
//    int pctVal = _wtoi(percentText);
//    pctVal = max(0, min(pctVal, 100));
//    float percent = pctVal / 100.0f;
//
//    float gdiStart = 225.0f;
//    float sweepTotal = (pctVal >= 100) ? 360.0f : 270.0f;
//
//    auto DrawArc = [&](float angleDeg, float sweepDeg,
//        COLORREF col, int penWidth)
//        {
//            if (sweepDeg <= 0.0f) return;
//            CPen arcPen(PS_SOLID, penWidth, col);
//            CPen* pOld = pDC->SelectObject(&arcPen);
//            pDC->SelectStockObject(NULL_BRUSH);
//
//           /* int N = max(4, (int)(sweepDeg * 2.0f));*/
//            int N = max(360, (int)(sweepDeg * 4.0f));
//            std::vector<POINT> pts;
//            pts.reserve(N + 1);
//            for (int i = 0; i <= N; i++)
//            {
//                float t = (angleDeg + sweepDeg * i / N)
//                    * (float)(M_PI / 180.0);
//                pts.push_back({
//                    center.x + (int)(radius * cos(t)),
//                    center.y + (int)(radius * sin(t)) });
//            }
//
//            if (sweepDeg >= 360.0f)
//                pts.push_back(pts.front());
//
//            pDC->Polyline(pts.data(), (int)pts.size());
//            pDC->SelectObject(pOld);
//        };
//
//    int trackW = max(3, radius / 6);
//
//    // Track background
//    DrawArc(gdiStart, sweepTotal, RGB(220, 225, 235), trackW);
//
//
//    // Filled portion
//    if (percent > 0.0f)
//    {
//        float filled = sweepTotal * percent;
//
//        if (pctVal >= 100)
//        {
//            // Draw a full 360° ring using the SAME arc routine/points
//            // as the track, so it overlaps exactly (no seam/partial arc).
//            DrawArc(gdiStart, 360.0f, CLR_GREEN, trackW);
//        }
//        else if (isCharging)
//        {
//            float greenPart = filled * 0.88f;
//            float yellowPart = filled * 0.12f;
//            DrawArc(gdiStart, greenPart, CLR_GREEN, trackW);
//            DrawArc(gdiStart + greenPart, yellowPart, CLR_BLUE, trackW);
//        }
//        else
//        {
//            if (pctVal <= 20)
//            {
//                DrawArc(gdiStart, filled, CLR_RED, trackW);
//            }
//            else
//            {
//                float greenPart = filled * 0.88f;
//                float yellowPart = filled * 0.12f;
//                DrawArc(gdiStart, greenPart, CLR_GREEN, trackW);
//                DrawArc(gdiStart + greenPart, yellowPart, CLR_YELLOW, trackW);
//            }
//        }
//    }
//
//    // ── Center text: use real string, e.g. "78%" ─────────────────
//    CString strNum = percentText;
//    strNum.Replace(_T("%"), _T(""));   // remove % so we can append our own symbol
//    strNum.Trim();
//
//    int numFS = max(6, radius * 52 / 100);
//    int symFS = max(5, radius * 22 / 100);
//
//    // Measure actual pixel width of the number string
//    LOGFONT lf = {};
//    lf.lfHeight = -MulDiv(numFS, GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
//    lf.lfWeight = FW_BOLD;
//    lf.lfQuality = CLEARTYPE_QUALITY;
//    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
//    CFont numFont;
//    numFont.CreateFontIndirect(&lf);
//    CFont* pOldFont = pDC->SelectObject(&numFont);
//    CSize numSize = pDC->GetTextExtent(strNum);
//    pDC->SelectObject(pOldFont);
//
//    // Measure % symbol width
//    LOGFONT lf2 = {};
//    lf2.lfHeight = -MulDiv(symFS, GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
//    lf2.lfWeight = FW_NORMAL;
//    lf2.lfQuality = CLEARTYPE_QUALITY;
//    _tcscpy_s(lf2.lfFaceName, _T("Segoe UI"));
//    CFont symFont;
//    symFont.CreateFontIndirect(&lf2);
//    pOldFont = pDC->SelectObject(&symFont);
//    CSize symSize = pDC->GetTextExtent(_T("%"));
//    pDC->SelectObject(pOldFont);
//
//    // Total width of "78%" together, centred on gauge center
//    int totalW = numSize.cx + symSize.cx;
//    int startX = center.x - totalW / 2;
//    int textCY = center.y;  // vertical center of number
//
//    // Number rect
//    CRect rcNum(startX,
//        textCY - numSize.cy / 2,
//        startX + numSize.cx,
//        textCY + numSize.cy / 2);
//    DrawTextEx(pDC, strNum, rcNum, CLR_DARK_TEXT, numFS, true,
//        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
//
//    // "%" rect — immediately right of number, raised slightly
//    CRect rcSym(startX + numSize.cx,
//        textCY - numSize.cy / 2,
//        startX + numSize.cx + symSize.cx + 2,
//        textCY);
//    DrawTextEx(pDC, _T("%"), rcSym, CLR_DARK_TEXT, symFS, false,
//        DT_LEFT | DT_TOP | DT_SINGLELINE);
//}


void CMFCUIDlg::DrawCircularGauge(
    CDC* pDC,
    CPoint center,
    int radius,
    const CString& percentText,
    bool isCharging,
    bool isHealth)
{
    if (radius < 8) return;

    int pctVal = _wtoi(percentText);
    pctVal = max(0, min(pctVal, 100));
    float percent = pctVal / 100.0f;

    // Full circle gauge: 0% = 0° filled, 100% = 360° filled
    float gdiStart = 270.0f;   // start at top (12 o'clock)
    float sweepTotal = 360.0f;

    auto DrawArc = [&](float angleDeg, float sweepDeg,
        COLORREF col, int penWidth)
        {
            if (sweepDeg <= 0.0f) return;
            CPen arcPen(PS_SOLID, penWidth, col);
            CPen* pOld = pDC->SelectObject(&arcPen);
            pDC->SelectStockObject(NULL_BRUSH);

            int N = max(360, (int)(sweepDeg * 4.0f));
            std::vector<POINT> pts;
            pts.reserve(N + 1);
            for (int i = 0; i <= N; i++)
            {
                float t = (angleDeg + sweepDeg * i / N)
                    * (float)(M_PI / 180.0);
                pts.push_back({
                    center.x + (int)(radius * cos(t)),
                    center.y + (int)(radius * sin(t)) });
            }

            if (sweepDeg >= 360.0f)
                pts.push_back(pts.front());

            pDC->Polyline(pts.data(), (int)pts.size());
            pDC->SelectObject(pOld);
        };

    int trackW = max(3, radius / 6);

    // Track background — always a full circle
    DrawArc(gdiStart, sweepTotal, RGB(220, 225, 235), trackW);

    // Filled portion — proportional to percent (1% = 3.6°)
    /*if (percent > 0.0f)
    {
        float filled = sweepTotal * percent;

        if (isCharging)
        {
            float greenPart = filled * 0.88f;
            float yellowPart = filled * 0.12f;
            DrawArc(gdiStart, greenPart, CLR_GREEN, trackW);
            DrawArc(gdiStart + greenPart, yellowPart, CLR_BLUE, trackW);
        }
        else
        {
            if (pctVal <= 20)
            {
                DrawArc(gdiStart, filled, CLR_RED, trackW);
            }
            else
            {
                float greenPart = filled * 0.88f;
                float yellowPart = filled * 0.12f;
                DrawArc(gdiStart, greenPart, CLR_GREEN, trackW);
                DrawArc(gdiStart + greenPart, yellowPart, CLR_YELLOW, trackW);
            }
        }
    }*/


    // Filled portion — proportional to percent (1% = 3.6°)
    if (percent > 0.0f)
    {
        float filled = sweepTotal * percent;

        COLORREF gaugeColor = CLR_BLUE;

        if (isHealth)
        {
            if (pctVal >= 80)
                gaugeColor = CLR_GREEN;
            else if (pctVal >= 60)
                gaugeColor = CLR_YELLOW;
            else
                gaugeColor = CLR_RED;
        }
        else
        {
            // Battery is always blue
            gaugeColor = CLR_BLUE;
        }

        DrawArc(gdiStart, filled, gaugeColor, trackW);
    }
    // ── Center text: use real string, e.g. "78%" ─────────────────
    CString strNum = percentText;
    strNum.Replace(_T("%"), _T(""));   // remove % so we can append our own symbol
    strNum.Trim();

    int numFS = max(6, radius * 34 / 100);
    int symFS = max(5, radius * 14 / 100);

    // Measure actual pixel width of the number string
    LOGFONT lf = {};
    lf.lfHeight = -MulDiv(numFS, GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
    lf.lfWeight = FW_BOLD;
    lf.lfQuality = CLEARTYPE_QUALITY;
    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
    CFont numFont;
    numFont.CreateFontIndirect(&lf);
    CFont* pOldFont = pDC->SelectObject(&numFont);
    CSize numSize = pDC->GetTextExtent(strNum);
    pDC->SelectObject(pOldFont);

    // Measure % symbol width
    LOGFONT lf2 = {};
    lf2.lfHeight = -MulDiv(symFS, GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
    lf2.lfWeight = FW_NORMAL;
    lf2.lfQuality = CLEARTYPE_QUALITY;
    _tcscpy_s(lf2.lfFaceName, _T("Segoe UI"));
    CFont symFont;
    symFont.CreateFontIndirect(&lf2);
    pOldFont = pDC->SelectObject(&symFont);
    CSize symSize = pDC->GetTextExtent(_T("%"));
    pDC->SelectObject(pOldFont);

    // Total width of "78%" together, centred on gauge center
    int totalW = numSize.cx + symSize.cx;
    int startX = center.x - totalW / 2;
    int textCY = center.y;  // vertical center of number

    // Number rect
    CRect rcNum(startX,
        textCY - numSize.cy / 2,
        startX + numSize.cx,
        textCY + numSize.cy / 2);
    DrawTextEx(pDC, strNum, rcNum, CLR_DARK_TEXT, numFS, true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // "%" rect — immediately right of number, raised slightly
    CRect rcSym(startX + numSize.cx,
        textCY - numSize.cy / 2,
        startX + numSize.cx + symSize.cx + 2,
        textCY);
    DrawTextEx(pDC, _T("%"), rcSym, CLR_DARK_TEXT, symFS, false,
        DT_LEFT | DT_TOP | DT_SINGLELINE);
}

// ─────────────────────────────────────────────────────────────────
// Draw Battery Overview Card
// ─────────────────────────────────────────────────────────────────
void CMFCUIDlg::DrawBatteryOverview(CDC* pDC, CRect rc)
{

    CString status = _T("Unknown");

    CString percentText = _T("Unknown");

    CString designCapacity = _T("Unknown");
    CString fullCapacity = _T("Unknown");
    CString currentCapacity = _T("Unknown");
    CString cycles = _T("Unknown");
    CString health = _T("Unknown");

    health = m_pBattDlg->healthStr;

    status = m_pBattDlg->statusText;
    percentText = m_pBattDlg->pct;

    designCapacity = m_pBattDlg->designCapStr;
    fullCapacity = m_pBattDlg->fullCapStr;
    currentCapacity = m_pBattDlg->currCapOut;
    cycles = m_pBattDlg->cycles;


    int W = m_clientWidth;
    int H = m_clientHeight;
    int mx = SW(16, W);

    // ── Card bounds ──────────────────────────────────────────────
    int cardTop = SH(52, H);
    int cardBottom = SH(272, H);
    CRect rcCard(mx, cardTop, W - mx, cardBottom);

    if (rcCard.Width() < 60 || rcCard.Height() < 40) return;

    DrawRoundRect(pDC, rcCard, SW(10, W), CLR_CARD, CLR_BORDER);

    int CW = rcCard.Width();
    int CH = rcCard.Height();
    int pad = max(6, CW * 14 / 468);

    // ── Card title ───────────────────────────────────────────────
    int titleH = max(14, CH * 20 / 220);
    CRect rcTitle(
        rcCard.left + pad,
        rcCard.top + CH * 8 / 220,
        rcCard.right - pad,
        rcCard.top + CH * 8 / 220 + titleH);
    DrawTextEx(pDC,
        L(_T("Battery Overview"),
            _T("バッテリー概要")),
        rcTitle,
        CLR_TITLE, OvFont(CW, CH, 10), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Divider
    int divY = rcCard.top + CH * 32 / 220;
    {
        CPen divPen(PS_SOLID, 1, CLR_BORDER);
        CPen* pOld = pDC->SelectObject(&divPen);
        pDC->MoveTo(rcCard.left + CW * 10 / 468, divY);
        pDC->LineTo(rcCard.right - CW * 10 / 468, divY);
        pDC->SelectObject(pOld);
    }

    // ── 3-Card layout: Battery | Health | Capacity ──────────────
    int innerTop = divY + CH * 4 / 220;
    int innerBottom = rcCard.bottom - CH * 8 / 220;
    int innerLeft = rcCard.left + CW * 8 / 468;
    int innerRight = rcCard.right - CW * 8 / 468;
    int gap = max(4, CW * 6 / 468);
    int totalInner = innerRight - innerLeft;

    // Each of the 3 cards gets equal width
    int cardW3 = (totalInner - 2 * gap) / 3;

    int card1L = innerLeft;
    int card1R = card1L + cardW3;
    int card2L = card1R + gap;
    int card2R = card2L + cardW3;
    int card3L = card2R + gap;
    int card3R = innerRight;   // absorb any rounding remainder

    int panelH = innerBottom - innerTop;
    if (panelH < 20) return;

    CString pctStr = _T("0%");
    bool charging = false;
    if (m_pBattDlg)
    {
        pctStr = m_pBattDlg->pct;
        charging = m_pBattDlg->IsCharging();
    }

    // ── Helper lambda: draw a single gauge card ──────────────────
    // title      : card header text
    // gaugeVal   : string passed to DrawCircularGauge
    // isHealth   : false = battery (blue), true = health (green/yellow/red)
    // subText    : optional small text below gauge (e.g. charging state) — "" to skip
    // subColor   : color for subText
    auto DrawGaugeCard = [&](CRect rcPanel,
        const CString& title,
        const CString& gaugeVal,
        bool           isChargingGauge,
        bool           isHealthGauge,
        const CString& subText,
        COLORREF       subColor)
        {
            int PW = rcPanel.Width();
            int PH = rcPanel.Height();
            if (PW < 20 || PH < 20) return;

            DrawRoundRect(pDC, rcPanel, max(4, min(PW, PH) / 10),
                RGB(250, 251, 253), CLR_BORDER);

            // Title
            int titleLblH = max(14, PH * 22 / 180);
            CRect rcTitleLbl(
                rcPanel.left + PW * 8 / 100,
                rcPanel.top + PH * 5 / 100,
                rcPanel.right - PW * 4 / 100,
                rcPanel.top + PH * 5 / 100 + titleLblH);
            DrawTextEx(pDC, title, rcTitleLbl,
                CLR_MID_TEXT, OvFont(PW, PH, 14), true,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // Gauge radius — same formula for both cards so circles are equal size
            int gaugeR = min(PW, PH) * 34 / 100;
            gaugeR = max(30, min(gaugeR, 74));

            // Gauge center — horizontally centred in card, vertically after title
            int gaugeCY = rcPanel.top + PH * 18 / 100 + gaugeR;
            gaugeCY = min(gaugeCY, rcPanel.bottom - gaugeR - PH * 20 / 100);
            gaugeCY = max(gaugeCY, rcPanel.top + gaugeR + PH * 5 / 100);
            CPoint gaugeCenter(rcPanel.left + PW / 2, gaugeCY);

            DrawCircularGauge(pDC, gaugeCenter, gaugeR, gaugeVal,
                isChargingGauge, isHealthGauge);

            // Sub-label below gauge (charging state / nothing)
            int rowH = max(14, PH * 16 / 100);
            int labelY = gaugeCY + gaugeR + max(2, PH * 2 / 100);

            if (!subText.IsEmpty() && labelY + rowH <= rcPanel.bottom)
            {
                CRect rcSub(rcPanel.left, labelY, rcPanel.right, labelY + rowH);
                DrawTextEx(pDC, subText, rcSub,
                    subColor, OvFont(PW, PH, 11), true,
                    DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
        };

    // Build charging state string
    CString stateText;
    if (m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::EN)
        stateText = charging ? L"\u25BA Charging" : L"\u25BA Discharging";
    else
        stateText = charging ? L"\u25BA 充電中" : L"\u25BA 放電中";

    // Card 1 — Battery
    CRect rcBattCard(card1L, innerTop, card1R, innerBottom);
    DrawGaugeCard(rcBattCard,
        L(_T("Battery"), _T("バッテリー")),
        pctStr,
        charging, false,
        stateText,
        charging ? CLR_GREEN : CLR_RED);

    // Card 2 — Health
    CRect rcHealthCard(card2L, innerTop, card2R, innerBottom);
    DrawGaugeCard(rcHealthCard,
        L(_T("Health"), _T("健康")),
        health,
        false, true,
        _T(""), RGB(0, 0, 0));

    // ── Card 3 (Capacity) ────────────────────────────────────────
    CRect rcRight(card3L, innerTop, card3R, innerBottom);
    int   RW = rcRight.Width();
    int   RH = rcRight.Height();

    if (RW < 20) return;

    DrawRoundRect(pDC, rcRight, max(4, min(RW, RH) / 10),
        RGB(250, 251, 253), CLR_BORDER);

    int rpx = rcRight.left + RW * 8 / 100;   // right panel pad X

    // "Capacity" header
    int capHdrH = max(16, RH * 24 / 180);
    CRect rcCapHdr(rpx,
        rcRight.top + RH * 5 / 100,
        rcRight.right - RW * 4 / 100,
        rcRight.top + RH * 5 / 100 + capHdrH);
    DrawTextEx(pDC,
        L(_T("Capacity"),
            _T("容量")),
        rcCapHdr,
        CLR_MID_TEXT, OvFont(RW, RH, 14), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Sub-divider
    int capDivY = rcRight.top + RH * 34 / 180;
    {
        CPen cp(PS_SOLID, 1, CLR_BORDER);
        CPen* pOld = pDC->SelectObject(&cp);
        pDC->MoveTo(rcRight.left + RW * 6 / 100, capDivY);
        pDC->LineTo(rcRight.right - RW * 6 / 100, capDivY);
        pDC->SelectObject(pOld);
    }

    // Row heights proportional to panel
    int rowUnit = max(22, RH * 26 / 180);

    int y = capDivY + max(8, RH * 10 / 180);

    //// Design Capacity
    //DrawTextEx(pDC,
    //    L(_T("Design Capacity:"),
    //        _T("設計容量:")),
    //    CRect(rpx, y,
    //        rcRight.right - RW * 4 / 100, y + rowUnit),
    //    CLR_MID_TEXT, OvFont(RW, RH, 7), false,
    //    DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    //y += rowUnit;

    //DrawTextEx(pDC, designCapacity,
    //    CRect(rpx, y,
    //        rcRight.right - RW * 4 / 100, y + rowUnit),
    //    CLR_DARK_TEXT, OvFont(RW, RH, 11), true,
    //    DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    //y += rowUnit + 4;

    //// Full Charge Capacity
    //DrawTextEx(pDC,
    //    L(_T("Full Charge Capacity:"),
    //        _T("満充電容量:")),
    //    CRect(rpx, y,
    //        rcRight.right - RW * 4 / 100, y + rowUnit),
    //    CLR_MID_TEXT, OvFont(RW, RH, 7), false,
    //    DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    //y += rowUnit;

    //DrawTextEx(pDC, fullCapacity,
    //    CRect(rpx, y,
    //        rcRight.right - RW * 4 / 100, y + rowUnit),
    //    CLR_DARK_TEXT, OvFont(RW, RH, 11), true,
    //    DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    //y += rowUnit + 4;

    //// Current Capacity
    //DrawTextEx(pDC,
    //    L(_T("Current Capacity:"),
    //        _T("現在容量:")),
    //    CRect(rpx, y,
    //        rcRight.right - RW * 4 / 100, y + rowUnit),
    //    CLR_MID_TEXT, OvFont(RW, RH, 7), false,
    //    DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    //y += rowUnit;

    //DrawTextEx(pDC, currentCapacity,
    //    CRect(rpx, y,
    //        rcRight.right - RW * 4 / 100, y + rowUnit),
    //    CLR_DARK_TEXT, OvFont(RW, RH, 11), true,
    //    DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    //y += rowUnit + 4;

    //// Cycle Count
    //DrawTextEx(pDC,
    //    L(_T("Cycle Count:"),
    //        _T("サイクル回数:")),
    //    CRect(rpx, y,
    //        rcRight.right - RW * 4 / 100, y + rowUnit),
    //    CLR_MID_TEXT, OvFont(RW, RH, 7), false,
    //    DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    //y += rowUnit;

    //DrawTextEx(pDC, cycles,
    //    CRect(rpx, y,
    //        rcRight.right - RW * 4 / 100, y + rowUnit),
    //    CLR_DARK_TEXT, OvFont(RW, RH, 11), true,
    //    DT_LEFT | DT_VCENTER | DT_SINGLELINE);



    // ── Capacity 2-Column Layout ─────────────────────────────

    int leftX = rpx;
    int rightX = rcRight.left + RW * 55 / 100;

    int labelFont = OvFont(RW, RH, 13);
    int valueFont = OvFont(RW, RH, 15);

    int rowGap = max(30, RH * 30 / 180);

    // Design Capacity
    DrawTextEx(pDC,
        L(_T("Design Capacity"), _T("設計容量")),
        CRect(leftX, y - 2, rightX - 10, y + rowUnit + 4),
        CLR_MID_TEXT, labelFont, true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawTextEx(pDC,
        designCapacity,
        CRect(rightX, y, rcRight.right - 10, y + rowUnit),
        CLR_DARK_TEXT, valueFont, true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    y += rowGap;

    // Full Charge Capacity
    DrawTextEx(pDC,
        L(_T("Full Charge Capacity"), _T("満充電容量")),
        CRect(leftX, y, rightX - 10, y + rowUnit),
        CLR_MID_TEXT, labelFont, true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawTextEx(pDC,
        fullCapacity,
        CRect(rightX, y, rcRight.right - 10, y + rowUnit),
        CLR_DARK_TEXT, valueFont, true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    y += rowGap;

    // Current Capacity
    DrawTextEx(pDC,
        L(_T("Current Capacity"), _T("現在容量")),
        CRect(leftX, y, rightX - 10, y + rowUnit),
        CLR_MID_TEXT, labelFont, true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawTextEx(pDC,
        currentCapacity,
        CRect(rightX, y, rcRight.right - 10, y + rowUnit),
        CLR_DARK_TEXT, valueFont, true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    y += rowGap;

    // Cycle Count
    DrawTextEx(pDC,
        L(_T("Cycle Count"), _T("サイクル回数")),
        CRect(leftX, y, rightX - 10, y + rowUnit),
        CLR_MID_TEXT, labelFont, true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    DrawTextEx(pDC,
        cycles,
        CRect(rightX, y, rcRight.right - 10, y + rowUnit),
        CLR_DARK_TEXT, valueFont, true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

}