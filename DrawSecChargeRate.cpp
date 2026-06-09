// DrawSecChargeRate.cpp

#include "pch.h"
#include "MFCUIDlg.h"

#include <cmath>
#include <vector>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─────────────────────────────────────────────────────────────────
// Scale font relative to a reference card size
// ─────────────────────────────────────────────────────────────────
static int CardFont(int cardW, int cardH, int baseSize)
{
    float scaleW = (float)cardW / 468.0f;   // reference card width at BASE_W=500
    float scaleH = (float)cardH / 192.0f;   // reference card height
    float scale = min(scaleW, scaleH);
    scale = max(0.55f, min(scale, 1.6f));
    return max(6, (int)(baseSize * scale));
}

// ─────────────────────────────────────────────────────────────────
// Filled waveform polygon
// ─────────────────────────────────────────────────────────────────
static void FillWave(
    CDC* pDC,
    const std::vector<std::pair<int, float>>& pts,
    int yBaseline,
    COLORREF fillColor,
    COLORREF lineColor)
{
    if (pts.size() < 2) return;

    std::vector<POINT> poly;
    poly.reserve(pts.size() * 2 + 2);
    for (auto& p : pts)
        poly.push_back({ p.first, (int)p.second });
    for (int i = (int)pts.size() - 1; i >= 0; i--)
        poly.push_back({ pts[i].first, yBaseline });

    CBrush br(fillColor);
    CPen   penNull(PS_NULL, 0, (COLORREF)0);
    CBrush* pOldB = pDC->SelectObject(&br);
    CPen* pOldP = pDC->SelectObject(&penNull);
    pDC->Polygon(poly.data(), (int)poly.size());
    pDC->SelectObject(pOldB);
    pDC->SelectObject(pOldP);

    // Top outline
    CPen linePen(PS_SOLID, 2, lineColor);
    pOldP = pDC->SelectObject(&linePen);
    std::vector<POINT> line;
    for (auto& p : pts)
        line.push_back({ p.first, (int)p.second });
    pDC->Polyline(line.data(), (int)line.size());
    pDC->SelectObject(pOldP);
}

