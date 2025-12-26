#include "pch.h"
#include "framework.h"
#include "resource.h"
#include "CManipulationDlg.h"
#include "afxdialogex.h"

#include <algorithm>
#include <regex>
#include <fstream>
#include <sstream>
#include <cwctype>
#include <vector>
#include <cmath>
#include <map>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// ===================== Language support (EN / JP) =====================



enum LANG_INDEX
{
    LANG_EN = 0,
    LANG_JP = 1
};

enum M_TEXT_KEY
{
    MTK_STATUS_GENUINE = 0,
    MTK_STATUS_SUSPICIOUS,
    MTK_STATUS_LIKELY_MANIP,

    MTK_PARSE_ERROR,
    MTK_GENPARSE_ERROR,

    // Row labels
    MTK_ROW_SUDDEN_JUMP,
    MTK_ROW_CAP_EXCEEDS,
    MTK_ROW_CYCLE_MISMATCH,
    MTK_ROW_HEALTH_FOR_AGE,
    MTK_ROW_ERRATIC,
    MTK_ROW_SERIAL,
   /* MTK_ROW_UNREAL_DISCHARGE,
    MTK_ROW_UNREAL_CHARGE,*/
    MTK_ROW_CAP_TIME_IMPLAUSIBLE, // NEW RULE 7
    MTK_ROW_DESIGN_CAP_CHANGED,     // RULE 8 (NEW)
    MTK_ROW_FULLTIME,
    MTK_ROW_FULLCAP_ANOM,

    // Row hints
    MTK_HINT_SUDDEN_JUMP,
    MTK_HINT_CAP_EXCEEDS,
    MTK_HINT_CYCLE_MISMATCH,
    MTK_HINT_HEALTH_FOR_AGE,
    MTK_HINT_ERRATIC,
    MTK_HINT_SERIAL,
    //MTK_HINT_UNREAL_DISCHARGE,
    //MTK_HINT_UNREAL_CHARGE,

    MTK_HINT_CAP_TIME_IMPLAUSIBLE, // NEW rule 7
    MTK_HINT_DESIGN_CAP_CHANGED,    //  RULE 8

    MTK_HINT_FULLTIME,
    MTK_HINT_FULLCAP_ANOM,

    // Detail format strings
    MTK_DETAIL_SUDDEN_JUMP,
    MTK_DETAIL_CAP_EXCEEDS,
    MTK_DETAIL_CYCLE_MISMATCH,
    MTK_DETAIL_HEALTH_FOR_AGE,
    MTK_DETAIL_ERRATIC,
    MTK_DETAIL_SERIAL,
    //MTK_DETAIL_UNREAL_DISCHARGE,
    //MTK_DETAIL_UNREAL_CHARGE,

    MTK_DETAIL_CAP_TIME_IMPLAUSIBLE, // NEW rule 7

    MTK_DETAIL_DESIGN_CAP_CHANGED,   //  RULE 8

    MTK_DETAIL_FULLTIME,
    MTK_DETAIL_FULLCAP_ANOM,

    MTK_COUNT
};





