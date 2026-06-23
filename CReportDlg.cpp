#include "pch.h"
#include "CReportDlg.h"
#include "afxdialogex.h"
#include <afxcmn.h>
#include <regex>
#include <vector>
#include <utility>
#include <cmath>
#include <shellapi.h>
#include <winspool.h>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winspool.lib")

IMPLEMENT_DYNAMIC(CReportDlg, CDialogEx)

CReportDlg::CReportDlg(const BatteryReportData& data, CWnd* pParent)
    : CDialogEx(IDD_REPORT_DIALOG, pParent),
    m_reportData(data)
{
}
CReportDlg::~CReportDlg() {}

void CReportDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

//////////////////////////////////////////////////////////////
// 🔹 HELPERS
//////////////////////////////////////////////////////////////

static CString StripHtml(const CString& input)
{
    std::wstring s = input.GetString();
    s = std::regex_replace(s, std::wregex(L"<[^>]*>"), L"");
    return CString(s.c_str());
}

static CString ReadTextAutoEncoding(const CString& path)
{
    CStdioFile file;
    if (!file.Open(path, CFile::modeRead | CFile::typeBinary))
        return L"";

    CStringA dataA;
    UINT len = (UINT)file.GetLength();
    file.Read(dataA.GetBuffer(len), len);
    dataA.ReleaseBuffer(len);
    file.Close();

    int wlen = MultiByteToWideChar(CP_UTF8, 0, dataA, -1, nullptr, 0);
    CStringW result;
    MultiByteToWideChar(CP_UTF8, 0, dataA, -1, result.GetBuffer(wlen), wlen);
    result.ReleaseBuffer();
    return result;
}

static double HmsTextToHours(const CString& hms)
{
    if (hms.IsEmpty() || hms == L"-") return 0.0;
    int h = 0, m = 0, s = 0;
    if (swscanf_s(hms.GetString(), L"%d:%d:%d", &h, &m, &s) == 3)
        return h + m / 60.0 + s / 3600.0;
    return 0.0;
}

// Parse "12 345 mWh" or "12345 mWh" -> double
static double ParseCapacityMwh(const CString& text)
{
    std::wstring s = text.GetString();
    // strip everything except digits and dot
    std::wstring digits;
    for (wchar_t c : s)
        if (iswdigit(c) || c == L'.') digits += c;
    if (digits.empty()) return 0.0;
    return _wtof(digits.c_str());
}

//////////////////////////////////////////////////////////////
// 🔹 PARSERS
//////////////////////////////////////////////////////////////

static bool GetUsageHistory(std::vector<CReportDlg::UsageHistoryRow>& outRows, const CString& html)
{
    std::wregex re(LR"(USAGE\s*HISTORY[\s\S]*?<table[^>]*>([\s\S]*?)</table>)", std::regex_constants::icase);
    std::wsmatch mh;
    std::wstring H = html.GetString();
    if (!std::regex_search(H, mh, re)) return false;

    std::wstring table = mh[1].str();
    std::wregex rowRe(L"<tr[^>]*>([\\s\\S]*?)</tr>");
    std::wregex cellRe(L"<t[dh][^>]*>([\\s\\S]*?)</t[dh]>");
    bool headerSkipped = false;

    for (std::wsregex_iterator it(table.begin(), table.end(), rowRe), end; it != end; ++it)
    {
        std::vector<CString> cells;
        for (std::wsregex_iterator ic((*it)[1].first, (*it)[1].second, cellRe); ic != end; ++ic)
        {
            CString c = StripHtml(CString((*ic)[1].str().c_str()));
            c.Trim();
            if (!c.IsEmpty()) cells.push_back(c);
        }
        if (cells.size() < 5) continue;
        if (!headerSkipped) { headerSkipped = true; continue; }

        CReportDlg::UsageHistoryRow r;
        r.period = cells[0];
        r.battActiveHrs = HmsTextToHours(cells[1]);
        r.battStandbyHrs = HmsTextToHours(cells[2]);
        r.acActiveHrs = HmsTextToHours(cells[3]);
        r.acStandbyHrs = HmsTextToHours(cells[4]);
        outRows.push_back(r);
    }
    return true;
}

static bool GetBatteryCapacity(std::vector<CReportDlg::BatteryCapacityRow>& out, const CString& html)
{
    std::wregex re(LR"(BATTERY\s*CAPACITY\s*HISTORY[\s\S]*?<table[^>]*>([\s\S]*?)</table>)", std::regex_constants::icase);
    std::wsmatch mh;
    std::wstring H = html.GetString();
    if (!std::regex_search(H, mh, re)) return false;

    std::wstring table = mh[1].str();
    std::wregex rowRe(L"<tr[^>]*>([\\s\\S]*?)</tr>");
    std::wregex cellRe(L"<t[dh][^>]*>([\\s\\S]*?)</t[dh]>");
    bool headerSkipped = false;

    for (std::wsregex_iterator it(table.begin(), table.end(), rowRe), end; it != end; ++it)
    {
        std::vector<CString> cells;
        for (std::wsregex_iterator ic((*it)[1].first, (*it)[1].second, cellRe); ic != end; ++ic)
        {
            CString c = StripHtml(CString((*ic)[1].str().c_str()));
            c.Trim();
            if (!c.IsEmpty()) cells.push_back(c);
        }
        if (cells.size() >= 3)
        {
            if (!headerSkipped) { headerSkipped = true; continue; }
            CReportDlg::BatteryCapacityRow r;
            r.period = cells[0];
            r.fullCharge = cells[1];
            r.designCapacity = cells[2];
            out.push_back(r);
        }
    }
    return true;
}

static bool GetBatteryLife(std::vector<CReportDlg::BatteryLifeRow>& out, const CString& html)
{
    std::wregex re(LR"(BATTERY\s*LIFE\s*ESTIMATES[\s\S]*?<table[^>]*>([\s\S]*?)</table>)", std::regex_constants::icase);
    std::wsmatch mh;
    std::wstring H = html.GetString();
    if (!std::regex_search(H, mh, re)) return false;

    std::wstring table = mh[1].str();
    std::wregex rowRe(L"<tr[^>]*>([\\s\\S]*?)</tr>");
    std::wregex cellRe(L"<t[dh][^>]*>([\\s\\S]*?)</t[dh]>");
    bool headerSkipped = false;

    for (std::wsregex_iterator it(table.begin(), table.end(), rowRe), end; it != end; ++it)
    {
        std::vector<CString> cells;
        for (std::wsregex_iterator ic((*it)[1].first, (*it)[1].second, cellRe); ic != end; ++ic)
        {
            CString c = StripHtml(CString((*ic)[1].str().c_str()));
            c.Trim();
            if (!c.IsEmpty()) cells.push_back(c);
        }
        if (cells.size() < 5) continue;
        if (!headerSkipped) { headerSkipped = true; continue; }

        CReportDlg::BatteryLifeRow r;
        r.period = cells[0];
        r.designActive = HmsTextToHours(cells[1]);
        r.designConnected = HmsTextToHours(cells[2]);
        r.fullActive = HmsTextToHours(cells[3]);
        r.fullConnected = HmsTextToHours(cells[4]);
        out.push_back(r);
    }
    return true;
}

