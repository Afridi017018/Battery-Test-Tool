#pragma once

#include "afxdialogex.h"

#include <Windows.h>
#include <powrprof.h>
#pragma comment(lib, "PowrProf.lib")


#include <gdiplus.h>
#pragma comment(lib, "gdiplus.lib")


#include <wbemidl.h>
#include <comdef.h>
#pragma comment(lib, "wbemuuid.lib")


#include <vector>
#include <limits>


#define TIMER_BATTERY_UPDATE 1001


struct BatteryTimeInfo {
    // Status flags
    bool  isCharging = false;
    bool  isOnAC = false;
    bool  isFullyCharged = false;

    // Capacity values (mWh)
    ULONG currentCapacity = 0;
    ULONG maxCapacity = 0;
    ULONG designCapacity = 0; // not provided by SystemBatteryState

    // Current state
    float chargePercent = 0.0f;  // 0..100
    LONG  currentRate_mW = 0;    // +charging, -discharging

    // Time estimates
    float hoursRemaining = 0.0f; // when discharging
    float hoursToFull = 0.0f;    // when charging

    // UI strings
    CString statusText;
    CString detailText;
};

class BatteryTimeEstimator {
public:
    BatteryTimeEstimator() : m_smoothedRate(0.0f), m_initialized(false) {}

    BatteryTimeInfo GetBatteryTime(int lang);             // read OS + compute ETAs
    void UpdateSmoothedRate(LONG currentRate_mW); // EMA or raw
    float GetSmoothedRate() const { return m_smoothedRate; }

private:
    float m_smoothedRate;   // absolute mW, smoothed or raw
    bool  m_initialized;
    const float ALPHA = 0.25f; // used if smoothing enabled

    // Sources
    bool GetBatteryState(SYSTEM_BATTERY_STATE& state);
    bool TryReadChargeRateFromWMI(LONG& signedRate_mW); // prefers WMI (sum+sign)

    // Compute + format
    void CalculateTimes(BatteryTimeInfo& info, const SYSTEM_BATTERY_STATE& state);
    void FormatStatusText(BatteryTimeInfo& info, int lang);
};

// ------------------------------------------------------
// Dialog with live chart (charge/discharge toggle)
// FIXED: Double-buffering to eliminate flicker
// ------------------------------------------------------
#ifndef IDD_RATEINFO
#define IDD_RATEINFO IDD_RATEINFO
#endif

class CRateInfoDlg : public CDialogEx {
public:
    CRateInfoDlg(CWnd* pParent = nullptr);

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    afx_msg void OnDestroy();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    DECLARE_MESSAGE_MAP()

private:
    // Battery estimator
    BatteryTimeEstimator m_estimator;
    BatteryTimeInfo      m_last;

    // GDI+
    ULONG_PTR m_gdiplusToken = 0;

    // Live chart data (rolling window)
    static constexpr int   kWindowSeconds = 120;              // last 2 minutes
    static constexpr float kMissing = std::numeric_limits<float>::quiet_NaN();
    std::vector<float>     m_samplesChargeW;                  // charge (W), NaN = gap
    std::vector<float>     m_samplesDischargeW;               // discharge (W), NaN = gap
    int                    m_cursor = 0;                      // circular index
    bool                   m_filled = false;                  // buffer filled at least once

    // Helpers
    void StartBatteryMonitoring();
    void UpdateBatteryInfo();
    void DrawBatteryCard(Gdiplus::Graphics& g, float x, float y, float w, float h);
    void DrawLivePowerChart(Gdiplus::Graphics& g, float x, float y, float w, float h);

    void PushSamples(float chargeW, float dischargeW); // circular buffer push

public:
    bool eng_lang;

    int getLang();

	HICON m_hIcon;
};