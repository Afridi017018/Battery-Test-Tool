#include "pch.h"
#include "CTrendDlg.h"
#include "afxdialogex.h"

#include <iterator>
#include <cmath>
#include <regex>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>   // std::min/std::max

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

using namespace Gdiplus;
#pragma comment(lib, "gdiplus.lib")

// ==================== Helpers (powercfg + IO + text) ====================
static CString GenBatteryReportPath()
{
    TCHAR tempPath[MAX_PATH] = {};
    GetTempPath(MAX_PATH, tempPath);
    CString path; path.Format(_T("%sbattery_report.html"), tempPath);
    return path;
}

static bool RunPowerCfgBatteryReport(const CString& path)
{
    CString cmd; cmd.Format(_T("powercfg /batteryreport /output \"%s\""), path);
    STARTUPINFO si{ sizeof(si) }; PROCESS_INFORMATION pi{};
    CString cmdBuf = cmd; // writable

    if (!CreateProcess(nullptr, cmdBuf.GetBuffer(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return false;

    WaitForSingleObject(pi.hProcess, 10000); // wait up to 10s
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

// UTF-16LE BOM / UTF-8 BOM / UTF-8 (no BOM) / ANSI
static CString ReadTextAutoEncoding(const CString& path)
{
    CFile f;
    if (!f.Open(path, CFile::modeRead | CFile::shareDenyNone)) return {};
    const ULONGLONG len = f.GetLength();
    if (len == 0 || len > 32ULL * 1024 * 1024) { f.Close(); return {}; }

    std::vector<BYTE> buf((size_t)len);
    f.Read(buf.data(), (UINT)len); f.Close();

    // UTF-16LE BOM
    if (len >= 2 && buf[0] == 0xFF && buf[1] == 0xFE) {
        return CString((LPCWSTR)(buf.data() + 2), (int)((len - 2) / 2));
    }
    // UTF-8 BOM
    if (len >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) {
        int n = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data() + 3, (int)len - 3, nullptr, 0);
        CStringW w; LPWSTR p = w.GetBuffer(n);
        MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data() + 3, (int)len - 3, p, n);
        w.ReleaseBuffer(n); return w;
    }
    // Try UTF-8 (no BOM), then ANSI
    int n = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data(), (int)len, nullptr, 0);
    if (n > 0) {
        CStringW w; LPWSTR p = w.GetBuffer(n);
        MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data(), (int)len, p, n);
        w.ReleaseBuffer(n); return w;
    }
    n = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)buf.data(), (int)len, nullptr, 0);
    if (n > 0) {
        CStringW w; LPWSTR p = w.GetBuffer(n);
        MultiByteToWideChar(CP_ACP, 0, (LPCSTR)buf.data(), (int)len, p, n);
        w.ReleaseBuffer(n); return w;
    }
    return {};
}

// Strip simple HTML noise from a cell
static CString StripHtml(const CString& in)
{
    std::wstring s(in);
    static const std::wregex reTags(L"<[^>]*>", std::regex_constants::icase);
    static const std::wregex reNBSP(L"&nbsp;", std::regex_constants::icase);
    static const std::wregex reWS(L"[ \\t\\r\\n]+");

    s = std::regex_replace(s, reTags, L" ");
    s = std::regex_replace(s, reNBSP, L" ");
    s = std::regex_replace(s, reWS, L" ");
    CString o(s.c_str()); o.Trim(); return o;
}

// ==================== Time parsing helpers ====================
static float HmsToHours(int h, int m, int s) {
    return (float)h + (float)m / 60.0f + (float)s / 3600.0f;
}

// Returns all H:MM:SS times as hours, filtered
static std::vector<float> ExtractTimesToHoursFromText(const std::wstring& text)
{
    std::vector<float> out;
    std::wregex re(LR"((\d{1,6}):([0-9]{2}):([0-9]{2}))");
    std::wsregex_iterator it(text.begin(), text.end(), re), end;
    for (; it != end; ++it) {
        int h = _wtoi(it->str(1).c_str());
        int m = _wtoi(it->str(2).c_str());
        int s = _wtoi(it->str(3).c_str());
        // filter pathological values (seen glitches like 114129:18:08)
        if (h > 10000) continue;
        out.push_back(HmsToHours(h, m, s));
    }
    return out;
}

