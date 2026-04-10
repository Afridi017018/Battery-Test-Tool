#include "pch.h"
#include "CReportDlg.h"
#include "afxdialogex.h"
#include <afxcmn.h>
#include <regex>
#include <vector>
#include <utility>
#include <shellapi.h>   // ShellExecute  (for opening the HTML / PDF)
#include <winspool.h>   // OpenPrinter, GetPrinter, DocumentProperties
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "winspool.lib")

IMPLEMENT_DYNAMIC(CReportDlg, CDialogEx)

CReportDlg::CReportDlg(const BatteryReportData& data, CWnd* pParent)
	: CDialogEx(IDD_REPORT_DIALOG, pParent),
	m_reportData(data)
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
	ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

//////////////////////////////////////////////////////////////
// 🔹 INIT / SCROLL
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
	GetBatteryLife(m_life, m_htmlCache);

	m_basicInfoHeight = m_rowHeight + 15 * m_rowHeight + 20;

	UpdateScrollBar();

	AfxMessageBox(m_reportData.dischargeResult);

	return TRUE;
}

void CReportDlg::UpdateScrollBar()
{
	CRect r;
	GetClientRect(&r);

	int totalHeight = m_basicInfoHeight
		+ 7 * m_rowHeight
		+ 6 * m_rowHeight
		+ (m_rows.size() + m_capacity.size() + m_life.size()) * m_rowHeight
		+ 500;

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
	case SB_LINEUP:     pos -= m_rowHeight; break;
	case SB_LINEDOWN:   pos += m_rowHeight; break;
	case SB_PAGEUP:     pos -= si.nPage;    break;
	case SB_PAGEDOWN:   pos += si.nPage;    break;
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
	case SB_LINELEFT:   pos -= 30;       break;
	case SB_LINERIGHT:  pos += 30;       break;
	case SB_PAGELEFT:   pos -= si.nPage; break;
	case SB_PAGERIGHT:  pos += si.nPage; break;
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

	// ── Step 1: Set viewport for scrolled content ──────────────────────────
	dc.SetViewportOrg(-m_scrollX, -m_scrollPos);

	int x = 20, y = 20;
	int colPeriod = 320;
	int col = 180;

	//----------------------------------------------------------
	// 🔹 FONTS
	//----------------------------------------------------------

	CFont fontBold;
	fontBold.CreateFont(
		18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
	);

	CFont fontNormal;
	fontNormal.CreateFont(
		16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
	);

	int labelCol = 220;
	CFont* pOldFont = nullptr;

	// Set default text appearance for scrolled content
	dc.SetBkMode(TRANSPARENT);
	dc.SetTextColor(RGB(0, 0, 0));

	//----------------------------------------------------------
	// 🔹 BASIC BATTERY INFO
	//----------------------------------------------------------

	pOldFont = dc.SelectObject(&fontBold);
	dc.TextOut(x, y, L"BATTERY INFO");
	dc.SelectObject(pOldFont);
	y += m_rowHeight + 5;

	auto DrawInfoRow = [&](const wchar_t* label, const CString& value)
		{
			pOldFont = dc.SelectObject(&fontBold);
			dc.TextOut(x, y, label);
			dc.SelectObject(&fontNormal);
			dc.TextOut(x + labelCol, y, value);
			dc.SelectObject(pOldFont);
			y += m_rowHeight;
		};

	DrawInfoRow(L"UUID:", m_reportData.uuid);
	DrawInfoRow(L"Manufacturer:", m_reportData.bid);
	DrawInfoRow(L"Name:", m_reportData.name);
	DrawInfoRow(L"Status:", m_reportData.status);
	DrawInfoRow(L"Percentage:", m_reportData.percentage);
	DrawInfoRow(L"Time Remaining:", m_reportData.remainingTime);


	DrawInfoRow(L"Voltage:", m_reportData.voltage);
	DrawInfoRow(L"Temperature:", m_reportData.temperature);
	DrawInfoRow(L"Design Capacity:", m_reportData.designCapacity + L"");
	DrawInfoRow(L"Full Charge Cap.:", m_reportData.fullChargeCapacity + L"");
	DrawInfoRow(L"Current Capacity:", m_reportData.currentCapacity + L"");
	DrawInfoRow(L"Cycles:", m_reportData.cycles);
	DrawInfoRow(L"Health:", m_reportData.health);




	/*DrawInfoRow(L"Discharge Result:", m_reportData.dischargeResult);
	DrawInfoRow(L"CPU Load Result:", m_reportData.cpuLoadResult);*/

	dc.SelectObject(pOldFont);
	y += 20;

	//----------------------------------------------------------
	// 🔹 CPU LOAD TEST RESULTS
	//----------------------------------------------------------

	auto DrawResultRow = [&](const wchar_t* label, const CString& value)
		{
			pOldFont = dc.SelectObject(&fontBold);
			dc.TextOut(x, y, label);
			dc.SelectObject(&fontNormal);
			dc.TextOut(x + colPeriod, y, value);
			dc.SelectObject(pOldFont);
			y += m_rowHeight;
		};

	pOldFont = dc.SelectObject(&fontBold);
	dc.TextOut(x, y, L"CPU LOAD TEST RESULTS (3 min)");
	y += m_rowHeight;

	dc.TextOut(x, y, L"Metric");
	dc.TextOut(x + colPeriod, y, L"Value");
	dc.SelectObject(pOldFont);
	y += m_rowHeight;

	CString val;

	val.Format(L"%d%%", m_reportData.cpuInitial);
	DrawResultRow(L"Initial Charge:", val);

	val.Format(L"%d%%", m_reportData.cpuCurrent);
	DrawResultRow(L"Current Charge:", val);

	val.Format(L"%d%%", m_reportData.cpuDrop);
	DrawResultRow(L"Drop:", val);

	val.Format(L"%.2f%% / min", m_reportData.cpuRate);
	DrawResultRow(L"Rate:", val);

	y += 40;

	//----------------------------------------------------------
	// 🔹 DISCHARGE TEST RESULTS
	//----------------------------------------------------------

	pOldFont = dc.SelectObject(&fontBold);
	dc.TextOut(x, y, L"DISCHARGE TEST RESULTS (5 min)");
	y += m_rowHeight;

	dc.TextOut(x, y, L"Metric");
	dc.TextOut(x + colPeriod, y, L"Value");
	dc.SelectObject(pOldFont);
	y += m_rowHeight;

	val.Format(L"%d%%", m_reportData.disInitial);
	DrawResultRow(L"Initial Charge:", val);

	val.Format(L"%d%%", m_reportData.disFinal);
	DrawResultRow(L"Final Charge:", val);

	val.Format(L"%d%%", m_reportData.disDrop);
	DrawResultRow(L"Drop:", val);

	val.Format(L"%.2f%% / min", m_reportData.disRate);
	DrawResultRow(L"Drain Rate:", val);

	y += 40;

	//----------------------------------------------------------
	// 🔹 USAGE HISTORY
	//----------------------------------------------------------

	pOldFont = dc.SelectObject(&fontBold);
	dc.TextOut(x, y, L"USAGE HISTORY");
	y += m_rowHeight;

	dc.TextOut(x, y, L"Period");
	dc.TextOut(x + colPeriod, y, L"Battery Active");
	dc.TextOut(x + colPeriod + col, y, L"Battery Standby");
	dc.TextOut(x + colPeriod + col * 2, y, L"AC Active");
	dc.TextOut(x + colPeriod + col * 3, y, L"AC Standby");
	dc.SelectObject(pOldFont);
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

	//----------------------------------------------------------
	// 🔹 BATTERY CAPACITY HISTORY
	//----------------------------------------------------------

	pOldFont = dc.SelectObject(&fontBold);
	dc.TextOut(x, y, L"BATTERY CAPACITY HISTORY");
	y += m_rowHeight;

	dc.TextOut(x, y, L"Period");
	dc.TextOut(x + colPeriod, y, L"Full Charge Capacity");
	dc.TextOut(x + colPeriod + col, y, L"Design Capacity");
	dc.SelectObject(pOldFont);
	y += m_rowHeight;

	for (auto& r : m_capacity)
	{
		dc.TextOut(x, y, r.period);
		dc.TextOut(x + colPeriod, y, r.fullCharge);
		dc.TextOut(x + colPeriod + col, y, r.designCapacity);

		y += m_rowHeight;
	}

	y += 40;

	//----------------------------------------------------------
	// 🔹 BATTERY LIFE ESTIMATES
	//----------------------------------------------------------

	pOldFont = dc.SelectObject(&fontBold);
	dc.TextOut(x, y, L"BATTERY LIFE ESTIMATES");
	y += m_rowHeight;

	dc.TextOut(x, y, L"Period");
	dc.TextOut(x + colPeriod, y, L"Design Active");
	dc.TextOut(x + colPeriod + col, y, L"Design Connected");
	dc.TextOut(x + colPeriod + col * 2, y, L"Full Active");
	dc.TextOut(x + colPeriod + col * 3, y, L"Full Connected");
	dc.SelectObject(pOldFont);
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

	// ── Step 2: Reset viewport to (0,0) so the button is fixed / unaffected
	//            by scroll. Draw it LAST so it always paints on top.
	// ─────────────────────────────────────────────────────────────────────
	dc.SetViewportOrg(0, 0);

	{
		CRect clientRect;
		GetClientRect(&clientRect);

		const int BW = 130, BH = 28, BM = 8;
		m_exportBtnRect = CRect(
			clientRect.right - BW - BM,
			BM,
			clientRect.right - BM,
			BM + BH
		);

		// ── Erase the background behind the button so no scrolled text bleeds through
		CBrush bgBrush(::GetSysColor(COLOR_BTNFACE));
		dc.FillRect(&m_exportBtnRect, &bgBrush);

		// ── Button background (blue rounded rect)
		COLORREF btnColor = RGB(0, 120, 215);
		CBrush btnBrush(btnColor);
		CPen   btnPen(PS_SOLID, 1, RGB(0, 90, 180));
		CBrush* pOldBrush = dc.SelectObject(&btnBrush);
		CPen* pOldPen = dc.SelectObject(&btnPen);
		dc.RoundRect(m_exportBtnRect, CPoint(6, 6));
		dc.SelectObject(pOldBrush);
		dc.SelectObject(pOldPen);

		// ── Button label — must set text color and bkmode AFTER resetting viewport
		CFont btnFont;
		btnFont.CreateFont(
			15, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
			DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
			DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
		);
		CFont* pOldBtnFont = dc.SelectObject(&btnFont);
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(RGB(255, 255, 255));
		dc.DrawText(L"Export Report", m_exportBtnRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		dc.SelectObject(pOldBtnFont);
	}
}

//////////////////////////////////////////////////////////////
// 🔹 Painted-button click handler
//////////////////////////////////////////////////////////////

void CReportDlg::OnLButtonUp(UINT nFlags, CPoint point)
{
	// m_exportBtnRect is always in client (fixed) coordinates — no scroll adjustment needed
	if (m_exportBtnRect.PtInRect(point))
	{
		int choice = AfxMessageBox(
			L"Choose export format:\n\nYes  → HTML\nNo   → PDF",
			MB_YESNOCANCEL | MB_ICONQUESTION
		);

		if (choice == IDYES)
		{
			ExportHtmlReport();
		}
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
	CString html;

	auto TR2 = [](const wchar_t* label, const CString& val) -> CString
		{
			CString s;
			s.Format(L"<tr><td class='lbl'>%s</td><td>%s</td></tr>\n", label, val.GetString());
			return s;
		};

	auto TH = [](const wchar_t* text) -> CString
		{
			CString s;
			s.Format(L"<th>%s</th>", text);
			return s;
		};

	html =
		L"<!DOCTYPE html>\n<html lang='en'>\n<head>\n"
		L"<meta charset='UTF-8'/>\n"
		L"<title>Battery Report</title>\n"
		L"<style>\n"
		L"  body{font-family:Segoe UI,Arial,sans-serif;font-size:13px;margin:30px;color:#222;}\n"
		L"  h2{color:#0078d4;border-bottom:2px solid #0078d4;padding-bottom:4px;margin-top:32px;}\n"
		L"  table{border-collapse:collapse;width:100%;margin-bottom:20px;}\n"
		L"  th{background:#0078d4;color:#fff;padding:6px 10px;text-align:left;}\n"
		L"  td{padding:5px 10px;border-bottom:1px solid #ddd;}\n"
		L"  tr:nth-child(even) td{background:#f4f8ff;}\n"
		L"  td.lbl{font-weight:bold;width:220px;}\n"
		L"  @media print{body{margin:15px;}}\n"
		L"</style>\n</head>\n<body>\n"
		L"<h1 style='color:#0078d4;'>Battery Report</h1>\n";



	html += L"<h2>Battery Info</h2>\n<table>\n";
	html += TR2(L"UUID", m_reportData.uuid);
	html += TR2(L"Manufacturer", m_reportData.bid);
	html += TR2(L"Name", m_reportData.name);
	html += TR2(L"Status", m_reportData.status);
	html += TR2(L"Percentage", m_reportData.percentage);
	html += TR2(L"Time Remaining", m_reportData.remainingTime);

	html += TR2(L"Voltage", m_reportData.voltage);
	html += TR2(L"Temperature", m_reportData.temperature);
	html += TR2(L"Design Capacity", m_reportData.designCapacity + L"");
	html += TR2(L"Full Charge Cap.", m_reportData.fullChargeCapacity + L"");
	html += TR2(L"Current Capacity", m_reportData.currentCapacity + L"");
	html += TR2(L"Cycles", m_reportData.cycles);
	html += TR2(L"Health", m_reportData.health);


	/*html += TR2(L"Discharge Result", m_reportData.dischargeResult);
	html += TR2(L"CPU Load Result", m_reportData.cpuLoadResult);*/
	html += L"</table>\n";

	html += L"<h2>CPU Load Test Results (3 min)</h2>\n<table>\n";
	html += TH(L"Metric"); html += TH(L"Value"); html += L"\n";

	CString val;
	val.Format(L"%d%%", m_reportData.cpuInitial);  html += TR2(L"Initial Charge", val);
	val.Format(L"%d%%", m_reportData.cpuCurrent);  html += TR2(L"Current Charge", val);
	val.Format(L"%d%%", m_reportData.cpuDrop);     html += TR2(L"Drop", val);
	val.Format(L"%.2f%% / min", m_reportData.cpuRate); html += TR2(L"Rate", val);
	html += L"</table>\n";

	html += L"<h2>Discharge Test Results (5 min)</h2>\n<table>\n";
	html += TH(L"Metric"); html += TH(L"Value"); html += L"\n";

	val.Format(L"%d%%", m_reportData.disInitial);       html += TR2(L"Initial Charge", val);
	val.Format(L"%d%%", m_reportData.disFinal);         html += TR2(L"Final Charge", val);
	val.Format(L"%d%%", m_reportData.disDrop);          html += TR2(L"Drop", val);
	val.Format(L"%.2f%% / min", m_reportData.disRate);  html += TR2(L"Drain Rate", val);
	html += L"</table>\n";

	html += L"<h2>Usage History</h2>\n<table>\n<tr>";
	html += TH(L"Period");
	html += TH(L"Battery Active (h)");
	html += TH(L"Battery Standby (h)");
	html += TH(L"AC Active (h)");
	html += TH(L"AC Standby (h)");
	html += L"</tr>\n";
	for (const auto& r : m_rows)
	{
		CString a, b, c, d;
		a.Format(L"%.2f", r.battActiveHrs);
		b.Format(L"%.2f", r.battStandbyHrs);
		c.Format(L"%.2f", r.acActiveHrs);
		d.Format(L"%.2f", r.acStandbyHrs);
		CString row;
		row.Format(L"<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
			r.period.GetString(), a.GetString(), b.GetString(), c.GetString(), d.GetString());
		html += row;
	}
	html += L"</table>\n";

	html += L"<h2>Battery Capacity History</h2>\n<table>\n<tr>";
	html += TH(L"Period");
	html += TH(L"Full Charge Capacity");
	html += TH(L"Design Capacity");
	html += L"</tr>\n";
	for (const auto& r : m_capacity)
	{
		CString row;
		row.Format(L"<tr><td>%s</td><td>%s</td><td>%s</td></tr>\n",
			r.period.GetString(), r.fullCharge.GetString(), r.designCapacity.GetString());
		html += row;
	}
	html += L"</table>\n";

	html += L"<h2>Battery Life Estimates</h2>\n<table>\n<tr>";
	html += TH(L"Period");
	html += TH(L"Design Active (h)");
	html += TH(L"Design Connected (h)");
	html += TH(L"Full Active (h)");
	html += TH(L"Full Connected (h)");
	html += L"</tr>\n";
	for (const auto& r : m_life)
	{
		CString a, b, c, d;
		a.Format(L"%.2f", r.designActive);
		b.Format(L"%.2f", r.designConnected);
		c.Format(L"%.2f", r.fullActive);
		d.Format(L"%.2f", r.fullConnected);
		CString row;
		row.Format(L"<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
			r.period.GetString(), a.GetString(), b.GetString(), c.GetString(), d.GetString());
		html += row;
	}
	html += L"</table>\n</body>\n</html>\n";

	return html;
}

//////////////////////////////////////////////////////////////
// 🔹 ExportHtmlReport  –  saves HTML and opens in browser
//////////////////////////////////////////////////////////////

void CReportDlg::ExportHtmlReport(const CString& destPath) const
{
	CString savePath = destPath;

	if (savePath.IsEmpty())
	{
		wchar_t filter[] = L"HTML Files (*.html)\0*.html\0All Files (*.*)\0*.*\0\0";
		wchar_t szFile[MAX_PATH] = L"battery_report.html";

		OPENFILENAME ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = GetSafeHwnd();
		ofn.lpstrFilter = filter;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = MAX_PATH;
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
		ofn.lpstrDefExt = L"html";

		if (!GetSaveFileName(&ofn))
			return;

		savePath = szFile;
	}

	CString htmlW = BuildHtmlReport();

	int bytes = WideCharToMultiByte(CP_UTF8, 0, htmlW, -1, nullptr, 0, nullptr, nullptr);
	std::string utf8(bytes, '\0');
	WideCharToMultiByte(CP_UTF8, 0, htmlW, -1, &utf8[0], bytes, nullptr, nullptr);

	CStdioFile f;
	if (f.Open(savePath, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary))
	{
		f.Write(utf8.c_str(), (UINT)utf8.size() - 1);
		f.Close();

		ShellExecute(nullptr, L"open", savePath, nullptr, nullptr, SW_SHOWNORMAL);
	}
	else
	{
		AfxMessageBox(L"Could not write HTML file.", MB_ICONERROR);
	}
}

//////////////////////////////////////////////////////////////
// 🔹 HELPER: enumerate local printers, return first name that
//            contains all given keywords (case-insensitive).
//////////////////////////////////////////////////////////////

static CString FindPrinterByKeywords(std::initializer_list<const wchar_t*> keywords)
{
	DWORD needed = 0, returned = 0;
	EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
		nullptr, 2, nullptr, 0, &needed, &returned);

	if (needed == 0) return L"";

	std::vector<BYTE> buf(needed);
	if (!EnumPrintersW(PRINTER_ENUM_LOCAL | PRINTER_ENUM_CONNECTIONS,
		nullptr, 2, buf.data(), needed, &needed, &returned))
		return L"";

	PRINTER_INFO_2W* pi = reinterpret_cast<PRINTER_INFO_2W*>(buf.data());

	for (DWORD i = 0; i < returned; ++i)
	{
		CString nameUpper(pi[i].pPrinterName);
		nameUpper.MakeUpper();

		bool allMatch = true;
		for (const wchar_t* kw : keywords)
		{
			CString kwU(kw);
			kwU.MakeUpper();
			if (nameUpper.Find(kwU) < 0) { allMatch = false; break; }
		}
		if (allMatch)
			return CString(pi[i].pPrinterName);
	}
	return L"";
}

//////////////////////////////////////////////////////////////
// 🔹 HELPER: shared GDI rendering — styled to match HTML design
//
//  HTML design replicated:
//   • Blue (#0078D4) H1 title "Battery Report"
//   • Blue H2 section headings with 2px blue underline
//   • Tables: blue filled header row (white bold text)
//   • Data rows: alternating white / #F4F8FF background
//   • 1px #DDDDDD bottom border on every data row
//   • Bold label column in info/result tables
//   • Text padding matches HTML (6px header, 5px data)
//////////////////////////////////////////////////////////////

void CReportDlg::RenderReportToDC(HDC hDC, int pageW, int pageH,
	int marginX, int marginY, float scaleX, float scaleY) const
{
	const COLORREF clrBlue = RGB(0, 120, 212);
	const COLORREF clrWhite = RGB(255, 255, 255);
	const COLORREF clrBlack = RGB(0, 0, 0);
	const COLORREF clrDark = RGB(34, 34, 34);
	const COLORREF clrAltRow = RGB(244, 248, 255);
	const COLORREF clrBorder = RGB(221, 221, 221);

	int rowH = (int)(22 * scaleY);
	int hdrH = (int)(26 * scaleY);
	int padX = (int)(10 * scaleX);
	int padY = (int)(5 * scaleY);
	int padYHdr = (int)(6 * scaleY);
	int h2Gap = (int)(32 * scaleY);
	int h2Under = (int)(2 * scaleY);

	int tableW = pageW - 2 * marginX;

	// --- Columns for 2-col and 3-col tables (unchanged) ---
	int col1 = (int)(220 * scaleX);
	int colPeriod = (int)(320 * scaleX);
	int col = (int)(180 * scaleX);

	// --- Narrower columns ONLY for 5-column wide tables ---
	int colPeriod5 = (int)(200 * scaleX);
	int col5 = (int)(120 * scaleX);

	int fontBody = -(int)(13 * scaleY);
	int fontH1 = -(int)(22 * scaleY);
	int fontH2 = -(int)(15 * scaleY);

	HFONT hFontBody = CreateFontW(fontBody, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	HFONT hFontBold = CreateFontW(fontBody, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	HFONT hFontH1 = CreateFontW(fontH1, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
	HFONT hFontH2 = CreateFontW(fontH2, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");

	HBRUSH hBrBlue = CreateSolidBrush(clrBlue);
	HBRUSH hBrAlt = CreateSolidBrush(clrAltRow);
	HBRUSH hBrWhite = CreateSolidBrush(clrWhite);
	HPEN   hPenBorder = CreatePen(PS_SOLID, max(1, (int)scaleY), clrBorder);
	HPEN   hPenBlue = CreatePen(PS_SOLID, h2Under, clrBlue);
	HPEN   hPenNull = (HPEN)GetStockObject(NULL_PEN);

	SetBkMode(hDC, TRANSPARENT);

	int x = marginX;
	int y = marginY;

	bool onPrinter = (pageH > 0);

	auto CheckBreak = [&](int neededH)
		{
			if (onPrinter && pageH > 0 && y + neededH > pageH - marginY)
			{
				EndPage(hDC);
				StartPage(hDC);
				y = marginY;
				SetBkMode(hDC, TRANSPARENT);
			}
		};

	auto FillRect2 = [&](int rx, int ry, int rw, int rh, HBRUSH br)
		{
			RECT rc = { rx, ry, rx + rw, ry + rh };
			FillRect(hDC, &rc, br);
		};

	auto DrawBottomBorder = [&](int rx, int ry, int rw, int rh)
		{
			HPEN old = (HPEN)SelectObject(hDC, hPenBorder);
			MoveToEx(hDC, rx, ry + rh - 1, nullptr);
			LineTo(hDC, rx + rw, ry + rh - 1);
			SelectObject(hDC, old);
		};

	auto CellText = [&](int cx, int cy, int cw, int ch,
		const wchar_t* txt, HFONT fnt, COLORREF clr, int pxOff = 0)
		{
			SelectObject(hDC, fnt);
			SetTextColor(hDC, clr);
			RECT rc = { cx + padX + pxOff, cy + padY, cx + cw, cy + ch };
			DrawTextW(hDC, txt, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
		};

	auto CellTextHdr = [&](int cx, int cy, int cw, int ch,
		const wchar_t* txt, HFONT fnt, COLORREF clr)
		{
			SelectObject(hDC, fnt);
			SetTextColor(hDC, clr);
			RECT rc = { cx + padX, cy + padYHdr, cx + cw, cy + ch };
			DrawTextW(hDC, txt, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
		};

	// ── H1 ─────────────────────────────────────────────────────────────
	{
		int h1H = (int)(30 * scaleY);
		CheckBreak(h1H);
		SelectObject(hDC, hFontH1);
		SetTextColor(hDC, clrBlue);
		RECT rc = { x, y, x + tableW, y + h1H };
		DrawTextW(hDC, L"Battery Report", -1, &rc,
			DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
		y += h1H + (int)(10 * scaleY);
	}

	struct ColDef { const wchar_t* hdr; int xOff; int w; };

	auto DrawH2 = [&](const wchar_t* title)
		{
			int h2H = (int)(20 * scaleY);
			CheckBreak(h2H + h2Under + (int)(6 * scaleY));
			y += (int)(h2Gap * 0.5f);

			SelectObject(hDC, hFontH2);
			SetTextColor(hDC, clrBlue);
			RECT rc = { x, y, x + tableW, y + h2H };
			DrawTextW(hDC, title, -1, &rc, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_NOPREFIX);
			y += h2H + (int)(4 * scaleY);

			HPEN old = (HPEN)SelectObject(hDC, hPenBlue);
			MoveToEx(hDC, x, y, nullptr);
			LineTo(hDC, x + tableW, y);
			SelectObject(hDC, old);
			y += h2Under + (int)(6 * scaleY);
		};

	auto DrawTableHeader = [&](const std::vector<ColDef>& cols)
		{
			CheckBreak(hdrH);
			FillRect2(x, y, tableW, hdrH, hBrBlue);
			for (const auto& c : cols)
				CellTextHdr(x + c.xOff, y, c.w, hdrH, c.hdr, hFontBold, clrWhite);
			y += hdrH;
		};

	int dataRowIdx = 0;
	auto DrawDataRow = [&](const std::vector<std::pair<int, CString>>& cells,
		bool boldFirst = false)
		{
			CheckBreak(rowH);
			HBRUSH bg = (dataRowIdx % 2 == 1) ? hBrAlt : hBrWhite;
			FillRect2(x, y, tableW, rowH, bg);
			DrawBottomBorder(x, y, tableW, rowH);

			for (int i = 0; i < (int)cells.size(); ++i)
			{
				HFONT f = (i == 0 && boldFirst) ? hFontBold : hFontBody;
				CellText(x + cells[i].first, y,
					(i + 1 < (int)cells.size()) ? (cells[i + 1].first - cells[i].first) : (tableW - cells[i].first),
					rowH, cells[i].second.GetString(), f, clrDark);
			}
			++dataRowIdx;
			y += rowH;
		};

	// ══════════════════════════════════════════════════════════════════
	// SECTION 1: Battery Info
	// ══════════════════════════════════════════════════════════════════
	DrawH2(L"Battery Info");
	{
		std::vector<ColDef> hdrs = {
			{ L"Property", 0,    col1 },
			{ L"Value",    col1, tableW - col1 }
		};
		DrawTableHeader(hdrs);
		dataRowIdx = 0;

		auto InfoRow = [&](const wchar_t* lbl, const CString& val)
			{
				DrawDataRow({ {0, CString(lbl)}, {col1, val} }, true);
			};

		InfoRow(L"UUID", m_reportData.uuid);
		InfoRow(L"Manufacturer", m_reportData.bid);
		InfoRow(L"Name", m_reportData.name);
		InfoRow(L"Status", m_reportData.status);
		InfoRow(L"Percentage", m_reportData.percentage);
		InfoRow(L"Time Remaining", m_reportData.remainingTime);
		InfoRow(L"Voltage", m_reportData.voltage);
		InfoRow(L"Temperature", m_reportData.temperature);
		InfoRow(L"Design Capacity", m_reportData.designCapacity);
		InfoRow(L"Full Charge Cap.", m_reportData.fullChargeCapacity);
		InfoRow(L"Current Capacity", m_reportData.currentCapacity);
		InfoRow(L"Cycles", m_reportData.cycles);
		InfoRow(L"Health", m_reportData.health);
	}

	// ══════════════════════════════════════════════════════════════════
	// SECTION 2: CPU Load Test
	// ══════════════════════════════════════════════════════════════════
	DrawH2(L"CPU Load Test Results (3 min)");
	{
		std::vector<ColDef> hdrs = {
			{ L"Metric", 0,         colPeriod },
			{ L"Value",  colPeriod, tableW - colPeriod }
		};
		DrawTableHeader(hdrs);
		dataRowIdx = 0;

		CString val;
		val.Format(L"%d%%", m_reportData.cpuInitial);
		DrawDataRow({ {0, L"Initial Charge"}, {colPeriod, val} }, true);
		val.Format(L"%d%%", m_reportData.cpuCurrent);
		DrawDataRow({ {0, L"Current Charge"}, {colPeriod, val} }, true);
		val.Format(L"%d%%", m_reportData.cpuDrop);
		DrawDataRow({ {0, L"Drop"          }, {colPeriod, val} }, true);
		val.Format(L"%.2f%% / min", m_reportData.cpuRate);
		DrawDataRow({ {0, L"Rate"          }, {colPeriod, val} }, true);
	}

	// ══════════════════════════════════════════════════════════════════
	// SECTION 3: Discharge Test
	// ══════════════════════════════════════════════════════════════════
	DrawH2(L"Discharge Test Results (5 min)");
	{
		std::vector<ColDef> hdrs = {
			{ L"Metric", 0,         colPeriod },
			{ L"Value",  colPeriod, tableW - colPeriod }
		};
		DrawTableHeader(hdrs);
		dataRowIdx = 0;

		CString val;
		val.Format(L"%d%%", m_reportData.disInitial);
		DrawDataRow({ {0, L"Initial Charge"}, {colPeriod, val} }, true);
		val.Format(L"%d%%", m_reportData.disFinal);
		DrawDataRow({ {0, L"Final Charge"  }, {colPeriod, val} }, true);
		val.Format(L"%d%%", m_reportData.disDrop);
		DrawDataRow({ {0, L"Drop"          }, {colPeriod, val} }, true);
		val.Format(L"%.2f%% / min", m_reportData.disRate);
		DrawDataRow({ {0, L"Drain Rate"    }, {colPeriod, val} }, true);
	}

	// ══════════════════════════════════════════════════════════════════
	// SECTION 4: Usage History  — 5 columns, uses col5/colPeriod5
	// ══════════════════════════════════════════════════════════════════
	DrawH2(L"Usage History");
	{
		int c0 = 0, c1 = colPeriod5, c2 = c1 + col5, c3 = c2 + col5, c4 = c3 + col5;
		std::vector<ColDef> hdrs = {
			{ L"Period",          c0, colPeriod5 },
			{ L"Battery Active",  c1, col5 },
			{ L"Battery Standby", c2, col5 },
			{ L"AC Active",       c3, col5 },
			{ L"AC Standby",      c4, tableW - c4 }
		};
		DrawTableHeader(hdrs);
		dataRowIdx = 0;

		for (const auto& r : m_rows)
		{
			CString a, b, c, d;
			a.Format(L"%.2f h", r.battActiveHrs);
			b.Format(L"%.2f h", r.battStandbyHrs);
			c.Format(L"%.2f h", r.acActiveHrs);
			d.Format(L"%.2f h", r.acStandbyHrs);
			DrawDataRow({ {c0, r.period}, {c1, a}, {c2, b}, {c3, c}, {c4, d} });
		}
	}

	// ══════════════════════════════════════════════════════════════════
	// SECTION 5: Battery Capacity History  — 3 columns (unchanged)
	// ══════════════════════════════════════════════════════════════════
	DrawH2(L"Battery Capacity History");
	{
		int c0 = 0, c1 = colPeriod, c2 = c1 + col;
		std::vector<ColDef> hdrs = {
			{ L"Period",               c0, colPeriod },
			{ L"Full Charge Capacity", c1, col },
			{ L"Design Capacity",      c2, tableW - c2 }
		};
		DrawTableHeader(hdrs);
		dataRowIdx = 0;

		for (const auto& r : m_capacity)
			DrawDataRow({ {c0, r.period}, {c1, r.fullCharge}, {c2, r.designCapacity} });
	}

	// ══════════════════════════════════════════════════════════════════
	// SECTION 6: Battery Life Estimates  — 5 columns, uses col5/colPeriod5
	// ══════════════════════════════════════════════════════════════════
	DrawH2(L"Battery Life Estimates");
	{
		int c0 = 0, c1 = colPeriod5, c2 = c1 + col5, c3 = c2 + col5, c4 = c3 + col5;
		std::vector<ColDef> hdrs = {
			{ L"Period",           c0, colPeriod5 },
			{ L"Design Active",    c1, col5 },
			{ L"Design Connected", c2, col5 },
			{ L"Full Active",      c3, col5 },
			{ L"Full Connected",   c4, tableW - c4 }
		};
		DrawTableHeader(hdrs);
		dataRowIdx = 0;

		for (const auto& r : m_life)
		{
			CString a, b, c, d;
			a.Format(L"%.2f h", r.designActive);
			b.Format(L"%.2f h", r.designConnected);
			c.Format(L"%.2f h", r.fullActive);
			d.Format(L"%.2f h", r.fullConnected);
			DrawDataRow({ {c0, r.period}, {c1, a}, {c2, b}, {c3, c}, {c4, d} });
		}
	}

	// ── Cleanup ────────────────────────────────────────────────────────
	DeleteObject(hFontBody);
	DeleteObject(hFontBold);
	DeleteObject(hFontH1);
	DeleteObject(hFontH2);
	DeleteObject(hBrBlue);
	DeleteObject(hBrAlt);
	DeleteObject(hBrWhite);
	DeleteObject(hPenBorder);
	DeleteObject(hPenBlue);
}

//////////////////////////////////////////////////////////////
// 🔹 ExportPrintToPdf  –  no admin rights required
//
//  Key insight: "Microsoft Print to PDF" honours DOCINFO.lpszOutput
//  as the destination file path — no SetPrinterW / port redirect needed.
//  We just CreateDC for the printer, set di.lpszOutput = chosen path,
//  call StartDoc, render, EndDoc.  Works for any standard user.
//////////////////////////////////////////////////////////////

bool CReportDlg::ExportPrintToPdf(CString& outPdfPath) const
{
	// ── 1. SaveFileDialog ───────────────────────────────────────────────
	wchar_t szFile[MAX_PATH] = L"battery_report.pdf";
	wchar_t filter[] = L"PDF Files (*.pdf)\0*.pdf\0All Files (*.*)\0*.*\0\0";

	OPENFILENAME ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = GetSafeHwnd();
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = MAX_PATH;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = L"pdf";

	if (!GetSaveFileName(&ofn))
		return false;   // cancelled

	outPdfPath = szFile;

	// ── 2. Find "Microsoft Print to PDF" (locale-safe) ──────────────────
	//    Try the most-specific match first, then broaden.
	CString printerName = FindPrinterByKeywords({ L"Microsoft", L"PDF" });
	if (printerName.IsEmpty())
		printerName = FindPrinterByKeywords({ L"PDF" });

	if (printerName.IsEmpty())
	{
		AfxMessageBox(
			L"'Microsoft Print to PDF' was not found on this PC.\n\n"
			L"Enable it via:\n"
			L"  Settings \u2192 Devices \u2192 Printers & scanners\n"
			L"  \u2192 Add a printer \u2192 Microsoft Print to PDF",
			MB_ICONWARNING);
		return false;
	}

	// ── 3. Get DEVMODE for the printer ──────────────────────────────────
	HANDLE hPrinter = nullptr;
	OpenPrinterW(const_cast<LPWSTR>(printerName.GetString()), &hPrinter, nullptr);

	std::vector<BYTE> dmBuf;
	DEVMODEW* pDM = nullptr;

	if (hPrinter)
	{
		LONG dmSz = DocumentPropertiesW(GetSafeHwnd(), hPrinter,
			const_cast<LPWSTR>(printerName.GetString()),
			nullptr, nullptr, 0);
		if (dmSz > 0)
		{
			dmBuf.resize(dmSz);
			pDM = reinterpret_cast<DEVMODEW*>(dmBuf.data());
			if (DocumentPropertiesW(GetSafeHwnd(), hPrinter,
				const_cast<LPWSTR>(printerName.GetString()),
				pDM, nullptr, DM_OUT_BUFFER) < 0)
				pDM = nullptr;
		}
		ClosePrinter(hPrinter);
	}

	// ── 4. Create a DC for the PDF printer ──────────────────────────────
	HDC hDC = CreateDCW(nullptr, printerName.GetString(), nullptr, pDM);
	if (!hDC)
	{
		AfxMessageBox(L"Could not create a DC for the PDF printer.", MB_ICONERROR);
		return false;
	}

	// ── 5. StartDoc — pass the output path via lpszOutput ───────────────
	//       This is the standard, no-admin way to control where the PDF lands.
	DOCINFOW di = {};
	di.cbSize = sizeof(di);
	di.lpszDocName = L"Battery Report";
	di.lpszOutput = outPdfPath.GetString();   // ← THE KEY FIX

	if (StartDocW(hDC, &di) <= 0)
	{
		DWORD err = GetLastError();
		DeleteDC(hDC);
		CString msg;
		msg.Format(L"StartDoc failed (error %lu).\nCould not create PDF.", err);
		AfxMessageBox(msg, MB_ICONERROR);
		return false;
	}

	StartPage(hDC);

	// ── 6. Render all content onto the printer DC ────────────────────────
	int pageW = GetDeviceCaps(hDC, HORZRES);
	int pageH = GetDeviceCaps(hDC, VERTRES);
	int dpiX = GetDeviceCaps(hDC, LOGPIXELSX);
	int dpiY = GetDeviceCaps(hDC, LOGPIXELSY);

	int   marginX = (dpiX * 3) / 4;   // ~0.75 inch left/right
	int   marginY = (dpiY * 3) / 4;   // ~0.75 inch top/bottom
	float scaleX = (float)dpiX / 96.0f;
	float scaleY = (float)dpiY / 96.0f;

	RenderReportToDC(hDC, pageW, pageH, marginX, marginY, scaleX, scaleY);

	EndPage(hDC);
	EndDoc(hDC);
	DeleteDC(hDC);

	return true;
}