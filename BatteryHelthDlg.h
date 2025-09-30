
// BatteryHelthDlg.h : header file
//

#pragma once
#include "pch.h" 

#include <Wbemidl.h>
#include <comdef.h>

#include <vector>
#include <afxwin.h>      
#include <afxcmn.h>      

#include <atomic>

#include <unordered_map>




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
	UINT_PTR m_dischargeTimerID = 3;         // Timer ID for discharge test
	int m_dischargeDurationMinutes = 5;     // Duration of discharge test (minutes)
	int m_elapsedMinutes = 0;
	bool m_dischargeClick = false;
	int m_elapsedSeconds = 0;

	std::atomic<bool> m_stopCpuLoad;
	bool m_cpuLoadTestRunning = false;
	bool m_cpuLoadClick = false;
	int m_cpuLoadDurationSeconds = 60; // 1 minute default
	int m_initialBatteryCPUPercent = 0;
	UINT_PTR m_cpuLoadTimerID = 4;
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

	double GetCurrentCPUUsage();

	afx_msg void OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);

	CProgressCtrl m_CPU_Progress;

	CFont m_Font16px;

	afx_msg void OnSize(UINT nType, int cx, int cy);

	struct CtlOrigPos {
		UINT id;
		CRect rect;
	};

	std::vector<CtlOrigPos> m_origPositions;
	bool m_origPositionsSaved = false;

	CSize m_origDialogSize;



	CFont m_scaledFont;        // Scaled regular font
	CFont m_scaledBoldFont;    // Scaled bold font
	CFont m_normalFont;

	// Methods
	void CreateFonts();
	void ApplyFonts(double scale);
	void CleanupFonts();


	void MonitorCPUUsage(std::atomic<bool>* stopFlag, std::atomic<double>* targetUsage);

	CString QueryBatteryCapacityHistory();

	void SetBoldFontScaled(CWnd* pWnd, double fontScale);
	void SetButtonFont(int controlId, bool useScaled);

	void BringControlToFront(int controlId);

	

	CProgressCtrl m_discharge_progress;

	CBrush m_bgBrush;

	afx_msg BOOL OnEraseBkgnd(CDC* pDC);

	afx_msg void OnDestroy();


	CButton Button1;


	public:
		// All the button control IDs you want to owner-draw
		std::vector<UINT> m_buttonIds{ IDC_BTN_CPULOAD, IDC_BTN_DISCHARGE, IDC_BTN_HISTORY, IDC_BTN_UPLOADPDF };

		// Per-button hover state, keyed by control ID
		std::unordered_map<UINT, BOOL> m_hover;

		// (Optional) Per-button PNG resource ID (if each button uses a different PNG)
		std::unordered_map<UINT, UINT> m_pngById{
			{ IDC_BTN_CPULOAD, IDB_PNG1 },
			{ IDC_BTN_DISCHARGE, IDB_PNG2 },
			{ IDC_BTN_HISTORY, IDB_PNG3 },
			{ IDC_BTN_UPLOADPDF, IDB_PNG4 }
		};

		void UpdateStaticNoGhosting(int ctrlId, const CString& text);
		CStatic textEdit;


		CToolTipCtrl m_toolTip;   // tooltip control

		virtual BOOL PreTranslateMessage(MSG* pMsg); // to relay mouse events
		void InitToolTips();



		public:
				double m_dpiScaleFactor;
				int m_baseWidth;
				int m_baseHeight;

				// Base font sizes (your original design sizes)
				int m_baseFontSize;        // e.g., 16 for main text
				int m_baseHeaderFontSize;  // e.g., 24 for headers
				int m_baseSmallFontSize;   // e.g., 12 for small text

				// Methods
				void CalculateDPIScale();
				void ScaleDialog();
				int ScaleDPI(int value);
				void CreateScaledFonts();
				void ApplyScaledFonts();

				CFont m_fontNormal;
				CFont m_fontBold;
				CFont m_fontHeader;
				CFont m_fontSmall;

				// Add this line:
				afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);



			


};