// ─────────────────────────────────────────────────────────────────
// DrawChargeRate
// ─────────────────────────────────────────────────────────────────
void CMFCUIDlg::DrawChargeRate(CDC* pDC, CRect rc)
{
    int W = rc.Width(), H = rc.Height();
    int mx = SW(16, W);

    // ── Card bounds ──────────────────────────────────────────────
    int cardTop = SH(308, H);
    int cardBottom = SH(500, H);
    CRect rcCard(mx, cardTop, W - mx, cardBottom);

    // Guard
    if (rcCard.Width() < 60 || rcCard.Height() < 40) return;

    DrawRoundRect(pDC, rcCard, SW(10, W), CLR_CARD, CLR_BORDER);

    int CW = rcCard.Width();
    int CH = rcCard.Height();
    int pad = max(6, CW * 14 / 468);

    // ── Card title ───────────────────────────────────────────────
    int titleH = max(16, CH * 20 / 192);
    CRect rcTitle(
        rcCard.left + pad, rcCard.top + CH * 8 / 192,
        rcCard.right - pad, rcCard.top + CH * 8 / 192 + titleH);
    DrawTextEx(pDC, _T("Charge / Discharge Rate"), rcTitle,
        CLR_TITLE, CardFont(CW, CH, 10), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Divider under title
    int divY = rcCard.top + CH * 30 / 192;
    {
        CPen divPen(PS_SOLID, 1, CLR_BORDER);
        CPen* pOldP = pDC->SelectObject(&divPen);
        pDC->MoveTo(rcCard.left + CW * 10 / 468, divY);
        pDC->LineTo(rcCard.right - CW * 10 / 468, divY);
        pDC->SelectObject(pOldP);
    }

    // ── "Current Rate:" label + value ────────────────────────────
    int rateRowH = max(14, CH * 18 / 192);
    int rateTop = divY + CH * 4 / 192;

    CRect rcRateLbl(rcCard.left + pad, rateTop,
        rcCard.left + pad + CW * 85 / 468,
        rateTop + rateRowH);
    DrawTextEx(pDC, _T("Current Rate:"), rcRateLbl,
        CLR_MID_TEXT, CardFont(CW, CH, 8), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CRect rcRateVal(rcCard.left + pad + CW * 88 / 468, rateTop,
        rcCard.right - pad, rateTop + rateRowH);
    DrawTextEx(pDC, _T("-2.35 W"), rcRateVal,
        CLR_BLUE, CardFont(CW, CH, 9), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // ── Chart area ───────────────────────────────────────────────
    // Y-axis label column width: proportional to card width
    int yAxisW = max(18, CW * 36 / 468);
    int xPadR = max(8, CW * 12 / 468);
    int xPadBot = max(14, CH * 26 / 192);  // room for X labels
    int chartTop = rateTop + rateRowH + CH * 6 / 192;
    int chartBottom = rcCard.bottom - xPadBot;
    int chartLeft = rcCard.left + yAxisW;
    int chartRight = rcCard.right - xPadR;

    // Clamp so chart never goes outside card
    chartTop = max(chartTop, rcCard.top + 2);
    chartBottom = min(chartBottom, rcCard.bottom - 2);
    chartLeft = max(chartLeft, rcCard.left + 2);
    chartRight = min(chartRight, rcCard.right - 2);

    int chartW = chartRight - chartLeft;
    int chartH = chartBottom - chartTop;

    if (chartW < 20 || chartH < 20) return;

    // Baseline at 62% from top (positive region above)
    int yBaseline = chartTop + chartH * 62 / 100;

    // yScale: 6W maps to 55% of chart height
    float yScale = (float)chartH * 0.55f / 6.0f;

    // ── Y axis grid + labels ─────────────────────────────────────
    struct YLine { float val; };
    std::vector<YLine> yLines = { {6.0f}, {2.0f}, {0.0f}, {-2.0f} };
    int axisFS = CardFont(CW, CH, 6);

    for (auto& yl : yLines)
    {
        int yPx = yBaseline - (int)(yl.val * yScale);
        // Keep inside chart vertically
        if (yPx < chartTop - 2 || yPx > chartBottom + 2) continue;

        CPen gridPen(PS_DOT, 1, RGB(210, 215, 225));
        CPen* pOldP = pDC->SelectObject(&gridPen);
        pDC->MoveTo(chartLeft, yPx);
        pDC->LineTo(chartRight, yPx);
        pDC->SelectObject(pOldP);

        CString lbl;
        if (yl.val == 0.0f) lbl = _T("0");
        else                 lbl.Format(_T("%.0f"), yl.val);

        CRect rcLbl(rcCard.left + 2, yPx - CH * 8 / 192,
            chartLeft - 2, yPx + CH * 8 / 192);
        DrawTextEx(pDC, lbl, rcLbl, CLR_MID_TEXT, axisFS, false,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    // "W" unit
    CRect rcUnit(rcCard.left + 2, chartTop,
        chartLeft - 2, chartTop + CH * 14 / 192);
    DrawTextEx(pDC, _T("W"), rcUnit, CLR_MID_TEXT, axisFS, false,
        DT_RIGHT | DT_TOP | DT_SINGLELINE);

    // ── Waveform sample points ────────────────────────────────────
    int N = 120;
    std::vector<std::pair<int, float>> greenPts, bluePts, redPts;

    for (int i = 0; i <= N; i++)
    {
        float t = (float)i / N;
        int   px = chartLeft + (int)(t * chartW);

        if (t <= 0.40f)
        {
            float lt = t / 0.40f;
            float val = 5.48f * (float)exp(-12.0 * (lt - 0.35) * (lt - 0.35));
            val = max(0.0f, val);
            int yPx = yBaseline - (int)(val * yScale);
            yPx = max(chartTop, min(yPx, chartBottom)); // clamp inside chart
            greenPts.push_back({ px, (float)yPx });
        }
        if (t >= 0.35f && t <= 0.62f)
        {
            float lt = (t - 0.35f) / 0.27f;
            float val = 1.2f * (float)exp(-8.0 * (lt - 0.5) * (lt - 0.5));
            val = max(0.0f, val);
            int yPx = yBaseline - (int)(val * yScale);
            yPx = max(chartTop, min(yPx, chartBottom));
            bluePts.push_back({ px, (float)yPx });
        }
        if (t >= 0.58f)
        {
            float lt = (t - 0.58f) / 0.42f;
            float val = 6.72f * (float)exp(-10.0 * (lt - 0.45) * (lt - 0.45));
            val = max(0.0f, val);
            int yPx = yBaseline + (int)(val * yScale);
            yPx = max(chartTop, min(yPx, chartBottom));
            redPts.push_back({ px, (float)yPx });
        }
    }

    // ── Clip all waveform drawing strictly inside chart rect ──────
    CRgn clipRgn;
    clipRgn.CreateRectRgn(chartLeft, chartTop, chartRight, chartBottom);
    pDC->SelectClipRgn(&clipRgn);

    FillWave(pDC, greenPts, yBaseline,
        RGB(180, 230, 180), RGB(60, 180, 80));
    FillWave(pDC, bluePts, yBaseline,
        RGB(180, 210, 240), RGB(60, 140, 220));

    if (redPts.size() >= 2)
    {
        std::vector<POINT> redPoly;
        redPoly.push_back({ redPts.front().first, yBaseline });
        for (auto& p : redPts)
            redPoly.push_back({ p.first, (int)p.second });
        redPoly.push_back({ redPts.back().first, yBaseline });

        CBrush  redBr(RGB(240, 180, 180));
        CPen    nullPen(PS_NULL, 0, (COLORREF)0);
        CBrush* pOldB = pDC->SelectObject(&redBr);
        CPen* pOldP = pDC->SelectObject(&nullPen);
        pDC->Polygon(redPoly.data(), (int)redPoly.size());
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);

        CPen redLine(PS_SOLID, 2, CLR_RED);
        pOldP = pDC->SelectObject(&redLine);
        std::vector<POINT> redLineP;
        for (auto& p : redPts)
            redLineP.push_back({ p.first, (int)p.second });
        pDC->Polyline(redLineP.data(), (int)redLineP.size());
        pDC->SelectObject(pOldP);
    }

    // Remove waveform clip
    pDC->SelectClipRgn(nullptr);

    // ── Baseline ─────────────────────────────────────────────────
    {
        CPen basePen(PS_SOLID, 1, RGB(180, 185, 195));
        CPen* pOldP = pDC->SelectObject(&basePen);
        pDC->MoveTo(chartLeft, yBaseline);
        pDC->LineTo(chartRight, yBaseline);
        pDC->SelectObject(pOldP);
    }

    // ── Peak annotations (drawn OUTSIDE clip so labels show) ─────
    int annotFS = CardFont(CW, CH, 7);
    int dotSz = max(3, min(CW, CH) * 4 / 468);

    // Green peak
    {
        int peakX = chartLeft + (int)(0.40f * 0.35f * chartW);
        int peakY = yBaseline - (int)(5.48f * yScale);
        peakY = max(chartTop, peakY);   // clamp

        // Dashed drop line
        CPen dashPen(PS_DOT, 1, CLR_GREEN);
        CPen* pOldP = pDC->SelectObject(&dashPen);
        pDC->MoveTo(peakX, peakY);
        pDC->LineTo(peakX, yBaseline);
        pDC->SelectObject(pOldP);

        // Dot
        CBrush dotBr(CLR_GREEN);
        CPen   dotPen(PS_SOLID, 1, CLR_GREEN);
        pOldP = pDC->SelectObject(&dotPen);
        CBrush* pOldB = pDC->SelectObject(&dotBr);
        pDC->Ellipse(peakX - dotSz, peakY - dotSz, peakX + dotSz, peakY + dotSz);
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);

        // Label — anchored above dot, clamped to card
        int lblH = max(12, CH * 16 / 192);
        int lblW = max(50, CW * 80 / 468);
        int lblX = max(rcCard.left + 2, peakX - lblW / 4);
        int lblY = max(rcCard.top + 2, peakY - lblH - dotSz);
        CRect rcLbl(lblX, lblY, lblX + lblW, lblY + lblH);
        DrawTextEx(pDC, _T("Peak 5.48 W"), rcLbl,
            CLR_GREEN, annotFS, true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // Red peak
    {
        int peakX = chartLeft + (int)((0.58f + 0.42f * 0.45f) * chartW);
        int peakY = yBaseline + (int)(6.72f * yScale);
        peakY = min(chartBottom, peakY); // clamp

        CPen dashPen(PS_DOT, 1, CLR_RED);
        CPen* pOldP = pDC->SelectObject(&dashPen);
        pDC->MoveTo(peakX, yBaseline);
        pDC->LineTo(peakX, peakY);
        pDC->SelectObject(pOldP);

        CBrush hollowBr(RGB(255, 255, 255));
        CPen   dotPen(PS_SOLID, 2, CLR_RED);
        pOldP = pDC->SelectObject(&dotPen);
        CBrush* pOldB = pDC->SelectObject(&hollowBr);
        pDC->Ellipse(peakX - dotSz, peakY - dotSz, peakX + dotSz, peakY + dotSz);
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);

        // "Peak -6.72 W" — above the dot
        int lblH = max(12, CH * 16 / 192);
        int lblW = max(60, CW * 90 / 468);
        int lblX = max(rcCard.left + 2, peakX - lblW / 2);
        // clamp right edge inside card
        if (lblX + lblW > rcCard.right - 2)
            lblX = rcCard.right - 2 - lblW;
        int lblY = max(rcCard.top + 2,
            min(peakY - lblH * 2 - dotSz,
                chartBottom - lblH * 2));
        CRect rcPkLbl(lblX, lblY, lblX + lblW, lblY + lblH);
        DrawTextEx(pDC, _T("Peak -6.72 W"), rcPkLbl,
            CLR_RED, annotFS, true,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // "Alert!" — right of dot, clamped
        int alW = max(40, CW * 55 / 468);
        int alX = min(peakX + dotSz + 4, rcCard.right - alW - 2);
        int alY = peakY - lblH / 2;
        CRect rcAlert(alX, alY, alX + alW, alY + lblH);
        DrawTextEx(pDC, _T("Alert !"), rcAlert,
            CLR_RED, annotFS, true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // ── X axis labels ─────────────────────────────────────────────
    const TCHAR* xLabels[] = {
        _T("5 min"), _T("5 min"), _T("10 min"),
        _T("15 min"), _T("20 min"), _T("25 min") };
    int nLabels = 6;
    int xLblH = max(10, xPadBot - 4);
    int xLblHalf = max(16, chartW / (nLabels * 2));

    for (int i = 0; i < nLabels; i++)
    {
        int xPx = chartLeft + chartW * i / (nLabels - 1);
        // clamp label rect inside card horizontally
        int lx = max(rcCard.left, xPx - xLblHalf);
        int rx = min(rcCard.right, xPx + xLblHalf);
        CRect rcXLbl(lx, chartBottom + 2, rx, chartBottom + 2 + xLblH);
        DrawTextEx(pDC, xLabels[i], rcXLbl,
            CLR_MID_TEXT, CardFont(CW, CH, 6), false,
            DT_CENTER | DT_TOP | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}