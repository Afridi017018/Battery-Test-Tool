#include "pch.h"
#include "CPredictionDlg.h"
#include "afxdialogex.h"

#include <iterator>
#include <cmath>
#include <regex>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

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
    CString cmdBuf = cmd;

    if (!CreateProcess(nullptr, cmdBuf.GetBuffer(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return false;

    WaitForSingleObject(pi.hProcess, 10000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

static CString ReadTextAutoEncoding(const CString& path)
{
    CFile f;
    if (!f.Open(path, CFile::modeRead | CFile::shareDenyNone)) return {};
    const ULONGLONG len = f.GetLength();
    if (len == 0 || len > 32ULL * 1024 * 1024) { f.Close(); return {}; }

    std::vector<BYTE> buf((size_t)len);
    f.Read(buf.data(), (UINT)len); f.Close();

    if (len >= 2 && buf[0] == 0xFF && buf[1] == 0xFE) {
        return CString((LPCWSTR)(buf.data() + 2), (int)((len - 2) / 2));
    }
    if (len >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) {
        int n = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data() + 3, (int)len - 3, nullptr, 0);
        CStringW w; LPWSTR p = w.GetBuffer(n);
        MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data() + 3, (int)len - 3, p, n);
        w.ReleaseBuffer(n); return w;
    }
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

static std::vector<float> ExtractTimesToHoursFromText(const std::wstring& text)
{
    std::vector<float> out;
    std::wregex re(LR"((\d{1,6}):([0-9]{2}):([0-9]{2}))");
    std::wsregex_iterator it(text.begin(), text.end(), re), end;
    for (; it != end; ++it) {
        int h = _wtoi(it->str(1).c_str());
        int m = _wtoi(it->str(2).c_str());
        int s = _wtoi(it->str(3).c_str());
        if (h > 10000) continue;
        out.push_back(HmsToHours(h, m, s));
    }
    return out;
}

static CStringW ExtractPeriodLabelFromCell(const CString& cell)
{
    std::wstring w = cell.GetString();
    std::wregex d(LR"((20\d{2})-(\d{2})-(\d{2}))");
    std::wsregex_iterator it(w.begin(), w.end(), d), end;

    if (it == end) return L"?";

    auto m1 = *it;
    CStringW s1; s1.Format(L"%s-%s", m1.str(2).c_str(), m1.str(3).c_str());

    ++it;
    if (it == end) return s1;

    auto m2 = *it;
    CStringW s2; s2.Format(L"%s-%s", m2.str(2).c_str(), m2.str(3).c_str());

    CStringW lab; lab.Format(L"%s–%s", s1.GetString(), s2.GetString());
    return lab;
}

// ==================== MFC dialog plumbing ====================
CPredictionDlg::CPredictionDlg(CWnd* pParent) : CDialogEx(IDD_PREDICTION_DIALOG, pParent) {}
void CPredictionDlg::DoDataExchange(CDataExchange* pDX) { CDialogEx::DoDataExchange(pDX); }

BEGIN_MESSAGE_MAP(CPredictionDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_DESTROY()
    ON_WM_SIZE()
    ON_WM_ERASEBKGND()
    ON_WM_GETMINMAXINFO()
END_MESSAGE_MAP()

void CPredictionDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
    lpMMI->ptMinTrackSize.x = 680;
    lpMMI->ptMinTrackSize.y = 530;  
    CDialogEx::OnGetMinMaxInfo(lpMMI);
}

BOOL CPredictionDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    GdiplusStartupInput in; GdiplusStartup(&m_gdiplusToken, &in, NULL);

    const CString reportPath = GenBatteryReportPath();
    RunPowerCfgBatteryReport(reportPath);
    LoadFromBatteryReport(reportPath);

    return TRUE;
}

void CPredictionDlg::OnDestroy() {
    CDialogEx::OnDestroy();
    if (m_gdiplusToken) GdiplusShutdown(m_gdiplusToken);
}

BOOL CPredictionDlg::OnEraseBkgnd(CDC*) { return TRUE; }
void CPredictionDlg::OnSize(UINT nType, int cx, int cy) {
    CDialogEx::OnSize(nType, cx, cy);
    if (IsWindowVisible()) Invalidate(FALSE);
}

void CPredictionDlg::SetData(const std::vector<CString>& p,
    const std::vector<float>& f,
    const std::vector<float>& d) {
    m_periods = p; m_full = f; m_design = d;
    Invalidate(FALSE);
}

// ==================== Battery Life Estimates parser ====================
bool CPredictionDlg::LoadFromBatteryReport(const CString& filePath)
{
    m_periods.clear(); m_full.clear(); m_design.clear();

    const CString html = ReadTextAutoEncoding(filePath);
    if (html.IsEmpty()) return false;

    std::wstring H = html.GetString();

    std::wsmatch mh;
    std::wregex reTable(
        LR"(BATTERY\s*LIFE\s*ESTIMATES[\s\S]*?based\s+on\s+observed\s+drains[\s\S]*?<table[^>]*>([\s\S]*?)</table>)",
        std::regex_constants::icase
    );

    if (!std::regex_search(H, mh, reTable)) {
        std::wregex reTableLoose(
            LR"(BATTERY\s*LIFE\s*ESTIMATES[\s\S]*?<table[^>]*>([\s\S]*?)</table>)",
            std::regex_constants::icase
        );
        if (!std::regex_search(H, mh, reTableLoose)) return false;
    }

    std::wstring tableHtml = mh[1].str();

    std::wregex reRow(L"<tr[^>]*>([\\s\\S]*?)</tr>", std::regex_constants::icase);
    std::wregex reCell(L"<t[dh][^>]*>([\\s\\S]*?)</t[dh]>", std::regex_constants::icase);

    for (std::wsregex_iterator it(tableHtml.begin(), tableHtml.end(), reRow), end; it != end; ++it) {
        std::wstring rowHtml = (*it)[1].str();

        std::vector<CString> cells;
        for (std::wsregex_iterator ic(rowHtml.begin(), rowHtml.end(), reCell); ic != end; ++ic) {
            CString cell = StripHtml(CString((*ic)[1].str().c_str()));
            if (!cell.IsEmpty()) cells.push_back(cell);
        }
        if (cells.empty()) continue;

        CString upper0 = cells[0]; upper0.MakeUpper();
        if (upper0.Find(L"PERIOD") >= 0) continue;
        if (upper0.Find(L"SINCE OS INSTALL") >= 0) continue;

        std::wstring rowText;
        for (const auto& c : cells) { rowText += c.GetString(); rowText += L" "; }

        auto times = ExtractTimesToHoursFromText(rowText);
        if (times.size() < 2) continue;

        float fullActive = times[0];
        float designActive = (times.size() >= 3) ? times[2] : times[1];

        if (fullActive > 120.f || designActive > 120.f) continue;

        CString periodLabel = ExtractPeriodLabelFromCell(cells[0]);
        if (periodLabel == L"?") continue;

        m_periods.push_back(periodLabel);
        m_full.push_back(fullActive);
        m_design.push_back(designActive);
    }

    size_t n = std::min(m_periods.size(), std::min(m_full.size(), m_design.size()));
    m_periods.resize(n); m_full.resize(n); m_design.resize(n);

    return n >= 2;
}

// ==================== Prediction Calculations ====================
struct PredictionStats {
    float currentHealth;
    float initialHealth;
    float linearRate;
    float lambda;
    float monthsElapsed;
    float linearMonthsTo50;
    float expMonthsTo50;
};

CPredictionDlg::PredictionStats CPredictionDlg::CalculatePredictions() const
{
    PredictionStats stats = {};

    if (m_full.size() < 2 || m_design.size() < 2) return stats;

    // Calculate health percentages
    std::vector<float> healthPct;
    for (size_t i = 0; i < std::min(m_full.size(), m_design.size()); ++i) {
        if (m_design[i] > 0.f) {
            healthPct.push_back((m_full[i] / m_design[i]) * 100.f);
        }
    }

    if (healthPct.empty()) return stats;

    stats.initialHealth = healthPct.front();
    stats.currentHealth = healthPct.back();
    stats.monthsElapsed = 2.4f; // Approximate from your data

    // Linear model: rate = (H0 - Hcurrent) / time
    stats.linearRate = (stats.initialHealth - stats.currentHealth) / stats.monthsElapsed;

    // Linear prediction to 50%
    if (stats.linearRate > 0.f) {
        stats.linearMonthsTo50 = (stats.currentHealth - 50.f) / stats.linearRate;
    }

    // Exponential model: ? = ln(H0/Hcurrent) / time
    if (stats.currentHealth > 0.f && stats.initialHealth > 0.f) {
        stats.lambda = logf(stats.initialHealth / stats.currentHealth) / stats.monthsElapsed;

        // Exp prediction to 50%: t = ln(H0/50) / ?
        if (stats.lambda > 0.f) {
            stats.expMonthsTo50 = logf(stats.initialHealth / 50.f) / stats.lambda;
        }
    }

    return stats;
}

// Generate future predictions
void CPredictionDlg::GenerateFuturePredictions(const PredictionStats& stats,
    int monthsAhead,
    std::vector<float>& linearPred,
    std::vector<float>& expPred) const
{
    linearPred.clear();
    expPred.clear();

    for (int i = 0; i <= monthsAhead; ++i) {
        float t = stats.monthsElapsed + i;

        // Linear: H(t) = H0 - rate*t
        float lin = stats.initialHealth - stats.linearRate * t;
        linearPred.push_back(std::max(0.f, std::min(100.f, lin)));

        // Exponential: H(t) = H0 * e^(-?t)
        float exp = stats.initialHealth * expf(-stats.lambda * t);
        expPred.push_back(std::max(0.f, std::min(100.f, exp)));
    }
}

// ==================== Paint with 3 Charts ====================
void CPredictionDlg::OnPaint()
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
    if (useN < 2) {
        Gdiplus::Font f(L"Segoe UI", 12.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush b(Gdiplus::Color(255, 60, 60, 60));
        Gdiplus::StringFormat c; c.SetAlignment(Gdiplus::StringAlignmentCenter);
        c.SetLineAlignment(Gdiplus::StringAlignmentCenter);
        g.DrawString(L"No battery estimate data found.\nGenerate a report again and reopen.",
            -1, &f,
            Gdiplus::PointF((Gdiplus::REAL)rc.Width() / 2.f, (Gdiplus::REAL)rc.Height() / 2.f),
            &c, &b);

        dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
        memDC.SelectObject(pOldBmp);
        return;
    }

    // --- predictions ---
    PredictionStats stats = CalculatePredictions();

    // If current health is ~50%, show a message and DO NOT draw the chart
    if (_finite(stats.currentHealth) && fabsf(stats.currentHealth - 50.f) < 0.1f) {

        Gdiplus::Font title(L"Segoe UI", 14.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
        Gdiplus::Font body(L"Segoe UI", 11.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
        Gdiplus::SolidBrush txt(Gdiplus::Color(255, 60, 60, 60));
        Gdiplus::StringFormat center; center.SetAlignment(Gdiplus::StringAlignmentCenter);
        center.SetLineAlignment(Gdiplus::StringAlignmentCenter);

        // Title line
        g.DrawString(L"Current health has reached 50%", -1, &title,
            Gdiplus::PointF((Gdiplus::REAL)rc.Width() / 2.f, (Gdiplus::REAL)rc.Height() / 2.f - 12.f),
            &center, &txt);

        // Optional body line
        g.DrawString(L"No prediction chart to display.", -1, &body,
            Gdiplus::PointF((Gdiplus::REAL)rc.Width() / 2.f, (Gdiplus::REAL)rc.Height() / 2.f + 14.f),
            &center, &txt);

        dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
        memDC.SelectObject(pOldBmp);
        return;
    }

    std::vector<float> linearPred, expPred;
    GenerateFuturePredictions(stats, 12, linearPred, expPred);

    // --- layout: ONLY the prediction chart, no statistics box ---
    const float margin = 60.f;
    const float labelLane = 36.f;

    // Use entire height (minus margins + label lane) for the chart
    float chartHeight = rc.Height() - 2.f * margin - labelLane;
    if (chartHeight < 160.f) chartHeight = 160.f;
    float chartWidth = rc.Width() - 2.f * margin;

    float chartY = margin;

    // Draw ONLY Chart 3 (Prediction)
    DrawHealthPredictionChart(g, margin, chartY, chartWidth, chartHeight, labelLane,
        stats, linearPred, expPred);

    // blit
    dc.BitBlt(0, 0, rc.Width(), rc.Height(), &memDC, 0, 0, SRCCOPY);
    memDC.SelectObject(pOldBmp);
}





// ========== CHART 1: Active Battery Life ==========
void CPredictionDlg::DrawActiveBatteryChart(Gdiplus::Graphics& g, float x, float y, float w, float h, float labelLane)
{
    const int useN = (int)std::min(m_full.size(), m_periods.size());

    Gdiplus::SolidBrush white(Gdiplus::Color(255, 255, 255, 255));
    g.FillRectangle(&white, x, y, w, h);

    Gdiplus::Pen axis(Gdiplus::Color(255, 0, 0, 0), 2.f);
    g.DrawLine(&axis, x, y + h, x + w, y + h);
    g.DrawLine(&axis, x, y, x, y + h);

    // Filter data
    std::vector<CString> fPeriods;
    std::vector<float> fFull, fDesign;
    const float MAX_ACTIVE = 16.f;

    for (int i = 0; i < useN; ++i) {
        if (m_full[i] >= 0.f && m_full[i] <= MAX_ACTIVE &&
            m_design[i] >= 0.f && m_design[i] <= MAX_ACTIVE) {
            fPeriods.push_back(m_periods[i]);
            fFull.push_back(m_full[i]);
            fDesign.push_back(m_design[i]);
        }
    }

    const int useF = (int)fFull.size();
    if (useF < 2) return;

    float maxY = 0.f;
    for (int i = 0; i < useF; ++i)
        maxY = std::max(maxY, std::max(fFull[i], fDesign[i]));
    if (maxY <= 0.f) maxY = 1.f;
    maxY = ceilf(maxY);

    const float stepX = w / (float)(std::max(1, useF - 1));
    auto mapY = [&](float v) { return y + h - (v / maxY) * h; };

    // Draw lines
    Gdiplus::Pen penFull(Gdiplus::Color(255, 0, 122, 204), 3.f);
    Gdiplus::Pen penDes(Gdiplus::Color(255, 255, 140, 0), 3.f);

    std::vector<Gdiplus::PointF> pFull, pDes;
    for (int i = 0; i < useF; ++i) {
        float px = x + stepX * i;
        pFull.emplace_back(px, mapY(fFull[i]));
        pDes.emplace_back(px, mapY(fDesign[i]));
    }

    if (pFull.size() >= 2) g.DrawLines(&penFull, pFull.data(), (INT)pFull.size());
    if (pDes.size() >= 2) g.DrawLines(&penDes, pDes.data(), (INT)pDes.size());

    // Draw points
    Gdiplus::SolidBrush dotBlue(Gdiplus::Color(255, 0, 122, 204));
    Gdiplus::SolidBrush dotOrg(Gdiplus::Color(255, 255, 140, 0));
    for (auto& p : pFull) g.FillEllipse(&dotBlue, Gdiplus::RectF(p.X - 3.f, p.Y - 3.f, 6.f, 6.f));
    for (auto& p : pDes) g.FillEllipse(&dotOrg, Gdiplus::RectF(p.X - 3.f, p.Y - 3.f, 6.f, 6.f));

    // Grid + Y ticks
    Gdiplus::Pen grid(Gdiplus::Color(80, 200, 200, 200), 1.f);
    Gdiplus::SolidBrush tickTxt(Gdiplus::Color(255, 60, 60, 60));
    Gdiplus::Font tickFont(L"Segoe UI", 8.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::StringFormat right; right.SetAlignment(Gdiplus::StringAlignmentFar);

    for (int i = 0; i <= 5; ++i) {
        float v = (maxY * i) / 5.f, py = mapY(v);
        g.DrawLine(&grid, x, py, x + w, py);
        wchar_t buf[32]; swprintf_s(buf, L"%.1f", v);
        g.DrawString(buf, -1, &tickFont, Gdiplus::PointF(x - 10.f, py - 7.f), &right, &tickTxt);
    }

    // Title
    Gdiplus::Font fTitle(L"Segoe UI", 11.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush titleBrush(Gdiplus::Color(255, 40, 40, 40));
    Gdiplus::StringFormat centerTop;
    centerTop.SetAlignment(Gdiplus::StringAlignmentCenter);
    g.DrawString(L"Active Battery Life (hours)", -1, &fTitle,
        Gdiplus::PointF(x + w / 2.f, y - 24.f), &centerTop, &titleBrush);

    // Legend
    Gdiplus::Font fLegend(L"Segoe UI", 8.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    float lx = x + w - 180.f, ly = y + 8.f;
    g.DrawLine(&penFull, lx, ly + 5.f, lx + 15.f, ly + 5.f);
    g.DrawString(L"Full Charge", -1, &fLegend, Gdiplus::PointF(lx + 20.f, ly), nullptr, &tickTxt);
    g.DrawLine(&penDes, lx, ly + 18.f, lx + 15.f, ly + 18.f);
    g.DrawString(L"Design Capacity", -1, &fLegend, Gdiplus::PointF(lx + 20.f, ly + 13.f), nullptr, &tickTxt);

    // X labels
    DrawRotatedLabels(g, fPeriods, x, y + h, stepX, labelLane);
}

// ========== CHART 2: Health Trend ==========
void CPredictionDlg::DrawHealthTrendChart(Gdiplus::Graphics& g, float x, float y, float w, float h, float /*labelLane*/)
{
    const int useN = (int)std::min(m_full.size(), m_periods.size());

    Gdiplus::SolidBrush white(Gdiplus::Color(255, 255, 255, 255));
    g.FillRectangle(&white, x, y, w, h);

    Gdiplus::Pen axis(Gdiplus::Color(255, 0, 0, 0), 2.f);
    g.DrawLine(&axis, x, y + h, x + w, y + h);
    g.DrawLine(&axis, x, y, x, y + h);

    // Build health% only where design > 0
    std::vector<float> healthPct;
    healthPct.reserve(useN);
    for (int i = 0; i < useN; ++i) {
        if (m_design[i] > 0.f) {
            healthPct.push_back((m_full[i] / m_design[i]) * 100.f);
        }
        else {
            // keep series continuous: repeat last valid or assume 0
            healthPct.push_back(healthPct.empty() ? 0.f : healthPct.back());
        }
    }

    const float stepX = w / (float)(std::max(1, useN - 1));
    auto mapY = [&](float v) { return y + h - (v / 100.f) * h; };

    std::vector<Gdiplus::PointF> hp;
    hp.reserve(useN);
    for (int i = 0; i < useN; ++i) {
        hp.emplace_back(x + stepX * i, mapY(healthPct[i]));
    }

    Gdiplus::Pen penH(Gdiplus::Color(255, 0, 160, 0), 3.f);
    if (hp.size() >= 2) g.DrawLines(&penH, hp.data(), (INT)hp.size());

    Gdiplus::SolidBrush dotG(Gdiplus::Color(255, 0, 160, 0));
    for (auto& p : hp) g.FillEllipse(&dotG, Gdiplus::RectF(p.X - 3.f, p.Y - 3.f, 6.f, 6.f));

    // Grid + Y ticks
    Gdiplus::Pen grid(Gdiplus::Color(80, 200, 200, 200), 1.f);
    Gdiplus::SolidBrush tickTxt(Gdiplus::Color(255, 60, 60, 60));
    Gdiplus::Font tickFont(L"Segoe UI", 8.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::StringFormat right; right.SetAlignment(Gdiplus::StringAlignmentFar);

    for (int i = 0; i <= 5; ++i) {
        float v = (100.f * i) / 5.f, py = mapY(v);
        g.DrawLine(&grid, x, py, x + w, py);
        wchar_t buf[32]; swprintf_s(buf, L"%.0f%%", v);
        g.DrawString(buf, -1, &tickFont, Gdiplus::PointF(x - 10.f, py - 7.f), &right, &tickTxt);
    }

    // Title
    Gdiplus::Font fTitle(L"Segoe UI", 11.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush titleBrush(Gdiplus::Color(255, 40, 40, 40));
    Gdiplus::StringFormat centerTop;
    centerTop.SetAlignment(Gdiplus::StringAlignmentCenter);
    g.DrawString(L"Battery Health (%)", -1, &fTitle,
        Gdiplus::PointF(x + w / 2.f, y - 24.f), &centerTop, &titleBrush);

    // ==== X labels: anchor each label to *its own point* directly below the chart line ====
    // Draw every label; if you want fewer, change the step from 1 to 2/3/etc.
    Gdiplus::StringFormat startFmt;
    startFmt.SetAlignment(Gdiplus::StringAlignmentNear);     // start drawing exactly at the point
    startFmt.SetLineAlignment(Gdiplus::StringAlignmentNear); // text extends down/right after rotation

    for (int i = 0; i < useN; ++i) {
        const float px = x + stepX * i;

        Gdiplus::Matrix oldT; g.GetTransform(&oldT);
        g.TranslateTransform(px, y + h + 4.f);   // just below the axis line
        g.RotateTransform(-45.f);

        g.DrawString(m_periods[i].GetString(), -1, &tickFont,
            Gdiplus::PointF(0.f, 0.f), &startFmt, &tickTxt);

        g.SetTransform(&oldT);
    }
}





// ========== CHART 3: Health Prediction ==========

void CPredictionDlg::DrawHealthPredictionChart(Gdiplus::Graphics& g, float x, float y, float w, float h, float labelLane,
    const PredictionStats& stats, const std::vector<float>& linearPred, const std::vector<float>& expPred)
{
    const int useN = (int)std::min(m_full.size(), m_periods.size());
    const int predN = (int)linearPred.size();

    Gdiplus::SolidBrush white(Gdiplus::Color(255, 255, 255, 255));
    g.FillRectangle(&white, x, y, w, h);

    Gdiplus::Pen axis(Gdiplus::Color(255, 0, 0, 0), 2.f);
    g.DrawLine(&axis, x, y + h, x + w, y + h);
    g.DrawLine(&axis, x, y, x, y + h);

    // Actual health
    std::vector<float> healthPct;
    for (int i = 0; i < useN; ++i) {
        if (m_design[i] > 0.f) healthPct.push_back((m_full[i] / m_design[i]) * 100.f);
    }

    // Overlap first prediction point with last actual X
    const int   overlapOffset = std::max(0, useN - 1);
    const int   totalPoints = overlapOffset + predN;
    const float stepX = w / (float)(std::max(1, totalPoints - 1));

    auto mapY = [&](float v) { return y + h - ((v - 40.f) / 60.f) * h; }; // 40–100%

    // 50% threshold line
    Gdiplus::Pen threshold(Gdiplus::Color(180, 255, 0, 0), 2.f);
    float dashPattern[] = { 8.f, 4.f };
    threshold.SetDashPattern(dashPattern, 2);
    const float y50 = mapY(50.f);
    g.DrawLine(&threshold, x, y50, x + w, y50);

    // Actual line + points
    std::vector<Gdiplus::PointF> actualPoints;
    for (int i = 0; i < useN; ++i)
        actualPoints.emplace_back(x + stepX * (float)i, mapY(healthPct[i]));

    Gdiplus::Pen penActual(Gdiplus::Color(255, 0, 160, 0), 3.f);
    if (actualPoints.size() >= 2) g.DrawLines(&penActual, actualPoints.data(), (INT)actualPoints.size());
    Gdiplus::SolidBrush dotGreen(Gdiplus::Color(255, 0, 160, 0));
    for (auto& p : actualPoints) g.FillEllipse(&dotGreen, Gdiplus::RectF(p.X - 3.f, p.Y - 3.f, 6.f, 6.f));

    // Prediction lines (continuous alignment)
    std::vector<Gdiplus::PointF> linearPoints, expPoints;
    for (int i = 0; i < predN; ++i) {
        const float px = x + stepX * (float)(overlapOffset + i);
        linearPoints.emplace_back(px, mapY(linearPred[i]));
        expPoints.emplace_back(px, mapY(expPred[i]));
    }

    Gdiplus::Pen penLinear(Gdiplus::Color(255, 239, 68, 68), 2.5f);
    float dashLinear[] = { 6.f, 4.f }; penLinear.SetDashPattern(dashLinear, 2);
    if (linearPoints.size() >= 2) g.DrawLines(&penLinear, linearPoints.data(), (INT)linearPoints.size());

    Gdiplus::Pen penExp(Gdiplus::Color(255, 245, 158, 11), 2.5f);
    float dashExp[] = { 10.f, 5.f }; penExp.SetDashPattern(dashExp, 2);
    if (expPoints.size() >= 2) g.DrawLines(&penExp, expPoints.data(), (INT)expPoints.size());

    // Grid + Y ticks
    Gdiplus::Pen grid(Gdiplus::Color(80, 200, 200, 200), 1.f);
    Gdiplus::SolidBrush tickTxt(Gdiplus::Color(255, 60, 60, 60));
    Gdiplus::Font tickFont(L"Segoe UI", 8.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::StringFormat right; right.SetAlignment(Gdiplus::StringAlignmentFar);
    for (int i = 0; i <= 6; ++i) {
        float v = 40.f + (60.f * i) / 6.f, py = mapY(v);
        g.DrawLine(&grid, x, py, x + w, py);
        wchar_t buf[32]; swprintf_s(buf, L"%.0f%%", v);
        g.DrawString(buf, -1, &tickFont, Gdiplus::PointF(x - 10.f, py - 7.f), &right, &tickTxt);
    }

    // Title + legend
    Gdiplus::Font fTitle(L"Segoe UI", 11.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush titleBrush(Gdiplus::Color(255, 40, 40, 40));
    Gdiplus::StringFormat centerTop; centerTop.SetAlignment(Gdiplus::StringAlignmentCenter);
    g.DrawString(L"Battery Health Prediction", -1, &fTitle,
        Gdiplus::PointF(x + w / 2.f, y - 24.f), &centerTop, &titleBrush);

    Gdiplus::Font fLegend(L"Segoe UI", 8.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    float lx = x + 10.f, ly = y + 8.f;
    g.DrawLine(&penActual, lx, ly + 5.f, lx + 20.f, ly + 5.f);
    g.DrawString(L"Actual", -1, &fLegend, Gdiplus::PointF(lx + 25.f, ly), nullptr, &tickTxt);
    g.DrawLine(&penLinear, lx, ly + 20.f, lx + 20.f, ly + 20.f);
    g.DrawString(L"Linear Prediction", -1, &fLegend, Gdiplus::PointF(lx + 25.f, ly + 15.f), nullptr, &tickTxt);
    g.DrawLine(&penExp, lx, ly + 35.f, lx + 20.f, ly + 35.f);
    g.DrawString(L"Exponential Prediction", -1, &fLegend, Gdiplus::PointF(lx + 25.f, ly + 30.f), nullptr, &tickTxt);
    g.DrawLine(&threshold, lx, ly + 50.f, lx + 20.f, ly + 50.f);
    g.DrawString(L"50% Threshold", -1, &fLegend, Gdiplus::PointF(lx + 25.f, ly + 45.f), nullptr, &tickTxt);

    // === Calculate intersection points and their month labels ===
    std::vector<std::pair<float, CString>> intersectionLabels; // {x-position, month label}

    auto calculateIntersectionLabel = [&](const std::vector<float>& pred, float etaMonths) -> std::pair<float, CString> {
        if (pred.size() < 2) return { -1.f, L"" };

        int crossIdx = -1;
        for (int i = 1; i < (int)pred.size(); ++i) {
            const float y0 = pred[i - 1], y1 = pred[i];
            if ((y0 - 50.f) * (y1 - 50.f) <= 0.f) { crossIdx = i; break; }
        }
        if (crossIdx < 0) return { -1.f, L"" };

        const float y0 = pred[crossIdx - 1], y1 = pred[crossIdx];
        const float denom = (y0 - y1);
        float tseg = 0.f;
        if (fabsf(denom) > 1e-6f) tseg = (y0 - 50.f) / denom;
        tseg = std::max(0.f, std::min(1.f, tseg));

        const float predIndex = (crossIdx - 1) + tseg;
        const float px = x + stepX * (float)(overlapOffset + predIndex);

        // Month label based on last period’s end
        SYSTEMTIME baseDate{};
        if (!m_periods.empty()) {
            CString lastPeriod = m_periods.back();
            std::wstring w = lastPeriod.GetString();
            std::wregex dateRe(LR"((\d{2})-(\d{2}))");
            std::wsregex_iterator it(w.begin(), w.end(), dateRe), end;
            std::wsregex_iterator lastMatch = end;
            for (; it != end; ++it) lastMatch = it;
            if (lastMatch != end) {
                int month = _wtoi(lastMatch->str(1).c_str());
                baseDate.wYear = 2025;
                baseDate.wMonth = (WORD)month;
                baseDate.wDay = 14;
            }
        }
        if (baseDate.wMonth == 0) { baseDate.wYear = 2025; baseDate.wMonth = 10; baseDate.wDay = 14; }

        int baseMonth = (int)baseDate.wMonth;
        int baseYear = (int)baseDate.wYear;

        int monthsAhead = (int)roundf(etaMonths);
        int targetMonth = baseMonth + monthsAhead;
        int targetYear = baseYear + (targetMonth - 1) / 12;
        targetMonth = ((targetMonth - 1) % 12) + 1;

        static const wchar_t* MONS[12] = {
            L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun",
            L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec"
        };

        CString monthLabel;
        monthLabel.Format(L"%s '%02d", MONS[targetMonth - 1], (targetYear % 100));

        return { px, monthLabel };
        };

    // Calculate intersection labels for both prediction models
    std::pair<float, CString> linearIntersect = calculateIntersectionLabel(linearPred, stats.linearMonthsTo50);
    std::pair<float, CString> expIntersect = calculateIntersectionLabel(expPred, stats.expMonthsTo50);

    if (linearIntersect.first >= 0.f) {
        intersectionLabels.push_back(linearIntersect);
    }
    if (expIntersect.first >= 0.f && expIntersect.second != linearIntersect.second) {
        intersectionLabels.push_back(expIntersect);
    }

    // Draw intersection point markers
    Gdiplus::SolidBrush linearMarker(Gdiplus::Color(255, 239, 68, 68));
    Gdiplus::SolidBrush expMarker(Gdiplus::Color(255, 245, 158, 11));

    if (linearIntersect.first >= 0.f) {
        g.FillEllipse(&linearMarker, Gdiplus::RectF(linearIntersect.first - 4.f, y50 - 4.f, 8.f, 8.f));
    }
    if (expIntersect.first >= 0.f) {
        g.FillEllipse(&expMarker, Gdiplus::RectF(expIntersect.first - 4.f, y50 - 4.f, 8.f, 8.f));
    }

    // === Rotated numeric ETA labels ("5.2 mon") near the markers ===
    Gdiplus::Font etaFont(L"Segoe UI", 8.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush etaBrush(Gdiplus::Color(255, 70, 70, 70));
    auto drawEtaLabel = [&](float px, float months) {
        if (!_finite(months) || months < 0.f) return;
        wchar_t txt[32]; swprintf_s(txt, L"%.1f mon", months);

        // Rotate text by -45 degrees around an anchor slightly to the right of the marker
        Gdiplus::StringFormat startFmt;
        startFmt.SetAlignment(Gdiplus::StringAlignmentNear);
        startFmt.SetLineAlignment(Gdiplus::StringAlignmentNear);

        Gdiplus::Matrix oldT; g.GetTransform(&oldT);
        g.TranslateTransform(px + 10.f, y50 - 6.f); // anchor near the dot
        g.RotateTransform(-45.f);                   // rotate 45 degrees
        g.DrawString(txt, -1, &etaFont, Gdiplus::PointF(0.f, 0.f), &startFmt, &etaBrush);
        g.SetTransform(&oldT);
        };
    if (linearIntersect.first >= 0.f) drawEtaLabel(linearIntersect.first, stats.linearMonthsTo50);
    if (expIntersect.first >= 0.f)    drawEtaLabel(expIntersect.first, stats.expMonthsTo50);

    // === Draw ONLY intersection month labels below the 50% line ===
    Gdiplus::StringFormat centerFmt;
    centerFmt.SetAlignment(Gdiplus::StringAlignmentCenter);
    centerFmt.SetLineAlignment(Gdiplus::StringAlignmentNear);

    for (size_t i = 0; i < intersectionLabels.size(); ++i) {
        float px = intersectionLabels[i].first;
        CString label = intersectionLabels[i].second;

        Gdiplus::Matrix oldT; g.GetTransform(&oldT);
        g.TranslateTransform(px, y + h + 8.f);  // below axis
        g.RotateTransform(-45.f);

        Gdiplus::SolidBrush highlightBrush(Gdiplus::Color(255, 220, 50, 50));
        g.DrawString(label.GetString(), -1, &tickFont,
            Gdiplus::PointF(0.f, 0.f), &centerFmt, &highlightBrush);

        g.SetTransform(&oldT);
    }

    // === Actual data labels (as before) ===
    std::vector<CString> actualLabels;
    for (int i = 0; i < useN; ++i) actualLabels.push_back(m_periods[i]);

    for (int i = 0; i < useN && i < (int)actualLabels.size(); ++i) {
        const float px = x + stepX * (float)i;

        Gdiplus::Matrix oldT; g.GetTransform(&oldT);
        g.TranslateTransform(px, y + h + 8.f);
        g.RotateTransform(-45.f);

        g.DrawString(actualLabels[i].GetString(), -1, &tickFont,
            Gdiplus::PointF(0.f, 0.f), &centerFmt, &tickTxt);

        g.SetTransform(&oldT);
    }
}










// ========== Statistics Box ==========

void CPredictionDlg::DrawStatisticsBox(Gdiplus::Graphics& g, float x, float y, float w, float h,
    const PredictionStats& stats)
{
    // ----- Colors & pens -----
    Gdiplus::SolidBrush boxBg(Gdiplus::Color(255, 255, 250, 240));
    Gdiplus::Pen       boxBorder(Gdiplus::Color(255, 255, 193, 7), 3.f);
    Gdiplus::SolidBrush textBrush(Gdiplus::Color(255, 40, 40, 40));
    Gdiplus::SolidBrush accentBrush(Gdiplus::Color(255, 230, 126, 34)); // for rate
    Gdiplus::SolidBrush faintBrush(Gdiplus::Color(255, 90, 90, 90));    // for labels
    Gdiplus::SolidBrush warnBrush(Gdiplus::Color(255, 200, 80, 0));     // for footer

    // ----- Background & border -----
    g.FillRectangle(&boxBg, x, y, w, h);
    g.DrawRectangle(&boxBorder, x, y, w, h);

    // ----- Fonts (use FontFamily-based ctor to avoid ambiguity) -----
    Gdiplus::FontFamily fam(L"Segoe UI");

    Gdiplus::Font fTitle(&fam, 10.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font fValue(&fam, 16.f, Gdiplus::FontStyleBold, Gdiplus::UnitPoint);
    Gdiplus::Font fLabel(&fam, 8.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::Font fFooter(&fam, 8.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);

    // ----- Layout constants -----
    const Gdiplus::REAL pad = 12.f;       // outer padding
    const Gdiplus::REAL vGap = 8.f;        // vertical gap between value and label
    const Gdiplus::REAL titleH = 18.f;       // title row height
    const Gdiplus::REAL cellGap = 10.f;       // gap between grid cells
    const Gdiplus::REAL footerH = 28.f;       // reserved footer height

    // ----- Title -----
    Gdiplus::RectF rcTitle(x + pad, y + pad, w - 2.f * pad, titleH);
    g.DrawString(L"Prediction Statistics", -1, &fTitle, rcTitle, nullptr, &textBrush);

    // ----- Inner content area (2x2 grid) -----
    Gdiplus::REAL innerX = x + pad;
    Gdiplus::REAL innerY = y + pad + titleH + 4.f;
    Gdiplus::REAL innerW = w - 2.f * pad;
    Gdiplus::REAL innerH = h - (innerY - y) - pad - footerH;
    if (innerH < 40.f) innerH = 40.f;

    Gdiplus::REAL colW = (innerW - cellGap) / 2.f;
    Gdiplus::REAL rowH = (innerH - cellGap) / 2.f;

    // ----- StringFormat helpers -----
    Gdiplus::StringFormat fmtLeftTop;
    fmtLeftTop.SetAlignment(Gdiplus::StringAlignmentNear);
    fmtLeftTop.SetLineAlignment(Gdiplus::StringAlignmentNear);
    fmtLeftTop.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    fmtLeftTop.SetFormatFlags(Gdiplus::StringFormatFlagsNoWrap);

    // Helper to draw a metric in a cell
    auto drawCell = [&](int col, int row, const WCHAR* value, const WCHAR* label, bool accent = false)
        {
            Gdiplus::REAL cx = innerX + (col == 0 ? 0.f : (colW + cellGap));
            Gdiplus::REAL cy = innerY + (row == 0 ? 0.f : (rowH + cellGap));
            Gdiplus::RectF rcCell(cx, cy, colW, rowH);

            // Value
            Gdiplus::RectF rcVal = rcCell;
            rcVal.Height = rcCell.Height * 0.58f;
            g.DrawString(value, -1, &fValue, rcVal, &fmtLeftTop, accent ? &accentBrush : &textBrush);

            // Label
            Gdiplus::RectF rcLbl = rcCell;
            rcLbl.Y = rcVal.GetBottom() + vGap;
            rcLbl.Height = rcCell.GetBottom() - rcLbl.Y;
            g.DrawString(label, -1, &fLabel, rcLbl, &fmtLeftTop, &faintBrush);
        };

    // ----- Safe formatting helpers -----
    auto fmtFloat = [](wchar_t* buf, size_t n, float v, const wchar_t* fmt) {
        if (!_finite(v) || v < -1e7f || v > 1e7f) { wcscpy_s(buf, n, L"—"); return; }
        swprintf_s(buf, n, fmt, v);
        };

    wchar_t sCurrent[64] = L"—";
    wchar_t sRate[64] = L"—";
    wchar_t sLinETA[64] = L"—";
    wchar_t sExpETA[64] = L"—";

    if (stats.currentHealth > 0.f)                       fmtFloat(sCurrent, 64, stats.currentHealth, L"%.1f%%");
    if (_finite(stats.linearRate))                       fmtFloat(sRate, 64, stats.linearRate, L"-%.2f%%/mo");
    if (_finite(stats.linearMonthsTo50) && stats.linearMonthsTo50 >= 0.f)
        fmtFloat(sLinETA, 64, stats.linearMonthsTo50, L"%.1f mo");
    if (_finite(stats.expMonthsTo50) && stats.expMonthsTo50 >= 0.f)
        fmtFloat(sExpETA, 64, stats.expMonthsTo50, L"%.1f mo");

    // ----- Draw 4 cells -----
    drawCell(0, 0, sCurrent, L"Current Health");
    drawCell(1, 0, sRate, L"Degradation Rate", /*accent*/true);
    drawCell(0, 1, sLinETA, L"50% ETA (Linear)");
    drawCell(1, 1, sExpETA, L"50% ETA (Exponential)");

    // ----- Footer (inside reserved area, with line-limit & ellipsis) -----
    const WCHAR* footer =
        L"Predictions use ~2.5 months of data. Expect ±2–3 months variance. Usage/temperature strongly affect accuracy.";

    Gdiplus::RectF rcFooter(x + pad, y + h - pad - footerH, w - 2.f * pad, footerH);
    Gdiplus::StringFormat fmtFooter;
    fmtFooter.SetAlignment(Gdiplus::StringAlignmentNear);
    fmtFooter.SetLineAlignment(Gdiplus::StringAlignmentNear);
    fmtFooter.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    fmtFooter.SetFormatFlags(Gdiplus::StringFormatFlagsLineLimit); // wrap within footer rect

    g.DrawString(footer, -1, &fFooter, rcFooter, &fmtFooter, &warnBrush);
}









// ========== Helper: Draw Rotated X-axis Labels ==========
void CPredictionDlg::DrawRotatedLabels(Gdiplus::Graphics& g, const std::vector<CString>& labels,
    float x, float baseY, float stepX, float labelLane)
{
    Gdiplus::Font tickFont(L"Segoe UI", 8.f, Gdiplus::FontStyleRegular, Gdiplus::UnitPoint);
    Gdiplus::SolidBrush lblBrush(Gdiplus::Color(255, 40, 40, 40));
    Gdiplus::StringFormat farFmt;
    farFmt.SetAlignment(Gdiplus::StringAlignmentFar);
    farFmt.SetLineAlignment(Gdiplus::StringAlignmentNear);

    for (int i = 0; i < (int)labels.size(); ++i) {
        float px = x + stepX * i;

        Gdiplus::Matrix oldT; g.GetTransform(&oldT);
        g.TranslateTransform(px, baseY + 10.f);
        g.RotateTransform(-45.f);

        g.DrawString(labels[i].GetString(), -1, &tickFont,
            Gdiplus::PointF(-2.f, 0.f), &farFmt, &lblBrush);

        g.SetTransform(&oldT);
    }
}