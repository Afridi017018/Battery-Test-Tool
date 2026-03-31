#include "pch.h"
#include "CReportDlg.h"
#include "afxdialogex.h"
#include <afxcmn.h>
#include <regex>

IMPLEMENT_DYNAMIC(CReportDlg, CDialogEx)

CReportDlg::CReportDlg(CWnd* pParent)
	: CDialogEx(IDD_REPORT_DIALOG, pParent)
{
}

CReportDlg::~CReportDlg()
{
}

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

//////////////////////////////////////////////////////////////
// 🔹 PARSERS
//////////////////////////////////////////////////////////////

static bool GetUsageHistory(std::vector<CReportDlg::UsageHistoryRow>& outRows, const CString& html)
{
	std::wregex re(LR"(USAGE\s*HISTORY[\s\S]*?<table[^>]*>([\s\S]*?)</table>)", std::regex_constants::icase);
	std::wsmatch mh;
	std::wstring H = html.GetString();

	if (!std::regex_search(H, mh, re))
		return false;

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

		if (!headerSkipped)
		{
			headerSkipped = true;
			continue;
		}

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
			if (!headerSkipped)
			{
				headerSkipped = true;
				continue;
			}

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

		if (!headerSkipped)
		{
			headerSkipped = true;
			continue;
		}

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
// 🔹 MESSAGE MAP
//////////////////////////////////////////////////////////////

BEGIN_MESSAGE_MAP(CReportDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_VSCROLL()
	ON_WM_HSCROLL()
	ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////
// 🔹 INIT / SCROLL
//////////////////////////////////////////////////////////////

BOOL CReportDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	//ModifyStyle(0, WS_VSCROLL | WS_HSCROLL);

	wchar_t tempPath[MAX_PATH];
	GetTempPath(MAX_PATH, tempPath);

	CString path;
	path.Format(L"%sbattery_report.html", tempPath);

	m_htmlCache = ReadTextAutoEncoding(path);

	GetUsageHistory(m_rows, m_htmlCache);
	GetBatteryCapacity(m_capacity, m_htmlCache);
	GetBatteryLife(m_life, m_htmlCache);

	UpdateScrollBar();

	return TRUE;
}

void CReportDlg::UpdateScrollBar()
{
	CRect r;
	GetClientRect(&r);

	int totalHeight = (m_rows.size() + m_capacity.size() + m_life.size()) * m_rowHeight + 300;

	SCROLLINFO vsi = { sizeof(vsi), SIF_RANGE | SIF_PAGE };
	vsi.nMin = 0;
	vsi.nMax = totalHeight - 1;
	vsi.nPage = r.Height();
	SetScrollInfo(SB_VERT, &vsi, TRUE);

	int totalWidth = 320 + 180 * 4 + 100;

	SCROLLINFO hsi = { sizeof(hsi), SIF_RANGE | SIF_PAGE };
	hsi.nMin = 0;
	hsi.nMax = totalWidth - 1;
	hsi.nPage = r.Width();
	SetScrollInfo(SB_HORZ, &hsi, TRUE);
}

void CReportDlg::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar*)
{
	SCROLLINFO si = { sizeof(si), SIF_ALL };
	GetScrollInfo(SB_VERT, &si);

	int pos = si.nPos;

	switch (nSBCode)
	{
	case SB_LINEUP: pos -= m_rowHeight; break;
	case SB_LINEDOWN: pos += m_rowHeight; break;
	case SB_PAGEUP: pos -= si.nPage; break;
	case SB_PAGEDOWN: pos += si.nPage; break;
	case SB_THUMBTRACK: pos = si.nTrackPos; break;
	}

	pos = max(si.nMin, min(pos, si.nMax - (int)si.nPage + 1));

	m_scrollPos = pos;
	SetScrollPos(SB_VERT, pos);
	Invalidate();
}

void CReportDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar*)
{
	SCROLLINFO si = { sizeof(si), SIF_ALL };
	GetScrollInfo(SB_HORZ, &si);

	int pos = si.nPos;

	switch (nSBCode)
	{
	case SB_LINELEFT: pos -= 30; break;
	case SB_LINERIGHT: pos += 30; break;
	case SB_PAGELEFT: pos -= si.nPage; break;
	case SB_PAGERIGHT: pos += si.nPage; break;
	case SB_THUMBTRACK: pos = si.nTrackPos; break;
	}

	pos = max(si.nMin, min(pos, si.nMax - (int)si.nPage + 1));

	m_scrollX = pos;
	SetScrollPos(SB_HORZ, pos);
	Invalidate();
}

