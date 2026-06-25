#include "pch.h"
#include "CReportDlg.h"
#include "afxdialogex.h"
#include <afxcmn.h>
#include <regex>
#include <vector>
#include <utility>
#include <cmath>
#include <shellapi.h>
#include "BatteryHelthDlg.h"
#pragma comment(lib, "shell32.lib")

IMPLEMENT_DYNAMIC(CReportDlg, CDialogEx)

// ── Language helper ──────────────────────────────────────────────────────────
// Returns JP string when parent dialog is in JP mode, else EN.
static bool IsJP(CBatteryHelthDlg* p)
{
    return p && p->m_lang == CBatteryHelthDlg::Lang::JP;
}

// Convenience macro: picks EN or JP string based on parent dialog language.
#define LS(dlg, en, jp)  (IsJP(dlg) ? CString(jp) : CString(en))

// ────────────────────────────────────────────────────────────────────────────

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
// 🔹 BATTERY CONDITION SCORING ALGORITHM  (v2 — 4 factors)
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

static double Clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

static CReportDlg::ScoreResult ComputeBatteryScore(
    const BatteryReportData& rd,
    const std::vector<CReportDlg::BatteryCapacityRow>& capacity,
    const std::vector<CReportDlg::BatteryLifeRow>&     /*life — not used*/)
{
    CReportDlg::ScoreResult s = {};
    CString missing;

    // ── F1: Health ratio (base weight 50) ───────────────────────────────────
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
            double score = Clamp01((healthRatio - 0.50) / 0.50) * 100.0;
            s.f1Health = score;
        }
        else
        {
            s.healthPct = -1.0;
            s.f1Health = -1.0;
            if (!missing.IsEmpty()) missing += L"|";
            missing += L"HEALTH";   // key — localised later
        }
    }

    // ── F4: CPU Load Drain Rate (base weight 20) ─────────────────────────────
    {
        s.cpuRatePerMin = rd.cpuRate;
        if (rd.cpuRate > 0.0)
        {
            double score = Clamp01(1.0 - (rd.cpuRate - 0.5) / 2.0) * 100.0;
            s.f4Cpu = score;
        }
        else
        {
            s.cpuRatePerMin = -1.0;
            s.f4Cpu = -1.0;
            if (!missing.IsEmpty()) missing += L"|";
            missing += L"CPU";      // key — localised later
        }
    }

    // ── F5: Idle Discharge Drain Rate (base weight 20) ───────────────────────
    {
        s.disRatePerMin = rd.disRate;
        if (rd.disRate > 0.0)
        {
            double score = Clamp01(1.0 - (rd.disRate - 0.1) / 1.4) * 100.0;
            s.f5Dis = score;
        }
        else
        {
            s.disRatePerMin = -1.0;
            s.f5Dis = -1.0;
            if (!missing.IsEmpty()) missing += L"|";
            missing += L"DISCHARGE"; // key — localised later
        }
    }

    // ── F6: Charge Cycle Count (base weight 10) ──────────────────────────────
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
            else if (cycles <= 500)  score = 100.0 - (cycles - 300) / 200.0 * 30.0;
            else if (cycles <= 800)  score = 70.0 - (cycles - 500) / 300.0 * 30.0;
            else if (cycles <= 1000) score = 40.0 - (cycles - 800) / 200.0 * 30.0;
            else                     score = max(0.0, 10.0 - (cycles - 1000) / 100.0 * 5.0);
            s.f6Cycles = Clamp01(score / 100.0) * 100.0;
        }
        else
        {
            s.f6Cycles = -1.0;
            if (!missing.IsEmpty()) missing += L"|";
            missing += L"CYCLES";   // key — localised later
        }
    }

    // ── Weighted sum with dynamic weight redistribution ───────────────────────
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
        if (f.subScore >= 0.0)
        {
            weightedSum += f.subScore * f.baseWeight;
            totalWeight += f.baseWeight;
        }
    }

    s.total = (totalWeight > 0.0) ? (weightedSum / totalWeight) : 0.0;

    // ── Missing-data note stored as raw pipe-delimited keys ──────────────────
    // Localised into human-readable text in OnInitDialog after lang is known.
    s.missingNote = missing;  // e.g. L"HEALTH|CPU|DISCHARGE|CYCLES"

    // ── Grade & advice (EN defaults; localised in OnInitDialog after lang is known) ──
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

    m_pBattDlg = dynamic_cast<CBatteryHelthDlg*>(GetParent());

    if (IsJP(m_pBattDlg))
        SetWindowText(L"結果");
    else
        SetWindowText(L"Result");

    wchar_t tempPath[MAX_PATH];
    GetTempPath(MAX_PATH, tempPath);

    SetIcon(nullptr, TRUE);
    SetIcon(nullptr, FALSE);

    CString path;
    path.Format(L"%sbattery_report.html", tempPath);
    m_htmlCache = ReadTextAutoEncoding(path);

    GetUsageHistory(m_rows, m_htmlCache);
    GetBatteryCapacity(m_capacity, m_htmlCache);
    GetBatteryLife(m_life, m_htmlCache);

    m_score = ComputeBatteryScore(m_reportData, m_capacity, m_life);

    // ── Localise advice ───────────────────────────────────────────────────────
  /*  bool jp = IsJP(m_pBattDlg);
    if (jp)
    {
        if (m_score.total >= 85.0)
            m_score.advice = L"バッテリーは最良の状態です。何もする必要はありません。";
        else if (m_score.total >= 70.0)
            m_score.advice = L"バッテリーは良好です。サイクル数を定期的に確認してください。";
        else if (m_score.total >= 50.0)
            m_score.advice = L"劣化が見られます。深放電と高温を避けてください。";
        else if (m_score.total >= 30.0)
            m_score.advice = L"バッテリーが著しく劣化しています。近いうちに交換を検討してください。";
        else
            m_score.advice = L"バッテリーが危機的状態です。突然のシャットダウンを防ぐため、直ちに交換してください。";
    }*/

    // REPLACE WITH:
    // ── Localise grade + advice ───────────────────────────────────────────────
    bool jp = IsJP(m_pBattDlg);
    if (jp)
    {
        if (m_score.total >= 85.0)
        {
            m_score.grade = L"優良";
            m_score.advice = L"バッテリーは最良の状態です。何もする必要はありません。";
        }
        else if (m_score.total >= 70.0)
        {
            m_score.grade = L"良好";
            m_score.advice = L"バッテリーは良好です。サイクル数を定期的に確認してください。";
        }
        else if (m_score.total >= 50.0)
        {
            m_score.grade = L"普通";
            m_score.advice = L"劣化が見られます。深放電と高温を避けてください。";
        }
        else if (m_score.total >= 30.0)
        {
            m_score.grade = L"不良";
            m_score.advice = L"バッテリーが著しく劣化しています。近いうちに交換を検討してください。";
        }
        else
        {
            m_score.grade = L"要交換";
            m_score.advice = L"バッテリーが危機的状態です。突然のシャットダウンを防ぐため、直ちに交換してください。";
        }
    }
    else
    {
        // EN grade strings (already set by ComputeBatteryScore, kept explicit for clarity)
        if (m_score.total >= 85.0) m_score.grade = L"EXCELLENT";
        else if (m_score.total >= 70.0) m_score.grade = L"GOOD";
        else if (m_score.total >= 50.0) m_score.grade = L"FAIR";
        else if (m_score.total >= 30.0) m_score.grade = L"POOR";
        else                            m_score.grade = L"REPLACE NOW";
    }

    // ── Localise missingNote (pipe-delimited keys → human text) ──────────────
    if (!m_score.missingNote.IsEmpty())
    {
        // Translate each key
        auto TranslateKey = [&](const CString& key) -> CString {
            if (key == L"HEALTH")
                return jp ? L"健全性比率" : L"Health ratio";
            if (key == L"CPU")
                return jp ? L"CPU消耗率" : L"CPU drain rate";
            if (key == L"DISCHARGE")
                return jp ? L"放電消耗率" : L"Discharge rate";
            if (key == L"CYCLES")
                return jp ? L"サイクル数" : L"Cycle count";
            return key; // fallback: show raw key
            };

        CString keys = m_score.missingNote;
        CString localised;
        int pos = 0;
        CString token;
        while (pos < keys.GetLength())
        {
            int pipe = keys.Find(L'|', pos);
            if (pipe < 0)
            {
                token = keys.Mid(pos);
                pos = keys.GetLength();
            }
            else
            {
                token = keys.Mid(pos, pipe - pos);
                pos = pipe + 1;
            }
            token.Trim();
            if (!token.IsEmpty())
            {
                if (!localised.IsEmpty()) localised += jp ? L"、" : L", ";
                localised += TranslateKey(token);
            }
        }

        CString prefix = jp
            ? L"未計測 / データなし: "
            : L"Not measured / unavailable: ";
        m_score.missingNote = prefix + localised;
    }

    ShowScrollBar(SB_BOTH, FALSE);

    return TRUE;
}

void CReportDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
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

    const double DESIGN_W = 760.0;
    const double DESIGN_H = 640.0;
    double scale = min(CX / DESIGN_W, CY / DESIGN_H);
    scale = max(0.55, min(scale, 1.6));

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
    {
        CString title = LS(m_pBattDlg, L"Battery Condition Report", L"バッテリー状態レポート");
        TextOut(hdc, LEFT, y, title, title.GetLength());
    }
    y += S(40);

    // ── Grade badge ──────────────────────────────────────────────────────────
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

    // ── Overall score bar ─────────────────────────────────────────────────────
    {
        int barX = LEFT;
        const int SCORE_LABEL_W = S(130);
        int barW = CX - LEFT * 2 - SCORE_LABEL_W - S(10);
        int barH = S(22);
        int barY = y;

        HBRUSH hTrack = CreateSolidBrush(RGB(210, 215, 225));
        HPEN hNoPen = CreatePen(PS_NULL, 0, 0);
        HPEN pOldPn = (HPEN)SelectObject(hdc, hNoPen);
        HBRUSH pOldBr = (HBRUSH)SelectObject(hdc, hTrack);
        RoundRect(hdc, barX, barY, barX + barW, barY + barH, barH, barH);
        SelectObject(hdc, pOldBr);
        DeleteObject(hTrack);

        int fillW = (int)(barW * sc.total / 100.0);
        if (fillW > barH)
        {
            HBRUSH hFill = CreateSolidBrush(sc.gradeColor);
            SelectObject(hdc, hFill);
            RoundRect(hdc, barX, barY, barX + fillW, barY + barH, barH, barH);
            SelectObject(hdc, pOldBr);
            DeleteObject(hFill);
        }
        SelectObject(hdc, pOldPn);
        DeleteObject(hNoPen);

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
    {
        CString sbLabel = LS(m_pBattDlg, L"Score Breakdown", L"スコア内訳");
        TextOut(hdc, LEFT, y, sbLabel, sbLabel.GetLength());
    }
    y += S(30);

    // ── Helper: draw one factor bar ──────────────────────────────────────────
    auto DrawFactor = [&](const CString& label, double score,
        const CString& rawLabel, const CString& rawValStr,
        int baseWeight)
        {
            const int LabelW = S(210);
            const int BarX = LEFT + LabelW;
            const int RIGHT_RESERVE = S(42) + S(10) + S(110);
            const int BarW = CX - BarX - LEFT - RIGHT_RESERVE;
            const int BarH = S(16);
            const int RowH = S(18);
            const int WtZoneW = S(50);

            bool unavailable = (score < 0.0);

            SelectObject(hdc, hFontBody);
            SetTextColor(hdc, unavailable ? RGB(160, 160, 170) : RGB(50, 50, 60));
            RECT rLabel = { LEFT, y, BarX - WtZoneW - S(8), y + RowH };
            DrawText(hdc, label, -1, &rLabel,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);

            CString wt;
            wt.Format(L"(%d%%)", baseWeight);
            SelectObject(hdc, hFontSmall);
            SetTextColor(hdc, unavailable ? RGB(180, 180, 190) : RGB(120, 120, 135));
            RECT rWt = { BarX - WtZoneW, y, BarX - S(8), y + RowH };
            DrawText(hdc, wt, -1, &rWt, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

            HPEN hNoPen = CreatePen(PS_NULL, 0, 0);
            HPEN pOldPn = (HPEN)SelectObject(hdc, hNoPen);
            HBRUSH hTrack = CreateSolidBrush(unavailable ? RGB(230, 230, 235) : RGB(215, 218, 228));
            HBRUSH pOldBr = (HBRUSH)SelectObject(hdc, hTrack);
            RoundRect(hdc, BarX, y + S(2), BarX + BarW, y + S(2) + BarH, BarH, BarH);
            SelectObject(hdc, pOldBr);
            DeleteObject(hTrack);

            if (!unavailable)
            {
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

                SelectObject(hdc, hFontSmall);
                SetTextColor(hdc, fillCol);
                CString pct;
                pct.Format(L"%.0f%%", score);
                RECT rPct = { BarX + BarW + S(6), y + S(3),
                              BarX + BarW + S(42), y + S(3) + BarH };
                DrawText(hdc, pct, -1, &rPct,
                    DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                if (!rawValStr.IsEmpty())
                {
                    SetTextColor(hdc, RGB(100, 100, 110));
                    CString rv;
                    rv.Format(L"[%s: %s]", rawLabel.GetString(), rawValStr.GetString());
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

                SelectObject(hdc, hFontSmall);
                SetTextColor(hdc, RGB(160, 160, 170));
                CString naStr = LS(m_pBattDlg, L"Not measured / unavailable", L"未計測 / データなし");
                RECT rNA = { BarX + S(6), y + S(2), BarX + BarW, y + S(2) + BarH };
                DrawText(hdc, naStr, -1, &rNA, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            }

            y += S(30);
        };

    // F1 – Battery Health Ratio (base weight 50%)
    {
        CString rv;
        if (sc.healthPct >= 0) rv.Format(L"%.1f%%", sc.healthPct);
        DrawFactor(
            LS(m_pBattDlg, L"Battery Health Ratio", L"バッテリー健全性比率"),
            sc.f1Health,
            LS(m_pBattDlg, L"health", L"健全性"),
            rv, 50);
    }
    // F4 – CPU Load Drain Rate (base weight 20%)
    {
        CString rv;
        if (sc.cpuRatePerMin >= 0) rv.Format(L"%.2f%%/min", sc.cpuRatePerMin);
        DrawFactor(
            LS(m_pBattDlg, L"CPU Load Drain Rate", L"CPU負荷時消耗率"),
            sc.f4Cpu,
            LS(m_pBattDlg, L"rate", L"消耗率"),
            rv, 20);
    }
    // F5 – Idle Discharge Drain Rate (base weight 20%)
    {
        CString rv;
        if (sc.disRatePerMin >= 0) rv.Format(L"%.2f%%/min", sc.disRatePerMin);
        DrawFactor(
            LS(m_pBattDlg, L"Idle Discharge Drain Rate", L"アイドル放電消耗率"),
            sc.f5Dis,
            LS(m_pBattDlg, L"rate", L"消耗率"),
            rv, 20);
    }
    // F6 – Charge Cycle Count (base weight 10%)
    {
        CString rv;
        if (sc.cycleCount >= 0) rv.Format(L"%d cycles", sc.cycleCount);
        DrawFactor(
            LS(m_pBattDlg, L"Charge Cycle Count", L"充電サイクル数"),
            sc.f6Cycles,
            LS(m_pBattDlg, L"cycles", L"回"),
            rv, 10);
    }

    // ── Missing-data notice ───────────────────────────────────────────────────
    if (!sc.missingNote.IsEmpty())
    {
        SelectObject(hdc, hFontSmall);
        SetTextColor(hdc, RGB(150, 100, 0));
        CString noteSuffix = LS(m_pBattDlg,
            L"  \u2014  score computed from available factors only.",
            L"  \u2014  利用可能な項目のみでスコアを算出しています。");
        CString note = L"\u26a0  " + sc.missingNote + noteSuffix;
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
    {
        CString kmLabel = LS(m_pBattDlg, L"Key Metrics", L"主要指標");
        TextOut(hdc, LEFT, y, kmLabel, kmLabel.GetLength());
    }
    y += S(28);

    auto DrawStatCard = [&](int cx, int cy, int cw, int ch,
        const CString& title, const CString& value, COLORREF valCol)
        {
            HBRUSH hCard = CreateSolidBrush(RGB(255, 255, 255));
            HPEN   hPen = CreatePen(PS_SOLID, 1, RGB(210, 215, 225));
            HBRUSH pOldBr = (HBRUSH)SelectObject(hdc, hCard);
            HPEN   pOldPn = (HPEN)SelectObject(hdc, hPen);
            RoundRect(hdc, cx, cy, cx + cw, cy + ch, S(10), S(10));
            SelectObject(hdc, pOldBr);
            SelectObject(hdc, pOldPn);
            DeleteObject(hCard);
            DeleteObject(hPen);

            SelectObject(hdc, hFontSmall);
            SetTextColor(hdc, RGB(100, 100, 115));
            RECT rt = { cx + S(6), cy + S(6), cx + cw - S(4), cy + ch / 2 };
            DrawText(hdc, title, -1, &rt, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, hFontSub);
            SetTextColor(hdc, valCol);
            RECT rv = { cx + S(4), cy + ch / 2, cx + cw - S(4), cy + ch - S(4) };
            DrawText(hdc, value, -1, &rv, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        };

    {
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
            DrawStatCard(cx, y, cardW, cardH,
                LS(m_pBattDlg, L"Battery Health", L"バッテリー健全性"), v, col);
            cx += cardW + gap;
        }
        // Card 2: Cycles
        {
            DrawStatCard(cx, y, cardW, cardH,
                LS(m_pBattDlg, L"Charge Cycles", L"充電サイクル数"),
                m_reportData.cycles.IsEmpty() ? L"N/A" : m_reportData.cycles,
                RGB(50, 100, 180));
            cx += cardW + gap;
        }
        // Card 3: Current %
        {
            DrawStatCard(cx, y, cardW, cardH,
                LS(m_pBattDlg, L"Current Charge", L"現在の充電量"),
                m_reportData.percentage.IsEmpty() ? L"N/A" : m_reportData.percentage,
                RGB(50, 130, 200));
        }

        y += cardH + gap;

        // Row 2 of cards
        cx = LEFT;
        // Card 4: Voltage
        {
            DrawStatCard(cx, y, cardW, cardH,
                LS(m_pBattDlg, L"Voltage", L"電圧"),
                m_reportData.voltage.IsEmpty() ? L"N/A" : m_reportData.voltage,
                RGB(80, 80, 160));
            cx += cardW + gap;
        }
        // Card 5: Full Charge Cap
        {
            DrawStatCard(cx, y, cardW, cardH,
                LS(m_pBattDlg, L"Full Charge Cap", L"満充電容量"),
                m_reportData.fullChargeCapacity.IsEmpty() ? L"N/A" : m_reportData.fullChargeCapacity,
                RGB(0, 130, 80));
            cx += cardW + gap;
        }
        // Card 6: Design Capacity
        {
            DrawStatCard(cx, y, cardW, cardH,
                LS(m_pBattDlg, L"Design Capacity", L"設計容量"),
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
        {
            CString exportLabel = LS(m_pBattDlg, L"Export Report", L"レポート出力");
            DrawText(hdc, exportLabel, -1, &rBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
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
        CString exportPrompt = LS(m_pBattDlg,
            L"Choose export format:\n\nYes  \u2192 HTML\nNo   \u2192 PDF",
            L"出力形式を選択してください：\n\nはい \u2192 HTML\nいいえ \u2192 PDF");
        int choice = AfxMessageBox(exportPrompt, MB_YESNOCANCEL | MB_ICONQUESTION);

        if (choice == IDYES)  ExportHtmlReport();
        else if (choice == IDNO)
        {
            CString pdfPath;
            ExportPrintToPdf(pdfPath);
            // Success message is shown inside ExportPrintToPdf
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

    bool jp = IsJP(m_pBattDlg);

    CString html;
    html =
        L"<!DOCTYPE html>\n<html lang='" + CString(jp ? L"ja" : L"en") + L"'>\n<head>\n"
        L"<meta charset='UTF-8'/>\n"
        L"<title>" + CString(jp ? L"バッテリー状態レポート" : L"Battery Condition Report") + L"</title>\n"
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
        L"<h1>" + CString(jp ? L"バッテリー状態レポート" : L"Battery Condition Report") + L"</h1>\n";

    // Grade badge
    CString color;
    color.Format(L"#%02X%02X%02X",
        GetRValue(sc.gradeColor), GetGValue(sc.gradeColor), GetBValue(sc.gradeColor));
    html += L"<div class='badge' style='background:" + color + L";'>" + sc.grade + L"</div>\n";

    // Score bar
    CString scoreHtml;
    scoreHtml.Format(
        L"<p><strong>" + CString(jp ? L"総合スコア" : L"Overall Score") + L": %.0f / 100</strong></p>\n"
        L"<div class='bar-track'><div class='bar-fill' style='width:%.0f%%;background:%s;'></div></div>\n"
        L"<p>%s</p>\n",
        sc.total, sc.total, color.GetString(), sc.advice.GetString());
    html += scoreHtml;

    // Factor table
    if (jp)
        html += L"<h2>スコア内訳</h2>\n<table>\n"
        L"<tr><th>項目</th><th>基本ウェイト</th><th>サブスコア</th><th>実測値</th></tr>\n";
    else
        html += L"<h2>Score Breakdown</h2>\n<table>\n"
        L"<tr><th>Factor</th><th>Base Weight</th><th>Sub-Score</th><th>Raw Value</th></tr>\n";

    // Helper: one table row
    auto FactorRow = [&](const wchar_t* name, int wt, double score, const CString& raw) -> CString
        {
            CString r;
            if (score >= 0.0)
                r.Format(L"<tr><td>%s</td><td>%d%%</td><td>%.0f%%</td><td>%s</td></tr>\n",
                    name, wt, score, raw.GetString());
            else
            {
                CString naCell = jp
                    ? L"<em>未計測 / データなし</em>"
                    : L"<em>Not measured / unavailable</em>";
                r.Format(L"<tr style='color:#aaa'><td>%s</td><td>%d%%</td>"
                    L"<td><em>N/A</em></td><td>%s</td></tr>\n",
                    name, wt, naCell.GetString());
            }
            return r;
        };

    CString rv;
    if (sc.healthPct >= 0) rv.Format(L"%.1f%%", sc.healthPct); else rv = L"";
    html += FactorRow(
        jp ? L"バッテリー健全性比率" : L"Battery Health Ratio", 50, sc.f1Health, rv);

    if (sc.cpuRatePerMin >= 0) rv.Format(L"%.2f%%/min", sc.cpuRatePerMin); else rv = L"";
    html += FactorRow(
        jp ? L"CPU負荷時消耗率" : L"CPU Load Drain Rate", 20, sc.f4Cpu, rv);

    if (sc.disRatePerMin >= 0) rv.Format(L"%.2f%%/min", sc.disRatePerMin); else rv = L"";
    html += FactorRow(
        jp ? L"アイドル放電消耗率" : L"Idle Discharge Drain Rate", 20, sc.f5Dis, rv);

    if (sc.cycleCount >= 0) rv.Format(jp ? L"%d 回" : L"%d cycles", sc.cycleCount); else rv = L"";
    html += FactorRow(
        jp ? L"充電サイクル数" : L"Charge Cycle Count", 10, sc.f6Cycles, rv);

    html += L"</table>\n";

    if (!sc.missingNote.IsEmpty())
    {
        html += L"<p style='color:#a06000;font-size:12px'>&#x26A0; ";
        html += sc.missingNote;
        html += jp
            ? L" &mdash; 利用可能な項目のみでスコアを算出しています。</p>\n"
            : L" &mdash; score computed from available factors only.</p>\n";
    }

    // Key metrics cards
    html += jp
        ? L"<h2>主要指標</h2>\n<div class='cards'>\n"
        : L"<h2>Key Metrics</h2>\n<div class='cards'>\n";

    auto Card = [](const wchar_t* title, const CString& val) -> CString {
        CString c;
        c.Format(L"<div class='card'><div class='card-title'>%s</div>"
            L"<div class='card-value'>%s</div></div>\n", title, val.GetString());
        return c;
        };

    html += Card(
        jp ? L"バッテリー健全性" : L"Battery Health",
        sc.healthPct >= 0
        ? ([&] { CString t; t.Format(L"%.1f%%", sc.healthPct); return t; }())
        : m_reportData.health);

    html += Card(jp ? L"充電サイクル数" : L"Charge Cycles", m_reportData.cycles);
    html += Card(jp ? L"現在の充電量" : L"Current Charge", m_reportData.percentage);
    html += Card(jp ? L"電圧" : L"Voltage", m_reportData.voltage);
    html += Card(jp ? L"満充電容量" : L"Full Charge Cap", m_reportData.fullChargeCapacity);
    html += Card(jp ? L"設計容量" : L"Design Capacity", m_reportData.designCapacity);
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
        CString errMsg = LS(m_pBattDlg,
            L"Failed to open file for writing.",
            L"ファイルを開けませんでした。");
        AfxMessageBox(errMsg, MB_ICONERROR);
        return;
    }
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

bool CReportDlg::ExportPrintToPdf(CString& outPdfPath) const
{
    // Strategy: write the same HTML the browser export uses to a temp file,
    // then open it with ShellExecute "print" verb so Windows prints it to
    // "Microsoft Print to PDF" (or whatever PDF printer the user picks in the
    // system print dialog).  This guarantees identical content & styling.

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

    // Write HTML to a temp file
    wchar_t tempPath[MAX_PATH];
    GetTempPath(MAX_PATH, tempPath);
    CString tempHtml;
    tempHtml.Format(L"%sbattery_report_print.html", tempPath);

    CString html = BuildHtmlReport();

    // Inject a print-on-load script so the browser prints automatically
    // and a CSS @page rule for better paper layout
    CString printCss =
        L"<style>@media print{"
        L"body{margin:15mm;background:#fff;}"
        L".badge{-webkit-print-color-adjust:exact;print-color-adjust:exact;}"
        L".bar-fill{-webkit-print-color-adjust:exact;print-color-adjust:exact;}"
        L"}</style>\n"
        L"<script>window.onload=function(){window.print();};</script>\n";

    // Insert before </head>
    html.Replace(L"</head>", printCss + L"</head>");

    // Write UTF-8 with BOM
    CStdioFile f;
    if (!f.Open(tempHtml, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
    {
        CString errMsg = LS(m_pBattDlg,
            L"Failed to write temporary HTML file.",
            L"一時HTMLファイルの書き込みに失敗しました。");
        AfxMessageBox(errMsg, MB_ICONERROR);
        return false;
    }
    const BYTE bom[] = { 0xEF, 0xBB, 0xBF };
    f.Write(bom, 3);
    CStringA utf8;
    int wlen = WideCharToMultiByte(CP_UTF8, 0, html.GetString(), -1,
        nullptr, 0, nullptr, nullptr);
    WideCharToMultiByte(CP_UTF8, 0, html.GetString(), -1,
        utf8.GetBuffer(wlen), wlen, nullptr, nullptr);
    utf8.ReleaseBuffer(wlen - 1);
    f.Write(utf8.GetString(), utf8.GetLength());
    f.Close();

    // Open in browser — the print dialog will appear automatically
    // The user selects "Microsoft Print to PDF" and sets outPdfPath as the destination
    HINSTANCE hr = ShellExecute(nullptr, L"open",
        tempHtml.GetString(), nullptr, nullptr, SW_SHOWNORMAL);

    if ((INT_PTR)hr <= 32)
    {
        CString errMsg = LS(m_pBattDlg,
            L"Failed to open browser for printing.",
            L"印刷用ブラウザを開けませんでした。");
        AfxMessageBox(errMsg, MB_ICONERROR);
        return false;
    }

    // Show guidance — the browser will open and auto-trigger the print dialog
    CString msg = LS(m_pBattDlg,
        L"Your browser will open with the report.\n\n"
        L"In the print dialog:\n"
        L"  1. Set the destination to \"Microsoft Print to PDF\"\n"
        L"  2. Click Print and save to your chosen location.",
        L"レポートをブラウザで開きます。\n\n"
        L"印刷ダイアログにて：\n"
        L"  1. 出力先を「Microsoft Print to PDF」に設定\n"
        L"  2. 印刷をクリックして保存先を指定してください。");
    AfxMessageBox(msg, MB_ICONINFORMATION);

    // outPdfPath is the user's chosen path from the Save dialog above;
    // actual writing is done by the PDF printer in the browser.
    return true;
}

void CReportDlg::RenderReportToDC(HDC /*hDC*/, int /*pageW*/, int /*pageH*/,
    int /*marginX*/, int /*marginY*/, float /*scaleX*/, float /*scaleY*/) const
{
    // No longer used — PDF export now goes through the browser (see ExportPrintToPdf).
    // Kept to satisfy the declaration in the header.
}