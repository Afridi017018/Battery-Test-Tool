// DrawSecOverview.cpp

#include "pch.h"
#include "MFCUIDlg.h"

#include <cmath>
#include <vector>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ─────────────────────────────────────────────────────────────
// Draw Circular Gauge
// ─────────────────────────────────────────────────────────────
void CMFCUIDlg::DrawCircularGauge(
    CDC* pDC,
    CPoint center,
    int radius,
    float percent)
{
    float gdiStart = 225.0f;
    float sweepTotal = 270.0f;

    auto DrawArc =
        [&](float angleDeg,
            float sweepDeg,
            COLORREF col,
            int penWidth)
        {
            if (sweepDeg <= 0.0f)
                return;

            CPen arcPen(PS_SOLID, penWidth, col);
            CPen* pOldPen = pDC->SelectObject(&arcPen);

            pDC->SelectStockObject(NULL_BRUSH);

            int N = max(4, (int)(sweepDeg * 2.0f));

            std::vector<POINT> pts;
            pts.reserve(N + 1);

            for (int i = 0; i <= N; i++)
            {
                float t =
                    (angleDeg + sweepDeg * i / N) *
                    (float)(M_PI / 180.0);

                POINT pt;
                pt.x = center.x + (int)(radius * cos(t));
                pt.y = center.y + (int)(radius * sin(t));

                pts.push_back(pt);
            }

            pDC->Polyline(pts.data(), (int)pts.size());

            pDC->SelectObject(pOldPen);
        };

    int trackW = max(4, radius / 5);

    // Background track
    DrawArc(
        gdiStart,
        sweepTotal,
        RGB(220, 225, 235),
        trackW);

    // Filled portion
    if (percent > 0.0f)
    {
        float filled = sweepTotal * percent;

        float greenPart = filled * 0.88f;
        float yellowPart = filled * 0.12f;

        DrawArc(
            gdiStart,
            greenPart,
            CLR_GREEN,
            trackW);

        DrawArc(
            gdiStart + greenPart,
            yellowPart,
            CLR_YELLOW,
            trackW);
    }

    CString strPct;
    strPct.Format(_T("%d%%"),
        (int)(percent * 100.0f));

    CRect rcNum(
        center.x - radius / 2,
        center.y - radius / 3,
        center.x + radius / 2,
        center.y + radius / 5);

    DrawTextEx(
        pDC,
        strPct,
        rcNum,
        CLR_DARK_TEXT,
        max(6, radius / 3),
        true,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

// ─────────────────────────────────────────────────────────────
// Draw Battery Overview
// ─────────────────────────────────────────────────────────────
void CMFCUIDlg::DrawBatteryOverview(
    CDC* pDC,
    CRect rc)
{
    int W = rc.Width();
    int H = rc.Height();

    int mx = SW(16, W);

    int cardTop = SH(76, H);
    int cardBottom = SH(296, H);

    CRect rcCard(
        mx,
        cardTop,
        W - mx,
        cardBottom);

    DrawRoundRect(
        pDC,
        rcCard,
        SW(10, W),
        CLR_CARD,
        CLR_BORDER);

    // Title
    int pad = SW(14, W);

    CRect rcTitle(
        rcCard.left + pad,
        rcCard.top + SH(8, H),
        rcCard.right - pad,
        rcCard.top + SH(28, H));

    DrawTextEx(
        pDC,
        _T("Battery Overview"),
        rcTitle,
        CLR_TITLE,
        SF(10, W),
        true,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Divider
    int divY = rcCard.top + SH(32, H);

    CPen divPen(PS_SOLID, 1, CLR_BORDER);
    CPen* pOldPen = pDC->SelectObject(&divPen);

    pDC->MoveTo(rcCard.left + SW(10, W), divY);
    pDC->LineTo(rcCard.right - SW(10, W), divY);

    pDC->SelectObject(pOldPen);

    int innerTop = divY + SH(4, H);
    int innerBottom = rcCard.bottom - SH(8, H);

    int innerLeft = rcCard.left + SW(8, W);
    int innerRight = rcCard.right - SW(8, W);

    int midX =
        innerLeft +
        (innerRight - innerLeft) / 2 -
        SW(3, W);

    // ====================================================
    // Left Panel
    // ====================================================

    CRect rcLeft(
        innerLeft,
        innerTop,
        midX,
        innerBottom);

    DrawRoundRect(
        pDC,
        rcLeft,
        SW(8, W),
        RGB(250, 251, 253),
        CLR_BORDER);

    CRect rcStatus(
        rcLeft.left + SW(10, W),
        rcLeft.top + SH(6, H),
        rcLeft.right,
        rcLeft.top + SH(22, H));

    DrawTextEx(
        pDC,
        _T("Status"),
        rcStatus,
        CLR_MID_TEXT,
        SF(8, W),
        false);

    int panelW = rcLeft.Width();
    int panelH = rcLeft.Height();

    int gaugeR =
        min(panelW, panelH) * 30 / 100;

    gaugeR = max(40, gaugeR);
    gaugeR = min(65, gaugeR);

    CPoint gaugeCenter(
        rcLeft.left + panelW / 2,
        rcLeft.top + SH(28, H) +
        gaugeR - SH(8, H));

    DrawCircularGauge(
        pDC,
        gaugeCenter,
        gaugeR,
        0.78f);

    int belowGauge =
        gaugeCenter.y +
        gaugeR +
        SH(12, H);

    CRect rcCharge(
        rcLeft.left,
        belowGauge,
        rcLeft.right,
        belowGauge + SH(18, H));

    DrawTextEx(
        pDC,
        _T("► Charging"),
        rcCharge,
        CLR_GREEN,
        SF(8, W),
        true,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    CRect rcPower(
        rcLeft.left,
        belowGauge + SH(18, H),
        rcLeft.right,
        belowGauge + SH(36, H));

    DrawTextEx(
        pDC,
        _T("Power: 12.5 V"),
        rcPower,
        CLR_MID_TEXT,
        SF(8, W),
        false,
        DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    // ====================================================
    // Right Panel
    // ====================================================

    CRect rcRight(
        midX + SW(6, W),
        innerTop,
        innerRight,
        innerBottom);

    DrawRoundRect(
        pDC,
        rcRight,
        SW(8, W),
        RGB(250, 251, 253),
        CLR_BORDER);

    CRect rcCapHdr(
        rcRight.left + SW(10, W),
        rcRight.top + SH(6, H),
        rcRight.right,
        rcRight.top + SH(22, H));

    DrawTextEx(
        pDC,
        _T("Capacity"),
        rcCapHdr,
        CLR_MID_TEXT,
        SF(8, W),
        false);

    int capDivY =
        rcRight.top + SH(26, H);

    CPen capPen(PS_SOLID, 1, CLR_BORDER);

    pOldPen = pDC->SelectObject(&capPen);

    pDC->MoveTo(
        rcRight.left + SW(8, W),
        capDivY);

    pDC->LineTo(
        rcRight.right - SW(8, W),
        capDivY);

    pDC->SelectObject(pOldPen);

    DrawTextEx(
        pDC,
        _T("Full Charge Capacity:"),
        CRect(
            rcRight.left + SW(10, W),
            capDivY + SH(8, H),
            rcRight.right,
            capDivY + SH(22, H)),
        CLR_MID_TEXT,
        SF(7, W),
        false);

    DrawTextEx(
        pDC,
        _T("4,200 mAh"),
        CRect(
            rcRight.left + SW(10, W),
            capDivY + SH(24, H),
            rcRight.right,
            capDivY + SH(42, H)),
        CLR_DARK_TEXT,
        SF(10, W),
        true);

    DrawTextEx(
        pDC,
        _T("Cycle Count:"),
        CRect(
            rcRight.left + SW(10, W),
            capDivY + SH(56, H),
            rcRight.right,
            capDivY + SH(70, H)),
        CLR_MID_TEXT,
        SF(7, W),
        false);

    DrawTextEx(
        pDC,
        _T("152"),
        CRect(
            rcRight.left + SW(10, W),
            capDivY + SH(72, H),
            rcRight.right,
            capDivY + SH(92, H)),
        CLR_DARK_TEXT,
        SF(10, W),
        true);
}