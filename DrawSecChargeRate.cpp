// DrawSecChargeRate.cpp
// Charge / Discharge Rate card for CMFCUIDlg
// Watt vs Time chart — mirrors CRateInfoDlg::DrawLivePowerChart style
// but rendered with GDI (CDC) instead of GDI+.
//
// NOTE: No NOMINMAX / #undef tricks here.
// We use __min / __max (MSVC intrinsics) everywhere so we never
// conflict with the Windows min/max macros that MFC headers define.

#include "pch.h"
#include "MFCUIDlg.h"

#include <cmath>
#include <vector>
#include <algorithm>   // std::min_element

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─────────────────────────────────────────────────────────────────
// Scale a font relative to card dimensions
// ─────────────────────────────────────────────────────────────────
static int CardFont(int cardW, int cardH, int baseSize)
{
    float scaleW = (float)cardW / 468.0f;
    float scaleH = (float)cardH / 192.0f;
    float scale = __min(scaleW, scaleH);
    scale = __max(0.55f, __min(scale, 1.6f));
    return __max(6, (int)(baseSize * scale));
}

// ─────────────────────────────────────────────────────────────────
// Helper: draw a single text string via CDC
// ─────────────────────────────────────────────────────────────────
static void CardText(CDC* pDC, const CString& str, CRect rc,
    COLORREF clr, int ptSize, bool bold,
    UINT fmt = DT_LEFT | DT_VCENTER | DT_SINGLELINE)
{
    LOGFONT lf = {};
    lf.lfHeight = -MulDiv(ptSize, GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfQuality = CLEARTYPE_QUALITY;
    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
    CFont font;
    font.CreateFontIndirect(&lf);
    CFont* pOld = pDC->SelectObject(&font);
    pDC->SetTextColor(clr);
    pDC->SetBkMode(TRANSPARENT);
    pDC->DrawText(str, &rc, fmt);
    pDC->SelectObject(pOld);
}

// ─────────────────────────────────────────────────────────────────
// Filled + outlined area waveform
// topPts : pixel points along the top of the wave, left to right
// yBaseline : Y pixel for 0 W (bottom of chart)
// ─────────────────────────────────────────────────────────────────
static void FillWave(CDC* pDC,
    const std::vector<POINT>& topPts,
    int yBaseline,
    COLORREF fillColor,
    COLORREF lineColor)
{
    if (topPts.size() < 2) return;

    // Closed polygon: baseline → top curve → baseline
    std::vector<POINT> poly;
    poly.reserve(topPts.size() + 2);
    poly.push_back({ topPts.front().x, yBaseline });
    for (const auto& p : topPts) poly.push_back(p);
    poly.push_back({ topPts.back().x, yBaseline });

    CBrush  fillBr(fillColor);
    CPen    nullPen(PS_NULL, 0, (COLORREF)0);
    CBrush* pOldB = pDC->SelectObject(&fillBr);
    CPen* pOldP = pDC->SelectObject(&nullPen);
    pDC->Polygon(poly.data(), (int)poly.size());
    pDC->SelectObject(pOldB);
    pDC->SelectObject(pOldP);

    // Top outline
    CPen linePen(PS_SOLID, 2, lineColor);
    pOldP = pDC->SelectObject(&linePen);
    pDC->Polyline(topPts.data(), (int)topPts.size());
    pDC->SelectObject(pOldP);
}

// ─────────────────────────────────────────────────────────────────
// DrawChargeRate
// Card: SH(410) .. SH(602) — same vertical slot as original.
// Chart: Watt (Y) vs Time in seconds (X), single active series,
//        auto-scaled Y, 5 grid lines, area fill, live badge,
//        peak annotation, legend.
// ─────────────────────────────────────────────────────────────────
void CMFCUIDlg::DrawChargeRate(CDC* pDC, CRect /*rc*/)
{
    const int W = m_clientWidth;
    const int H = m_clientHeight;
    const int mx = SW(16, W);

    // ── Card bounds (same as original) ───────────────────────────
    const int cardTop = SH(410, H);
    const int cardBottom = SH(602, H);
    CRect rcCard(mx, cardTop, W - mx, cardBottom);
    if (rcCard.Width() < 60 || rcCard.Height() < 40) return;

    // Card background + border
    {
        CBrush  br(CLR_CARD);
        CPen    pen(PS_SOLID, 1, CLR_BORDER);
        CBrush* pOldB = pDC->SelectObject(&br);
        CPen* pOldP = pDC->SelectObject(&pen);
        int r = SW(10, W);
        pDC->RoundRect(rcCard, CPoint(r, r));
        pDC->SelectObject(pOldB);
        pDC->SelectObject(pOldP);
    }

    const int CW = rcCard.Width();
    const int CH = rcCard.Height();
    const int pad = __max(6, CW * 14 / 468);

    // ── Title ────────────────────────────────────────────────────
    const int titleH = __max(16, CH * 20 / 192);
    CRect rcTitle(rcCard.left + pad,
        rcCard.top + CH * 6 / 192,
        rcCard.right - pad,
        rcCard.top + CH * 6 / 192 + titleH);
    CardText(pDC, _T("Charge / Discharge Rate (W vs Time)"),
        rcTitle, CLR_TITLE, CardFont(CW, CH, 10), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // ── Divider ──────────────────────────────────────────────────
    const int divY = rcCard.top + CH * 28 / 192;
    {
        CPen  divPen(PS_SOLID, 1, CLR_BORDER);
        CPen* pOldP = pDC->SelectObject(&divPen);
        pDC->MoveTo(rcCard.left + CW * 10 / 468, divY);
        pDC->LineTo(rcCard.right - CW * 10 / 468, divY);
        pDC->SelectObject(pOldP);
    }

    // ── Current Rate label + value ───────────────────────────────
    const int rateRowH = __max(14, CH * 18 / 192);
    const int rateTop = divY + CH * 4 / 192;

    CRect rcRateLbl(rcCard.left + pad,
        rateTop,
        rcCard.left + pad + CW * 85 / 468,
        rateTop + rateRowH);
    CardText(pDC, _T("Current Rate:"), rcRateLbl,
        CLR_MID_TEXT, CardFont(CW, CH, 8), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CString  rateText;
    COLORREF rateClr = CLR_MID_TEXT;

    if (m_last.currentRate_mW > 0)
    {
        rateText.Format(_T("+%.2f W  Charging"),
            m_last.currentRate_mW / 1000.0);
        rateClr = CLR_GREEN;
    }
    else if (m_last.currentRate_mW < 0)
    {
        rateText.Format(_T("-%.2f W  Discharging"),
            fabs((double)m_last.currentRate_mW) / 1000.0);
        rateClr = CLR_RED;
    }
    else
    {
        rateText = _T("0.00 W  Idle");
        rateClr = CLR_MID_TEXT;
    }

    CRect rcRateVal(rcCard.left + pad + CW * 90 / 468,
        rateTop,
        rcCard.right - pad,
        rateTop + rateRowH);
    CardText(pDC, rateText, rcRateVal,
        rateClr, CardFont(CW, CH, 9), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // ── Which series is active? ───────────────────────────────────
    const bool showCharge = (m_last.currentRate_mW >= 0);
    const std::vector<float>& series =
        showCharge ? m_samplesChargeW : m_samplesDischargeW;
    const int N = kWindowSeconds;

    // ── Chart geometry ────────────────────────────────────────────
    const int yAxisW = __max(22, CW * 38 / 468);   // Y-label column
    const int xPadR = __max(8, CW * 12 / 468);   // right padding
    const int xPadBot = __max(16, CH * 28 / 192);   // X-label row
    const int chartTop = rateTop + rateRowH + CH * 5 / 192;
    const int chartBot = rcCard.bottom - xPadBot;
    const int chartLeft = rcCard.left + yAxisW;
    const int chartRight = rcCard.right - xPadR;
    const int chartW = chartRight - chartLeft;
    const int chartH = chartBot - chartTop;
    if (chartW < 20 || chartH < 20) return;

    // ── Auto-scale Y axis ─────────────────────────────────────────
    float maxW = 1.0f;
    {
        int count = m_filled ? N : m_cursor;
        for (int i = 0; i < count; i++)
        {
            int   idx = m_filled ? (m_cursor + i) % N : i;
            float v = series[idx];
            if (!std::isnan(v) && v > maxW) maxW = v;
        }
    }
    maxW *= 1.15f;                              // 15% headroom
    {
        float rounded = ceilf(maxW * 2.0f) / 2.0f;   // round to 0.5 W
        maxW = (rounded > 1.0f) ? rounded : 1.0f;
    }

    // ── Pixel mapping helpers ─────────────────────────────────────
    // mapX: sample index k (0..N-1) → pixel X
    auto mapX = [&](int k) -> int {
        return chartLeft + (int)((float)k * (float)chartW / (float)(N - 1));
        };
    // mapY: watts → pixel Y (0 W = chartBot, maxW = chartTop)
    auto mapY = [&](float watts) -> int {
        float t = (maxW <= 0.0f) ? 0.0f : (watts / maxW);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return chartBot - (int)(t * (float)chartH);
        };
    // idxOf: logical position k → sample value (NaN if not yet filled)
    auto idxOf = [&](int k) -> float {
        if (!m_filled && k >= m_cursor) return kMissing;
        int base = m_filled ? m_cursor : 0;
        int real = m_filled ? (base + k) % N : k;
        return series[real];
        };

    // ── Y axis: 5 grid lines + labels ────────────────────────────
    const int gridLines = 5;
    const int axisFS = CardFont(CW, CH, 6);

    for (int i = 0; i <= gridLines; i++)
    {
        float val = maxW * (float)i / (float)gridLines;
        int   yPx = mapY(val);
        if (yPx < chartTop - 2 || yPx > chartBot + 2) continue;

        CPen  gridPen(PS_DOT, 1, RGB(210, 215, 225));
        CPen* pOldP = pDC->SelectObject(&gridPen);
        pDC->MoveTo(chartLeft, yPx);
        pDC->LineTo(chartRight, yPx);
        pDC->SelectObject(pOldP);

        CString lbl;
        if (val < 0.05f) lbl = _T("0");
        else             lbl.Format(_T("%.1f"), val);

        CRect rcLbl(rcCard.left + 2, yPx - CH * 8 / 192,
            chartLeft - 3, yPx + CH * 8 / 192);
        CardText(pDC, lbl, rcLbl, CLR_MID_TEXT, axisFS, false,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    // "W" unit at top of Y axis
    {
        CRect rcUnit(rcCard.left + 2, chartTop,
            chartLeft - 3, chartTop + CH * 14 / 192);
        CardText(pDC, _T("W"), rcUnit, CLR_MID_TEXT, axisFS, false,
            DT_RIGHT | DT_TOP | DT_SINGLELINE);
    }

    // ── Axis lines (Y spine + X baseline) ────────────────────────
    {
        CPen  axisPen(PS_SOLID, 1, RGB(180, 185, 200));
        CPen* pOldP = pDC->SelectObject(&axisPen);
        pDC->MoveTo(chartLeft, chartTop);
        pDC->LineTo(chartLeft, chartBot);
        pDC->MoveTo(chartLeft, chartBot);
        pDC->LineTo(chartRight, chartBot);
        pDC->SelectObject(pOldP);
    }

    // ── Build waveform points ─────────────────────────────────────
    std::vector<POINT> wavePts;
    wavePts.reserve(N);
    for (int k = 0; k < N; k++)
    {
        float v = idxOf(k);
        if (!std::isnan(v))
            wavePts.push_back({ mapX(k), mapY(v) });
    }

    // ── Draw area fill (clipped to chart rect) ────────────────────
    {
        CRgn clipRgn;
        clipRgn.CreateRectRgn(chartLeft, chartTop, chartRight, chartBot);
        pDC->SelectClipRgn(&clipRgn);

        if (showCharge)
            FillWave(pDC, wavePts, chartBot, RGB(185, 235, 185), CLR_GREEN);
        else
            FillWave(pDC, wavePts, chartBot, RGB(245, 185, 185), CLR_RED);

        pDC->SelectClipRgn(nullptr);
    }

    // ── X axis: time labels (0 s … 120 s) ────────────────────────
    {
        const int nTicks = 5;
        const int xLblH = __max(10, xPadBot - 4);
        const int halfLW = __max(18, chartW / (nTicks * 2));
        const int xFS = CardFont(CW, CH, 6);

        for (int i = 0; i < nTicks; i++)
        {
            int secVal = (int)((float)N * i / (float)(nTicks - 1));
            int xPx = mapX((int)((float)(N - 1) * i / (float)(nTicks - 1)));

            CString lbl;
            lbl.Format(_T("%ds"), secVal);

            int lx = __max(rcCard.left, xPx - halfLW);
            int rx = __min(rcCard.right, xPx + halfLW);
            CRect rcXLbl(lx, chartBot + 3, rx, chartBot + 3 + xLblH);
            CardText(pDC, lbl, rcXLbl, CLR_MID_TEXT, xFS, false,
                DT_CENTER | DT_TOP | DT_SINGLELINE);
        }

        // "Time (s)" centred below ticks
        CRect rcXUnit(chartLeft, chartBot + 3, chartRight, chartBot + 3 + xLblH);
        CardText(pDC, _T("Time (s)"), rcXUnit, CLR_SUBTITLE,
            __max(axisFS - 1, 5), false,
            DT_CENTER | DT_TOP | DT_SINGLELINE);
    }

    // ── Live value badge (top-right of chart) ────────────────────
    {
        float latest = kMissing;
        if (m_filled || m_cursor > 0)
        {
            int newestIdx = (m_cursor + N - 1) % N;
            latest = series[newestIdx];
        }

        CString  badge;
        COLORREF badgeClr;

        if (std::isnan(latest))
        {
            badge = showCharge ? _T("-- W (not charging)")
                : _T("-- W (not discharging)");
            badgeClr = CLR_SUBTITLE;
        }
        else
        {
            badge.Format(_T("%.2f W"), latest);
            badgeClr = showCharge ? CLR_GREEN : CLR_RED;
        }

        int bdgW = __max(70, CW * 110 / 468);
        int bdgH = __max(12, CH * 16 / 192);
        CRect rcBadge(chartRight - bdgW - 2,
            chartTop + 3,
            chartRight - 2,
            chartTop + 3 + bdgH);
        CardText(pDC, badge, rcBadge, badgeClr,
            CardFont(CW, CH, 8), true,
            DT_RIGHT | DT_TOP | DT_SINGLELINE);
    }

    // ── Peak annotation (only when we have data) ──────────────────
    if ((int)wavePts.size() >= 2)
    {
        // Highest point = lowest Y pixel value
        auto itPeak = std::min_element(wavePts.begin(), wavePts.end(),
            [](const POINT& a, const POINT& b) { return a.y < b.y; });

        if (itPeak != wavePts.end())
        {
            int      px = itPeak->x;
            int      py = itPeak->y;
            COLORREF clrPeak = showCharge ? CLR_GREEN : CLR_RED;

            // Clamp inside chart
            if (py < chartTop + 2) py = chartTop + 2;
            if (py > chartBot - 2) py = chartBot - 2;

            // Dashed drop line from peak to baseline
            {
                CPen  dashPen(PS_DOT, 1, clrPeak);
                CPen* pOldP = pDC->SelectObject(&dashPen);
                pDC->MoveTo(px, py);
                pDC->LineTo(px, chartBot);
                pDC->SelectObject(pOldP);
            }

            // Dot at peak
            const int dotR = __max(3, __min(CW, CH) * 4 / 468);
            {
                CBrush  dotBr(clrPeak);
                CPen    dotPen(PS_SOLID, 1, clrPeak);
                CBrush* pOldB = pDC->SelectObject(&dotBr);
                CPen* pOldP = pDC->SelectObject(&dotPen);
                pDC->Ellipse(px - dotR, py - dotR, px + dotR, py + dotR);
                pDC->SelectObject(pOldB);
                pDC->SelectObject(pOldP);
            }

            // Actual peak watt value
            float peakW = 0.0f;
            {
                int count = m_filled ? N : m_cursor;
                for (int i = 0; i < count; i++)
                {
                    int   idx = m_filled ? (m_cursor + i) % N : i;
                    float v = series[idx];
                    if (!std::isnan(v) && v > peakW) peakW = v;
                }
            }

            // Peak label: above and right of the dot
            int lblH = __max(12, CH * 16 / 192);
            int lblW = __max(60, CW * 90 / 468);
            int lblX = px + dotR + 4;
            if (lblX + lblW > chartRight - 2) lblX = chartRight - lblW - 2;
            int lblY = py - lblH - dotR - 2;
            if (lblY < rcCard.top + 2) lblY = rcCard.top + 2;

            CString peakLbl;
            peakLbl.Format(showCharge ? _T("Peak +%.2f W")
                : _T("Peak -%.2f W"), peakW);

            CRect rcPeak(lblX, lblY, lblX + lblW, lblY + lblH);
            CardText(pDC, peakLbl, rcPeak, clrPeak,
                CardFont(CW, CH, 7), true,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        }
    }

    // ── Legend (bottom-left of chart) ────────────────────────────
    {
        const int legFS = CardFont(CW, CH, 7);
        const int legH = __max(10, CH * 14 / 192);
        const int legW = __max(80, CW * 120 / 468);
        CRect rcLeg(chartLeft + 4,
            chartBot - legH - 3,
            chartLeft + 4 + legW,
            chartBot - 3);

        CString  legTxt = showCharge ? _T("Charge rate") : _T("Discharge rate");
        COLORREF legClr = showCharge ? CLR_GREEN : CLR_RED;
        CardText(pDC, legTxt, rcLeg, legClr, legFS, true,
            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    }
}