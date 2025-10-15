#include "pch.h"
#include "CDischargeDlg.h"
#include "afxdialogex.h"

#include <gdiplus.h>
#include <algorithm>   // std::max / std::min
#include <cmath>       // std::ceil

#pragma comment(lib, "gdiplus.lib")
using namespace Gdiplus;

BEGIN_MESSAGE_MAP(CDischargeDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_DESTROY()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

BOOL CDischargeDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();
    GdiplusStartupInput in;
    GdiplusStartup(&m_gdiplusToken, &in, nullptr);
    return TRUE;
}

void CDischargeDlg::OnDestroy()
{
    CDialogEx::OnDestroy();
    if (m_gdiplusToken)
        GdiplusShutdown(m_gdiplusToken);
}

void CDischargeDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    if (cx > 0 && cy > 0 && IsWindowVisible()) {
        Invalidate(FALSE);   // use double-buffer in OnPaint
        UpdateWindow();
    }
}

void CDischargeDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    CDialogEx::OnGetMinMaxInfo(lpMMI);
    // Sensible minimum so chart never collapses
    lpMMI->ptMinTrackSize.x = 520;
    lpMMI->ptMinTrackSize.y = 380;
}

void CDischargeDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);

    CDC memDC; memDC.CreateCompatibleDC(&dc);
    CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);
    memDC.FillSolidRect(&rc, RGB(245, 248, 255));

    Graphics g(memDC.m_hDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    // Save/restore graphics state + clip for robust resizing
    GraphicsState gs = g.Save();
    g.SetClip(Rect(0, 0, rc.Width(), rc.Height()));

    SolidBrush white(Color(255, 255, 255, 255));

    // Layout
    const REAL margin = 60.f;
    const REAL yPadTop = 20.f;     // space for title
    const REAL x = margin, y = margin + yPadTop;
    const REAL w = (REAL)rc.Width() - 2 * margin;
    const REAL h = (REAL)rc.Height() - 2 * margin - 50.f; // room for 45° labels

    // Title (centered)
    {
        Gdiplus::Font fTitle(L"Segoe UI", 13.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        SolidBrush tb(Color(255, 40, 40, 40));
        StringFormat c; c.SetAlignment(StringAlignmentCenter);
        g.DrawString(L"Discharge Test Result", -1, &fTitle,
            PointF(x + w / 2.f, margin - 18.f), &c, &tb);
    }

    // Too small to draw chart
    if (w < 50.f || h < 50.f) {
        Gdiplus::Font f(L"Segoe UI", 10.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        SolidBrush b(Color(255, 60, 60, 60));
        StringFormat c; c.SetAlignment(StringAlignmentCenter); c.SetLineAlignment(StringAlignmentCenter);
        g.DrawString(L"Window too small to draw chart.", -1, &f,
            PointF(rc.Width() / 2.f, rc.Height() / 2.f), &c, &b);

        g.ResetTransform(); g.ResetClip(); g.Restore(gs);
        dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
        memDC.SelectObject(pOldBmp);
        return;
    }

    // Chart area + axes
    g.FillRectangle(&white, x, y, w, h);
    Pen axis(Color(255, 0, 0, 0), 2.f);
    g.DrawLine(&axis, x, y + h, x + w, y + h);
    g.DrawLine(&axis, x, y, x, y + h);

    const bool hasSeries = (m_times.size() >= 2 && m_times.size() == m_perc.size());

    // If no series: show “No data to plot.” + multi-line info centered (no background)
    if (!hasSeries)
    {
        // Message
        {
            Gdiplus::Font fMsg(L"Segoe UI", 11.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
            SolidBrush brMsg(Color(255, 60, 60, 60));
            StringFormat c; c.SetAlignment(StringAlignmentCenter); c.SetLineAlignment(StringAlignmentCenter);
            g.DrawString(L"No data to plot.", -1, &fMsg,
                PointF(rc.Width() / 2.f, rc.Height() / 2.f - 26.f), &c, &brMsg);
        }
        // Multi-line info (centered, no background)
        {
            CStringW info;
            info.Format(L"Initial: %.0f%%\nCurrent: %.0f%%\nDrop: %.0f%%\nRate: %.2f%%/min\n",
                m_initial, m_current, (m_initial - m_current), m_rate);

            Gdiplus::Font fInfo(L"Segoe UI", 9.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
            SolidBrush brInfo(Color(255, 40, 40, 40));
            StringFormat c; c.SetAlignment(StringAlignmentCenter); c.SetLineAlignment(StringAlignmentCenter);

            // use a small rect to allow line breaks (explicit \n)
            RectF box((REAL)rc.Width() / 2.f - 120.f, (REAL)rc.Height() / 2.f - 2.f, 240.f, 120.f);
            g.DrawString(info, -1, &fInfo, box, &c, &brInfo);
        }

        g.ResetTransform(); g.ResetClip(); g.Restore(gs);
        dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
        memDC.SelectObject(pOldBmp);
        return;
    }

    // Scales
    auto mapY = [&](Gdiplus::REAL pct) {
        Gdiplus::REAL v = std::max<Gdiplus::REAL>(0, std::min<Gdiplus::REAL>(100, pct));
        return y + h - (v / 100.f) * h;
        };
    float tMax = m_times.back();
    if (tMax <= 0.f) tMax = 1.f;
    auto mapX = [&](Gdiplus::REAL t) {
        Gdiplus::REAL clamped = std::max<Gdiplus::REAL>(0, t);
        return x + (clamped / tMax) * w;
        };

    // Grid + ticks
    Pen grid(Color(70, 200, 200, 200), 1.f);
    SolidBrush tTxt(Color(255, 60, 60, 60));
    Gdiplus::Font fTick(L"Segoe UI", 9.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    StringFormat right;  right.SetAlignment(StringAlignmentFar);
    StringFormat center; center.SetAlignment(StringAlignmentCenter);

    // Y ticks every 10%
    for (int i = 0; i <= 10; i++) {
        REAL v = (REAL)(i * 10);
        REAL yy = mapY(v);
        g.DrawLine(&grid, x, yy, x + w, yy);
        wchar_t buf[16]; swprintf_s(buf, L"%.0f%%", (double)v);
        g.DrawString(buf, -1, &fTick, PointF(x - 8.f, yy - 7.f), &right, &tTxt);
    }

    // X ticks every 10 minutes, labels rotated 45°
    int maxMin = (int)std::ceil(tMax);
    const int xTickStepMin = 10;
    for (int m = 0; m <= maxMin; m += xTickStepMin) {
        REAL xx = mapX((REAL)m);
        g.DrawLine(&axis, xx, y + h, xx, y + h + 4.f);

        GraphicsState s = g.Save();
        g.TranslateTransform(xx, y + h + 10.f);
        g.RotateTransform(-45.f);

        Gdiplus::StringFormat nearFmt;
        nearFmt.SetAlignment(Gdiplus::StringAlignmentNear);
        nearFmt.SetLineAlignment(Gdiplus::StringAlignmentNear);

        wchar_t buf[16]; swprintf_s(buf, L"%d", m);
        g.DrawString(buf, -1, &fTick, PointF(0.f, 0.f), &nearFmt, &tTxt);
        g.Restore(s);
    }

    // Series
    std::vector<PointF> pts;
    pts.reserve(m_times.size());
    for (size_t i = 0; i < m_times.size(); ++i)
        pts.emplace_back(mapX(m_times[i]), mapY(m_perc[i]));

    Pen penLine(Color(255, 0, 122, 204), 3.f);
    if (pts.size() >= 2) g.DrawLines(&penLine, pts.data(), (INT)pts.size());

    SolidBrush dot(Color(255, 0, 122, 204));
    for (auto& p : pts)
        g.FillEllipse(&dot, RectF(p.X - 3.f, p.Y - 3.f, 6.f, 6.f));

    // Axis labels (Y with EXTRA clearance from chart; moved 10px further left)
    Gdiplus::Font fAxis(L"Segoe UI", 10.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    SolidBrush aBrush(Color(255, 40, 40, 40));
    {
        GraphicsState sYL = g.Save();
        g.TranslateTransform(x - 65.f, y + h / 2.f);  // was -55.f → now -65.f for more padding
        g.RotateTransform(-90.f);
        g.DrawString(L"Charge (%)", -1, &fAxis, PointF(0, 0), &center, &aBrush);
        g.Restore(sYL);
    }
    g.DrawString(L"Time (minutes)", -1, &fAxis,
        PointF(x + w / 2.f, y + h + 36.f), &center, &aBrush);

    // Multi-line info at top-right (NO background), with right padding
    {
        CStringW info;
        info.Format(L"Initial: %.0f%%\nCurrent: %.0f%%\nDrop: %.0f%%\nRate: %.2f%%/min\n",
            m_initial, m_current, (m_initial - m_current), m_rate);

        const REAL padRight = 14.f;   // right margin
        RectF box(x + w - 220.f - padRight, y - 6.f, 220.f, 100.f);

        Gdiplus::Font fInfo(L"Segoe UI", 9.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        SolidBrush ib(Color(255, 40, 40, 40));
        // draw inside a rect to honor line breaks (\n)
        g.DrawString(info, -1, &fInfo, box, nullptr, &ib);
    }

    // Present
    g.ResetTransform(); g.ResetClip(); g.Restore(gs);
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}

