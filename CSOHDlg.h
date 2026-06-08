#pragma once
#include "resource.h"
#include <fstream>
#include <string>
#include <afxcmn.h>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

class CSOHDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CSOHDlg)
public:
    CSOHDlg(CWnd* pParent = nullptr);
    virtual ~CSOHDlg();
    enum { IDD = IDD_SOH };

protected:
    virtual void DoDataExchange(CDataExchange* pDX) override;
    virtual BOOL OnInitDialog() override;
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnPaint();
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg BOOL OnEraseBkgnd(CDC* pDC);
    afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
    afx_msg void OnSize(UINT nType, int cx, int cy);
    DECLARE_MESSAGE_MAP()

private:
    void UpdateBatteryStatus();
    void LogData(int percent, ULONGLONG duration);
    void UpdateInstruction();
    void UpdateUI(int percent);
    void StartCpuStress();
    void StopCpuStress();
    void DrawStartButton(CDC* pDC);
    void DrawRangeButtons(CDC* pDC);
    void DrawTimeSlider(CDC* pDC);
    void StartTest();
    void RecalcLayout();          // responsive layout recalc

    // State
    int             m_lastPercent;
    ULONGLONG       m_lastDropTime;
    bool            m_isFullyCharged;
    bool            m_isDischarging;
    bool            m_testStarted;
    bool            m_testComplete;
    int             m_targetStopPercent;
    CString         m_instruction;
    std::ofstream   m_logFile;
    std::string     m_testID;

    // Controls
    CProgressCtrl   m_progress;
    CStatic         m_lblPercent;
    CStatic         m_lblInstruction;
    CStatic         m_lblElapsed;
    CBrush          m_brWhite;

    // Fonts
    CFont           m_fontTitle;
    CFont           m_fontInstruction;
    CFont           m_fontPercent;
    CFont           m_fontBtn;
    CFont           m_fontRange;

    ULONGLONG       m_startDischargeTime;

    // CPU stress
    std::vector<std::thread>  m_stressThreads;
    std::atomic<bool>         m_stressRunning;

    // Painted button rects (recomputed on resize)
    CRect           m_rcStartBtn;
    bool            m_startBtnHover;
    bool            m_startBtnPressed;

    struct RangeBtn { CRect rc; int stopAt; CString label; };
    std::vector<RangeBtn> m_rangeBtns;
    int             m_hoveredRange;

    struct TimeSlider {
        CRect   rcTrack;   // full track rect
        int     minVal = 10, maxVal = 120;
        bool    dragging = false;
        bool    hovering = false;
    };
    TimeSlider  m_timeSlider;
    int         m_targetMinutes;   // 0 = not selected (same as before)
    bool        m_sliderDragging;

    int m_currentPercent;   // live battery % for range validation

    void PaintBuffer(CDC* pDC, int W, int H);   // double-buffer helper



};