// Extract a compact label from a "PERIOD" string like "2025-08-01 - 2025-08-04" or "2025-09-30"
static CStringW ExtractPeriodLabelFromCell(const CString& cell)
{
    // Find up to two dates in the cell (YYYY-MM-DD)
    std::wstring w = cell.GetString();
    std::wregex d(LR"((20\d{2})-(\d{2})-(\d{2}))");
    std::wsregex_iterator it(w.begin(), w.end(), d), end;

    if (it == end) return L"?";

    // First date
    auto m1 = *it;
    CStringW s1; s1.Format(L"%s-%s", m1.str(2).c_str(), m1.str(3).c_str()); // MM-DD

    // Second date (if any)
    ++it;
    if (it == end) return s1; // single-day period

    auto m2 = *it;
    CStringW s2; s2.Format(L"%s-%s", m2.str(2).c_str(), m2.str(3).c_str()); // MM-DD

    // Return "MM-DD–MM-DD"
    CStringW lab; lab.Format(L"%s–%s", s1.GetString(), s2.GetString());
    return lab;
}


// ==================== MFC dialog plumbing ====================
CTrendDlg::CTrendDlg(CWnd* pParent) : CDialogEx(IDD_TREND_DIALOG, pParent) {}
void CTrendDlg::DoDataExchange(CDataExchange* pDX) { CDialogEx::DoDataExchange(pDX); }

BEGIN_MESSAGE_MAP(CTrendDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()



void CTrendDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    // Set minimal track size (width x height)
    lpMMI->ptMinTrackSize.x = 750;  // min width in pixels
    lpMMI->ptMinTrackSize.y = 650;  // min height in pixels

    CDialogEx::OnGetMinMaxInfo(lpMMI);
}

BOOL CTrendDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    GdiplusStartupInput in; GdiplusStartup(&m_gdiplusToken, &in, NULL);

    // === Always generate a fresh report, then parse ===
    const CString reportPath = GenBatteryReportPath();
    RunPowerCfgBatteryReport(reportPath); // best effort
    LoadFromBatteryReport(reportPath);    // populate m_periods/m_full/m_design

    return TRUE;
}

void CTrendDlg::OnDestroy() {
    CDialogEx::OnDestroy();
    if (m_gdiplusToken) GdiplusShutdown(m_gdiplusToken);
}

BOOL CTrendDlg::OnEraseBkgnd(CDC*) { return TRUE; }
void CTrendDlg::OnSize(UINT nType, int cx, int cy) {
    CDialogEx::OnSize(nType, cx, cy);
    if (IsWindowVisible()) Invalidate(FALSE);
}

void CTrendDlg::SetData(const std::vector<CString>& p,
    const std::vector<float>& f,
    const std::vector<float>& d) {
    m_periods = p; m_full = f; m_design = d;
    Invalidate(FALSE);
}

