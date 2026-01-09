#pragma once
#include <chrono>
#include <vector>

class CSleepDataDlg : public CDialogEx
{
public:
    CSleepDataDlg(CWnd* pParent = nullptr);
    virtual ~CSleepDataDlg();

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_SLEEPDATA_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    // Message handlers
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnPaint();
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
    afx_msg void OnDestroy();
    afx_msg LRESULT OnRefreshMsg(WPARAM, LPARAM);

    DECLARE_MESSAGE_MAP()

private:
    // ===== Sleep tracking snapshot =====
    struct SleepSnapshot {
        bool valid = false;
        bool onAC = false;
        ULONGLONG mWh = 0;
        int percent = -1;
        std::chrono::system_clock::time_point t;
    };

    // Static data (persist across modal openings)
    static SleepSnapshot        s_beforeSleep;
    static std::vector<CString> s_logs;
    static HWND                 s_hwndOpen;
    static ULONGLONG            s_lastResumeTick;
    static bool                 s_resumeLogged;

    // Cause/state machine
    enum class Cause : UINT8 { None = 0, Real, Screen };
    static Cause s_activeCause;

    // Custom message for refresh notification
    enum { WM_SLEEPDATA_REFRESH = WM_APP + 1 };

    // Owner-draw + layout
    CFont  m_monoFont;
    int    m_lineHeight = 18;
    int    m_charWidth = 8;
    int    m_scrollPosV = 0;    // Vertical scroll position (line index)
    int    m_scrollPosH = 0;    // Horizontal scroll position (pixels)
    int    m_contentWidth = 0;  // Total width of content
    CRect  m_client{ 0,0,0,0 };

    // Font sizing
    int    m_baseFontPx = 16;
    int    m_minFontPx = 10;

    // Helper methods
    void   CreateMonoFont(int pixelHeight);
    void   RecalcTextMetrics(CDC& dc);
    void   CalculateContentWidth(CDC& dc);
    void   UpdateScrollBars();
    int    TotalLines() const;

    // Static helpers (non-UI)
    static bool    QueryBattery_mWh(ULONGLONG& out_mWh, bool& outAC);
    static bool    QueryBattery_Percent(int& outPct, bool& outAC);
    static CString FormatSysTime(const std::chrono::system_clock::time_point& tp);
    static void    TrackBeforeSleep_NoUI();
    static void    TrackAfterResume_NoUI();
    static void    AppendLogStatic(const CString& line);
    static CString MakeRow(const CString& sleepTime,
        const CString& awakeTime,
        const CString& pctBefore,
        const CString& pctAfter);

    // UI helpers (viewer refresh only)
    void OnBeforeSleep_UI();
    void OnAfterResume_UI();
    void AppendLog_UI(const CString& line);

public:
    // Public interface for main dialog
    static void HandlePowerBroadcast(UINT nPowerEvent);
    static void HandleDisplayState(DWORD displayState);

    static bool eng_lang;

	HICON m_hIcon;
};