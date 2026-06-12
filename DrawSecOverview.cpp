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

// ─────────────────────────────────────────────────────────────────
// Draw Circular Gauge — fully responsive
// ─────────────────────────────────────────────────────────────────
void CMFCUIDlg::DrawCircularGauge(
    CDC* pDC,
    CPoint center,
    int    radius,
    float  percent)
{
    if (radius < 8) return;

    float gdiStart = 225.0f;
    float sweepTotal = 270.0f;

    auto DrawArc = [&](float angleDeg, float sweepDeg,
        COLORREF col, int penWidth)
        {
            if (sweepDeg <= 0.0f) return;
            CPen arcPen(PS_SOLID, penWidth, col);
            CPen* pOld = pDC->SelectObject(&arcPen);
            pDC->SelectStockObject(NULL_BRUSH);

            int N = max(4, (int)(sweepDeg * 2.0f));
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
            pDC->Polyline(pts.data(), (int)pts.size());
            pDC->SelectObject(pOld);
        };

    int trackW = max(3, radius / 6);

    // Track background
    DrawArc(gdiStart, sweepTotal, RGB(220, 225, 235), trackW);

    // Filled portion
    if (percent > 0.0f)
    {
        float filled = sweepTotal * percent;
        float greenPart = filled * 0.88f;
        float yellowPart = filled * 0.12f;
        DrawArc(gdiStart, greenPart, CLR_GREEN, trackW);
        DrawArc(gdiStart + greenPart, yellowPart, CLR_YELLOW, trackW);
    }

    // ── Center text: "78%" ───────────────────────────────────────
    int pctVal = (int)(percent * 100.0f);
    CString strNum;
    strNum.Format(_T("%d"), pctVal);

    int numFS = max(6, radius * 52 / 100);
    int symFS = max(5, radius * 22 / 100);

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

    status = m_pBattDlg->statusText;
	percentText = m_pBattDlg->pct;



    int W = m_clientWidth;
    int H = m_clientHeight;
    int mx = SW(16, W);

    // ── Card bounds ──────────────────────────────────────────────
    int cardTop = SH(76, H);
    int cardBottom = SH(296, H);
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
    DrawTextEx(pDC, _T("Battery Overview"), rcTitle,
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

    // ── Inner panels layout ──────────────────────────────────────
    int innerTop = divY + CH * 4 / 220;
    int innerBottom = rcCard.bottom - CH * 8 / 220;
    int innerLeft = rcCard.left + CW * 8 / 468;
    int innerRight = rcCard.right - CW * 8 / 468;
    int gap = max(3, CW * 6 / 468);
    int midX = innerLeft + (innerRight - innerLeft) / 2 - gap / 2;

    int panelH = innerBottom - innerTop;
    if (panelH < 20) return;

    // ── Left Panel (Status + Gauge) ──────────────────────────────
    CRect rcLeft(innerLeft, innerTop, midX, innerBottom);
    int   LW = rcLeft.Width();
    int   LH = rcLeft.Height();

    if (LW < 20) return;

    DrawRoundRect(pDC, rcLeft, max(4, min(LW, LH) / 10),
        RGB(250, 251, 253), CLR_BORDER);

    // "Status" label
    int sLblH = max(10, LH * 16 / 180);
    CRect rcStatusLbl(
        rcLeft.left + LW * 8 / 100,
        rcLeft.top + LH * 5 / 100,
        rcLeft.right - LW * 4 / 100,
        rcLeft.top + LH * 5 / 100 + sLblH);
    DrawTextEx(pDC, _T("Status"), rcStatusLbl,
        CLR_MID_TEXT, OvFont(LW, LH, 8), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Gauge radius: 32% of smaller panel dimension, clamped
    int gaugeR = min(LW, LH) * 32 / 100;
    gaugeR = max(28, min(gaugeR, 70));

    // Gauge center: horizontally centred, vertically after label
    int gaugeCY = rcLeft.top + LH * 18 / 100 + gaugeR;
    // Clamp so gauge stays inside panel
    gaugeCY = min(gaugeCY, rcLeft.bottom - gaugeR - LH * 20 / 100);
    gaugeCY = max(gaugeCY, rcLeft.top + gaugeR + LH * 5 / 100);

    CPoint gaugeCenter(rcLeft.left + LW / 2, gaugeCY);
    DrawCircularGauge(pDC, gaugeCenter, gaugeR, 0.79f);

    // "► Charging" below gauge
    int belowY = gaugeCY + gaugeR + max(4, LH * 5 / 100);
    int rowH = max(10, LH * 14 / 100);

    if (belowY + rowH <= rcLeft.bottom)
    {
        CRect rcChg(rcLeft.left, belowY,
            rcLeft.right, belowY + rowH);
        
        CString chgText;
        chgText.Format(_T("\u25BA %s"), status);

        DrawTextEx(pDC, chgText, rcChg,
            CLR_GREEN, OvFont(LW, LH, 8), true,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // "Power: 12.5 V"
    int pwrY = belowY + rowH;
    if (pwrY + rowH <= rcLeft.bottom)
    {
        CRect rcPwr(rcLeft.left, pwrY,
            rcLeft.right, pwrY + rowH);
        DrawTextEx(pDC, _T("Power: 12.5 V"), rcPwr,
            CLR_MID_TEXT, OvFont(LW, LH, 8), false,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // ── Right Panel (Capacity) ───────────────────────────────────
    CRect rcRight(midX + gap, innerTop, innerRight, innerBottom);
    int   RW = rcRight.Width();
    int   RH = rcRight.Height();

    if (RW < 20) return;

    DrawRoundRect(pDC, rcRight, max(4, min(RW, RH) / 10),
        RGB(250, 251, 253), CLR_BORDER);

    int rpx = rcRight.left + RW * 8 / 100;   // right panel pad X

    // "Capacity" header
    int capHdrH = max(10, RH * 16 / 180);
    CRect rcCapHdr(rpx,
        rcRight.top + RH * 5 / 100,
        rcRight.right - RW * 4 / 100,
        rcRight.top + RH * 5 / 100 + capHdrH);
    DrawTextEx(pDC, _T("Capacity"), rcCapHdr,
        CLR_MID_TEXT, OvFont(RW, RH, 8), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Sub-divider
    int capDivY = rcRight.top + RH * 26 / 180;
    {
        CPen cp(PS_SOLID, 1, CLR_BORDER);
        CPen* pOld = pDC->SelectObject(&cp);
        pDC->MoveTo(rcRight.left + RW * 6 / 100, capDivY);
        pDC->LineTo(rcRight.right - RW * 6 / 100, capDivY);
        pDC->SelectObject(pOld);
    }

    // Row heights proportional to panel
    int rowUnit = max(12, RH * 16 / 180);

    int y = capDivY + max(4, RH * 6 / 180);

    // Design Capacity
    DrawTextEx(pDC, _T("Design Capacity:"),
        CRect(rpx, y,
            rcRight.right - RW * 4 / 100, y + rowUnit),
        CLR_MID_TEXT, OvFont(RW, RH, 7), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    y += rowUnit;

    DrawTextEx(pDC, _T("4,500 mAh"),
        CRect(rpx, y,
            rcRight.right - RW * 4 / 100, y + rowUnit),
        CLR_DARK_TEXT, OvFont(RW, RH, 9), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    y += rowUnit + 4;

    // Full Charge Capacity
    DrawTextEx(pDC, _T("Full Charge Capacity:"),
        CRect(rpx, y,
            rcRight.right - RW * 4 / 100, y + rowUnit),
        CLR_MID_TEXT, OvFont(RW, RH, 7), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    y += rowUnit;

    DrawTextEx(pDC, _T("4,200 mAh"),
        CRect(rpx, y,
            rcRight.right - RW * 4 / 100, y + rowUnit),
        CLR_DARK_TEXT, OvFont(RW, RH, 9), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    y += rowUnit + 4;

    // Current Capacity
    DrawTextEx(pDC, _T("Current Capacity:"),
        CRect(rpx, y,
            rcRight.right - RW * 4 / 100, y + rowUnit),
        CLR_MID_TEXT, OvFont(RW, RH, 7), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    y += rowUnit;

    DrawTextEx(pDC, _T("3,950 mAh"),
        CRect(rpx, y,
            rcRight.right - RW * 4 / 100, y + rowUnit),
        CLR_DARK_TEXT, OvFont(RW, RH, 9), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    y += rowUnit + 4;

    // Cycle Count
    DrawTextEx(pDC, _T("Cycle Count:"),
        CRect(rpx, y,
            rcRight.right - RW * 4 / 100, y + rowUnit),
        CLR_MID_TEXT, OvFont(RW, RH, 7), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    y += rowUnit;

    DrawTextEx(pDC, _T("152"),
        CRect(rpx, y,
            rcRight.right - RW * 4 / 100, y + rowUnit),
        CLR_DARK_TEXT, OvFont(RW, RH, 9), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);
}