static const wchar_t* g_Texts[2][MTK_COUNT] =
{
    // ==================== English ====================
    {
        L"Genuine",                         // MTK_STATUS_GENUINE
        L"Suspicious",                      // MTK_STATUS_SUSPICIOUS
        L"Likely Manipulated",              // MTK_STATUS_LIKELY_MANIP

        L"[BMD][Parse] File read failed or empty.", // MTK_PARSE_ERROR

        // MTK_GENPARSE_ERROR
        L"Failed to generate/parse powercfg battery report.\n"
        L"- Ensure you're on Windows with a battery\n"
        L"- Try running 'powercfg /batteryreport' in CMD\n"
        L"- If it needs elevation, run the app as admin",

    // Row labels
    L"Sudden capacity jump",            // MTK_ROW_SUDDEN_JUMP
    L"Capacity exceeds design",         // MTK_ROW_CAP_EXCEEDS
    L"Cycle vs health mismatch",        // MTK_ROW_CYCLE_MISMATCH
    L"Unrealistic battery health for age",   // MTK_ROW_HEALTH_FOR_AGE
    L"Erratic health pattern",          // MTK_ROW_ERRATIC
    L"Suspicious serial number",        // MTK_ROW_SERIAL
    //L"Unrealistic discharge speed",     // MTK_ROW_UNREAL_DISCHARGE
    //L"Unrealistic charge speed",        // MTK_ROW_UNREAL_CHARGE

    L"Capacity change too fast over time",   // NEW RULE 7 -  MTK_ROW_CAP_TIME_IMPLAUSIBLE

    L"Design capacity changed over time",      // MTK_ROW_DESIGN_CAP_CHANGED

    L"Impossible full charge time",     // MTK_ROW_FULLTIME
    L"Sudden full capacity anomaly",    // MTK_ROW_FULLCAP_ANOM

    // Row hints
    L"Big one-step rise in health/capacity",     // MTK_HINT_SUDDEN_JUMP
    L"Full > design by >5%",                     // MTK_HINT_CAP_EXCEEDS
    L"Health too good for cycle count",          // MTK_HINT_CYCLE_MISMATCH
    L"Very high health after many months",       // MTK_HINT_HEALTH_FOR_AGE
    L"Multiple upward jumps >3%",                // MTK_HINT_ERRATIC
    L"Placeholder/format mismatch",              // MTK_HINT_SERIAL
    //L">100 W discharge",                         // MTK_HINT_UNREAL_DISCHARGE
    //L">150 W charge",                            // MTK_HINT_UNREAL_CHARGE

    L"Capacity changes faster than physically possible", // NEW rule 7 - MTK_HINT_CAP_TIME_IMPLAUSIBLE

    L"Design capacity should never change",    // MTK_HINT_DESIGN_CAP_CHANGED

    L"0–100% in <30 min",                        // MTK_HINT_FULLTIME
    L"±12% jump in full mWh",                    // MTK_HINT_FULLCAP_ANOM

    // Detail format strings
    L"Capacity %.0f → %.0f mWh or health %.1f%% → %.1f%%",           // MTK_DETAIL_SUDDEN_JUMP
    L"Full %.0f mWh > design %.0f mWh (+%.1f%%)",                    // MTK_DETAIL_CAP_EXCEEDS
    L"Health %.1f%% too good for cycle %d (expected ≤ %.1f%%)",      // MTK_DETAIL_CYCLE_MISMATCH
    L"%.1f%% after %d months",                                        // MTK_DETAIL_HEALTH_FOR_AGE
    L"%d upward jumps > %.1f%%",                                      // MTK_DETAIL_ERRATIC
    L"Serial \"%s\" looks placeholder/invalid",                       // MTK_DETAIL_SERIAL
    //L"Discharge rate %.1f W exceeds %.1f W",                          // MTK_DETAIL_UNREAL_DISCHARGE
    //L"Charge rate %.1f W exceeds %.1f W",                             // MTK_DETAIL_UNREAL_CHARGE

    L"Capacity changed %.1f%% over %.1f months (%.1f%% per month)",   // NEW rule 7 - MTK_DETAIL_CAP_TIME_IMPLAUSIBLE

    L"Design capacity changed from %llu to %llu mWh", // MTK_DETAIL_DESIGN_CAP_CHANGED

    L"0–100%% in %.1f min (too fast)",                                // MTK_DETAIL_FULLTIME
    L"Full capacity jump %.1f%% (%llu → %llu mWh)"                    // MTK_DETAIL_FULLCAP_ANOM
},

// ==================== Japanese ====================
{
    L"正常",                               // MTK_STATUS_GENUINE
    L"不審",                               // MTK_STATUS_SUSPICIOUS
    L"改ざんの可能性が高い",               // MTK_STATUS_LIKELY_MANIP

    L"[BMD][解析] ファイルの読み込みに失敗したか、空のファイルです。", // MTK_PARSE_ERROR

    // MTK_GENPARSE_ERROR
    L"powercfg のバッテリーレポートの生成/解析に失敗しました。\n"
    L"- バッテリー搭載の Windows 環境で実行していることを確認してください\n"
    L"- コマンド プロンプトで 'powercfg /batteryreport' を実行してみてください\n"
    L"- 管理者権限が必要な場合は、このアプリを管理者として実行してください",

    // Row labels
    L"容量が急に変化",                       // MTK_ROW_SUDDEN_JUMP
    L"容量が設計を超えています",                 // MTK_ROW_CAP_EXCEEDS
    L"サイクル数とヘルスの不一致",           // MTK_ROW_CYCLE_MISMATCH
    L"経年に対して不自然なバッテリーヘルス", // MTK_ROW_HEALTH_FOR_AGE
    L"不規則な健康パターン",                   // MTK_ROW_ERRATIC
    L"シリアル番号が不審",                   // MTK_ROW_SERIAL
    //L"非現実的な排出速度",                   // MTK_ROW_UNREAL_DISCHARGE
    //L"非現実的な充電速度",                   // MTK_ROW_UNREAL_CHARGE

    L"時間の経過とともに容量が非常に速く変化する",   // NEW RULE 7 -  MTK_ROW_CAP_TIME_IMPLAUSIBLE

    L"設計容量が時間とともに変化しています",   // MTK_ROW_DESIGN_CAP_CHANGED

    L"不可能なフル充電時間",                 // MTK_ROW_FULLTIME
    L"突然のバッテリー容量異常",           // MTK_ROW_FULLCAP_ANOM

    // Row hints
    L"ヘルス/容量が一度に大きく増える場合",             // MTK_HINT_SUDDEN_JUMP
    L"フル充電容量 > 設計容量 > 5%",               // MTK_HINT_CAP_EXCEEDS
    L"サイクル数に対してヘルスが良すぎる",             // MTK_HINT_CYCLE_MISMATCH
    L"長期間使用後なのにヘルスが非常に高い",           // MTK_HINT_HEALTH_FOR_AGE
    L"3%を超える複数回の上方ジャンプ",               // MTK_HINT_ERRATIC
    L"プレースホルダ/形式の不一致",                  // MTK_HINT_SERIAL
    //L"100W を超える放電",                         // MTK_HINT_UNREAL_DISCHARGE
    //L"150W を超える充電",                         // MTK_HINT_UNREAL_CHARGE

    L"容量は %.1f か月で %.1f%% 変化しました (月あたり %.1f%%)",    // NEW rule 7 - MTK_HINT_CAP_TIME_IMPLAUSIBLE

    L"設計容量は通常変化しません",             // MTK_HINT_DESIGN_CAP_CHANGED

    L"0〜100% が 30 分未満で充電される",               // MTK_HINT_FULLTIME
    L"満充電容量が ±12% 以上一度に変化",                // MTK_HINT_FULLCAP_ANOM

    // Detail format strings
    L"容量 %.0f → %.0f mWh またはヘルス %.1f%% → %.1f%%",           // MTK_DETAIL_SUDDEN_JUMP
    L"フル %.0f mWh > 設計 %.0f mWh (+%.1f%%)",                      // MTK_DETAIL_CAP_EXCEEDS
    L"ヘルス %.1f%% はサイクル数 %d に対して良すぎる (期待値 ≤ %.1f%%)", // MTK_DETAIL_CYCLE_MISMATCH
    L"%d ヶ月後に %.1f%%",                                            // MTK_DETAIL_HEALTH_FOR_AGE
    L"%.1f%% を超える上昇が %d 回",                                   // MTK_DETAIL_ERRATIC
    L"シリアル番号 \"%s\" はプレースホルダ/無効な可能性",            // MTK_DETAIL_SERIAL
    //L"放電速度 %.1f W が %.1f W を超えています",                      // MTK_DETAIL_UNREAL_DISCHARGE
    //L"充電速度 %.1f W が %.1f W を超えています",                      // MTK_DETAIL_UNREAL_CHARGE

    L"容量は %.1f か月で %.1f%% 変化しました (月あたり %.1f%%)"          // NEW rule 7 - MTK_DETAIL_CAP_TIME_IMPLAUSIBLE

    L"設計容量が %llu mWh から %llu mWh に変化しました"              // MTK_DETAIL_DESIGN_CAP_CHANGED

    L"0〜100%% が %.1f 分で完了 (速すぎる)",                         // MTK_DETAIL_FULLTIME
    L"満充電容量が %.1f%% 変化 (%llu → %llu mWh)"                    // MTK_DETAIL_FULLCAP_ANOM
}
};



// ===================== Robust helpers (encoding, powercfg, text) =====================

static bool RunPowerCfgBatteryReport(const CString& path)
{
    CString cmd; cmd.Format(_T("powercfg /batteryreport /output \"%s\""), path);
    STARTUPINFO si{ sizeof(si) }; PROCESS_INFORMATION pi{};
    CString cmdBuf = cmd;

    LPTSTR p = cmdBuf.GetBuffer(); // ensure writable buffer
    BOOL ok = CreateProcess(nullptr, p, nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    cmdBuf.ReleaseBuffer();
    if (!ok) return false;

    // 10s timeout, then clean up
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
    // try UTF-8 (no BOM)
    int n = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data(), (int)len, nullptr, 0);
    if (n > 0) {
        CStringW w; LPWSTR p = w.GetBuffer(n);
        MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data(), (int)len, p, n);
        w.ReleaseBuffer(n); return w;
    }
    // fallback ACP
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
    static const std::wregex reNBSP(L"&nbsp;?", std::regex_constants::icase);
    static const std::wregex reWS(L"[ \t\r\n]+");

    s = std::regex_replace(s, reTags, L" ");
    s = std::regex_replace(s, reNBSP, L" ");
    s = std::regex_replace(s, reWS, L" ");
    CString o(s.c_str()); o.Trim(); return o;
}