BOOL CReportDlg::OnMouseWheel(UINT, short zDelta, CPoint)
{
	SCROLLINFO si = { sizeof(si), SIF_ALL };
	GetScrollInfo(SB_VERT, &si);

	int pos = m_scrollPos;
	pos += (zDelta > 0 ? -1 : 1) * m_rowHeight * 3;

	pos = max(si.nMin, min(pos, si.nMax - (int)si.nPage + 1));

	m_scrollPos = pos;

	SetScrollPos(SB_VERT, pos);
	Invalidate();

	return TRUE;
}

//////////////////////////////////////////////////////////////
// 🔹 DRAW
//////////////////////////////////////////////////////////////

void CReportDlg::OnPaint()
{
	CPaintDC dc(this);

	// 🔥 BOTH directions applied
	dc.SetViewportOrg(-m_scrollX, -m_scrollPos);

	int x = 20, y = 20;
	int colPeriod = 320;
	int col = 180;

	dc.TextOut(x, y, L"USAGE HISTORY");
	y += m_rowHeight;

	dc.TextOut(x, y, L"Period");
	dc.TextOut(x + colPeriod, y, L"Battery Active");
	dc.TextOut(x + colPeriod + col, y, L"Battery Standby");
	dc.TextOut(x + colPeriod + col * 2, y, L"AC Active");
	dc.TextOut(x + colPeriod + col * 3, y, L"AC Standby");

	y += m_rowHeight;

	for (auto& r : m_rows)
	{
		CString a, b, c, d;
		a.Format(L"%.2f", r.battActiveHrs);
		b.Format(L"%.2f", r.battStandbyHrs);
		c.Format(L"%.2f", r.acActiveHrs);
		d.Format(L"%.2f", r.acStandbyHrs);

		dc.TextOut(x, y, r.period);
		dc.TextOut(x + colPeriod, y, a);
		dc.TextOut(x + colPeriod + col, y, b);
		dc.TextOut(x + colPeriod + col * 2, y, c);
		dc.TextOut(x + colPeriod + col * 3, y, d);

		y += m_rowHeight;
	}

	y += 40;

	dc.TextOut(x, y, L"BATTERY CAPACITY HISTORY");
	y += m_rowHeight;

	dc.TextOut(x, y, L"Period");
	dc.TextOut(x + colPeriod, y, L"Full Charge Capacity");
	dc.TextOut(x + colPeriod + col, y, L"Design Capacity");

	y += m_rowHeight;

	for (auto& r : m_capacity)
	{
		dc.TextOut(x, y, r.period);
		dc.TextOut(x + colPeriod, y, r.fullCharge);
		dc.TextOut(x + colPeriod + col, y, r.designCapacity);

		y += m_rowHeight;
	}

	y += 40;

	dc.TextOut(x, y, L"BATTERY LIFE ESTIMATES");
	y += m_rowHeight;

	dc.TextOut(x, y, L"Period");
	dc.TextOut(x + colPeriod, y, L"Design Active");
	dc.TextOut(x + colPeriod + col, y, L"Design Connected");
	dc.TextOut(x + colPeriod + col * 2, y, L"Full Active");
	dc.TextOut(x + colPeriod + col * 3, y, L"Full Connected");

	y += m_rowHeight;

	for (auto& r : m_life)
	{
		CString a, b, c, d;

		a.Format(L"%.2f", r.designActive);
		b.Format(L"%.2f", r.designConnected);
		c.Format(L"%.2f", r.fullActive);
		d.Format(L"%.2f", r.fullConnected);

		dc.TextOut(x, y, r.period);
		dc.TextOut(x + colPeriod, y, a);
		dc.TextOut(x + colPeriod + col, y, b);
		dc.TextOut(x + colPeriod + col * 2, y, c);
		dc.TextOut(x + colPeriod + col * 3, y, d);

		y += m_rowHeight;
	}
}