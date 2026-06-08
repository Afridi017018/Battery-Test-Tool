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
// Helper: Build a smooth filled waveform polygon (above baseline)
// points: list of (x, value) pairs
// yBaseline: the y pixel of value=0
// positive: if true fill above baseline, else below
// ─────────────────────────────────────────────────────────────────
static void FillWave(
    CDC* pDC,
    const std::vector<std::pair<int, float>>& pts,
    int yBaseline,
    COLORREF fillColor,
    COLORREF lineColor,
    bool above)
{
    if (pts.size() < 2) return;

    // Build polygon: line across top, then back along baseline
    std::vector<POINT> poly;
    poly.reserve(pts.size() * 2 + 2);

    for (auto& p : pts)
        poly.push_back({ p.first, p.second > 0 ? (int)p.second : yBaseline });

    // Close along baseline (right to left)
    for (int i = (int)pts.size() - 1; i >= 0; i--)
        poly.push_back({ pts[i].first, yBaseline });

    // Fill
    CBrush br(fillColor);
    CPen   penNull(PS_NULL, 0, (COLORREF)RGB(0, 0, 0));
    CBrush* pOldB = pDC->SelectObject(&br);
    CPen*   pOldP = pDC->SelectObject(&penNull);
    pDC->Polygon(poly.data(), (int)poly.size());
    pDC->SelectObject(pOldB);
    pDC->SelectObject(pOldP);

    // Draw top line
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
// Card sits below Battery Overview (base: top=308, height=190)
// ─────────────────────────────────────────────────────────────────
void CMFCUIDlg::DrawChargeRate(CDC* pDC, CRect rc)
{
    int W = rc.Width(), H = rc.Height();
    int mx = SW(16, W);

    // ── Card bounds ──────────────────────────────────────────────
    int cardTop    = SH(308, H);
    int cardBottom = SH(500, H);
    CRect rcCard(mx, cardTop, W - mx, cardBottom);
    DrawRoundRect(pDC, rcCard, SW(10, W), CLR_CARD, CLR_BORDER);

    int pad = SW(14, W);

    // ── Card title ───────────────────────────────────────────────
    CRect rcTitle(
        rcCard.left + pad, rcCard.top + SH(8, H),
        rcCard.right - pad, rcCard.top + SH(26, H));
    DrawTextEx(pDC, _T("Charge / Discharge Rate"), rcTitle,
        CLR_TITLE, SF(10, W), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Divider
    int divY = rcCard.top + SH(30, H);
    CPen divPen(PS_SOLID, 1, CLR_BORDER);
    CPen* pOldP = pDC->SelectObject(&divPen);
    pDC->MoveTo(rcCard.left + SW(10, W), divY);
    pDC->LineTo(rcCard.right - SW(10, W), divY);
    pDC->SelectObject(pOldP);

    // ── Current Rate label ───────────────────────────────────────
    CRect rcRate(
        rcCard.left + pad, divY + SH(4, H),
        rcCard.right - pad, divY + SH(22, H));
    DrawTextEx(pDC, _T("Current Rate:"), rcRate,
        CLR_MID_TEXT, SF(8, W), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Value in blue bold (positioned right of label)
    CRect rcRateVal(
        rcCard.left + pad + SW(90, W), divY + SH(4, H),
        rcCard.right - pad,            divY + SH(22, H));
    DrawTextEx(pDC, _T("-2.35 W"), rcRateVal,
        CLR_BLUE, SF(9, W), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // ── Chart area ───────────────────────────────────────────────
    int chartLeft   = rcCard.left  + SW(40, W);  // leave room for Y axis labels
    int chartRight  = rcCard.right - SW(12, W);
    int chartTop    = divY + SH(28, H);
    int chartBottom = rcCard.bottom - SH(28, H); // leave room for X axis labels
    int chartW      = chartRight - chartLeft;
    int chartH      = chartBottom - chartTop;

    // Y baseline = 0W line, at ~65% from chart top (positive values above)
    int yBaseline = chartTop + chartH * 65 / 100;

    // ── Y axis grid lines & labels ───────────────────────────────
    // Draw horizontal grid lines at: 6.0, 2.0, 0.0, -2.0
    struct YLine { float val; };
    std::vector<YLine> yLines = { {6.0f}, {2.0f}, {0.0f}, {-2.0f} };

    float yScale = (float)chartH * 0.60f / 6.0f; // 6W maps to 60% of chart height

    for (auto& yl : yLines)
    {
        int yPx = yBaseline - (int)(yl.val * yScale);

        // Grid line (dashed feel — draw dotted via short segments)
        CPen gridPen(PS_DOT, 1, RGB(210, 215, 225));
        pOldP = pDC->SelectObject(&gridPen);
        pDC->MoveTo(chartLeft, yPx);
        pDC->LineTo(chartRight, yPx);
        pDC->SelectObject(pOldP);

        // Label
        CString lbl;
        if (yl.val == 0.0f) lbl = _T("0");
        else                 lbl.Format(_T("%.0f"), yl.val);

        CRect rcLbl(
            rcCard.left + SW(4, W), yPx - SH(8, H),
            chartLeft   - SW(2, W), yPx + SH(8, H));
        DrawTextEx(pDC, lbl, rcLbl,
            CLR_MID_TEXT, SF(6, W), false,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    // "W" unit label at top-left of chart
    CRect rcUnit(rcCard.left + SW(4, W), chartTop,
                 chartLeft,              chartTop + SH(12, H));
    DrawTextEx(pDC, _T("W"), rcUnit,
        CLR_MID_TEXT, SF(6, W), false,
        DT_RIGHT | DT_TOP | DT_SINGLELINE);

    // ── Waveform data (static, mimics the screenshot) ────────────
    // X goes 0..chartW across 25 minutes
    // Green zone: 0..~38% of width (charge peak 5.48W at ~18%)
    // Blue zone:  ~38%..~58% (near zero, slight positive)
    // Red zone:   ~58%..100% (discharge, peak -6.72W at ~83%)

    int N = 120; // number of sample points

    std::vector<std::pair<int,float>> greenPts, bluePts, redPts;

    for (int i = 0; i <= N; i++)
    {
        float t  = (float)i / N;            // 0..1 across full time
        int   px = chartLeft + (int)(t * chartW);

        // ── Green wave (charging, 0..0.40 of time) ──────────────
        if (t <= 0.40f)
        {
            float lt = t / 0.40f;           // local 0..1
            // bell-ish curve peaking at lt~0.35, peak ~5.48W
            float val = 5.48f
                * (float)exp(-12.0 * (lt - 0.35) * (lt - 0.35));
            val = max(0.0f, val);
            int yPx = yBaseline - (int)(val * yScale);
            greenPts.push_back({ px, (float)yPx });
        }

        // ── Blue wave (near idle, 0.35..0.62 of time) ───────────
        if (t >= 0.35f && t <= 0.62f)
        {
            float lt = (t - 0.35f) / 0.27f;
            float val = 1.2f
                * (float)exp(-8.0 * (lt - 0.5) * (lt - 0.5));
            val = max(0.0f, val);
            int yPx = yBaseline - (int)(val * yScale);
            bluePts.push_back({ px, (float)yPx });
        }

        // ── Red wave (discharging, 0.58..1.0 of time) ───────────
        if (t >= 0.58f)
        {
            float lt = (t - 0.58f) / 0.42f;
            // negative values (discharge) — mirror below baseline
            float val = 6.72f
                * (float)exp(-10.0 * (lt - 0.45) * (lt - 0.45));
            val = max(0.0f, val);
            // flip: below baseline
            int yPx = yBaseline + (int)(val * yScale);
            redPts.push_back({ px, (float)yPx });
        }
    }

    // ── Clip drawing to chart area ───────────────────────────────
    CRgn clipRgn;
    clipRgn.CreateRectRgn(chartLeft, chartTop, chartRight, chartBottom);
    pDC->SelectClipRgn(&clipRgn);

    // Draw green fill (above baseline)
    FillWave(pDC, greenPts, yBaseline,
        RGB(180, 230, 180), RGB(60, 180, 80), true);

    // Draw blue fill (above baseline)
    FillWave(pDC, bluePts, yBaseline,
        RGB(180, 210, 240), RGB(60, 140, 220), true);

    // Draw red fill (below baseline — polygon goes DOWN from baseline)
    if (redPts.size() >= 2)
    {
        std::vector<POINT> redPoly;
        // top edge = baseline
        redPoly.push_back({ redPts.front().first, yBaseline });
        for (auto& p : redPts)
            redPoly.push_back({ p.first, (int)p.second });
        redPoly.push_back({ redPts.back().first, yBaseline });

        CBrush redBr(RGB(240, 180, 180));
        CPen   nullPen(PS_NULL, 0, (COLORREF)0);
        CBrush* pOldB = pDC->SelectObject(&redBr);
        pOldP = pDC->SelectObject(&nullPen);
        pDC->Polygon(redPoly.data(), (int)redPoly.size());
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);

        // Red outline
        CPen redLine(PS_SOLID, 2, CLR_RED);
        pOldP = pDC->SelectObject(&redLine);
        std::vector<POINT> redLine2;
        for (auto& p : redPts)
            redLine2.push_back({ p.first, (int)p.second });
        pDC->Polyline(redLine2.data(), (int)redLine2.size());
        pDC->SelectObject(pOldP);
    }

    // Remove clip
    pDC->SelectClipRgn(nullptr);

    // ── Baseline (0W line) ───────────────────────────────────────
    CPen basePen(PS_SOLID, 1, RGB(180, 185, 195));
    pOldP = pDC->SelectObject(&basePen);
    pDC->MoveTo(chartLeft,  yBaseline);
    pDC->LineTo(chartRight, yBaseline);
    pDC->SelectObject(pOldP);

    // ── Peak annotations ─────────────────────────────────────────
    // Green peak dot + label
    {
        int peakX = chartLeft + (int)(0.40f * 0.35f * chartW);
        int peakY = yBaseline - (int)(5.48f * yScale);

        // Dashed vertical line
        CPen dashPen(PS_DOT, 1, CLR_GREEN);
        pOldP = pDC->SelectObject(&dashPen);
        pDC->MoveTo(peakX, peakY);
        pDC->LineTo(peakX, yBaseline);
        pDC->SelectObject(pOldP);

        // Dot
        CBrush dotBr(CLR_GREEN);
        CPen   dotPen(PS_SOLID, 1, CLR_GREEN);
        pOldP = pDC->SelectObject(&dotPen);
        CBrush* pOldB = pDC->SelectObject(&dotBr);
        pDC->Ellipse(peakX - 4, peakY - 4, peakX + 4, peakY + 4);
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);

        // Label
        CRect rcPkLbl(peakX - SW(30, W), peakY - SH(18, H),
                      peakX + SW(50, W), peakY);
        DrawTextEx(pDC, _T("Peak 5.48 W"), rcPkLbl,
            CLR_GREEN, SF(7, W), true,
            DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
    }

    // Red peak dot + label + "Alert!"
    {
        int peakX = chartLeft + (int)((0.58f + 0.42f * 0.45f) * chartW);
        int peakY = yBaseline + (int)(6.72f * yScale);

        CPen dashPen(PS_DOT, 1, CLR_RED);
        pOldP = pDC->SelectObject(&dashPen);
        pDC->MoveTo(peakX, yBaseline);
        pDC->LineTo(peakX, peakY);
        pDC->SelectObject(pOldP);

        // Dot (hollow circle)
        CBrush hollowBr(RGB(255,255,255));
        CPen   dotPen(PS_SOLID, 2, CLR_RED);
        pOldP = pDC->SelectObject(&dotPen);
        CBrush* pOldB = pDC->SelectObject(&hollowBr);
        pDC->Ellipse(peakX - 5, peakY - 5, peakX + 5, peakY + 5);
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);

        // "Peak -6.72 W" label above
        CRect rcPkLbl(peakX - SW(60, W), peakY - SH(36, H),
                      peakX + SW(10, W), peakY - SH(18, H));
        DrawTextEx(pDC, _T("Peak -6.72 W"), rcPkLbl,
            CLR_RED, SF(7, W), true,
            DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // "Alert!" label to the right
        CRect rcAlert(peakX + SW(6, W),  peakY - SH(10, H),
                      peakX + SW(60, W), peakY + SH(12, H));
        DrawTextEx(pDC, _T("Alert !"), rcAlert,
            CLR_RED, SF(8, W), true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }

    // ── X axis time labels ───────────────────────────────────────
    // 6 labels: 5min, 5min, 10min, 15min, 20min, 25min
    const TCHAR* xLabels[] = {
        _T("5 min"), _T("5 min"), _T("10 min"),
        _T("15 min"), _T("20 min"), _T("25 min") };
    int nLabels = 6;
    for (int i = 0; i < nLabels; i++)
    {
        int xPx = chartLeft + chartW * i / (nLabels - 1);
        CRect rcXLbl(xPx - SW(20, W), chartBottom + SH(2, H),
                     xPx + SW(20, W), chartBottom + SH(16, H));
        DrawTextEx(pDC, xLabels[i], rcXLbl,
            CLR_MID_TEXT, SF(6, W), false,
            DT_CENTER | DT_TOP | DT_SINGLELINE);
    }
}