// (Optional) time helpers
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

// ===================== Internal detection engine =====================

namespace BMD_Internal {

    struct Weights {
        int suddenCapacityJump = 25;
        int capacityExceedsDesign = 20;
        int unrealisticCycleCount = 15;
        int healthTooGoodForAge = 10;
        int erraticHealthPattern = 15;
        int suspiciousSerial = 5;
        // NEW:
        //int unrealisticRate = 20;     // charge/discharge/time-to-full

        int capacityTimeImplausible = 20; // NEW RULE 7
        int designCapacityChanged = 40; // RULE 8 (STRONG SIGNAL)

        int suddenFullCapJump = 20;   // sudden ± jump in full capacity
    };

    struct Thresholds {
        double jumpFullChargeRatio = 1.10; // +10%
        double jumpHealthPct = 8.0;        // +8% pts
        double exceedDesignPct = 5.0;      // > +5%
        double cyclesPer1pctLoss = 20.0;   // ~1% loss per 20 cycles
        double cycleSlackPct = 6.0;        // +6% over expected
        double minFloorHealth = 40.0;      // clamp expected >= 40%
        int    monthsForHighHealthCheck = 6;
        double healthTooGoodAfterMonths = 95.0;
        double upwardJumpPct = 3.0;        // count jumps > +3%
        int    upwardJumpCountToFlag = 2;
        // NEW (rate/time/jump)
        //double maxDischargeWatt = 100.0;   // >100 W discharge => suspect
        //double maxChargeWatt = 150.0;      // >150 W charge    => suspect

        double maxCapacityChangePctPerMonth = 4.0; // NEW RULE 7

        double minFullChargeMins = 30.0;   // 0?100% <30 min   => suspect
        double fullCapacityJumpPct = 12.0; // sudden ±12% change in full capacity
    };

    static Weights    g_weights;
    static Thresholds g_thresholds;
    Weights& GetWeights() { return g_weights; }
    Thresholds& GetThresholds() { return g_thresholds; }