//////////////////////////////////////////////////////////////
// 🔹 BATTERY CONDITION SCORING ALGORITHM
//
//  Score 0-100 built from 5 weighted factors:
//
//  F1  Health ratio          (fullCharge / design)     weight 35
//  F2  Capacity trend        (degradation slope)        weight 20
//  F3  Life ratio            (fullActive / designActive)weight 20
//  F4  CPU drain rate        (% per min under load)     weight 13
//  F5  Discharge drain rate  (% per min idle)           weight 12
//
//  Score -> Grade:
//    85-100  Excellent
//    70-84   Good
//    50-69   Fair
//    30-49   Poor
//    0-29    Replace Now
//////////////////////////////////////////////////////////////


static double Clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

//////////////////////////////////////////////////////////////
// 🔹 BATTERY CONDITION SCORING ALGORITHM  (v2 — 4 factors)
//
//  Only real measured / device-reported data is used.
//  If a factor's data is unavailable the factor is excluded
//  and the remaining factor weights are scaled up proportionally
//  so the total is always out of 100 — never artificially padded.
//
//  Base weights (must sum to 100):
//    F1  Battery Health Ratio      50
//    F4  CPU Load Drain Rate       20
//    F5  Idle Discharge Drain Rate 20
//    F6  Charge Cycle Count        10
//
//  Score → Grade:
//    85-100  Excellent
//    70-84   Good
//    50-69   Fair
//    30-49   Poor
//    0-29    Replace Now
//////////////////////////////////////////////////////////////

static CReportDlg::ScoreResult ComputeBatteryScore(
    const BatteryReportData& rd,
    const std::vector<CReportDlg::BatteryCapacityRow>& capacity,
    const std::vector<CReportDlg::BatteryLifeRow>&     /*life — not used*/)
{
    CReportDlg::ScoreResult s = {};
    CString missing;

    // ── F1: Health ratio (base weight 50) ───────────────────────────────────
    //  Sources tried in order:
    //   1. fullChargeCapacity / designCapacity from m_reportData strings
    //   2. Same fields from the most-recent capacity history row
    //   3. The pre-computed "health" string field (e.g. "87" or "87%")
    {
        double designMwh = ParseCapacityMwh(rd.designCapacity);
        double fullMwh = ParseCapacityMwh(rd.fullChargeCapacity);

        if (designMwh <= 0 && !capacity.empty())
            designMwh = ParseCapacityMwh(capacity.front().designCapacity);
        if (fullMwh <= 0 && !capacity.empty())
            fullMwh = ParseCapacityMwh(capacity.front().fullCharge);

        double healthRatio = (designMwh > 0 && fullMwh > 0) ? (fullMwh / designMwh) : -1.0;

        if (healthRatio < 0 && !rd.health.IsEmpty())
        {
            double h = _wtof(rd.health.GetString());
            if (h > 1.0 && h <= 100.0) healthRatio = h / 100.0;
            else if (h > 0.0 && h <= 1.0)   healthRatio = h;
        }

        if (healthRatio >= 0.0)
        {
            s.healthPct = healthRatio * 100.0;
            // 100% health → sub-score 100,  50% health → 0,  <50% clamped to 0
            double score = Clamp01((healthRatio - 0.50) / 0.50) * 100.0;
            s.f1Health = score;
        }
        else
        {
            s.healthPct = -1.0;   // unknown — factor excluded
            s.f1Health = -1.0;
            if (!missing.IsEmpty()) missing += L", ";
            missing += L"Health ratio";
        }
    }

    // ── F4: CPU Load Drain Rate (base weight 20) ─────────────────────────────
    //  rd.cpuRate > 0  means the auto-test ran and produced a value.
    //  Healthy laptop/notebook: ~0.5–0.8 %/min under load.
    //  Very bad (swollen/failing cell): ≥ 2.5 %/min.
    {
        s.cpuRatePerMin = rd.cpuRate;
        if (rd.cpuRate > 0.0)
        {
            // Linear map: 0.5 %/min → 1.0,  2.5 %/min → 0.0
            double score = Clamp01(1.0 - (rd.cpuRate - 0.5) / 2.0) * 100.0;
            s.f4Cpu = score;
        }
        else
        {
            s.cpuRatePerMin = -1.0;
            s.f4Cpu = -1.0;
            if (!missing.IsEmpty()) missing += L", ";
            missing += L"CPU drain rate";
        }
    }

    // ── F5: Idle Discharge Drain Rate (base weight 20) ───────────────────────
    //  rd.disRate > 0  means the discharge phase ran.
    //  Healthy idle: ~0.1–0.3 %/min.  Very bad: ≥ 1.5 %/min.
    {
        s.disRatePerMin = rd.disRate;
        if (rd.disRate > 0.0)
        {
            // Linear map: 0.1 %/min → 1.0,  1.5 %/min → 0.0
            double score = Clamp01(1.0 - (rd.disRate - 0.1) / 1.4) * 100.0;
            s.f5Dis = score;
        }
        else
        {
            s.disRatePerMin = -1.0;
            s.f5Dis = -1.0;
            if (!missing.IsEmpty()) missing += L", ";
            missing += L"Discharge rate";
        }
    }

    // ── F6: Charge Cycle Count (base weight 10) ──────────────────────────────
    //  Standard Li-ion cycle life: ~500 (consumer) to ~1000 (premium).
    //  Scoring curve (IEC 61960 / Apple-style guidelines):
    //    0   – 300  cycles  → 100 pts  (barely used)
    //    300 – 500  cycles  → 100→70   (good range)
    //    500 – 800  cycles  → 70→40    (fair — noticeable wear)
    //    800 – 1000 cycles  → 40→10    (poor)
    //    > 1000     cycles  → 10→0     (replace)
    //  rd.cycles is a CString (e.g. "247" or "N/A" or empty).
    {
        int cycles = -1;
        if (!rd.cycles.IsEmpty())
        {
            wchar_t* end = nullptr;
            long v = wcstol(rd.cycles.GetString(), &end, 10);
            if (end != rd.cycles.GetString() && v >= 0)
                cycles = (int)v;
        }

        s.cycleCount = cycles;

        if (cycles >= 0)
        {
            double score;
            if (cycles <= 300)  score = 100.0;
            else if (cycles <= 500)  score = 100.0 - (cycles - 300) / 200.0 * 30.0; // 100→70
            else if (cycles <= 800)  score = 70.0 - (cycles - 500) / 300.0 * 30.0; // 70→40
            else if (cycles <= 1000) score = 40.0 - (cycles - 800) / 200.0 * 30.0; // 40→10
            else                     score = max(0.0, 10.0 - (cycles - 1000) / 100.0 * 5.0);
            s.f6Cycles = Clamp01(score / 100.0) * 100.0;
        }
        else
        {
            s.f6Cycles = -1.0;   // device did not report cycle count
            if (!missing.IsEmpty()) missing += L", ";
            missing += L"Cycle count";
        }
    }

    // ── Weighted sum with dynamic weight redistribution ───────────────────────
    //  For each factor that IS available, accumulate (sub-score × base-weight).
    //  Then divide by the total base-weight of available factors only.
    //  Result: always 0–100, never inflated by assumed values for missing data.
    struct Factor { double subScore; double baseWeight; };
    Factor factors[] = {
        { s.f1Health,  50.0 },
        { s.f4Cpu,     20.0 },
        { s.f5Dis,     20.0 },
        { s.f6Cycles,  10.0 },
    };

    double weightedSum = 0.0;
    double totalWeight = 0.0;
    for (auto& f : factors)
    {
        if (f.subScore >= 0.0)          // -1 means excluded
        {
            weightedSum += f.subScore * f.baseWeight;
            totalWeight += f.baseWeight;
        }
    }

    s.total = (totalWeight > 0.0) ? (weightedSum / totalWeight) : 0.0;

    // ── Missing-data note ─────────────────────────────────────────────────────
    if (!missing.IsEmpty())
        s.missingNote = L"Not measured / unavailable: " + missing;

    // ── Grade & advice ────────────────────────────────────────────────────────
    if (s.total >= 85.0)
    {
        s.grade = L"EXCELLENT";
        s.gradeColor = RGB(0, 160, 80);
        s.advice = L"Battery is in excellent condition. No action needed.";
    }
    else if (s.total >= 70.0)
    {
        s.grade = L"GOOD";
        s.gradeColor = RGB(80, 180, 0);
        s.advice = L"Battery is performing well. Keep an eye on cycle count over time.";
    }
    else if (s.total >= 50.0)
    {
        s.grade = L"FAIR";
        s.gradeColor = RGB(220, 160, 0);
        s.advice = L"Noticeable degradation detected. Avoid deep discharges and extreme heat.";
    }
    else if (s.total >= 30.0)
    {
        s.grade = L"POOR";
        s.gradeColor = RGB(220, 80, 0);
        s.advice = L"Battery is significantly degraded. Plan for replacement in the near future.";
    }
    else
    {
        s.grade = L"REPLACE NOW";
        s.gradeColor = RGB(200, 0, 0);
        s.advice = L"Battery health is critical. Replace immediately to prevent sudden shutdowns.";
    }

    return s;
}

