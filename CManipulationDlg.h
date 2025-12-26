#pragma once

#include "resource.h"      
#include <vector>
#include <string>
#include <cstdint>
#include <afxwin.h>

// ===================== Public data models =====================

struct BMD_Sample
{
    std::wstring dateISO;        
    uint64_t     fullCharge_mWh{ 0 };
    float        healthPercent{ 0.f }; 
    uint64_t     designCapacity_mWh{ 0 };
};

struct BMD_BatteryInfo
{
    uint64_t                designCapacity_mWh{ 0 };
    std::vector<BMD_Sample> samples;            // ascending by date
    int                     cycleCount{ -1 };   // -1 if unknown
    std::wstring            serialNumber;
    std::wstring            manufacturer;

    // Parsed (if present) for anomaly checks
    uint64_t                currentRate_mW{ 0 };     // charge/discharge rate from report (mW)
    double                  lastChargeMinutes{ 0.0 };// 0?100% time in minutes (if found)
};

struct BMD_Flag
{
    CString title;     // short headline
    CString detail;    // concise evidence text
    int     points{ 0 }; // contribution 0..25
};

struct BMD_DetectionResult
{
    int                   score{ 0 };               // 0..100
    std::vector<BMD_Flag> flags;                    // sorted by points desc
    CString               status;                   // "Genuine" / "Suspicious" / "Likely Manipulated"
};

// ===================== Your dialog =====================

class CManipulationDlg : public CDialogEx
{
public:
    CManipulationDlg(CWnd* pParent = nullptr);

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_MANIPULATION }; // <-- keep your real dialog ID
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()

public:
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
    afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);

private:
    void RunBatteryManipulationCheck();  // helper to run the whole pipeline
    void UpdateVScrollBar();             // set range/page/pos for vertical scrollbar
    void ClampScroll();                  // keep m_scrollY valid

    BMD_DetectionResult m_bmdResult;     // last result to paint
    CString             m_lastReportPath;

    // Scrolling state for the panel
    int m_scrollY{ 0 };      // current vertical offset in pixels
    int m_contentH{ 0 };     // total content height measured in last paint (list area)

public:
    bool eng_lang;

    CString cycles;
};