    // ---------- utils ----------
    static std::wstring ToLower(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](wchar_t c) { return (wchar_t)std::towlower(c); });
        return s;
    }
    static std::wstring Trim(const std::wstring& in) {
        size_t a = 0, b = in.size();
        while (a < b && iswspace(in[a])) ++a;
        while (b > a && iswspace(in[b - 1])) --b;
        return in.substr(a, b - a);
    }
    static void AddFlag(std::vector<BMD_Flag>& flags, const CString& t, const CString& d, int pts) {
        BMD_Flag f; f.title = t; f.detail = d; f.points = pts; flags.push_back(f);
    }

    // tolerant numeric extraction after a label (CString-based)
    static bool Extract_mWh_AfterLabel_CStr(const CString& text, const CString& label, uint64_t& out)
    {
        CString lowerText = text; lowerText.MakeLower();
        CString lowerLabel = label; lowerLabel.MakeLower();

        int p = lowerText.Find(lowerLabel);
        if (p < 0) return false;

        int i = p + lowerLabel.GetLength();
        while (i < lowerText.GetLength() && !iswdigit(lowerText[i])) ++i;
        if (i >= lowerText.GetLength()) return false;

        CString num;
        while (i < lowerText.GetLength()) {
            WCHAR c = lowerText[i];
            if (iswdigit(c) || c == L',') { num.AppendChar(c); ++i; }
            else break;
        }
        if (num.IsEmpty()) return false;

        num.Remove(L',');
        unsigned long long v = 0ULL;
        if (swscanf_s(num, L"%llu", &v) != 1) return false;
        out = (uint64_t)v;
        return true;
    }

    static int ExtractIntAfterLabel_CStr(const CString& text, const CString& label, int defVal = -1)
    {
        CString lowerText = text; lowerText.MakeLower();
        CString lowerLabel = label; lowerLabel.MakeLower();

        int p = lowerText.Find(lowerLabel);
        if (p < 0) return defVal;

        int i = p + lowerLabel.GetLength();
        while (i < lowerText.GetLength() && !iswdigit(lowerText[i]) && lowerText[i] != L'-') ++i;
        if (i >= lowerText.GetLength()) return defVal;

        CString num;
        if (lowerText[i] == L'-') { num.AppendChar(L'-'); ++i; }
        while (i < lowerText.GetLength() && iswdigit(lowerText[i])) {
            num.AppendChar(lowerText[i]); ++i;
        }
        if (num.IsEmpty() || num == L"-") return defVal;
        return _wtoi(num);
    }

    // Relaxed minutes extraction after a cue phrase (e.g., "to full", "time to full charge")
    static bool ExtractMinutesAfterCue_CStr(const CString& text, const std::vector<CString>& cues, double& minutesOut)
    {
        CString lower = text; lower.MakeLower();
        int pos = -1;
        for (const auto& cue : cues) {
            CString lc = cue; lc.MakeLower();
            pos = lower.Find(lc);
            if (pos >= 0) break;
        }
        if (pos < 0) return false;

        // Search forward for number + unit (min, minutes)
        for (int i = pos; i < lower.GetLength(); ++i) {
            if (iswdigit(lower[i])) {
                int j = i;
                CString num;
                while (j < lower.GetLength() && (iswdigit(lower[j]) || lower[j] == L'.')) {
                    num.AppendChar(lower[j]); ++j;
                }
                while (j < lower.GetLength() && iswspace(lower[j])) ++j;

                bool isMin = false;
                if (j + 2 <= lower.GetLength()) {
                    CString unit = lower.Mid(j, min(12, lower.GetLength() - j)); // "min", "mins", "minutes"
                    unit.Trim();
                    if (unit.Find(L"min") == 0) isMin = true;
                    if (unit.Find(L"minute") == 0) isMin = true;
                }
                if (!num.IsEmpty() && isMin) {
                    minutesOut = _wtof(num);
                    return true;
                }
            }
        }
        return false;
    }

    // --------------------- Robust HTML parse + powercfg ---------------------

    bool ParseBatteryReportHtml(const CString& htmlPath, BMD_BatteryInfo& outInfo, int lang)
    {
        outInfo = BMD_BatteryInfo();

        // Read with auto-encoding + strip tags ? tolerant buffer
        CString raw = ReadTextAutoEncoding(htmlPath);
        if (raw.IsEmpty()) {
            AfxMessageBox(g_Texts[lang][MTK_PARSE_ERROR], MB_ICONERROR | MB_TOPMOST);
            return false;
        }

        CString plain = StripHtml(raw);
        CString lower = plain; lower.MakeLower();

        // Manufacturer / Serial (tolerant "line after" grab)
        auto grabLineAfter = [&](const wchar_t* label, int maxLen = 64) -> std::wstring {
            CString L = label; L.MakeLower();
            int p = lower.Find(L);
            if (p < 0) return L"";
            int i = p + L.GetLength();
            while (i < lower.GetLength() && (lower[i] == L' ' || lower[i] == L':' || lower[i] == L'\t')) ++i;
            int j = i;
            while (j < plain.GetLength() && plain[j] != L'\r' && plain[j] != L'\n') ++j;
            CString line = plain.Mid(i, j - i);
            line.Trim();
            if (line.GetLength() > maxLen) line = line.Left(maxLen);
            return std::wstring(line);
            };

        outInfo.manufacturer = grabLineAfter(L"manufacturer");
        outInfo.serialNumber = grabLineAfter(L"serial number");

        // Design capacity (multiple label attempts)
        uint64_t design = 0;
        if (!Extract_mWh_AfterLabel_CStr(plain, L"design capacity", design)) {
            (void)Extract_mWh_AfterLabel_CStr(plain, L"design", design);
        }
        outInfo.designCapacity_mWh = design;

        // Cycle count if present
        outInfo.cycleCount = ExtractIntAfterLabel_CStr(plain, L"cycle count", -1);

        // Capacity history: accept "YYYY-MM-DD  full  design" with optional commas and 'mWh'
        std::wstring wplain = plain.GetString();
        std::wregex reLine(LR"((\d{4}-\d{2}-\d{2})\s+([\d,]+)(?:\s*mwh)?\s+([\d,]+)(?:\s*mwh)?)",
            std::regex_constants::icase);

        std::wsregex_iterator it(wplain.begin(), wplain.end(), reLine), end;
        for (; it != end; ++it) {
            std::wstring d = (*it)[1].str();
            std::wstring sFull = (*it)[2].str();
            std::wstring sDes = (*it)[3].str();

            sFull.erase(std::remove(sFull.begin(), sFull.end(), L','), sFull.end());
            sDes.erase(std::remove(sDes.begin(), sDes.end(), L','), sDes.end());

            unsigned long long fv = 0, dv = 0;
            if (swscanf_s(sFull.c_str(), L"%llu", &fv) == 1 &&
                swscanf_s(sDes.c_str(), L"%llu", &dv) == 1)
            {
                BMD_Sample s;
                s.dateISO = d;
                s.fullCharge_mWh = (uint64_t)fv;
                s.designCapacity_mWh = (uint64_t)dv;
                if (outInfo.designCapacity_mWh == 0 && dv > 0) outInfo.designCapacity_mWh = (uint64_t)dv;
                outInfo.samples.push_back(s);
            }
        }

        // Health calc + sort
        if (outInfo.designCapacity_mWh > 0) {
            for (auto& smp : outInfo.samples) {
                smp.healthPercent = (float)((double)smp.fullCharge_mWh * 100.0 / (double)outInfo.designCapacity_mWh);
            }
        }
        std::sort(outInfo.samples.begin(), outInfo.samples.end(),
            [](const BMD_Sample& a, const BMD_Sample& b) { return a.dateISO < b.dateISO; });

        // NEW: try to extract current charge/discharge rate (mW) if present
        {
            uint64_t rate = 0;
            if (Extract_mWh_AfterLabel_CStr(plain, L"charge rate", rate) ||
                Extract_mWh_AfterLabel_CStr(plain, L"discharge rate", rate))
            {
                outInfo.currentRate_mW = rate;
            }
        }

        // NEW: try to extract 0?100% time in minutes (very tolerant)
        {
            double mins = 0.0;
            std::vector<CString> cues = {
                L"time to full charge", L"to full charge", L"0 to 100", L"0–100", L"0?100", L"full charge in"
            };
            if (ExtractMinutesAfterCue_CStr(plain, cues, mins)) {
                outInfo.lastChargeMinutes = mins;
            }
        }

        return outInfo.designCapacity_mWh > 0 && !outInfo.samples.empty();
    }

    bool GenerateAndParseBatteryReport(BMD_BatteryInfo& outInfo, CString* outHtmlPath, int lang)
    {
        TCHAR tempPath[MAX_PATH]{};
        if (!GetTempPath(MAX_PATH, tempPath)) return false;

        SYSTEMTIME st; GetLocalTime(&st);
        CString outPath;
        outPath.Format(L"%sBatteryReport_%04u%02u%02u_%02u%02u%02u.html",
            tempPath, st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

        if (!RunPowerCfgBatteryReport(outPath))
            return false;

        bool ok = ParseBatteryReportHtml(outPath, outInfo, lang);
        if (ok && outHtmlPath) *outHtmlPath = outPath;
        return ok;
    }

    // --------------------- Detectors + scoring ---------------------

    BMD_DetectionResult RunManipulationChecks(const BMD_BatteryInfo& info, int lang, CString cycles)
    {
        const auto& W = GetWeights();
        const auto& T = GetThresholds();



        BMD_DetectionResult R;
        std::vector<BMD_Flag> flags;
        int score = 0;

        if (info.samples.empty() || info.designCapacity_mWh == 0) {
            R.score = 0;
            R.status = g_Texts[lang][MTK_STATUS_GENUINE];
            return R;
        }

        const auto& s = info.samples;
        const double design = (double)info.designCapacity_mWh;
        const double latestF = (double)s.back().fullCharge_mWh;
        const double latestHealth = (double)((design > 0) ? (latestF * 100.0 / design) : 0.0);


     

        // 1) Sudden Capacity Jump (health or full ratio)
        for (size_t i = 1; i < s.size(); ++i) {
            double oldF = (double)s[i - 1].fullCharge_mWh;
            double newF = (double)s[i].fullCharge_mWh;
            double hOld = s[i - 1].healthPercent;
            double hNew = s[i].healthPercent;

            
            if ((oldF > 0 && newF >= oldF * T.jumpFullChargeRatio) ||
                (hNew - hOld) >= T.jumpHealthPct)
            {
                CString detail;
                detail.Format(g_Texts[lang][MTK_DETAIL_SUDDEN_JUMP],
                    oldF, newF, hOld, hNew);
                AddFlag(flags, g_Texts[lang][MTK_ROW_SUDDEN_JUMP], detail, W.suddenCapacityJump);
                score += W.suddenCapacityJump;
                break;
            }
        }


     

        // 2) Capacity Exceeds Design
        if (design > 0.0) {
            double overPct = (latestF / design - 1.0) * 100.0;


            if (overPct > T.exceedDesignPct) {
                CString d; d.Format(g_Texts[lang][MTK_DETAIL_CAP_EXCEEDS], latestF, design, overPct);
                AddFlag(flags, g_Texts[lang][MTK_ROW_CAP_EXCEEDS], d, W.capacityExceedsDesign);
                score += W.capacityExceedsDesign;
            }
        }


        // 3) Unrealistic Cycle Count vs Health

        if ((float)_wtof(cycles) >= 0) {
            double expected = max(100.0 - (double)(float)_wtof(cycles) / T.cyclesPer1pctLoss, T.minFloorHealth);
            if (latestHealth >= expected + T.cycleSlackPct) {
                CString d; d.Format(g_Texts[lang][MTK_DETAIL_CYCLE_MISMATCH],
                    latestHealth, (float)_wtof(cycles), expected + T.cycleSlackPct);
                AddFlag(flags, g_Texts[lang][MTK_ROW_CYCLE_MISMATCH], d, W.unrealisticCycleCount);
                score += W.unrealisticCycleCount;
            }
        }


        // 4) Health Too Good for Age (months span)
        {
            auto ym = [](const std::wstring& iso) {
                int y = 0, m = 0; if (swscanf_s(iso.c_str(), L"%4d-%2d", &y, &m) != 2) return 0;
                return y * 12 + m;
                };


            
            int months = 0;
            if (s.size() >= 2) {
                int a = ym(s.front().dateISO);
                int b = ym(s.back().dateISO);
                if (a && b && b >= a) months = b - a;
            }
            if (months >= T.monthsForHighHealthCheck && latestHealth >= T.healthTooGoodAfterMonths) {
                CString d; d.Format(g_Texts[lang][MTK_DETAIL_HEALTH_FOR_AGE], latestHealth, months);
                AddFlag(flags, g_Texts[lang][MTK_ROW_HEALTH_FOR_AGE], d, W.healthTooGoodForAge);
                score += W.healthTooGoodForAge;
            }
        }


        // 5) Erratic Health Pattern (multiple upward jumps)
        {
            int upJumps = 0;
            for (size_t i = 1; i < s.size(); ++i) {
                if ((s[i].healthPercent - s[i - 1].healthPercent) > T.upwardJumpPct) ++upJumps;
            }
            if (upJumps >= T.upwardJumpCountToFlag) {
                CString d; d.Format(g_Texts[lang][MTK_DETAIL_ERRATIC], upJumps, T.upwardJumpPct);
                AddFlag(flags, g_Texts[lang][MTK_ROW_ERRATIC], d, W.erraticHealthPattern);
                score += W.erraticHealthPattern;
            }
        }

        // (Removed old #6 Design Capacity Mismatch – maker table deleted)

        // 6) Suspicious Serial Number (generic only)

        if (!info.serialNumber.empty()) {
            std::wstring srl = info.serialNumber;
            auto isAllSame = [&](wchar_t c) {
                return !srl.empty() && std::all_of(srl.begin(), srl.end(), [&](wchar_t x) { return x == c || iswspace(x); });
                };
            bool suspicious = false;
            if (isAllSame(L'0') || isAllSame(L'A') || srl.size() < 6) suspicious = true;
            auto srlLower = ToLower(srl);
            if (srlLower.find(L"reset") != std::wstring::npos) suspicious = true;

            if (suspicious) {
                CString d; d.Format(g_Texts[lang][MTK_DETAIL_SERIAL], srl.c_str());
                AddFlag(flags, g_Texts[lang][MTK_ROW_SERIAL], d, W.suspiciousSerial);
                score += W.suspiciousSerial;
            }
        }
        
        
        



// 7) Capacity vs Time Implausibility (NEW)
{
    auto ym = [](const std::wstring& iso) {
        int y = 0, m = 0;
        if (swscanf_s(iso.c_str(), L"%4d-%2d", &y, &m) != 2)
            return 0;
        return y * 12 + m;
        };

    for (size_t i = 1; i < s.size(); ++i) {

        int m1 = ym(s[i - 1].dateISO);
        int m2 = ym(s[i].dateISO);
        if (!m1 || !m2 || m2 <= m1)
            continue;

        double months = (double)(m2 - m1);
        double prev = (double)s[i - 1].fullCharge_mWh;
        double now = (double)s[i].fullCharge_mWh;
        if (prev <= 0)
            continue;

        double pctChange = fabs(now - prev) * 100.0 / prev;
        double pctPerMonth = pctChange / months;

        if (pctPerMonth > T.maxCapacityChangePctPerMonth) {

            CString d;
            d.Format(
                g_Texts[lang][MTK_DETAIL_CAP_TIME_IMPLAUSIBLE],
                pctChange,
                months,
                pctPerMonth
            );

            AddFlag(
                flags,
                g_Texts[lang][MTK_ROW_CAP_TIME_IMPLAUSIBLE],
                d,
                W.capacityTimeImplausible
            );

            score += W.capacityTimeImplausible;
            break;
        }
    }
}



        //// 7) Charge/Discharge Rate Anomaly (from report)
        //if (info.currentRate_mW > 0) {

        //    

        //    double watts = info.currentRate_mW / 1000.0; // mW ? W
        //    if (watts > T.maxDischargeWatt) {
        //        CString d; d.Format(g_Texts[lang][MTK_DETAIL_UNREAL_DISCHARGE], watts, T.maxDischargeWatt);
        //        AddFlag(flags, g_Texts[lang][MTK_ROW_UNREAL_DISCHARGE], d, W.unrealisticRate);
        //        score += W.unrealisticRate;
        //    }
        //    if (watts > T.maxChargeWatt) {
        //        CString d; d.Format(g_Texts[lang][MTK_DETAIL_UNREAL_CHARGE], watts, T.maxChargeWatt);
        //        AddFlag(flags, g_Texts[lang][MTK_ROW_UNREAL_CHARGE], d, W.unrealisticRate);
        //        score += W.unrealisticRate;
        //    }
        //}




// 8) Design Capacity Changed Over Time (NO threshold)
{
    uint64_t oldestDesign = 0;
    uint64_t latestDesign = 0;

    for (const auto& smp : s) {

        if (smp.designCapacity_mWh > 0) {

            if (oldestDesign == 0)
                oldestDesign = smp.designCapacity_mWh;

            latestDesign = smp.designCapacity_mWh;
        }
    }

   /* CString dbg;
    dbg.Format(L"[BMD] design=%.0f currentRate_mW=%llu\n",
        design,
        oldestDesign);

    OutputDebugString(dbg);*/

    // If both exist and differ → flag immediately
    if (oldestDesign > 0 &&
        latestDesign > 0 &&
        oldestDesign != latestDesign)
    {
        CString d;
        d.Format(
            g_Texts[lang][MTK_DETAIL_DESIGN_CAP_CHANGED],
            (unsigned long long)oldestDesign,
            (unsigned long long)latestDesign
        );

        AddFlag(
            flags,
            g_Texts[lang][MTK_ROW_DESIGN_CAP_CHANGED],
            d,
            W.designCapacityChanged
        );

        score += W.designCapacityChanged;
    }
}


        //// 8) Impossible fast full charge (0?100% in < threshold minutes)
        //if (info.lastChargeMinutes > 0.0 && info.lastChargeMinutes < T.minFullChargeMins) {
        //    CString d; d.Format(g_Texts[lang][MTK_DETAIL_FULLTIME], info.lastChargeMinutes);
        //    AddFlag(flags, g_Texts[lang][MTK_ROW_FULLTIME], d, W.unrealisticRate);
        //    score += W.unrealisticRate;
        //}

        // 9) Sudden big drop or big increase in full charge capacity (± threshold)
        for (size_t i = 1; i < s.size(); ++i) {
            double prev = (double)s[i - 1].fullCharge_mWh;
            double now = (double)s[i].fullCharge_mWh;

            /*CString dbg;
            dbg.Format(L"[BMD] design=%0.1f currentRate_mW=%0.1f\n",
                prev,
                now);

            OutputDebugString(dbg);*/

            if (prev <= 0) continue;
            double deltaPct = (now - prev) * 100.0 / prev;
            if (fabs(deltaPct) >= T.fullCapacityJumpPct) {
                CString d; d.Format(g_Texts[lang][MTK_DETAIL_FULLCAP_ANOM],
                    deltaPct,
                    (unsigned long long)prev,
                    (unsigned long long)now);
                AddFlag(flags, g_Texts[lang][MTK_ROW_FULLCAP_ANOM], d, W.suddenFullCapJump);
                score += W.suddenFullCapJump;
                break;
            }
        }

        if (score > 100) score = 100;
        std::sort(flags.begin(), flags.end(), [](const BMD_Flag& a, const BMD_Flag& b) { return a.points > b.points; });
        R.score = score;
        if (score <= 39) R.status = g_Texts[lang][MTK_STATUS_GENUINE];
        else if (score <= 69) R.status = g_Texts[lang][MTK_STATUS_SUSPICIOUS];
        else R.status = g_Texts[lang][MTK_STATUS_LIKELY_MANIP];
        R.flags = std::move(flags);
        return R;
    }

    // --------------------- UI helpers (checkbox) ---------------------

    static void DrawCheckbox(CDC* pDC, const CRect& box, bool checked, COLORREF accent)
    {
        CPen pen(PS_SOLID, 1, RGB(120, 120, 120));
        CBrush brush(RGB(255, 255, 255));
        CPen* oldPen = pDC->SelectObject(&pen);
        CBrush* oldBr = pDC->SelectObject(&brush);

        pDC->Rectangle(box);

        if (checked) {
            CPen penTick(PS_SOLID, 2, accent);
            pDC->SelectObject(&penTick);
            int x1 = box.left + 3, y1 = box.top + box.Height() / 2;
            int x2 = box.left + box.Width() / 2 - 2, y2 = box.bottom - 4;
            int x3 = box.right - 3, y3 = box.top + 4;
            pDC->MoveTo(x1, y1);
            pDC->LineTo(x2, y2);
            pDC->LineTo(x3, y3);
        }

        pDC->SelectObject(oldPen);
        pDC->SelectObject(oldBr);
    }

    // --------------------- Measurement + Drawing (scroll-aware) ---------------------

    static int MeasureAndDrawPanel(CDC* pDC, const CRect& rc, const BMD_DetectionResult& r, int scrollY, bool doDraw, int lang)
    {
        if (!pDC) return 0;

        // Colors & fonts
        COLORREF band = RGB(0, 160, 80);   // green
        if (r.score >= 70) band = RGB(210, 60, 60);         // red
        else if (r.score >= 40) band = RGB(240, 170, 40);   // orange

        CFont fontTitle, fontItem, fontHint;
        LOGFONT lf{}; SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0);

        // Scale fonts a touch for very small panels
        int rcW = rc.Width(), rcH = rc.Height();
        double scale = 1.0;
        if (rcW < 520 || rcH < 340) scale = 0.9;
        if (rcW < 420 || rcH < 280) scale = 0.8;

        auto mkHeight = [&](LONG h) { return (LONG)std::lround(h * scale); };

        // Title font (bold, slightly larger)
        LONG baseH = lf.lfHeight;
        lf.lfWeight = FW_BOLD; lf.lfHeight = mkHeight((LONG)(baseH * 1.2));
        fontTitle.CreateFontIndirect(&lf);

        // Item font (normal)
        lf.lfWeight = FW_NORMAL; lf.lfHeight = mkHeight((LONG)(baseH * 0.95));
        fontItem.CreateFontIndirect(&lf);

        // Hint font (smaller)
        lf.lfHeight = mkHeight((LONG)(lf.lfHeight * 0.9));
        fontHint.CreateFontIndirect(&lf);

        // Outer panel
        CPen pen(PS_SOLID, 1, RGB(200, 205, 210));
        CBrush brBody(RGB(248, 250, 252));

        // Header band sizes
        int bandH = (int)std::lround(44 * scale);
        int meterTopPad = (int)std::lround(10 * scale);
        int meterH = max(10, (int)std::lround(12 * scale));
        int sidePad = (int)std::lround(12 * scale);

        // List paddings
        int listTopPad = (int)std::lround(10 * scale);
        int listBotPad = (int)std::lround(12 * scale);
        int boxSize = max(12, (int)std::lround(16 * scale));
        int gapX = max(6, (int)std::lround(8 * scale));
        int vPad = max(3, (int)std::lround(5 * scale));

        // Triggered flags lookup
        std::map<CString, BMD_Flag, bool(*)(const CString&, const CString&)> hitMap(
            [](const CString& a, const CString& b) { return a.CompareNoCase(b) < 0; });
        for (const auto& f : r.flags) hitMap[f.title] = f;

        // Full criteria list (always show)
        struct Row { const wchar_t* name; const wchar_t* hint; };
        const Row rows[] = {
            { g_Texts[lang][MTK_ROW_SUDDEN_JUMP],            g_Texts[lang][MTK_HINT_SUDDEN_JUMP] },
            { g_Texts[lang][MTK_ROW_CAP_EXCEEDS],         g_Texts[lang][MTK_HINT_CAP_EXCEEDS] },
            { g_Texts[lang][MTK_ROW_CYCLE_MISMATCH],        g_Texts[lang][MTK_HINT_CYCLE_MISMATCH] },
            { g_Texts[lang][MTK_ROW_HEALTH_FOR_AGE], g_Texts[lang][MTK_HINT_HEALTH_FOR_AGE] },
            { g_Texts[lang][MTK_ROW_ERRATIC],          g_Texts[lang][MTK_HINT_ERRATIC] },
            { g_Texts[lang][MTK_ROW_SERIAL],        g_Texts[lang][MTK_HINT_SERIAL] },
            //{ g_Texts[lang][MTK_ROW_UNREAL_DISCHARGE],     g_Texts[lang][MTK_HINT_UNREAL_DISCHARGE] },
            //{ g_Texts[lang][MTK_ROW_UNREAL_CHARGE],        g_Texts[lang][MTK_HINT_UNREAL_CHARGE] },

              // NEW RULE #7 (replaces charge/discharge rate)
              { g_Texts[lang][MTK_ROW_CAP_TIME_IMPLAUSIBLE], g_Texts[lang][MTK_HINT_CAP_TIME_IMPLAUSIBLE] },

              //  RULE 8
    { g_Texts[lang][MTK_ROW_DESIGN_CAP_CHANGED], g_Texts[lang][MTK_HINT_DESIGN_CAP_CHANGED] },

          /*  { g_Texts[lang][MTK_ROW_FULLTIME],     g_Texts[lang][MTK_HINT_FULLTIME] },*/
            { g_Texts[lang][MTK_ROW_FULLCAP_ANOM],    g_Texts[lang][MTK_HINT_FULLCAP_ANOM] },
        };

        // Colors
        COLORREF textHit = RGB(35, 38, 41);
        COLORREF textDim = RGB(120, 125, 130);
        COLORREF hintDim = RGB(150, 155, 160);
        COLORREF accent = band;

        // Begin measuring full content height (single column)
        CRect rcBand = rc; rcBand.bottom = rcBand.top + bandH;
        CRect rcMeter(rc.left + sidePad, rcBand.bottom + meterTopPad,
            rc.right - sidePad, rcBand.bottom + meterTopPad + meterH);
        CRect rcList(rc.left + sidePad, rcMeter.bottom + listTopPad,
            rc.right - sidePad, rc.bottom - listBotPad);

        // 1) Compute height needed for list (single column, wrapped text + optional hint)
        CDC* dc = pDC;
        int contentListHeight = 0;

        // Use font to measure
        CFont* oldItem = dc->SelectObject(&fontItem);
        for (size_t i = 0; i < std::size(rows); ++i)
        {
            const auto& rdef = rows[i];
            bool matched = (hitMap.find(rdef.name) != hitMap.end());

            // Main text
            CString toDraw = rdef.name;
            if (matched) {
                const auto& f = hitMap[rdef.name];
                if (!f.detail.IsEmpty()) {
                    toDraw += L" — ";
                    toDraw += f.detail;
                }
            }

            CRect calcMain(0, 0, rcList.Width() - (boxSize + gapX), 10000);
            dc->DrawText(toDraw, &calcMain, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);

            int rowHeight = max(boxSize, calcMain.Height());
            int hintHeight = 0;

            if (!matched && rows[i].hint && *rows[i].hint) {
                CFont* oldHint = dc->SelectObject(&fontHint);
                CRect calcHint(0, 0, rcList.Width() - (boxSize + gapX), 10000);
                dc->DrawText(rows[i].hint, &calcHint, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
                dc->SelectObject(oldHint);
                hintHeight = calcHint.Height();
            }

            contentListHeight += rowHeight + hintHeight + vPad;
        }
        dc->SelectObject(oldItem);

        // Full panel content height (header + meter + list padding + list content)
        int fullContentHeight =
            bandH + meterTopPad + meterH + listTopPad + contentListHeight + listBotPad;

        if (!doDraw) {
            return fullContentHeight; // measurement-only
        }

        // 2) DRAW (double-buffered) with vertical scroll offset

        // Back buffer
        CMemDC memDC(*pDC, rc);
        CDC* pDraw = &memDC.GetDC();

        int saved = pDraw->SaveDC();
        pDraw->IntersectClipRect(&rc);

        // Fill panel + border
        CPen* oldPen = pDraw->SelectObject(&pen);
        CBrush* oldBr = pDraw->SelectObject(&brBody);
        pDraw->Rectangle(rc);

        // Header band (no scroll offset for header/meter)
        {
            CBrush brBand(band);
            pDraw->FillRect(&rcBand, &brBand);

            CString title; title.Format(L"%s   (%d/100)", r.status.GetString(), r.score);
            CRect rcTxt = rcBand; rcTxt.DeflateRect(sidePad, (int)std::lround(8 * scale));
            pDraw->SetBkMode(TRANSPARENT);
            pDraw->SetTextColor(RGB(255, 255, 255));
            CFont* oldF = pDraw->SelectObject(&fontTitle);
            pDraw->DrawText(title, &rcTxt, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
            pDraw->SelectObject(oldF);

            // Score meter
            pDraw->Rectangle(rcMeter);
            CRect rcFill = rcMeter;
            int w = rcMeter.Width();
            int fillW = (int)((r.score / 100.0) * (w - 2));
            rcFill.right = rcFill.left + fillW;
            CBrush brFill(band);
            pDraw->FillRect(&rcFill, &brFill);
        }

        // Scrollable list area: set clip + translate by -scrollY
        CRect scrollViewport = rcList;
        pDraw->IntersectClipRect(&scrollViewport);
        pDraw->SetViewportOrg(0, -scrollY); // translate drawing upward by scrollY

        // Draw list
        COLORREF textColor;
        int y = rcList.top; // NOTE: rcList.top is absolute; we translated viewport origin above

        for (size_t i = 0; i < std::size(rows); ++i)
        {
            const auto& rdef = rows[i];
            bool matched = (hitMap.find(rdef.name) != hitMap.end());

            // Checkbox
            CRect cb(rcList.left, y + 2, rcList.left + boxSize, y + 2 + boxSize);
            DrawCheckbox(pDraw, cb, matched, accent);

            // Main text (wrap)
            CString toDraw = rdef.name;
            if (matched) {
                const auto& f = hitMap[rdef.name];
                if (!f.detail.IsEmpty()) {
                    toDraw += L" — ";
                    toDraw += f.detail;
                }
            }

            CRect label(cb.right + gapX, y, rcList.right, y + 10000);
            CFont* oldItem2 = pDraw->SelectObject(&fontItem);
            textColor = matched ? RGB(35, 38, 41) : RGB(120, 125, 130);
            pDraw->SetTextColor(textColor);

            CRect calcMain = label;
            pDraw->DrawText(toDraw, &calcMain, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);

            // Draw main
            CRect drawMain = label; drawMain.bottom = drawMain.top + calcMain.Height();
            pDraw->DrawText(toDraw, &drawMain, DT_WORDBREAK | DT_NOPREFIX);
            pDraw->SelectObject(oldItem2);

            int rowHeight = max(boxSize, calcMain.Height());
            int hintHeight = 0;

            // Optional hint
            if (!matched && rows[i].hint && *rows[i].hint) {
                CRect hintRc(label.left, drawMain.bottom, label.right, drawMain.bottom + 10000);
                CFont* oldHint = pDraw->SelectObject(&fontHint);
                pDraw->SetTextColor(RGB(150, 155, 160));
                CRect calcHint = hintRc;
                pDraw->DrawText(rows[i].hint, &calcHint, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
                hintHeight = calcHint.Height();
                // Draw hint
                pDraw->DrawText(rows[i].hint, &hintRc, DT_WORDBREAK | DT_NOPREFIX);
                pDraw->SelectObject(oldHint);
            }

            y += rowHeight + hintHeight + vPad;
        }

        // restore
        pDraw->RestoreDC(saved);
        pDraw->SelectObject(oldPen);
        pDraw->SelectObject(oldBr);

        return fullContentHeight;
    }

} // namespace BMD_Internal

using namespace BMD_Internal;

// ===================== Dialog boilerplate =====================

CManipulationDlg::CManipulationDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_MANIPULATION, pParent) // <-- replace with your real dialog ID
{
}

void CManipulationDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BOOL CManipulationDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Enable vertical scroll bar on the dialog
    ModifyStyle(0, WS_VSCROLL);

    RunBatteryManipulationCheck(); // auto-run
    Invalidate(); // paint panel
    return TRUE;
}