//////////////////////////////////////////////////////////////
// 🔹 MESSAGE MAP
//////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CReportDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////
// 🔹 INIT / RESIZE
//////////////////////////////////////////////////////////////

BOOL CReportDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    wchar_t tempPath[MAX_PATH];
    GetTempPath(MAX_PATH, tempPath);

    CString path;
    path.Format(L"%sbattery_report.html", tempPath);
    m_htmlCache = ReadTextAutoEncoding(path);

    GetUsageHistory(m_rows, m_htmlCache);
    GetBatteryCapacity(m_capacity, m_htmlCache);
    // m_life parsing kept for completeness but not used in scoring
    GetBatteryLife(m_life, m_htmlCache);

    // Pre-compute score (F2/F3 removed; only Health, CPU drain, Dis drain, Cycles)
    m_score = ComputeBatteryScore(m_reportData, m_capacity, m_life);

    // The dialog template still has WS_HSCROLL/WS_VSCROLL set from the old
    // scrollable layout. We no longer drive those scrollbars (no SetScrollInfo
    // calls remain), so explicitly hide them rather than leaving a dead,
    // unusable scrollbar visible at the edge of the dialog.
    ShowScrollBar(SB_BOTH, FALSE);

    return TRUE;
}

void CReportDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);

    // The whole dashboard is laid out and scaled live from the client rect
    // inside OnPaint, so resizing the dialog just needs a repaint — there is
    // no separate scroll range to recompute any more.
    Invalidate();
}

//////////////////////////////////////////////////////////////
// 🔹 PAINT — Battery Condition Dashboard
//////////////////////////////////////////////////////////////

void CReportDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect clientRect;
    GetClientRect(&clientRect);

    // ── Background fill ──────────────────────────────────────────────────────
    CBrush bgBrush(RGB(245, 247, 250));
    dc.FillRect(&clientRect, &bgBrush);

    dc.SetBkMode(TRANSPARENT);

    const int CX = clientRect.Width();
    const int CY = clientRect.Height();

    // ── Responsive scale ──────────────────────────────────────────────────────
    // The dashboard was authored against this design canvas. Every margin,
    // font, bar and card below is expressed through S(), which scales that
    // baseline value to whatever size the dialog actually is — so the whole
    // report grows or shrinks to fit the client area with no scrollbars.
    const double DESIGN_W = 760.0;
    const double DESIGN_H = 640.0;   // actual content height: title + badge +
    // score bar + advice + 4 factor rows +
    // optional missing-data note + 2 rows
    // of key-metric cards
    double scale = min(CX / DESIGN_W, CY / DESIGN_H);
    scale = max(0.55, min(scale, 1.6));   // keep things legible at extremes

    auto S = [scale](int v) -> int { return (int)std::lround(v * scale); };

    const int LEFT = S(30);
    int y = S(24);

    // ── Fonts ────────────────────────────────────────────────────────────────
    HFONT hFontTitle = CreateFont(S(28), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT hFontGrade = CreateFont(S(52), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT hFontScore = CreateFont(S(38), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT hFontSub = CreateFont(S(17), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT hFontBody = CreateFont(S(15), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HFONT hFontSmall = CreateFont(S(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

    HDC hdc = dc.GetSafeHdc();
    const CReportDlg::ScoreResult& sc = m_score;

    // ── Title ────────────────────────────────────────────────────────────────
    SelectObject(hdc, hFontTitle);
    SetTextColor(hdc, RGB(30, 80, 160));
    TextOut(hdc, LEFT, y, L"Battery Condition Report", 24);
    y += S(40);

    // ── Grade badge ──────────────────────────────────────────────────────────
    // Draw a filled rounded rectangle badge centred at top
    {
        int bW = S(320), bH = S(70);
        int bX = (CX - bW) / 2;
        int bY = y;

        HBRUSH hBadge = CreateSolidBrush(sc.gradeColor);
        HPEN   hPen = CreatePen(PS_SOLID, 0, sc.gradeColor);
        HBRUSH pOldBr = (HBRUSH)SelectObject(hdc, hBadge);
        HPEN   pOldPn = (HPEN)SelectObject(hdc, hPen);
        RoundRect(hdc, bX, bY, bX + bW, bY + bH, S(16), S(16));
        SelectObject(hdc, pOldBr);
        SelectObject(hdc, pOldPn);
        DeleteObject(hBadge);
        DeleteObject(hPen);

        SelectObject(hdc, hFontGrade);
        SetTextColor(hdc, RGB(255, 255, 255));
        RECT rBadge = { bX, bY, bX + bW, bY + bH };
        DrawText(hdc, sc.grade, -1, &rBadge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        y += bH + S(14);
    }

    // ── Overall score arc / progress bar ─────────────────────────────────────
    // Draw a horizontal score bar (0-100) with gradient segments
    {
        int barX = LEFT;
        const int SCORE_LABEL_W = S(130);  // reserved width for "NN / 100" right of bar
        int barW = CX - LEFT * 2 - SCORE_LABEL_W - S(10);
        int barH = S(22);
        int barY = y;

        // Background track
        HBRUSH hTrack = CreateSolidBrush(RGB(210, 215, 225));
        HPEN hNoPen = CreatePen(PS_NULL, 0, 0);
        HPEN pOldPn = (HPEN)SelectObject(hdc, hNoPen);
        HBRUSH pOldBr = (HBRUSH)SelectObject(hdc, hTrack);
        RoundRect(hdc, barX, barY, barX + barW, barY + barH, barH, barH);
        SelectObject(hdc, pOldBr);
        DeleteObject(hTrack);

        // Filled portion
        int fillW = (int)(barW * sc.total / 100.0);
        if (fillW > barH) // only draw if wide enough for rounded rect
        {
            HBRUSH hFill = CreateSolidBrush(sc.gradeColor);
            SelectObject(hdc, hFill);
            RoundRect(hdc, barX, barY, barX + fillW, barY + barH, barH, barH);
            SelectObject(hdc, pOldBr);
            DeleteObject(hFill);
        }
        SelectObject(hdc, pOldPn);
        DeleteObject(hNoPen);

        // Score label on right — medium-bold font (S(24)) sits between body and the old oversized score font
        HFONT hFontScoreMed = CreateFont(S(24), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        SelectObject(hdc, hFontScoreMed);
        SetTextColor(hdc, sc.gradeColor);
        CString scoreStr;
        scoreStr.Format(L"%.0f / 100", sc.total);
        RECT rScore = { barX + barW + S(8), barY - S(6), CX - LEFT, barY + barH + S(6) };
        DrawText(hdc, scoreStr, -1, &rScore, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        DeleteObject(hFontScoreMed);

        y += barH + S(12);
    }

    // ── Advice text ──────────────────────────────────────────────────────────
    {
        SelectObject(hdc, hFontBody);
        SetTextColor(hdc, RGB(60, 60, 60));
        RECT rAdvice = { LEFT, y, CX - LEFT, y + S(50) };
        DrawText(hdc, sc.advice, -1, &rAdvice, DT_LEFT | DT_WORDBREAK);
        y += S(46);
    }

    // ── Divider ──────────────────────────────────────────────────────────────
    {
        HPEN hDiv = CreatePen(PS_SOLID, 1, RGB(200, 205, 215));
        HPEN pOld = (HPEN)SelectObject(hdc, hDiv);
        MoveToEx(hdc, LEFT, y, nullptr);
        LineTo(hdc, CX - LEFT, y);
        SelectObject(hdc, pOld);
        DeleteObject(hDiv);
        y += S(16);
    }

    // ── Factor breakdown header ───────────────────────────────────────────────
    SelectObject(hdc, hFontSub);
    SetTextColor(hdc, RGB(30, 80, 160));
    TextOut(hdc, LEFT, y, L"Score Breakdown", 15);
    y += S(30);

    // ── Helper: draw one factor bar ──────────────────────────────────────────
    // score == -1.0 → factor unavailable; draw a greyed "N/A" bar instead.
    auto DrawFactor = [&](const wchar_t* label, double score,
        const wchar_t* rawLabel, const CString& rawValStr,
        int baseWeight)
        {
            const int LabelW = S(210);
            const int BarX = LEFT + LabelW;
            // Reserve: S(42) for score%, S(10) gap, S(110) for raw-value, S(LEFT) right margin
            const int RIGHT_RESERVE = S(42) + S(10) + S(110);
            const int BarW = CX - BarX - LEFT - RIGHT_RESERVE;
            const int BarH = S(16);
            const int RowH = S(18);     // label/weight row line height
            const int WtZoneW = S(50);     // reserved width for the "(NN%)" badge

            bool unavailable = (score < 0.0);

            // Factor label — clipped to its own column (ellipsis if too long)
            // so it can never run into the weight badge or the bar.
            SelectObject(hdc, hFontBody);
            SetTextColor(hdc, unavailable ? RGB(160, 160, 170) : RGB(50, 50, 60));
            RECT rLabel = { LEFT, y, BarX - WtZoneW - S(8), y + RowH };
            DrawText(hdc, label, -1, &rLabel,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

            // Weight badge — right-aligned flush against the bar's left edge,
            // so "(20%)" always lines up cleanly regardless of label length.
            CString wt;
            wt.Format(L"(%d%%)", baseWeight);
            SelectObject(hdc, hFontSmall);
            SetTextColor(hdc, unavailable ? RGB(180, 180, 190) : RGB(120, 120, 135));
            RECT rWt = { BarX - WtZoneW, y, BarX - S(8), y + RowH };
            DrawText(hdc, wt, -1, &rWt, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            // Track (always drawn)
            HPEN hNoPen = CreatePen(PS_NULL, 0, 0);
            HPEN pOldPn = (HPEN)SelectObject(hdc, hNoPen);
            HBRUSH hTrack = CreateSolidBrush(unavailable ? RGB(230, 230, 235) : RGB(215, 218, 228));
            HBRUSH pOldBr = (HBRUSH)SelectObject(hdc, hTrack);
            RoundRect(hdc, BarX, y + S(2), BarX + BarW, y + S(2) + BarH, BarH, BarH);
            SelectObject(hdc, pOldBr);
            DeleteObject(hTrack);

            if (!unavailable)
            {
                // Filled bar — colour: green(>70) amber(40-70) red(<40)
                COLORREF fillCol = score >= 70.0 ? RGB(0, 160, 80)
                    : score >= 40.0 ? RGB(220, 150, 0)
                    : RGB(200, 40, 40);
                int fillW = (int)(BarW * score / 100.0);
                if (fillW >= BarH)
                {
                    HBRUSH hFill = CreateSolidBrush(fillCol);
                    SelectObject(hdc, hFill);
                    RoundRect(hdc, BarX, y + S(2), BarX + fillW, y + S(2) + BarH, BarH, BarH);
                    SelectObject(hdc, pOldBr);
                    DeleteObject(hFill);
                }
                SelectObject(hdc, pOldPn);
                DeleteObject(hNoPen);

                // Score % — drawn in the S(42) slot immediately right of the bar
                SelectObject(hdc, hFontSmall);
                SetTextColor(hdc, fillCol);
                CString pct;
                pct.Format(L"%.0f%%", score);
                RECT rPct = { BarX + BarW + S(6), y + S(3),
                              BarX + BarW + S(42), y + S(3) + BarH };
                DrawText(hdc, pct, -1, &rPct,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                // Raw value in brackets — clipped to right margin so it never overflows
                if (!rawValStr.IsEmpty())
                {
                    SetTextColor(hdc, RGB(100, 100, 110));
                    CString rv;
                    rv.Format(L"[%s: %s]", rawLabel, rawValStr.GetString());
                    // Place the raw label right after the score%, bounded by CX-LEFT
                    int rawX = BarX + BarW + S(42) + S(10);
                    RECT rRaw = { rawX, y + S(3), CX - LEFT, y + S(3) + BarH };
                    SelectObject(hdc, hFontSmall);
                    DrawText(hdc, rv, -1, &rRaw,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
                }
            }
            else
            {
                SelectObject(hdc, pOldPn);
                DeleteObject(hNoPen);

                // "Not available" text inside the greyed bar
                SelectObject(hdc, hFontSmall);
                SetTextColor(hdc, RGB(160, 160, 170));
                RECT rNA = { BarX + S(6), y + S(2), BarX + BarW, y + S(2) + BarH };
                DrawText(hdc, L"Not measured / unavailable", -1, &rNA, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }

            y += S(30);
        };

    // F1 – Battery Health Ratio (base weight 50%)
    {
        CString rv;
        if (sc.healthPct >= 0) rv.Format(L"%.1f%%", sc.healthPct);
        DrawFactor(L"Battery Health Ratio", sc.f1Health, L"health", rv, 50);
    }
    // F4 – CPU Load Drain Rate (base weight 20%)
    {
        CString rv;
        if (sc.cpuRatePerMin >= 0) rv.Format(L"%.2f%%/min", sc.cpuRatePerMin);
        DrawFactor(L"CPU Load Drain Rate", sc.f4Cpu, L"rate", rv, 20);
    }
    // F5 – Idle Discharge Drain Rate (base weight 20%)
    {
        CString rv;
        if (sc.disRatePerMin >= 0) rv.Format(L"%.2f%%/min", sc.disRatePerMin);
        DrawFactor(L"Idle Discharge Drain Rate", sc.f5Dis, L"rate", rv, 20);
    }
    // F6 – Charge Cycle Count (base weight 10%)
    {
        CString rv;
        if (sc.cycleCount >= 0) rv.Format(L"%d cycles", sc.cycleCount);
        DrawFactor(L"Charge Cycle Count", sc.f6Cycles, L"cycles", rv, 10);
    }

    // ── Missing-data notice ───────────────────────────────────────────────────
    if (!sc.missingNote.IsEmpty())
    {
        SelectObject(hdc, hFontSmall);
        SetTextColor(hdc, RGB(150, 100, 0));
        CString note = L"\u26a0  " + sc.missingNote + L"  \u2014  score computed from available factors only.";
        RECT rNote = { LEFT, y, CX - LEFT, y + S(36) };
        DrawText(hdc, note, -1, &rNote, DT_LEFT | DT_WORDBREAK);
        y += S(34);
    }

    y += S(10);

    // ── Divider ──────────────────────────────────────────────────────────────
    {
        HPEN hDiv = CreatePen(PS_SOLID, 1, RGB(200, 205, 215));
        HPEN pOld = (HPEN)SelectObject(hdc, hDiv);
        MoveToEx(hdc, LEFT, y, nullptr);
        LineTo(hdc, CX - LEFT, y);
        SelectObject(hdc, pOld);
        DeleteObject(hDiv);
        y += S(16);
    }

    // ── Key stats summary row ─────────────────────────────────────────────────
    SelectObject(hdc, hFontSub);
    SetTextColor(hdc, RGB(30, 80, 160));
    TextOut(hdc, LEFT, y, L"Key Metrics", 11);
    y += S(28);

    auto DrawStatCard = [&](int cx, int cy, int cw, int ch,
        const wchar_t* title, const CString& value, COLORREF valCol)
        {
            // Card background
            HBRUSH hCard = CreateSolidBrush(RGB(255, 255, 255));
            HPEN   hPen = CreatePen(PS_SOLID, 1, RGB(210, 215, 225));
            HBRUSH pOldBr = (HBRUSH)SelectObject(hdc, hCard);
            HPEN   pOldPn = (HPEN)SelectObject(hdc, hPen);
            RoundRect(hdc, cx, cy, cx + cw, cy + ch, S(10), S(10));
            SelectObject(hdc, pOldBr);
            SelectObject(hdc, pOldPn);
            DeleteObject(hCard);
            DeleteObject(hPen);

            // Title
            SelectObject(hdc, hFontSmall);
            SetTextColor(hdc, RGB(100, 100, 115));
            RECT rt = { cx + S(6), cy + S(6), cx + cw - S(4), cy + ch / 2 };
            DrawText(hdc, title, -1, &rt, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            // Value
            SelectObject(hdc, hFontSub);
            SetTextColor(hdc, valCol);
            RECT rv = { cx + S(4), cy + ch / 2, cx + cw - S(4), cy + ch - S(4) };
            DrawText(hdc, value, -1, &rv, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        };

    {
        // 6 cards, 3 per row. (Temperature and Time Remaining were removed.)
        int cardH = S(62);
        int gap = S(10);
        int nCards = 3;
        int cardW = (CX - LEFT * 2 - gap * (nCards - 1)) / nCards;
        int cx = LEFT;

        // Card 1: Health %
        {
            CString v;
            if (sc.healthPct >= 0) v.Format(L"%.1f%%", sc.healthPct);
            else v = m_reportData.health.IsEmpty() ? L"N/A" : m_reportData.health;
            COLORREF col = sc.healthPct >= 80 ? RGB(0, 160, 80)
                : sc.healthPct >= 60 ? RGB(220, 150, 0)
                : RGB(200, 40, 40);
            DrawStatCard(cx, y, cardW, cardH, L"Battery Health", v, col);
            cx += cardW + gap;
        }
        // Card 2: Cycles
        {
            DrawStatCard(cx, y, cardW, cardH, L"Charge Cycles",
                m_reportData.cycles.IsEmpty() ? L"N/A" : m_reportData.cycles,
                RGB(50, 100, 180));
            cx += cardW + gap;
        }
        // Card 3: Current %
        {
            DrawStatCard(cx, y, cardW, cardH, L"Current Charge",
                m_reportData.percentage.IsEmpty() ? L"N/A" : m_reportData.percentage,
                RGB(50, 130, 200));
        }

        y += cardH + gap;

        // Row 2 of cards
        cx = LEFT;
        // Card 4: Voltage
        {
            DrawStatCard(cx, y, cardW, cardH, L"Voltage",
                m_reportData.voltage.IsEmpty() ? L"N/A" : m_reportData.voltage,
                RGB(80, 80, 160));
            cx += cardW + gap;
        }
        // Card 5: Full Charge Cap
        {
            DrawStatCard(cx, y, cardW, cardH, L"Full Charge Cap",
                m_reportData.fullChargeCapacity.IsEmpty() ? L"N/A" : m_reportData.fullChargeCapacity,
                RGB(0, 130, 80));
            cx += cardW + gap;
        }
        // Card 6: Design Capacity
        {
            DrawStatCard(cx, y, cardW, cardH, L"Design Capacity",
                m_reportData.designCapacity.IsEmpty() ? L"N/A" : m_reportData.designCapacity,
                RGB(100, 100, 120));
        }

        y += cardH + S(20);
    }

    // ── Export button (fixed top-right corner of the client area) ────────────
    {
        const int BW = S(130), BH = S(28), BM = S(8);
        m_exportBtnRect = CRect(
            clientRect.right - BW - BM, BM,
            clientRect.right - BM, BM + BH);

        CBrush bgErase(::GetSysColor(COLOR_BTNFACE));
        dc.FillRect(&m_exportBtnRect, &bgErase);

        HBRUSH hBtn = CreateSolidBrush(RGB(0, 120, 215));
        HPEN   hPen = CreatePen(PS_SOLID, 1, RGB(0, 90, 180));
        HBRUSH pOldBr = (HBRUSH)SelectObject(hdc, hBtn);
        HPEN   pOldPn = (HPEN)SelectObject(hdc, hPen);
        RECT   rBtn = m_exportBtnRect;
        RoundRect(hdc, rBtn.left, rBtn.top, rBtn.right, rBtn.bottom, S(6), S(6));
        SelectObject(hdc, pOldBr);
        SelectObject(hdc, pOldPn);
        DeleteObject(hBtn);
        DeleteObject(hPen);

        HFONT hBtnFont = CreateFont(S(14), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        HFONT pOldFnt = (HFONT)SelectObject(hdc, hBtnFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        DrawText(hdc, L"Export Report", -1, &rBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        SelectObject(hdc, pOldFnt);
        DeleteObject(hBtnFont);
    }

    // ── Cleanup fonts ────────────────────────────────────────────────────────
    DeleteObject(hFontTitle);
    DeleteObject(hFontGrade);
    DeleteObject(hFontScore);
    DeleteObject(hFontSub);
    DeleteObject(hFontBody);
    DeleteObject(hFontSmall);
}

//////////////////////////////////////////////////////////////
// 🔹 Painted-button click handler
//////////////////////////////////////////////////////////////

void CReportDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
    if (m_exportBtnRect.PtInRect(point))
    {
        int choice = AfxMessageBox(
            L"Choose export format:\n\nYes  \u2192 HTML\nNo   \u2192 PDF",
            MB_YESNOCANCEL | MB_ICONQUESTION);

        if (choice == IDYES)  ExportHtmlReport();
        else if (choice == IDNO)
        {
            CString pdfPath;
            if (ExportPrintToPdf(pdfPath))
            {
                CString msg;
                msg.Format(L"PDF saved:\n%s", pdfPath.GetString());
                AfxMessageBox(msg, MB_ICONINFORMATION);
            }
        }
    }
    CDialogEx::OnLButtonUp(nFlags, point);
}

//////////////////////////////////////////////////////////////
// 🔹 BuildHtmlReport  –  assembles a self-contained HTML string
//////////////////////////////////////////////////////////////

CString CReportDlg::BuildHtmlReport() const
{
    const CReportDlg::ScoreResult& sc = m_score;

    CString html;
    html =
        L"<!DOCTYPE html>\n<html lang='en'>\n<head>\n"
        L"<meta charset='UTF-8'/>\n"
        L"<title>Battery Condition Report</title>\n"
        L"<style>\n"
        L"body{font-family:Segoe UI,Arial,sans-serif;font-size:13px;margin:30px;color:#222;background:#f5f7fa;}\n"
        L"h1{color:#1e50a0;}\n"
        L"h2{color:#1e50a0;border-bottom:2px solid #1e50a0;padding-bottom:4px;margin-top:28px;}\n"
        L".badge{display:inline-block;padding:12px 40px;border-radius:10px;color:#fff;font-size:2em;font-weight:bold;margin:10px 0;}\n"
        L".bar-track{background:#d2d7e1;border-radius:12px;height:22px;width:100%;margin:8px 0;}\n"
        L".bar-fill{height:22px;border-radius:12px;}\n"
        L"table{border-collapse:collapse;width:100%;margin-bottom:20px;}\n"
        L"th{background:#1e50a0;color:#fff;padding:6px 10px;text-align:left;}\n"
        L"td{padding:5px 10px;border-bottom:1px solid #ddd;}\n"
        L"tr:nth-child(even) td{background:#eef2fb;}\n"
        L".cards{display:flex;gap:12px;flex-wrap:wrap;margin:16px 0;}\n"
        L".card{background:#fff;border:1px solid #d0d5e0;border-radius:8px;padding:12px 18px;min-width:150px;flex:1;}\n"
        L".card-title{font-size:11px;color:#888;}\n"
        L".card-value{font-size:18px;font-weight:bold;}\n"
        L"</style>\n</head>\n<body>\n"
        L"<h1>Battery Condition Report</h1>\n";

    // Grade badge
    CString color;
    color.Format(L"#%02X%02X%02X",
        GetRValue(sc.gradeColor), GetGValue(sc.gradeColor), GetBValue(sc.gradeColor));
    html += L"<div class='badge' style='background:" + color + L";'>" + sc.grade + L"</div>\n";

    // Score bar
    CString scoreHtml;
    scoreHtml.Format(
        L"<p><strong>Overall Score: %.0f / 100</strong></p>\n"
        L"<div class='bar-track'><div class='bar-fill' style='width:%.0f%%;background:%s;'></div></div>\n"
        L"<p>%s</p>\n",
        sc.total, sc.total, color.GetString(), sc.advice.GetString());
    html += scoreHtml;

    // Factor table
    html += L"<h2>Score Breakdown</h2>\n<table>\n"
        L"<tr><th>Factor</th><th>Base Weight</th><th>Sub-Score</th><th>Raw Value</th></tr>\n";

    // Helper: one table row; score < 0 means unavailable
    auto FactorRow = [](const wchar_t* name, int wt, double score, const CString& raw) -> CString
        {
            CString r;
            if (score >= 0.0)
                r.Format(L"<tr><td>%s</td><td>%d%%</td><td>%.0f%%</td><td>%s</td></tr>\n",
                    name, wt, score, raw.GetString());
            else
                r.Format(L"<tr style='color:#aaa'><td>%s</td><td>%d%%</td>"
                    L"<td><em>N/A</em></td><td><em>Not measured / unavailable</em></td></tr>\n",
                    name, wt);
            return r;
        };

    CString rv;
    if (sc.healthPct >= 0) rv.Format(L"%.1f%%", sc.healthPct); else rv = L"";
    html += FactorRow(L"Battery Health Ratio", 50, sc.f1Health, rv);
    if (sc.cpuRatePerMin >= 0) rv.Format(L"%.2f%%/min", sc.cpuRatePerMin); else rv = L"";
    html += FactorRow(L"CPU Load Drain Rate", 20, sc.f4Cpu, rv);
    if (sc.disRatePerMin >= 0) rv.Format(L"%.2f%%/min", sc.disRatePerMin); else rv = L"";
    html += FactorRow(L"Idle Discharge Drain Rate", 20, sc.f5Dis, rv);
    if (sc.cycleCount >= 0) rv.Format(L"%d cycles", sc.cycleCount); else rv = L"";
    html += FactorRow(L"Charge Cycle Count", 10, sc.f6Cycles, rv);
    html += L"</table>\n";

    if (!sc.missingNote.IsEmpty())
    {
        html += L"<p style='color:#a06000;font-size:12px'>&#x26A0; ";
        html += sc.missingNote;
        html += L" &mdash; score computed from available factors only.</p>\n";
    }

    // Key metrics cards
    html += L"<h2>Key Metrics</h2>\n<div class='cards'>\n";
    auto Card = [](const wchar_t* title, const CString& val) -> CString {
        CString c;
        c.Format(L"<div class='card'><div class='card-title'>%s</div>"
            L"<div class='card-value'>%s</div></div>\n", title, val.GetString());
        return c;
        };
    html += Card(L"Battery Health", sc.healthPct >= 0 ? ([&] { CString t; t.Format(L"%.1f%%", sc.healthPct); return t; }()) : m_reportData.health);
    html += Card(L"Charge Cycles", m_reportData.cycles);
    html += Card(L"Current Charge", m_reportData.percentage);
    html += Card(L"Voltage", m_reportData.voltage);
    html += Card(L"Full Charge Cap", m_reportData.fullChargeCapacity);
    html += Card(L"Design Capacity", m_reportData.designCapacity);
    html += L"</div>\n";

    html += L"</body>\n</html>\n";
    return html;
}

void CReportDlg::ExportHtmlReport(const CString& destPath) const
{
    CString path = destPath;

    if (path.IsEmpty())
    {
        wchar_t szFile[MAX_PATH] = L"battery_condition_report.html";
        wchar_t filter[] = L"HTML Files (*.html)\0*.html\0All Files (*.*)\0*.*\0\0";
        OPENFILENAME ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetSafeHwnd();
        ofn.lpstrFilter = filter;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
        ofn.lpstrDefExt = L"html";
        if (!GetSaveFileName(&ofn)) return;
        path = szFile;
    }

    CString html = BuildHtmlReport();
    CStdioFile f;
    if (!f.Open(path, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
    {
        AfxMessageBox(L"Failed to open file for writing.", MB_ICONERROR);
        return;
    }
    // Write UTF-8 BOM + content
    const BYTE bom[] = { 0xEF, 0xBB, 0xBF };
    f.Write(bom, 3);

    CStringA utf8;
    int wlen = WideCharToMultiByte(CP_UTF8, 0, html.GetString(), -1, nullptr, 0, nullptr, nullptr);
    WideCharToMultiByte(CP_UTF8, 0, html.GetString(), -1, utf8.GetBuffer(wlen), wlen, nullptr, nullptr);
    utf8.ReleaseBuffer(wlen - 1);
    f.Write(utf8.GetString(), utf8.GetLength());
    f.Close();

    ShellExecute(nullptr, L"open", path.GetString(), nullptr, nullptr, SW_SHOWNORMAL);
}

// ── FindPrinterByKeywords helper (unchanged from original) ─────────────────
static CString FindPrinterByKeywords(std::initializer_list<const wchar_t*> keywords)
{
    DWORD needed = 0, count = 0;
    EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
        nullptr, 2, nullptr, 0, &needed, &count);
    if (needed == 0) return L"";

    std::vector<BYTE> buf(needed);
    if (!EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
        nullptr, 2, buf.data(), needed, &needed, &count))
        return L"";

    auto* info = reinterpret_cast<PRINTER_INFO_2W*>(buf.data());
    for (DWORD i = 0; i < count; ++i)
    {
        CString name = info[i].pPrinterName;
        name.MakeLower();
        bool match = true;
        for (auto* kw : keywords)
        {
            CString k = kw; k.MakeLower();
            if (name.Find(k) < 0) { match = false; break; }
        }
        if (match) return info[i].pPrinterName;
    }
    return L"";
}

bool CReportDlg::ExportPrintToPdf(CString& outPdfPath) const
{
    wchar_t szFile[MAX_PATH] = L"battery_condition_report.pdf";
    wchar_t filter[] = L"PDF Files (*.pdf)\0*.pdf\0All Files (*.*)\0*.*\0\0";

    OPENFILENAME ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetSafeHwnd();
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = L"pdf";
    if (!GetSaveFileName(&ofn)) return false;
    outPdfPath = szFile;

    CString printerName = FindPrinterByKeywords({ L"Microsoft", L"PDF" });
    if (printerName.IsEmpty()) printerName = FindPrinterByKeywords({ L"PDF" });
    if (printerName.IsEmpty())
    {
        AfxMessageBox(L"'Microsoft Print to PDF' was not found on this PC.", MB_ICONWARNING);
        return false;
    }

    HANDLE hPrinter = nullptr;
    OpenPrinterW(const_cast<LPWSTR>(printerName.GetString()), &hPrinter, nullptr);
    std::vector<BYTE> dmBuf;
    DEVMODEW* pDM = nullptr;
    if (hPrinter)
    {
        LONG dmSz = DocumentPropertiesW(GetSafeHwnd(), hPrinter,
            const_cast<LPWSTR>(printerName.GetString()), nullptr, nullptr, 0);
        if (dmSz > 0)
        {
            dmBuf.resize(dmSz);
            pDM = reinterpret_cast<DEVMODEW*>(dmBuf.data());
            if (DocumentPropertiesW(GetSafeHwnd(), hPrinter,
                const_cast<LPWSTR>(printerName.GetString()),
                pDM, nullptr, DM_OUT_BUFFER) < 0) pDM = nullptr;
        }
        ClosePrinter(hPrinter);
    }

    HDC hDC = CreateDCW(nullptr, printerName.GetString(), nullptr, pDM);
    if (!hDC) { AfxMessageBox(L"Could not create DC for PDF printer.", MB_ICONERROR); return false; }

    DOCINFOW di = {};
    di.cbSize = sizeof(di);
    di.lpszDocName = L"Battery Condition Report";
    di.lpszOutput = outPdfPath.GetString();
    if (StartDocW(hDC, &di) <= 0)
    {
        DeleteDC(hDC);
        AfxMessageBox(L"StartDoc failed.", MB_ICONERROR);
        return false;
    }

    StartPage(hDC);
    int pageW = GetDeviceCaps(hDC, HORZRES);
    int pageH = GetDeviceCaps(hDC, VERTRES);
    int dpiX = GetDeviceCaps(hDC, LOGPIXELSX);
    int dpiY = GetDeviceCaps(hDC, LOGPIXELSY);
    int mX = (dpiX * 3) / 4;
    int mY = (dpiY * 3) / 4;
    RenderReportToDC(hDC, pageW, pageH, mX, mY, (float)dpiX / 96.f, (float)dpiY / 96.f);
    EndPage(hDC);
    EndDoc(hDC);
    DeleteDC(hDC);
    return true;
}

void CReportDlg::RenderReportToDC(HDC hDC, int pageW, int pageH,
    int marginX, int marginY, float scaleX, float scaleY) const
{
    // For PDF: render the HTML via ShellExecute print or just draw text summary.
    // Here we do a simple text dump of the condition score to the DC.
    const CReportDlg::ScoreResult& sc = m_score;

    auto ScaledFont = [&](int ptSize, bool bold) -> HFONT {
        return CreateFont(-(int)(ptSize * scaleY), 0, 0, 0,
            bold ? FW_BOLD : FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        };

    HFONT hTitle = ScaledFont(22, true);
    HFONT hH2 = ScaledFont(14, true);
    HFONT hBody = ScaledFont(11, false);

    int x = marginX, y = marginY;
    int lineH = (int)(20 * scaleY);

    SetBkMode(hDC, TRANSPARENT);

    // Title
    SelectObject(hDC, hTitle);
    SetTextColor(hDC, RGB(30, 80, 160));
    TextOut(hDC, x, y, L"Battery Condition Report", 24);
    y += lineH * 2;

    // Grade
    SelectObject(hDC, hH2);
    SetTextColor(hDC, sc.gradeColor);
    CString gradeStr;
    gradeStr.Format(L"Grade: %s   (Score: %.0f / 100)", sc.grade.GetString(), sc.total);
    TextOut(hDC, x, y, gradeStr, gradeStr.GetLength());
    y += lineH * 2;

    // Advice
    SelectObject(hDC, hBody);
    SetTextColor(hDC, RGB(60, 60, 60));
    TextOut(hDC, x, y, sc.advice, sc.advice.GetLength());
    y += lineH * 2;

    // Factors
    SelectObject(hDC, hH2);
    SetTextColor(hDC, RGB(30, 80, 160));
    TextOut(hDC, x, y, L"Score Breakdown:", 16);
    y += lineH;

    SelectObject(hDC, hBody);
    SetTextColor(hDC, RGB(40, 40, 40));

    struct { const wchar_t* name; double score; } factors[] = {
        { L"Battery Health Ratio      (50%)", sc.f1Health  },
        { L"CPU Load Drain Rate        (20%)", sc.f4Cpu    },
        { L"Idle Discharge Drain Rate  (20%)", sc.f5Dis    },
        { L"Charge Cycle Count         (10%)", sc.f6Cycles },
    };
    for (auto& f : factors)
    {
        CString row;
        if (f.score >= 0.0)
            row.Format(L"  %-44s  %.0f%%", f.name, f.score);
        else
            row.Format(L"  %-44s  N/A (not measured)", f.name);
        TextOut(hDC, x, y, row, row.GetLength());
        y += lineH;
    }
    if (!sc.missingNote.IsEmpty())
    {
        y += lineH / 2;
        SetTextColor(hDC, RGB(160, 100, 0));
        CString note = L"  Note: " + sc.missingNote + L" - score from available factors only.";
        TextOut(hDC, x, y, note, note.GetLength());
    }

    DeleteObject(hTitle);
    DeleteObject(hH2);
    DeleteObject(hBody);
}