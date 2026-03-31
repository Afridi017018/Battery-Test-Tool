#include "pch.h"
#include "CAutoReportDlg.h"
#include <regex>

void CAutoReportDlg::DoDataExchange(CDataExchange* pDX) {
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAutoReportDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_VSCROLL()
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

// ✅ Use IDD (ensure enum exists in header OUTSIDE ifdef)
CAutoReportDlg::CAutoReportDlg(CWnd* pParent)
    : CDialogEx(IDD, pParent)
{
}

BOOL CAutoReportDlg::OnInitDialog() {
    CDialogEx::OnInitDialog();
    CreateGDIObjects();
    LoadData();
    return TRUE;
}

void CAutoReportDlg::CreateGDIObjects() {
    auto mkF = [&](CFont& f, int sz, int w) {
        f.CreateFont(sz, 0, 0, 0, w, 0, 0, 0, 0, 0, 0,
            CLEARTYPE_QUALITY, 0, L"Segoe UI");
        };

    mkF(m_fontSection, 18, FW_BOLD);
    mkF(m_fontColLabel, 12, FW_BOLD);
    mkF(m_fontRow, 13, FW_NORMAL);

    m_penDivider.CreatePen(PS_SOLID, 1, RGB(230, 230, 235));
    m_brushStripe.CreateSolidBrush(RGB(246, 247, 249));
    m_brushBg.CreateSolidBrush(RGB(255, 255, 255));
}

void CAutoReportDlg::LoadData() {
    m_usageRows.clear();
    m_capacityRows.clear();
    m_estimateRows.clear();

    FetchAllBatteryData();
    UpdateScrollBar();
}

void CAutoReportDlg::UpdateScrollBar() {
    CRect rc; GetClientRect(&rc);

    int totalH = 100 +
        (int)(m_usageRows.size() + m_capacityRows.size() + m_estimateRows.size()) * 26
        + 150;

    m_scrollMax = max(0, totalH - rc.Height());

    SCROLLINFO si{ sizeof(si), SIF_ALL, 0, totalH, (UINT)rc.Height(), m_scrollPos };
    SetScrollInfo(SB_VERT, &si, TRUE);
}

void CAutoReportDlg::OnPaint() {
    CPaintDC pdc(this);
    CRect rc; GetClientRect(&rc);

    CDC dc;
    CBitmap bmp;

    dc.CreateCompatibleDC(&pdc);
    bmp.CreateCompatibleBitmap(&pdc, rc.Width(), rc.Height());
    dc.SelectObject(&bmp);

    dc.FillRect(&rc, &m_brushBg);

    int curY = 20 - m_scrollPos;

    curY = DrawSection(dc, L"USAGE HISTORY", m_usageRows,
        { L"Period", L"Batt Act", L"Batt Std", L"AC Act", L"AC Std" },
        curY, RGB(15, 115, 85));

    curY += 40;

    curY = DrawSection(dc, L"CAPACITY HISTORY", m_capacityRows,
        { L"Period", L"Full Chg", L"Design" },
        curY, RGB(200, 100, 0));

    curY += 40;

    curY = DrawSection(dc, L"LIFE ESTIMATES", m_estimateRows,
        { L"Period", L"Life (Full)", L"Life (Design)" },
        curY, RGB(0, 102, 204));

    pdc.BitBlt(0, 0, rc.Width(), rc.Height(), &dc, 0, 0, SRCCOPY);
}

int CAutoReportDlg::DrawSection(CDC& dc, const CString& title,
    const std::vector<ReportRow>& rows,
    const std::vector<CString>& cols,
    int y, COLORREF clr)
{
    CRect rc; GetClientRect(&rc);

    dc.SelectObject(&m_fontSection);
    dc.SetTextColor(clr);
    dc.TextOut(16, y, title);
    y += 35;

    dc.SelectObject(&m_fontColLabel);
    dc.SetTextColor(RGB(120, 120, 120));

    int colW = (rc.Width() - 40) / (int)cols.size();

    for (int i = 0; i < (int)cols.size(); i++)
        dc.TextOut(16 + (i * colW), y, cols[i]);

    y += 25;

    dc.SelectObject(&m_fontRow);

    for (int i = 0; i < (int)rows.size(); i++) {
        if (y > -26 && y < rc.Height()) {

            if (i % 2 != 0)
                dc.FillRect(CRect(16, y, rc.Width() - 16, y + 26), &m_brushStripe);

            dc.SetTextColor(RGB(30, 30, 30));
            dc.TextOut(16, y + 4, rows[i].period);

            auto draw = [&](int c, double v) {
                if (v <= 0) return;
                CString s;
                s.Format(v > 500 ? L"%.0f" : L"%.2f", v);
                dc.TextOut(16 + (c * colW), y + 4, s);
                };

            draw(1, rows[i].val1);
            draw(2, rows[i].val2);
            draw(3, rows[i].val3);
            draw(4, rows[i].val4);
        }
        y += 26;
    }
    return y;
}

bool CAutoReportDlg::FetchAllBatteryData() {
    TCHAR tmp[MAX_PATH];
    GetTempPath(MAX_PATH, tmp);

    CString path;
    path.Format(L"%sbatt_rep.html", tmp);

    CString cmd;
    cmd.Format(L"powercfg /batteryreport /output \"%s\"", path);

    STARTUPINFO si{ sizeof(si) };
    PROCESS_INFORMATION pi{};

    if (CreateProcess(nullptr, cmd.GetBuffer(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {

        WaitForSingleObject(pi.hProcess, 8000);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    std::wstring html = ReadFile(path).GetString();
    if (html.empty()) return false;

    ParseUsageTable(html);
    ParseCapacityTable(html);
    ParseEstimatesTable(html);

    return true;
}

//
// ✅ FINAL FIXED USAGE PARSER
//
void CAutoReportDlg::ParseUsageTable(const std::wstring& html) {

    std::wregex re(LR"(USAGE\s*HISTORY[\s\S]*?<table[^>]*>([\s\S]*?)</table>)",
        std::regex_constants::icase);

    std::wsmatch m;
    if (!std::regex_search(html, m, re)) return;

    std::wstring table = m[1].str();

    std::wregex reRow(L"<tr[^>]*>([\\s\\S]*?)</tr>");

    for (std::wsregex_iterator it(table.begin(), table.end(), reRow), end; it != end; ++it)
    {
        std::wstring rowHtml = (*it)[1].str();

        CString rowText = StripHtml(rowHtml.c_str()).Trim();
        if (rowText.IsEmpty()) continue;

        CString upper = rowText;
        upper.MakeUpper();
        if (upper.Find(L"PERIOD") >= 0) continue;

        ReportRow r;

        // ✅ Extract FIRST CELL properly (full date or range)
        std::wregex reCell(L"<td[^>]*>([\\s\\S]*?)</td>");
        std::wsmatch cellMatch;

        if (std::regex_search(rowHtml, cellMatch, reCell)) {
            CString periodRaw = cellMatch[1].str().c_str();
            r.period = StripHtml(periodRaw).Trim();
        }
        else {
            r.period = rowText;
        }

        // ✅ Extract all time values safely
        auto hmsList = ExtractAllHms(rowHtml.c_str());

        if (hmsList.size() >= 1) r.val1 = HmsToHours(hmsList[0]);
        if (hmsList.size() >= 2) r.val2 = HmsToHours(hmsList[1]);
        if (hmsList.size() >= 3) r.val3 = HmsToHours(hmsList[2]);
        if (hmsList.size() >= 4) r.val4 = HmsToHours(hmsList[3]);

        m_usageRows.push_back(r);
    }
}

void CAutoReportDlg::ParseCapacityTable(const std::wstring& html) {
    std::wregex re(LR"(BATTERY\s*CAPACITY\s*HISTORY[\s\S]*?<table[^>]*>([\s\S]*?)</table>)",
        std::regex_constants::icase);

    std::wsmatch m;
    if (!std::regex_search(html, m, re)) return;

    std::wstring table = m[1].str();

    std::wregex reRow(L"<tr[^>]*>([\\s\\S]*?)</tr>");
    std::wregex reVal(L"(\\d{1,3}(?:,\\d{3})*)\\s*mWh");

    for (std::wsregex_iterator it(table.begin(), table.end(), reRow), end; it != end; ++it) {

        std::wstring rHtml = (*it)[1].str();

        CString period = StripHtml(rHtml.substr(0, rHtml.find(L"</td>")).c_str()).Trim();
        if (period.IsEmpty() || period.MakeUpper().Find(L"PERIOD") >= 0) continue;

        ReportRow r;
        r.period = period;

        int count = 0;
        for (std::wsregex_iterator iv(rHtml.begin(), rHtml.end(), reVal); iv != end; ++iv) {
            if (count == 0) r.val1 = ParseMwh((*iv)[1].str().c_str());
            else if (count == 1) r.val2 = ParseMwh((*iv)[1].str().c_str());
            count++;
        }

        m_capacityRows.push_back(r);
    }
}

void CAutoReportDlg::ParseEstimatesTable(const std::wstring& html) {
    std::wregex re(LR"(BATTERY\s*LIFE\s*ESTIMATES[\s\S]*?<table[^>]*>([\s\S]*?)</table>)",
        std::regex_constants::icase);

    std::wsmatch m;
    if (!std::regex_search(html, m, re)) return;

    std::wstring table = m[1].str();
    std::wregex reRow(L"<tr[^>]*>([\\s\\S]*?)</tr>");

    for (std::wsregex_iterator it(table.begin(), table.end(), reRow), end; it != end; ++it) {

        std::wstring rHtml = (*it)[1].str();

        CString period = StripHtml(rHtml.substr(0, rHtml.find(L"</td>")).c_str()).Trim();
        if (period.IsEmpty() || period.MakeUpper().Find(L"PERIOD") >= 0) continue;

        auto hmsList = ExtractAllHms(rHtml.c_str());

        if (hmsList.size() >= 2) {
            ReportRow r;
            r.period = period;
            r.val1 = HmsToHours(hmsList[0]);
            r.val2 = HmsToHours(hmsList[1]);
            m_estimateRows.push_back(r);
        }
    }
}

double CAutoReportDlg::ParseMwh(CString text) {
    text.Replace(L",", L"");
    return _wtof(text);
}

CString CAutoReportDlg::StripHtml(const CString& input) {
    return CString(std::regex_replace(std::wstring(input),
        std::wregex(L"<[^>]+>"), L" ").c_str());
}

std::vector<CString> CAutoReportDlg::ExtractAllHms(const CString& t) {
    std::vector<CString> res;

    std::wstring s(t);
    std::wregex re(L"(\\d+):(\\d{2}):(\\d{2})");

    for (std::wsregex_iterator it(s.begin(), s.end(), re), end; it != end; ++it)
        res.push_back((*it)[0].str().c_str());

    return res;
}

double CAutoReportDlg::HmsToHours(const CString& hms) {
    int h = 0, m = 0, s = 0;
    swscanf_s(hms, L"%d:%d:%d", &h, &m, &s);
    return h + (m / 60.0) + (s / 3600.0);
}

CString CAutoReportDlg::ReadFile(const CString& path) {
    CFile f;
    if (!f.Open(path, CFile::modeRead)) return L"";

    UINT len = (UINT)f.GetLength();
    std::vector<char> b(len + 1, 0);
    f.Read(b.data(), len);

    return CString(CA2W(b.data(), CP_UTF8));
}

void CAutoReportDlg::OnSize(UINT n, int cx, int cy) {
    CDialogEx::OnSize(n, cx, cy);
    UpdateScrollBar();
    Invalidate();
}

void CAutoReportDlg::OnVScroll(UINT code, UINT pos, CScrollBar* p) {
    if (code == SB_THUMBTRACK) m_scrollPos = pos;
    else if (code == SB_LINEUP) m_scrollPos -= 20;
    else if (code == SB_LINEDOWN) m_scrollPos += 20;

    m_scrollPos = max(0, min(m_scrollPos, m_scrollMax));

    SetScrollPos(SB_VERT, m_scrollPos);
    Invalidate(FALSE);
}

BOOL CAutoReportDlg::OnMouseWheel(UINT f, short z, CPoint p) {
    m_scrollPos -= (z / 120) * 60;
    m_scrollPos = max(0, min(m_scrollPos, m_scrollMax));

    SetScrollPos(SB_VERT, m_scrollPos);
    Invalidate(FALSE);

    return TRUE;
}

BOOL CAutoReportDlg::OnEraseBkgnd(CDC* p) {
    return TRUE;
}