BEGIN_MESSAGE_MAP(CManipulationDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_SIZE()
    ON_WM_VSCROLL()
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

// --------------------- Paint (double-buffered) ---------------------

void CManipulationDlg::OnPaint()
{
    CPaintDC dcPaint(this);

    int lang = eng_lang ? LANG_EN : LANG_JP;

    // Double buffer the whole client area to avoid flicker
    CRect rcClient; GetClientRect(&rcClient);
    CMemDC mem(dcPaint, rcClient);
    CDC* pDC = &mem.GetDC();

    // Panel rect uses almost entire client area
    CRect panel(rcClient.left + 6, rcClient.top + 6, rcClient.right - 6, rcClient.bottom - 6);

    // Measure first (doDraw=false) to update content height, then draw with current scroll
    int fullH = MeasureAndDrawPanel(pDC, panel, m_bmdResult, m_scrollY, /*doDraw=*/true, lang);
    m_contentH = fullH; // store measured height for scrolling

    UpdateVScrollBar();
}

// --------------------- Resize -> recompute scroll page size ---------------------

void CManipulationDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);
    UpdateVScrollBar();
    Invalidate(FALSE); // repaint during resize (lower flicker)
}

// --------------------- Scrollbar + wheel handling ---------------------

void CManipulationDlg::UpdateVScrollBar()
{
    CRect rc; GetClientRect(&rc);
    int viewportH = max(0, rc.Height() - 12); // panel padding approx

    SCROLLINFO si{};
    si.cbSize = sizeof(si);
    si.fMask = SIF_PAGE | SIF_RANGE | SIF_POS;
    si.nMin = 0;
    si.nMax = max(0, m_contentH - 1);
    si.nPage = max(1, viewportH);
    si.nPos = min(max(0, m_scrollY), max(0, m_contentH - viewportH));

    SetScrollInfo(SB_VERT, &si, TRUE);
}