// ==================== Battery Life Estimates parser ====================
//
// Parses the "Battery life estimates based on observed drains" table.
// We keep two series for your top chart:
//   - Active (At Full Charge)
//   - Active (At Design Capacity)
//
// Row order is kept as in the report; we skip headers and the "Since OS install" summary.
//
bool CTrendDlg::LoadFromBatteryReport(const CString& filePath)
{
    m_periods.clear(); m_full.clear(); m_design.clear();

    const CString html = ReadTextAutoEncoding(filePath);
    if (html.IsEmpty()) return false;

    std::wstring H = html.GetString();

    // 1) Find the "Battery life estimates" section and the following table
    //    (use [\\s\\S]*? to span newlines; case-insensitive)
    std::wsmatch mh;
    std::wregex reTable(
        LR"(BATTERY\s*LIFE\s*ESTIMATES[\s\S]*?based\s+on\s+observed\s+drains[\s\S]*?<table[^>]*>([\s\S]*?)</table>)",
        std::regex_constants::icase
    );

    if (!std::regex_search(H, mh, reTable)) {
        // Some Windows builds have slightly different capitalization/text; try a looser pattern
        std::wregex reTableLoose(
            LR"(BATTERY\s*LIFE\s*ESTIMATES[\s\S]*?<table[^>]*>([\s\S]*?)</table>)",
            std::regex_constants::icase
        );
        if (!std::regex_search(H, mh, reTableLoose)) return false;
    }

    std::wstring tableHtml = mh[1].str();

    // 2) Iterate rows and cells
    std::wregex reRow(L"<tr[^>]*>([\\s\\S]*?)</tr>", std::regex_constants::icase);
    std::wregex reCell(L"<t[dh][^>]*>([\\s\\S]*?)</t[dh]>", std::regex_constants::icase);

    for (std::wsregex_iterator it(tableHtml.begin(), tableHtml.end(), reRow), end; it != end; ++it) {
        std::wstring rowHtml = (*it)[1].str();

        // Gather cells
        std::vector<CString> cells;
        for (std::wsregex_iterator ic(rowHtml.begin(), rowHtml.end(), reCell); ic != end; ++ic) {
            CString cell = StripHtml(CString((*ic)[1].str().c_str()));
            // keep empties? we skip blank cells
            if (!cell.IsEmpty()) cells.push_back(cell);
        }
        if (cells.empty()) continue;

        // Skip header-like rows or summary rows
        CString upper0 = cells[0]; upper0.MakeUpper();
        if (upper0.Find(L"PERIOD") >= 0) continue;
        if (upper0.Find(L"SINCE OS INSTALL") >= 0) continue;

        // Expect: PERIOD | ACTIVE(Full) | CONNECTED STANDBY(Full) | ACTIVE(Design) | CONNECTED STANDBY(Design)
        // We only need PERIOD + Active(Full) + Active(Design).
        // Extract times from the WHOLE row, then index 0=Active Full, 2=Active Design (common layout).
        std::wstring rowText; // combine for regex
        for (const auto& c : cells) { rowText += c.GetString(); rowText += L" "; }

        auto times = ExtractTimesToHoursFromText(rowText);
        if (times.size() < 2) continue; // not enough to form both series

        float fullActive = times[0];              // Active at Full Charge
        float designActive = (times.size() >= 3) ? times[2] : times[1]; // Active at Design (best guess layout)

        // Filter obviously bogus glitch values (optional): drop rows where hours > 120h
        if (fullActive > 120.f || designActive > 120.f) continue;

        CString periodLabel = ExtractPeriodLabelFromCell(cells[0]);
        if (periodLabel == L"?") {
            // if we couldn't spot a date, skip (keeps chart clean)
            continue;
        }

        m_periods.push_back(periodLabel);
        m_full.push_back(fullActive);
        m_design.push_back(designActive);
    }

    // Keep series aligned and non-empty
    size_t n = std::min(m_periods.size(), std::min(m_full.size(), m_design.size()));
    m_periods.resize(n); m_full.resize(n); m_design.resize(n);

    return n >= 2;
}


void CTrendDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect rc; GetClientRect(&rc);

    CDC memDC; memDC.CreateCompatibleDC(&dc);
    CBitmap bmp; bmp.CreateCompatibleBitmap(&dc, rc.Width(), rc.Height());
    CBitmap* pOldBmp = memDC.SelectObject(&bmp);
    memDC.FillSolidRect(&rc, RGB(245, 248, 255));

    Gdiplus::Graphics g(memDC.m_hDC);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

    const int useN = (int)std::min(m_full.size(), (size_t)m_periods.size());
    Gdiplus::SolidBrush white(Gdiplus::Color(255, 255, 255, 255));

    if (useN < 2) {
        Gdiplus::Font f(L"Segoe UI", 12.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush b(Gdiplus::Color(255, 60, 60, 60));
        Gdiplus::StringFormat c; c.SetAlignment(Gdiplus::StringAlignmentCenter); c.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(L"No battery estimate data found.\nGenerate a report again and reopen.",
            -1, &f, Gdiplus::PointF((Gdiplus::REAL)rc.Width() / 2.f, (Gdiplus::REAL)rc.Height() / 2.f), &c, &b);

        dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
        memDC.SelectObject(pOldBmp);
        return;
    }

    // ---------- TOP CHART: Active Battery Life (Full vs Design) ----------
    const Gdiplus::REAL m1 = 70.f;                // outer margin
    const Gdiplus::REAL labelLaneTop = 36.f;      // reserved lane for labels below top chart
    const Gdiplus::REAL labelGap = 8.f;           // visual gap from axis to labels

    const Gdiplus::REAL x1 = m1, y1 = m1;
    const Gdiplus::REAL w1 = (Gdiplus::REAL)rc.Width() - 2.f * m1;

    // half height minus top/bottom margins and minus lane for labels
    const Gdiplus::REAL h1 = (Gdiplus::REAL)rc.Height() / 2.f - 2.f * m1 - labelLaneTop;

    if (w1 > 10.f && h1 > 10.f) {
        g.FillRectangle(&white, x1, y1, w1, h1);
        Gdiplus::Pen axis(Gdiplus::Color(255, 0, 0, 0), 2.f);
        g.DrawLine(&axis, x1, y1 + h1, x1 + w1, y1 + h1);
        g.DrawLine(&axis, x1, y1, x1, y1 + h1);

        // Strict ACTIVE filter (drop standby/glitches)
        const float MAX_ACTIVE_HOURS = 16.f;
        std::vector<CString>         fPeriods;
        std::vector<Gdiplus::REAL>   fFull, fDesign;

        for (size_t i = 0; i < m_full.size() && i < m_design.size() && i < m_periods.size(); ++i) {
            const float af = (float)m_full[i];
            const float ad = (float)m_design[i];
            if (af >= 0.f && af <= MAX_ACTIVE_HOURS &&
                ad >= 0.f && ad <= MAX_ACTIVE_HOURS) {
                fPeriods.push_back(m_periods[i]);
                fFull.push_back((Gdiplus::REAL)af);
                fDesign.push_back((Gdiplus::REAL)ad);
            }
        }

        const int useF = (int)fFull.size();
        if (useF < 2) {
            Gdiplus::Font f(L"Segoe UI", 10.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
            Gdiplus::SolidBrush b(Gdiplus::Color(255, 120, 120, 120));
            Gdiplus::StringFormat c; c.SetAlignment(Gdiplus::StringAlignmentCenter);
            c.SetLineAlignment(Gdiplus::StringAlignmentCenter);
            g.DrawString(L"No valid Active data for chart (standby/glitched rows skipped).",
                -1, &f, Gdiplus::PointF(x1 + w1 / 2.f, y1 + h1 / 2.f), &c, &b);
        }
        else {
            // y-scale
            Gdiplus::REAL maxY = 0.f;
            for (int i = 0; i < useF; ++i)
                maxY = std::max(maxY, std::max(fFull[i], fDesign[i]));
            if (maxY <= 0.f) maxY = 1.f;
            maxY = (Gdiplus::REAL)ceilf(maxY);

            const Gdiplus::REAL stepX = w1 / (Gdiplus::REAL)(std::max(1, useF - 1));
            auto mapY = [&](Gdiplus::REAL v) { return y1 + h1 - (v / maxY) * h1; };

            std::vector<Gdiplus::PointF> pFull, pDes;
            pFull.reserve(useF); pDes.reserve(useF);
            for (int i = 0; i < useF; ++i) {
                Gdiplus::REAL x = x1 + stepX * i;
                pFull.emplace_back(x, mapY(fFull[i]));
                pDes.emplace_back(x, mapY(fDesign[i]));
            }

            // lines & points
            Gdiplus::Pen penFull(Gdiplus::Color(255, 0, 122, 204), 3.f);
            Gdiplus::Pen penDes(Gdiplus::Color(255, 255, 140, 0), 3.f);
            if (pFull.size() >= 2) g.DrawLines(&penFull, pFull.data(), (INT)pFull.size());
            if (pDes.size() >= 2) g.DrawLines(&penDes, pDes.data(), (INT)pDes.size());

            Gdiplus::SolidBrush dotBlue(Gdiplus::Color(255, 0, 122, 204));
            Gdiplus::SolidBrush dotOrg(Gdiplus::Color(255, 255, 140, 0));
            for (auto& p : pFull) g.FillEllipse(&dotBlue, Gdiplus::RectF(p.X - 3.f, p.Y - 3.f, 6.f, 6.f));
            for (auto& p : pDes)  g.FillEllipse(&dotOrg, Gdiplus::RectF(p.X - 3.f, p.Y - 3.f, 6.f, 6.f));

            // grid + Y ticks
            Gdiplus::Pen grid(Gdiplus::Color(80, 200, 200, 200), 1.f);
            Gdiplus::SolidBrush tickTxt(Gdiplus::Color(255, 60, 60, 60));
            Gdiplus::Font tickFont(L"Segoe UI", 9.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
            Gdiplus::StringFormat right; right.SetAlignment(Gdiplus::StringAlignmentFar);
            const int yDiv = 5;
            for (int i = 0; i <= yDiv; ++i) {
                Gdiplus::REAL v = (maxY * i) / yDiv, y = mapY(v);
                g.DrawLine(&grid, x1, y, x1 + w1, y);
                wchar_t buf[32]; swprintf_s(buf, L"%.1f", v);
                g.DrawString(buf, -1, &tickFont, Gdiplus::PointF(x1 - 10.f, y - 7.f), &right, &tickTxt);
            }

            // x-axis ticks
            for (int i = 0; i < useF; ++i) {
                const Gdiplus::REAL x = x1 + stepX * i;
                g.DrawLine(&axis, x, y1 + h1, x, y1 + h1 + 4.f);
            }

            // ----- Top chart title (centered) -----
            {
                Gdiplus::SolidBrush titleBrush(Gdiplus::Color(255, 40, 40, 40));
                Gdiplus::Font fTitle(L"Segoe UI", 13.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
                Gdiplus::StringFormat centerTop;
                centerTop.SetAlignment(Gdiplus::StringAlignmentCenter);
                centerTop.SetLineAlignment(Gdiplus::StringAlignmentNear);
                g.DrawString(L"Active Battery Life Trend (hours)", -1, &fTitle,
                    Gdiplus::PointF(x1 + w1 / 2.f, y1 - 28.f), &centerTop, &titleBrush);
            }

            // Y axis label
            {
                Gdiplus::SolidBrush axisBrush(Gdiplus::Color(255, 40, 40, 40));
                Gdiplus::Font fAxis(L"Segoe UI", 10.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
                Gdiplus::StringFormat center; center.SetAlignment(Gdiplus::StringAlignmentCenter);
                g.TranslateTransform(x1 - 70.f, y1 + h1 / 2.f);
                g.RotateTransform(-90.f);
                g.DrawString(L"Hours", -1, &fAxis, Gdiplus::PointF(0, 0), &center, &axisBrush);
                g.ResetTransform();
            }

            // Legend
            {
                Gdiplus::Font fLegend(L"Segoe UI", 9.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
                Gdiplus::SolidBrush tLegend(Gdiplus::Color(255, 40, 40, 40));
                float lx = x1 + w1 - 200.f, ly = y1 + 10.f;
                Gdiplus::Pen legendFull(Gdiplus::Color(255, 0, 122, 204), 3.f);
                Gdiplus::Pen legendDes(Gdiplus::Color(255, 255, 140, 0), 3.f);
                g.DrawLine(&legendFull, lx, ly + 5.f, lx + 20.f, ly + 5.f);
                g.DrawString(L"Active (Full Charge)", -1, &fLegend, Gdiplus::PointF(lx + 28.f, ly - 4.f), nullptr, &tLegend);
                g.DrawLine(&legendDes, lx, ly + 25.f, lx + 20.f, ly + 25.f);
                g.DrawString(L"Active (Design Capacity)", -1, &fLegend, Gdiplus::PointF(lx + 28.f, ly + 16.f), nullptr, &tLegend);
            }

            // ---------- PERIOD LABELS: in reserved lane BELOW the chart ----------
            const float baseY = y1 + h1 + labelGap; // a bit below the axis
            for (int i = 0; i < useF; ++i) {
                Gdiplus::REAL x = x1 + stepX * i;

                // small guide tick pointing to label lane (optional)
                g.DrawLine(&axis, x, y1 + h1 + 4.f, x, y1 + h1 + 8.f);

                // rotate -45° and draw text anchored to the RIGHT of the tick,
                // so the label grows leftward (prevents clipping at the right edge)
                Gdiplus::Matrix oldT; g.GetTransform(&oldT);
                g.TranslateTransform(x, baseY + 10.f); // +10 so it stays well clear of axis
                g.RotateTransform(-45.f);

                Gdiplus::SolidBrush lblBrush(Gdiplus::Color(255, 40, 40, 40));
                Gdiplus::Font tickFont(L"Segoe UI", 9.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);

                Gdiplus::StringFormat farFmt;
                farFmt.SetAlignment(Gdiplus::StringAlignmentFar);
                farFmt.SetLineAlignment(Gdiplus::StringAlignmentNear);

                // negative small X-offset keeps last label fully visible
                g.DrawString(fPeriods[i].GetString(), -1, &tickFont,
                    Gdiplus::PointF(-2.f, 0.f), &farFmt, &lblBrush);

                g.SetTransform(&oldT);
            }

            // ----- X axis title for TOP chart -----
            {
                Gdiplus::SolidBrush axisText(Gdiplus::Color(255, 40, 40, 40));
                Gdiplus::Font fAxisX(L"Segoe UI", 10.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
                Gdiplus::StringFormat centerX;
                centerX.SetAlignment(Gdiplus::StringAlignmentCenter);
                centerX.SetLineAlignment(Gdiplus::StringAlignmentNear);

                // place it inside the reserved label lane
                g.DrawString(L"Period", -1, &fAxisX,
                    Gdiplus::PointF(x1 + w1 / 2.f, y1 + h1 + labelLaneTop - 18.f),
                    &centerX, &axisText);
            }
        }
    }

    // ---------- BOTTOM CHART: Health % ----------
    const Gdiplus::REAL m2 = 70.f;
    const Gdiplus::REAL labelLaneBottom = 36.f;   // reserved lane for labels below bottom chart
    int midY = rc.top + rc.Height() / 2;
    const Gdiplus::REAL x2 = m2, y2 = (Gdiplus::REAL)midY + m2;
    const Gdiplus::REAL w2 = (Gdiplus::REAL)rc.Width() - 2.f * m2;
    const Gdiplus::REAL h2 = (Gdiplus::REAL)(rc.bottom - midY) - 2.f * m2 - labelLaneBottom;

    if (w2 > 10.f && h2 > 10.f) {
        g.FillRectangle(&white, x2, y2, w2, h2);
        Gdiplus::Pen axis(Gdiplus::Color(255, 0, 0, 0), 2.f);
        g.DrawLine(&axis, x2, y2 + h2, x2 + w2, y2 + h2);
        g.DrawLine(&axis, x2, y2, x2, y2 + h2);

        std::vector<Gdiplus::REAL> hpct; hpct.reserve(useN);
        for (int i = 0; i < useN; ++i) hpct.push_back((m_design[i] > 0.f) ? (m_full[i] / m_design[i]) * 100.f : 0.f);
        auto mapY2 = [&](Gdiplus::REAL v) { return y2 + h2 - (v / 100.f) * h2; };

        std::vector<Gdiplus::PointF> hp; hp.reserve(useN);
        const Gdiplus::REAL stepX2 = w2 / (Gdiplus::REAL)(std::max(1, useN - 1));
        for (int i = 0; i < useN; ++i) hp.emplace_back(x2 + stepX2 * i, mapY2(hpct[i]));

        Gdiplus::Pen penH(Gdiplus::Color(255, 0, 160, 0), 3.f);
        if (hp.size() >= 2) g.DrawLines(&penH, hp.data(), (INT)hp.size());
        Gdiplus::SolidBrush dotG(Gdiplus::Color(255, 0, 160, 0));
        for (auto& p : hp) g.FillEllipse(&dotG, Gdiplus::RectF(p.X - 3.f, p.Y - 3.f, 6.f, 6.f));

        // grid + Y ticks
        Gdiplus::Pen grid2(Gdiplus::Color(80, 200, 200, 200), 1.f);
        Gdiplus::SolidBrush tick2(Gdiplus::Color(255, 60, 60, 60));
        Gdiplus::Font fTick2(L"Segoe UI", 9.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::StringFormat right2; right2.SetAlignment(Gdiplus::StringAlignmentFar);
        for (int i = 0; i <= 5; ++i) {
            Gdiplus::REAL v = (100.f * i) / 5.f, y = mapY2(v);
            g.DrawLine(&grid2, x2, y, x2 + w2, y);
            wchar_t buf[32]; swprintf_s(buf, L"%.0f%%", v);
            g.DrawString(buf, -1, &fTick2, Gdiplus::PointF(x2 - 10.f, y - 7.f), &right2, &tick2);
        }

        // x-axis ticks
        for (int i = 0; i < useN; ++i) {
            const Gdiplus::REAL x = x2 + stepX2 * i;
            g.DrawLine(&axis, x, y2 + h2, x, y2 + h2 + 4.f);
        }

        // ----- Bottom chart title (centered) -----
        {
            Gdiplus::SolidBrush tBrush2(Gdiplus::Color(255, 40, 40, 40));
            tBrush2.SetColor(Gdiplus::Color(255, 40, 40, 40));
            Gdiplus::Font fTitle2(L"Segoe UI", 13.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);

            Gdiplus::StringFormat centerBottom;
            centerBottom.SetAlignment(Gdiplus::StringAlignmentCenter);
            centerBottom.SetLineAlignment(Gdiplus::StringAlignmentNear);

            g.DrawString(L"Battery Health Trend Over Time", -1, &fTitle2,
                Gdiplus::PointF(x2 + w2 / 2.f, y2 - 28.f), &centerBottom, &tBrush2);
        }

        // Y axis label
        {
            Gdiplus::SolidBrush axisBrush(Gdiplus::Color(255, 40, 40, 40));
            Gdiplus::Font fAxis2(L"Segoe UI", 10.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
            Gdiplus::StringFormat center2; center2.SetAlignment(Gdiplus::StringAlignmentCenter);
            g.TranslateTransform(x2 - 70.f, y2 + h2 / 2.f);
            g.RotateTransform(-90.f);
            g.DrawString(L"Health (%)", -1, &fAxis2, Gdiplus::PointF(0, 0), &center2, &axisBrush);
            g.ResetTransform();
        }

        // ---------- PERIOD LABELS BELOW THE HEALTH CHART ----------
        const float baseY2 = y2 + h2 + labelGap;
        for (int i = 0; i < useN; ++i) {
            Gdiplus::REAL x = x2 + stepX2 * i;

            // small guide tick
            g.DrawLine(&axis, x, y2 + h2 + 4.f, x, y2 + h2 + 8.f);

            Gdiplus::Matrix oldT; g.GetTransform(&oldT);
            g.TranslateTransform(x, baseY2 + 10.f);
            g.RotateTransform(-45.f);

            Gdiplus::SolidBrush lblBrush(Gdiplus::Color(255, 40, 40, 40));
            Gdiplus::StringFormat farFmt;
            farFmt.SetAlignment(Gdiplus::StringAlignmentFar);
            farFmt.SetLineAlignment(Gdiplus::StringAlignmentNear);

            g.DrawString(m_periods[i].GetString(), -1, &fTick2,
                Gdiplus::PointF(-2.f, 0.f), &farFmt, &lblBrush);

            g.SetTransform(&oldT);
        }

        // ----- X axis title for BOTTOM chart -----
        {
            Gdiplus::SolidBrush axisText2(Gdiplus::Color(255, 40, 40, 40));
            Gdiplus::Font fAxisX2(L"Segoe UI", 10.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
            Gdiplus::StringFormat centerX2;
            centerX2.SetAlignment(Gdiplus::StringAlignmentCenter);
            centerX2.SetLineAlignment(Gdiplus::StringAlignmentNear);

            g.DrawString(L"Period", -1, &fAxisX2,
                Gdiplus::PointF(x2 + w2 / 2.f, y2 + h2 + labelLaneBottom - 18.f),
                &centerX2, &axisText2);
        }
    }

    // present
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}







