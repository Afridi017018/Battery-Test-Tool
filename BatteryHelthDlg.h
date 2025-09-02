
// BatteryHelthDlg.h : header file
//

#pragma once
#include <Wbemidl.h>
#include <comdef.h>

#include <vector>
#include <afxwin.h>      
#include <afxcmn.h>      


// CBatteryHelthDlg dialog
class CBatteryHelthDlg : public CDialogEx
{
// Construction
public:
	CBatteryHelthDlg(CWnd* pParent = nullptr);	// standard constructor

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_BATTERYHELTH_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
protected:
	HICON m_hIcon;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg LRESULT OnCPULoadFinished(WPARAM wParam, LPARAM lParam);

	DECLARE_MESSAGE_MAP()

public:
	void GetBatteryInfo();
	CProgressCtrl m_BatteryProgress;
	afx_msg void OnTimer(UINT_PTR nIDEvent);

	// Discharge Test variables
	int m_initialBatteryPercent = -1;        // Battery % at start of test
	bool m_dischargeTestRunning = false;     // Is the discharge test active?
	UINT_PTR m_dischargeTimerID = 2;         // Timer ID for discharge test
	int m_dischargeDurationMinutes = 10;     // Duration of discharge test (minutes)
	int m_elapsedMinutes = 0;


	bool m_cpuLoadTestRunning = false;
	int m_cpuLoadDurationSeconds = 120; // 2 minutes default
	int m_initialBatteryCPUPercent = 0;
	UINT_PTR m_cpuLoadTimerID = 3;
	int m_cpuLoadElapsed = 0;

	afx_msg void OnBnClickedBtnDischarge();
	afx_msg void OnBnClickedBtnCpuload();
	afx_msg void OnBnClickedBtnUploadpdf();

	bool m_cpuLoadTestCompleted = false;
	CString m_cpuLoadResult = L"Not tested";

	bool m_dischargeTestCompleted = false;
	CString m_dischargeResult = L"Not tested";

	void UpdateDischargeButtonStatus();

	struct BatteryEvent
	{
		bool charging;       // true = charging, false = discharging
		CTime timestamp;     // Time of event
	};
	std::vector<BatteryEvent> m_batteryHistory;
	bool m_lastChargingState = false;

	void CheckBatteryTransition();


	afx_msg void OnBnClickedBtnHistory();
	CStatic m_bb;
	CStatic m_abt;
	CStatic m_dh;
	CFont m_boldFont;
	CStatic m_header;

	afx_msg void OnStnClickedStaticDid();
	afx_msg void OnStnClickedStaticHeader();


	afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
	CBrush m_brushWhite;

};