void CManipulationDlg::ClampScroll()
{
    CRect rc; GetClientRect(&rc);
    int viewportH = max(1, rc.Height() - 12);
    int maxPos = max(0, m_contentH - viewportH);
    if (m_scrollY < 0) m_scrollY = 0;
    if (m_scrollY > maxPos) m_scrollY = maxPos;
}

void CManipulationDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
    CRect rc; GetClientRect(&rc);
    int viewportH = max(1, rc.Height() - 12);

    switch (nSBCode)
    {
    case SB_LINEUP:       m_scrollY -= 20; break;
    case SB_LINEDOWN:     m_scrollY += 20; break;
    case SB_PAGEUP:       m_scrollY -= viewportH; break;
    case SB_PAGEDOWN:     m_scrollY += viewportH; break;
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
    {
        SCROLLINFO si{}; si.cbSize = sizeof(si); si.fMask = SIF_TRACKPOS;
        GetScrollInfo(SB_VERT, &si);
        m_scrollY = si.nTrackPos;
    }
    break;
    default: break;
    }

    ClampScroll();
    UpdateVScrollBar();
    Invalidate(FALSE);
    CDialogEx::OnVScroll(nSBCode, nPos, pScrollBar);
}

BOOL CManipulationDlg::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
    // Scroll by wheel (120 units per notch). Scale to sensible pixels.
    int delta = (zDelta > 0) ? -60 : 60;
    m_scrollY += delta;
    ClampScroll();
    UpdateVScrollBar();
    Invalidate(FALSE);
    return TRUE; // handled
}

// ============== Helper to run pipeline once ==============

void CManipulationDlg::RunBatteryManipulationCheck()
{
    BMD_BatteryInfo info;
    CString htmlPath;

    int lang = eng_lang ? LANG_EN : LANG_JP;

    if (!GenerateAndParseBatteryReport(info, &htmlPath, lang)) {
        AfxMessageBox(g_Texts[lang][MTK_GENPARSE_ERROR],
            MB_ICONERROR | MB_TOPMOST);
        m_bmdResult = BMD_DetectionResult{};
        m_bmdResult.status = g_Texts[lang][MTK_STATUS_GENUINE];
        m_scrollY = 0;
        m_contentH = 0;
        UpdateVScrollBar();
        return;
    }


    m_lastReportPath = htmlPath;
    m_bmdResult = RunManipulationChecks(info, lang, cycles);

    // Reset scroll to top on new result
    m_scrollY = 0;

    Invalidate(FALSE);
}
