#pragma once
#include "resource.h"
#include <vector>
#include <gdiplus.h>

class CPredictionDlg : public CDialogEx
{
public:
    CPredictionDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_PREDICTION_DIALOG };
#endif

    // Optional: pass custom data; otherwise HTML loader / defaults are used
    void SetData(const std::vector<CString>& periods,
        const std::vector<float>& fullHours,
        const std::vector<float>& designHours);

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

    ULONG_PTR m_gdiplusToken{};
    std::vector<CString> m_periods;
    std::vector<float>   m_full, m_design;

    // === NEW: data sourcing ===
    void EnsureDefaultData();                   // fallback hardcoded dataset
    bool LoadFromBatteryReport(const CString&); // parse battery-report.html

    // === drawing ===
    DECLARE_MESSAGE_MAP()
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg void OnDestroy();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
public:
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);




    // ========== NEW: Prediction-related structures and methods ==========

    struct PredictionStats {
        float currentHealth;
        float initialHealth;
        float linearRate;
        float lambda;
        float monthsElapsed;
        float linearMonthsTo50;
        float expMonthsTo50;
    };

    // Calculate prediction statistics from battery data
    PredictionStats CalculatePredictions() const;

    // Generate future predictions for N months
    void GenerateFuturePredictions(const PredictionStats& stats,
        int monthsAhead,
        std::vector<float>& linearPred,
        std::vector<float>& expPred) const;

    // Drawing helper methods for each chart
    void DrawActiveBatteryChart(Gdiplus::Graphics& g,
        float x, float y, float w, float h,
        float labelLane);

    void DrawHealthTrendChart(Gdiplus::Graphics& g,
        float x, float y, float w, float h,
        float labelLane);

    void DrawHealthPredictionChart(Gdiplus::Graphics& g,
        float x, float y, float w, float h,
        float labelLane,
        const PredictionStats& stats,
        const std::vector<float>& linearPred,
        const std::vector<float>& expPred);

    void DrawStatisticsBox(Gdiplus::Graphics& g,
        float x, float y, float w, float h,
        const PredictionStats& stats);

    void DrawRotatedLabels(Gdiplus::Graphics& g,
        const std::vector<CString>& labels,
        float x, float baseY, float stepX,
        float labelLane);

    // Parse battery report HTML




};

