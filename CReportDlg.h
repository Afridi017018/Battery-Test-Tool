#pragma once

#include <afxwin.h>
#include <vector>
#include "resource.h"

class CReportDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CReportDlg)

public:
	CReportDlg(CWnd* pParent = nullptr);
	virtual ~CReportDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_REPORT_DIALOG };
#endif

	// 🔹 Usage History
	struct UsageHistoryRow {
		CString period;
		double battActiveHrs = 0;
		double battStandbyHrs = 0;
		double acActiveHrs = 0;
		double acStandbyHrs = 0;
	};

	// 🔹 Capacity
	struct BatteryCapacityRow {
		CString period;
		CString fullCharge;
		CString designCapacity;
	};

	// 🔹 FIXED Battery Life (ALL COLUMNS)
	struct BatteryLifeRow {
		CString period;

		double designActive = 0;
		double designConnected = 0;

		double fullActive = 0;
		double fullConnected = 0;
	};

protected:
	virtual void DoDataExchange(CDataExchange* pDX);
	virtual BOOL OnInitDialog();

	afx_msg void OnPaint();
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

	DECLARE_MESSAGE_MAP()

private:
	std::vector<UsageHistoryRow> m_rows;
	std::vector<BatteryCapacityRow> m_capacity;
	std::vector<BatteryLifeRow> m_life;

	CString m_htmlCache;

	int m_scrollPos = 0;
	int m_rowHeight = 25;

	void LoadUsageHistory();
	void UpdateScrollBar();

	int m_scrollX = 0;   // ✅ horizontal scroll position
};