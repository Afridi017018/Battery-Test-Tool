// DrawSecChargeRate.cpp
// Charge / Discharge Rate card for CMFCUIDlg
//
// CHART LAYOUT  — bipolar waveform
//
//   Y-axis (left):
//     +maxW  ──────  top
//     +mid   - - -
//      0 W   ──────  centre (zero line, also the time axis)
//     -mid   - - -
//     -maxW  ──────  bottom
//
//   X-axis (bottom): time 0 s → 120 s, left to right
//
//   Charge   fills ABOVE zero with green area.
//   Discharge fills BELOW zero with red area.
//
//   Each series is drawn in CONTIGUOUS SEGMENTS only.
//   When the battery is charging  the discharge trace has NaN → gap.
//   When the battery is discharging the charge trace has NaN → gap.
//   Gaps are left blank so the two traces never overlap or mislead.
//
// NOTE: No NOMINMAX / #undef tricks — we use __min / __max throughout.

#include "pch.h"
#include "MFCUIDlg.h"

#include <cmath>
#include <vector>
#include <algorithm>
#include "BatteryHelthDlg.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─────────────────────────────────────────────────────────────────────────────
// Scale a font size relative to card pixel dimensions
// ─────────────────────────────────────────────────────────────────────────────
static int CardFont(int cardW, int cardH, int baseSize)
{
    float scaleW = (float)cardW / 468.0f;
    float scaleH = (float)cardH / 192.0f;
    float scale = __min(scaleW, scaleH);
    scale = __max(0.55f, __min(scale, 1.6f));
    return __max(6, (int)(baseSize * scale));
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw a text string via GDI, using Segoe UI Variable (falls back to Segoe UI)
// ─────────────────────────────────────────────────────────────────────────────
static void CardText(CDC* pDC, const CString& str, CRect rc,
    COLORREF clr, int ptSize, bool bold,
    UINT fmt = DT_LEFT | DT_VCENTER | DT_SINGLELINE)
{
    LOGFONT lf = {};
    lf.lfHeight = -MulDiv(ptSize, GetDeviceCaps(pDC->m_hDC, LOGPIXELSY), 72);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfQuality = CLEARTYPE_QUALITY;
    _tcscpy_s(lf.lfFaceName, _T("Segoe UI Variable"));
    CFont font;
    if (!font.CreateFontIndirect(&lf))
    {
        _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
        font.CreateFontIndirect(&lf);
    }
    CFont* pOld = pDC->SelectObject(&font);
    pDC->SetTextColor(clr);
    pDC->SetBkMode(TRANSPARENT);
    pDC->DrawText(str, &rc, fmt);
    pDC->SelectObject(pOld);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw ONE contiguous waveform segment as a filled area + outline.
// edgePts   : pixel points along the wave edge, left to right, no NaNs.
// yBaseline : Y pixel of the zero line (fill boundary).
// fillColor : semi-transparent-like solid fill colour.
// lineColor : solid outline colour.
// ─────────────────────────────────────────────────────────────────────────────
static void FillSegment(CDC* pDC,
    const std::vector<POINT>& edgePts,
    int    yBaseline,
    COLORREF fillColor,
    COLORREF lineColor)
{
    if (edgePts.size() < 2) return;

    // Closed polygon: baseline-start → edge points → baseline-end
    std::vector<POINT> poly;
    poly.reserve(edgePts.size() + 2);
    poly.push_back({ edgePts.front().x, yBaseline });
    for (const auto& p : edgePts) poly.push_back(p);
    poly.push_back({ edgePts.back().x,  yBaseline });

    CBrush  fillBr(fillColor);
    CPen    nullPen(PS_NULL, 0, (COLORREF)0);
    CBrush* pOldB = pDC->SelectObject(&fillBr);
    CPen* pOldP = pDC->SelectObject(&nullPen);
    pDC->Polygon(poly.data(), (int)poly.size());
    pDC->SelectObject(pOldB);
    pDC->SelectObject(pOldP);

    // Outline only the top edge
    CPen linePen(PS_SOLID, 2, lineColor);
    pOldP = pDC->SelectObject(&linePen);
    pDC->Polyline(edgePts.data(), (int)edgePts.size());
    pDC->SelectObject(pOldP);
}

// ─────────────────────────────────────────────────────────────────────────────
// Draw all contiguous segments of one series.
// Each run of non-NaN samples becomes one filled segment;
// NaN samples (= the other state was active) produce visible gaps.
//
// pts[k].x  comes from mapX(k)
// pts[k].y  comes from mapY(signed_value)  — already mapped to pixel space.
// Pass NaN-sentinel POINT as { -1, -1 } to break segments.
// ─────────────────────────────────────────────────────────────────────────────
static void DrawSegmentedWave(CDC* pDC,
    const std::vector<POINT>& pts,       // length N, some sentinel
    const std::vector<bool>& valid,     // true = real sample
    int   yBaseline,
    COLORREF fillColor,
    COLORREF lineColor)
{
    const int N = (int)pts.size();
    std::vector<POINT> seg;
    seg.reserve(16);

    for (int k = 0; k < N; ++k)
    {
        if (valid[k])
        {
            seg.push_back(pts[k]);
        }
        else
        {
            // End of a run — flush
            FillSegment(pDC, seg, yBaseline, fillColor, lineColor);
            seg.clear();
        }
    }
    // Flush last run
    FillSegment(pDC, seg, yBaseline, fillColor, lineColor);
}

// ─────────────────────────────────────────────────────────────────────────────
// DrawChargeRate  — called from CMFCUIDlg::OnPaint()
// ─────────────────────────────────────────────────────────────────────────────
void CMFCUIDlg::DrawChargeRate(CDC* pDC, CRect /*rc*/)
{
    const int W = m_clientWidth;
    const int H = m_clientHeight;
    const int mx = SW(16, W);

    // ── Card bounds ───────────────────────────────────────────────────────────
    /*const int cardTop = SH(410, H);
    const int cardBottom = SH(602, H);*/

    const int cardTop = SH(522, H);
    const int cardBottom = SH(714, H);   // same 192px height

    CRect rcCard(mx, cardTop, W - mx, cardBottom);
    if (rcCard.Width() < 80 || rcCard.Height() < 60) return;

    // Card background + rounded border
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

    // ── Card title ────────────────────────────────────────────────────────────
    const int titleH = __max(16, CH * 20 / 192);
    CRect rcTitle(rcCard.left + pad,
        rcCard.top + CH * 6 / 192,
        rcCard.right - pad,
        rcCard.top + CH * 6 / 192 + titleH);
    CardText(pDC,
        L(_T("Charge / Discharge Rate  (W vs Time)"),
            _T("充電 / 放電レート (W 対 時間)")),
        rcTitle, CLR_TITLE, CardFont(CW, CH, 10), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // ── Divider ───────────────────────────────────────────────────────────────
    const int divY = rcCard.top + CH * 28 / 192;
    {
        CPen  divPen(PS_SOLID, 1, CLR_BORDER);
        CPen* pOldP = pDC->SelectObject(&divPen);
        pDC->MoveTo(rcCard.left + CW * 10 / 468, divY);
        pDC->LineTo(rcCard.right - CW * 10 / 468, divY);
        pDC->SelectObject(pOldP);
    }

    // ── Current rate status row ───────────────────────────────────────────────
    const int rateRowH = __max(14, CH * 18 / 192);
    const int rateTop = divY + CH * 4 / 192;

    CRect rcRateLbl(rcCard.left + pad,
        rateTop,
        rcCard.left + pad + CW * 85 / 468,
        rateTop + rateRowH);
    CardText(pDC,
        L(_T("Current Rate:"),
            _T("現在レート:")),
        rcRateLbl,
        CLR_MID_TEXT, CardFont(CW, CH, 8), false,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    CString  rateText;
    COLORREF rateClr;
    if (m_last.currentRate_mW > 0)
    {
        rateText.Format(
            (m_pBattDlg && m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
            ? _T("+%.2f W  充電中")
            : _T("+%.2f W  Charging"),
            m_last.currentRate_mW / 1000.0);
        rateClr = CLR_GREEN;
    }
    else if (m_last.currentRate_mW < 0)
    {
        rateText.Format(
            (m_pBattDlg && m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
            ? _T("\u2212%.2f W  放電中")
            : _T("\u2212%.2f W  Discharging"),
            fabs((double)m_last.currentRate_mW) / 1000.0);
        rateClr = CLR_RED;
    }
    else
    {
        rateText =
            L(_T("0.00 W  Idle"),
                _T("0.00 W  アイドル"));
        rateClr = CLR_MID_TEXT;
    }

    // Watt value (left portion of the right side)
    int rateValW = __max(100, CW * 150 / 468);
    CRect rcRateVal(rcCard.left + pad + CW * 90 / 468,
        rateTop,
        rcCard.left + pad + CW * 90 / 468 + rateValW,
        rateTop + rateRowH);
    CardText(pDC, rateText, rcRateVal,
        rateClr, CardFont(CW, CH, 9), true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Inline legend: "● Charge (+W)   ● Discharge (−W)" on the same row, right-aligned
    const int legFS = CardFont(CW, CH, 7);

    CString discLegLbl = L(_T("\u25CF Discharge (\u2212W)"), _T("\u25CF 放電 (\u2212W)"));
    int discLegW = __max(90, CW * 125 / 468);
    CRect rcDiscLeg(rcCard.right - pad - discLegW,
        rateTop,
        rcCard.right - pad,
        rateTop + rateRowH);
    CardText(pDC, discLegLbl, rcDiscLeg, CLR_RED,
        legFS, false, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    CString chargeLegLbl = L(_T("\u25CF Charge (+W)"), _T("\u25CF 充電 (+W)"));
    int chargeLegW = __max(80, CW * 110 / 468);
    CRect rcChargeLeg(rcDiscLeg.left - 8 - chargeLegW,
        rateTop,
        rcDiscLeg.left - 8,
        rateTop + rateRowH);
    CardText(pDC, chargeLegLbl, rcChargeLeg, CLR_GREEN,
        legFS, false, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    // ─────────────────────────────────────────────────────────────────────────
    //  CHART GEOMETRY
    //  Bipolar:  +maxW at top, 0 in the middle, -maxW at bottom.
    //  Y-axis labels: "+x.x W" on the positive side, "-x.x W" on the negative.
    //  yAxisW is widened to accommodate signed labels like "+10.5".
    // ─────────────────────────────────────────────────────────────────────────
    const int N = kWindowSeconds;   // 120

    const int yAxisW = __max(36, CW * 50 / 468);   // wider for "+xx.x" labels
    const int xPadR = __max(8, CW * 12 / 468);
    const int xPadBot = __max(18, CH * 30 / 192);

    const int chartTop = rateTop + rateRowH + CH * 5 / 192;
    const int chartBot = rcCard.bottom - xPadBot;
    const int chartLeft = rcCard.left + yAxisW;
    const int chartRight = rcCard.right - xPadR;
    const int chartW = chartRight - chartLeft;
    const int chartH = chartBot - chartTop;
    if (chartW < 30 || chartH < 30) return;

    //  Zero line sits exactly at the vertical mid-point
    const int yZero = (chartTop + chartBot) / 2;

    // ── Auto-scale: peak of both series together ──────────────────────────────
    float maxW = 1.0f;
    {
        int count = m_filled ? N : m_cursor;
        for (int i = 0; i < count; ++i)
        {
            int cidx = m_filled ? (m_cursor + i) % N : i;
            float vc = m_samplesChargeW[cidx];
            float vd = m_samplesDischargeW[cidx];
            if (!std::isnan(vc) && vc > maxW) maxW = vc;
            if (!std::isnan(vd) && vd > maxW) maxW = vd;
        }
    }
    maxW *= 1.15f;
    {
        float rounded = ceilf(maxW * 2.0f) / 2.0f;   // round to 0.5 W
        maxW = (rounded > 1.0f) ? rounded : 1.0f;
    }

    // ── Pixel-mapping lambdas ─────────────────────────────────────────────────
    // mapX: logical sample index k (0=oldest, N-1=newest) → pixel X
    auto mapX = [&](int k) -> int
        {
            return chartLeft + (int)((float)k * (float)chartW / (float)(N - 1));
        };

    // mapY: signed watts → pixel Y
    //   +maxW → chartTop,   0 → yZero,   -maxW → chartBot
    auto mapY = [&](float watts) -> int
        {
            float t = (maxW <= 0.0f) ? 0.0f : (watts / maxW);
            t = __max(-1.0f, __min(t, 1.0f));
            int halfH = yZero - chartTop;
            return yZero - (int)(t * (float)halfH);
        };

    // idxOf: logical position k → physical ring-buffer index
    auto idxOf = [&](const std::vector<float>& s, int k) -> float
        {
            if (!m_filled && k >= m_cursor) return kMissing;
            int base = m_filled ? m_cursor : 0;
            int real = m_filled ? (base + k) % N : k;
            return s[real];
        };

    // ── Clip to chart rectangle ────────────────────────────────────────────────
    CRgn clipRgn;
    clipRgn.CreateRectRgn(chartLeft, chartTop, chartRight, chartBot);
    pDC->SelectClipRgn(&clipRgn);

    // ─────────────────────────────────────────────────────────────────────────
    //  GRID LINES + Y-AXIS LABELS
    //
    //  We draw 4 lines on each side (+ the explicit zero line below).
    //  Labels:
    //    positive side → "+x.x"  (green tint)
    //    zero          → "0"     (neutral)
    //    negative side → "-x.x"  (red tint, using proper minus U+2212)
    // ─────────────────────────────────────────────────────────────────────────
    const int gridCount = 4;    // lines each side, excluding zero
    const int axisFS = CardFont(CW, CH, 6);
    const int lblHalf = __max(7, CH * 9 / 192);   // half-height of label rect

    for (int i = 1; i <= gridCount; ++i)
    {
        float posW = maxW * (float)i / (float)gridCount;

        // ── Positive grid line ────────────────────────────────────────────────
        {
            int yPx = mapY(+posW);
            if (yPx >= chartTop - 1 && yPx <= chartBot + 1)
            {
                CPen  gPen(PS_DOT, 1, RGB(200, 230, 205));  // green-tinted dots
                CPen* pOldP = pDC->SelectObject(&gPen);
                pDC->MoveTo(chartLeft, yPx);
                pDC->LineTo(chartRight, yPx);
                pDC->SelectObject(pOldP);

                // Label: "+x.x" right-aligned to left of spine
                CString lbl;
                lbl.Format(_T("+%.1f"), posW);
                CRect rcLbl(rcCard.left + 2, yPx - lblHalf,
                    chartLeft - 3, yPx + lblHalf);
                CardText(pDC, lbl, rcLbl, RGB(40, 160, 70), axisFS, false,
                    DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
        }

        // ── Negative grid line ────────────────────────────────────────────────
        {
            int yPx = mapY(-posW);
            if (yPx >= chartTop - 1 && yPx <= chartBot + 1)
            {
                CPen  gPen(PS_DOT, 1, RGB(235, 200, 200));  // red-tinted dots
                CPen* pOldP = pDC->SelectObject(&gPen);
                pDC->MoveTo(chartLeft, yPx);
                pDC->LineTo(chartRight, yPx);
                pDC->SelectObject(pOldP);

                // Label: "−x.x"  (U+2212 proper minus)
                CString lbl;
                lbl.Format(_T("\u2212%.1f"), posW);
                CRect rcLbl(rcCard.left + 2, yPx - lblHalf,
                    chartLeft - 3, yPx + lblHalf);
                CardText(pDC, lbl, rcLbl, RGB(200, 55, 55), axisFS, false,
                    DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
            }
        }
    }

    // "W" unit caption above top grid line
    {
        CRect rcUnit(rcCard.left + 2, chartTop,
            chartLeft - 3, chartTop + CH * 13 / 192);
        CardText(pDC, _T("W"), rcUnit, CLR_MID_TEXT, axisFS, false,
            DT_RIGHT | DT_TOP | DT_SINGLELINE);
    }

    // ── Axis spine (left, full chart height) ──────────────────────────────────
    {
        CPen spinePen(PS_SOLID, 1, RGB(180, 185, 200));
        CPen* pOldP = pDC->SelectObject(&spinePen);
        pDC->MoveTo(chartLeft, chartTop);
        pDC->LineTo(chartLeft, chartBot);
        pDC->SelectObject(pOldP);
    }

    // ── Zero / centre line (solid, slightly darker) ───────────────────────────
    {
        CPen zeroPen(PS_SOLID, 1, RGB(140, 148, 168));
        CPen* pOldP = pDC->SelectObject(&zeroPen);
        pDC->MoveTo(chartLeft, yZero);
        pDC->LineTo(chartRight, yZero);
        pDC->SelectObject(pOldP);

        // "0" label centred on the zero line
        CRect rcZeroLbl(rcCard.left + 2, yZero - lblHalf,
            chartLeft - 3, yZero + lblHalf);
        CardText(pDC, _T("0"), rcZeroLbl, CLR_MID_TEXT, axisFS, false,
            DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    }

    // ─────────────────────────────────────────────────────────────────────────
    //  BUILD PIXEL POINT ARRAYS FOR BOTH SERIES
    //
    //  For each logical sample k (0=oldest … N-1=newest):
    //    chargeValid[k] = true only when m_samplesChargeW[k] is a real value.
    //    dischargValid[k] = true only when m_samplesDischargeW[k] is real.
    //
    //  When a sample is NaN the corresponding valid[] entry is false,
    //  causing DrawSegmentedWave() to leave a visible gap — so the charge trace
    //  is blank during discharge periods and vice versa.
    // ─────────────────────────────────────────────────────────────────────────
    std::vector<POINT> chargePts(N), dischargePts(N);
    std::vector<bool>  chargeValid(N, false), dischargeValid(N, false);

    for (int k = 0; k < N; ++k)
    {
        int xPx = mapX(k);

        float vc = idxOf(m_samplesChargeW, k);
        if (!std::isnan(vc))
        {
            chargePts[k] = { xPx, mapY(+vc) };
            chargeValid[k] = true;
        }

        float vd = idxOf(m_samplesDischargeW, k);
        if (!std::isnan(vd))
        {
            dischargePts[k] = { xPx, mapY(-vd) };  // negate → below zero
            dischargeValid[k] = true;
        }
    }

    // Draw charge area (above zero, green)
    DrawSegmentedWave(pDC, chargePts, chargeValid,
        yZero,
        RGB(185, 235, 185),   // fill
        CLR_GREEN);           // outline

    // Draw discharge area (below zero, red)
    DrawSegmentedWave(pDC, dischargePts, dischargeValid,
        yZero,
        RGB(245, 185, 185),   // fill
        CLR_RED);             // outline

    // Remove clip region
    pDC->SelectClipRgn(nullptr);

    // ─────────────────────────────────────────────────────────────────────────
    //  X-AXIS TIME LABELS   0 s … 120 s
    // ─────────────────────────────────────────────────────────────────────────
    {
        const int nTicks = 5;
        const int xLblH = __max(10, xPadBot - 4);
        const int halfLW = __max(18, chartW / (nTicks * 2));
        const int xFS = CardFont(CW, CH, 6);

        CPen tickPen(PS_SOLID, 1, RGB(150, 158, 178));

        for (int i = 0; i < nTicks; ++i)
        {
            int secVal = (int)((float)N * i / (float)(nTicks - 1));
            int xPx = mapX((int)((float)(N - 1) * i / (float)(nTicks - 1)));

            // Short tick mark below the chart
            {
                CPen* pOldP = pDC->SelectObject(&tickPen);
                pDC->MoveTo(xPx, chartBot);
                pDC->LineTo(xPx, chartBot + 4);
                pDC->SelectObject(pOldP);
            }

            CString lbl;
            lbl.Format(_T("%ds"), secVal);

            int lx = __max(rcCard.left, xPx - halfLW);
            int rx = __min(rcCard.right, xPx + halfLW);
            CRect rcXLbl(lx, chartBot + 4, rx, chartBot + 4 + xLblH);
            CardText(pDC, lbl, rcXLbl, CLR_MID_TEXT, xFS, false,
                DT_CENTER | DT_TOP | DT_SINGLELINE);
        }

        // "Time (s)" caption centred below all ticks
        CRect rcXUnit(chartLeft, chartBot + 4,
            chartRight, chartBot + 4 + xLblH);
        CardText(pDC,
            L(_T("Time (s)"),
                _T("時間 (秒)")),
            rcXUnit, CLR_SUBTITLE,
            __max(axisFS - 1, 5), false,
            DT_CENTER | DT_TOP | DT_SINGLELINE);
    }

//    // ─────────────────────────────────────────────────────────────────────────────
////  LIVE VALUE BADGE + INLINE LEGEND  (top-right corner of chart, one line)
//// ─────────────────────────────────────────────────────────────────────────────
//    {
//        auto latestOf = [&](const std::vector<float>& s) -> float
//            {
//                if (!m_filled && m_cursor == 0) return kMissing;
//                return s[(m_cursor + N - 1) % N];
//            };
//
//        CString  badge;
//        COLORREF badgeClr;
//        float    latestC = latestOf(m_samplesChargeW);
//        float    latestD = latestOf(m_samplesDischargeW);
//
//        if (m_last.currentRate_mW > 0 && !std::isnan(latestC))
//        {
//            badge.Format(_T("+%.2f W"), latestC);
//            badgeClr = CLR_GREEN;
//        }
//        else if (m_last.currentRate_mW < 0 && !std::isnan(latestD))
//        {
//            badge.Format(_T("\u2212%.2f W"), latestD);
//            badgeClr = CLR_RED;
//        }
//        else
//        {
//            badge = L(_T("0.00 W  Idle"), _T("0.00 W  アイドル"));
//            badgeClr = CLR_MID_TEXT;
//        }
//
//        const int badgeFS = CardFont(CW, CH, 8);
//        const int legendFS = CardFont(CW, CH, 7);
//        const int rowH = __max(12, CH * 16 / 192);
//        const int dotR = __max(3, rowH / 3);
//
//        // ── right-side: watt value ────────────────────────────────────────────────
//        int bdgW = __max(70, CW * 110 / 468);
//        CRect rcBadge(chartRight - bdgW - 2, chartTop + 3,
//            chartRight - 2, chartTop + 3 + rowH);
//        CardText(pDC, badge, rcBadge, badgeClr,
//            badgeFS, true, DT_RIGHT | DT_TOP | DT_SINGLELINE);
//
//        // ── left-of-badge: "● Discharge (−W)"  then  "● Charge (+W)" ─────────────
//        //  Draw right-to-left so we know where each label ends.
//
//        // Discharge label + dot  ("● Discharge (−W)")
//        CString discLbl = L(_T("  \u25CF Discharge (\u2212W)"), _T("  \u25CF 放電 (\u2212W)"));
//        int discLblW = __max(80, CW * 115 / 468);
//        CRect rcDiscLbl(rcBadge.left - discLblW - 4, chartTop + 3,
//            rcBadge.left - 4, chartTop + 3 + rowH);
//        CardText(pDC, discLbl, rcDiscLbl, CLR_RED,
//            legendFS, false, DT_RIGHT | DT_TOP | DT_SINGLELINE);
//
//        // Charge label + dot  ("● Charge (+W)")
//        CString chargeLbl = L(_T("  \u25CF Charge (+W)"), _T("  \u25CF 充電 (+W)"));
//        int chargeLblW = __max(70, CW * 100 / 468);
//        CRect rcChargeLbl(rcDiscLbl.left - chargeLblW - 4, chartTop + 3,
//            rcDiscLbl.left - 4, chartTop + 3 + rowH);
//        CardText(pDC, chargeLbl, rcChargeLbl, CLR_GREEN,
//            legendFS, false, DT_RIGHT | DT_TOP | DT_SINGLELINE);
//    }

    // ─────────────────────────────────────────────────────────────────────────
    //  PEAK ANNOTATIONS  — dot + dashed drop-line + value label
    //  One annotation per series.  Only shown when there is at least 1 sample.
    // ─────────────────────────────────────────────────────────────────────────
    auto AnnotatePeak = [&](const std::vector<float>& series,
        const std::vector<bool>& valid,
        bool     isCharge,
        COLORREF clrPeak)
        {
            if (!m_filled && m_cursor < 2) return;

            float peakW = 0.0f;
            int   peakK = -1;
            int   count = m_filled ? N : m_cursor;
            for (int i = 0; i < count; ++i)
            {
                if (!valid[i]) continue;
                int   idx = m_filled ? (m_cursor + i) % N : i;
                float v = series[idx];
                if (!std::isnan(v) && v > peakW) { peakW = v; peakK = i; }
            }
            if (peakK < 0 || peakW < 0.05f) return;

            int px = mapX(peakK);
            int py = isCharge ? mapY(+peakW) : mapY(-peakW);
            py = __max(chartTop + 2, __min(py, chartBot - 2));

            // Dashed drop-line peak → zero
            {
                CPen  dashPen(PS_DOT, 1, clrPeak);
                CPen* pOldP = pDC->SelectObject(&dashPen);
                pDC->MoveTo(px, py);
                pDC->LineTo(px, yZero);
                pDC->SelectObject(pOldP);
            }

            // Filled dot at peak
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

            // Value label  ("Peak +x.xx W" or "Peak −x.xx W")
            CString peakLbl;
            if (m_pBattDlg &&
                m_pBattDlg->m_lang == CBatteryHelthDlg::Lang::JP)
            {
                peakLbl.Format(
                    isCharge
                    ? _T("ピーク +%.2f W")
                    : _T("ピーク \u2212%.2f W"),
                    peakW);
            }
            else
            {
                peakLbl.Format(
                    isCharge
                    ? _T("Peak +%.2f W")
                    : _T("Peak \u2212%.2f W"),
                    peakW);
            }

            int lblH = __max(12, CH * 16 / 192);
            int lblW = __max(68, CW * 98 / 468);
            int lblX = px + dotR + 4;
            if (lblX + lblW > chartRight - 2) lblX = px - dotR - 4 - lblW;
            if (lblX < chartLeft + 2)         lblX = chartLeft + 2;

            // Charge label goes above dot, discharge below
            int lblY = isCharge ? (py - lblH - dotR - 2) : (py + dotR + 2);
            lblY = __max(chartTop + 2, __min(lblY, chartBot - lblH - 2));

            CRect rcPeak(lblX, lblY, lblX + lblW, lblY + lblH);
            CardText(pDC, peakLbl, rcPeak, clrPeak,
                CardFont(CW, CH, 7), true,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        };

    AnnotatePeak(m_samplesChargeW, chargeValid, true, CLR_GREEN);
    AnnotatePeak(m_samplesDischargeW, dischargeValid, false, CLR_RED);

    //// ─────────────────────────────────────────────────────────────────────────
    ////  LEGEND  — two colour swatches stacked at bottom-left of chart
    //// ─────────────────────────────────────────────────────────────────────────
    //{
    //    const int legFS = CardFont(CW, CH, 7);
    //    const int legH = __max(10, CH * 13 / 192);
    //    const int swSz = legH;
    //    const int gap = __max(4, CW * 6 / 468);

    //    // Charge swatch + label
    //    /*int legY = chartBot - legH * 2 - gap - 3;*/
    //    // Charge swatch + label
    //    int legY = chartTop + 8;
    //    int legX = rcCard.left + 4;

    //    {
    //        CBrush  swBr(CLR_GREEN);
    //        CPen    swPen(PS_SOLID, 1, CLR_GREEN);
    //        CBrush* pOldB = pDC->SelectObject(&swBr);
    //        CPen* pOldP = pDC->SelectObject(&swPen);
    //        pDC->Rectangle(legX, legY,
    //            legX + swSz, legY + swSz);
    //        pDC->SelectObject(pOldB);
    //        pDC->SelectObject(pOldP);

    //        CRect rcLeg(
    //            legX + swSz + 3,
    //            legY,
    //            chartLeft - 4,
    //            legY + legH);
    //        CardText(pDC,
    //            L(_T("Charge (+W)"),
    //                _T("充電 (+W)")),
    //            rcLeg, CLR_GREEN, legFS, true,
    //            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    //    }

    //    //// Discharge swatch + label
    //    //legY = chartBot - legH - 3;

    //    // Discharge swatch + label
    //    legY = chartTop + 8 + legH + gap;

    //    {
    //        CBrush  swBr(CLR_RED);
    //        CPen    swPen(PS_SOLID, 1, CLR_RED);
    //        CBrush* pOldB = pDC->SelectObject(&swBr);
    //        CPen* pOldP = pDC->SelectObject(&swPen);
    //        pDC->Rectangle(legX, legY,
    //            legX + swSz, legY + swSz);
    //        pDC->SelectObject(pOldB);
    //        pDC->SelectObject(pOldP);

    //        CRect rcLeg(
    //            legX + swSz + 3,
    //            legY,
    //            chartLeft - 4,
    //            legY + legH);
    //        CardText(pDC,
    //            L(_T("Discharge (\u2212W)"),
    //                _T("放電 (\u2212W)")),
    //            rcLeg, CLR_RED, legFS, true,
    //            DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    //    }
    //}
}