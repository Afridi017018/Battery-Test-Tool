
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

#include "Resource.h"
#include <shellapi.h>   // tray
#pragma comment(lib, "Shell32.lib")

#include <unordered_map>

#include <PowerSetting.h> 
#pragma comment(lib, "User32.lib")


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
	int m_dischargeDurationMinutes = 0;     // Duration of discharge test (minutes)
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
		std::vector<UINT> m_buttonIds{ IDC_BTN_CPULOAD, IDC_BTN_DISCHARGE, 
			IDC_BTN_HISTORY, IDC_BTN_UPLOADPDF, IDC_BTN_CAPHIS, 
			IDC_BTN_ACTIVE, IDC_BTN_STANDBY,
			IDC_BTN_USAGE,IDC_BTN_MANIPULATIOIN, IDC_BTN_RATEINFO,
			IDC_BTN_BGAPP,IDC_BTN_SLEEP
		};

		// Per-button hover state, keyed by control ID
		std::unordered_map<UINT, BOOL> m_hover;

		// (Optional) Per-button PNG resource ID (if each button uses a different PNG)
		std::unordered_map<UINT, UINT> m_pngById{
			{ IDC_BTN_CPULOAD, IDB_PNG1 },
			{ IDC_BTN_DISCHARGE, IDB_PNG2 },
			{ IDC_BTN_HISTORY, IDB_PNG3 },
			{ IDC_BTN_UPLOADPDF, IDB_PNG4 },
			{ IDC_BTN_CAPHIS, IDB_PNG5 },
	/*		{ IDC_BTN_PREDICTION, IDB_PNG6 },*/
			{ IDC_BTN_ACTIVE, IDB_PNG7 },
		    { IDC_BTN_STANDBY, IDB_PNG8 },
			{ IDC_BTN_USAGE, IDB_PNG9 },
			{ IDC_BTN_MANIPULATIOIN, IDB_PNG10 },
		     { IDC_BTN_RATEINFO, IDB_PNG11 },	
			{IDC_BTN_BGAPP, IDB_PNG12},
			{IDC_BTN_SLEEP, IDB_PNG13},
		};

		void UpdateStaticNoGhosting(int ctrlId, const CString& text);
		CStatic textEdit;


		CToolTipCtrl m_toolTip;   

		virtual BOOL PreTranslateMessage(MSG* pMsg); 
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

			public:
				struct BatteryCapacityRecord {
					COleDateTime date;     // parsed; if parsing fails, .m_dt == 0
					CString      dateText; // original text from the report
					int          fullCharge_mWh = 0;
					int          design_mWh = 0;
					double       healthPct = 0.0; // 100 * full/design
				};


				bool GetBatteryCapacityHistory(std::vector<BatteryCapacityRecord>& out);

				void GetStaticBatteryInfo();

				afx_msg void OnBnClickedBtnCaphis();


				struct BatteryHealthPrediction {
					int          targetPct = 0;     
					bool         valid = false;
					COleDateTime predictedDate;
					CString      note;
				};


	
				afx_msg void OnBnClickedBtnPrediction();
				afx_msg void OnBnClickedBtnActive();
				afx_msg void OnBnClickedBtnStandby();
				afx_msg void OnBnClickedButton1();


				public:
					
					afx_msg void OnBnClickedButton2();
					afx_msg void OnBnClickedUsage();
					afx_msg void OnBnClickedBtnUsage();
					afx_msg void OnBnClickedBtnManipulatioin();


					protected:
		/*				virtual void DoDataExchange(CDataExchange* pDX) override;*/
				/*		virtual BOOL OnInitDialog() override;*/

						afx_msg void OnBnClickedBtnShow2();
	

				

protected:
	// --- Report builder ---
	CString BuildVisibleAppsReport();

	// --- Tray notification helpers ---
	void EnsureTrayIcon();
	void RemoveTrayIcon();
	void ShowBalloon(LPCWSTR title, LPCWSTR text, DWORD infoFlags);

	// Checks top visible app by uptime; if >=30mins, shows balloon with idle/active status
	void CheckAndNotifyTopLongRunning();

	// --- Idle detection helpers ---
	// Returns idle time in seconds for a given process/window
	ULONGLONG GetProcessIdleTime(DWORD pid, HWND mainWindow);

	// Check if window is responding to input
	bool IsWindowResponding(HWND hWnd);

	// Get global input idle time (seconds)
	ULONGLONG GetGlobalInputIdleTime();

protected:
	// tray + timer
	NOTIFYICONDATAW m_nid{};
	bool            m_trayAdded{ false };
	UINT_PTR        m_timerId{ 0 };

	// Track last input time (not strictly required for current logic but kept)
	ULONGLONG       m_lastInputCheck{ 0 };

public:
	afx_msg void OnBnClickedBtnBgapp();
	afx_msg void OnBnClickedBtnRateinfo();


	protected:
		// === Toggle state ===
		enum class Lang { EN, JP };
		Lang m_lang = Lang::EN; // default: English

		// Helpers
		void RedrawToggleButtons();
		void DrawToggleButton(LPDRAWITEMSTRUCT lpDrawItemStruct, bool active, const wchar_t* text);
	

		void UpdateLanguageTexts();

		// Single power-broadcast sink for the whole app
		afx_msg UINT OnPowerBroadcast(UINT nPowerEvent, LPARAM nEventData);

		// Power setting notification handle (display on/off/dim)
		HPOWERNOTIFY m_hDispNotify = nullptr;


public:
	afx_msg void OnBnClickedBtnEn();
	afx_msg void OnBnClickedBtnJp();
	afx_msg void OnBnClickedBtnSleep();
};




