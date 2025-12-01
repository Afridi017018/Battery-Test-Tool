#pragma once
#include "resource.h"
#include <vector>

class CPerfDlg : public CDialogEx
{
public:
    CPerfDlg(CWnd* pParent = nullptr) : CDialogEx(IDD_PERF_DIALOG, pParent) {}

    // Pass-in data
    void SetData(float initialPct, float currentPct, float ratePctPerMin, double gflops,
        const std::vector<float>& timesMin,
        const std::vector<float>& percents)
    {
        m_initial = initialPct;
        m_current = currentPct;
        m_rate = ratePctPerMin;
        m_gflops = gflops;
        m_times = timesMin;
        m_perc = percents;
    }

#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_PERF_DIALOG };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX) { CDialogEx::DoDataExchange(pDX); }
    virtual BOOL OnInitDialog();

    afx_msg void OnDestroy();
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC*) { return TRUE; }

    // Resize friendliness
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);

    DECLARE_MESSAGE_MAP()

private:
    ULONG_PTR m_gdiplusToken = 0;

    float m_initial = 0.f;
    float m_current = 0.f;
    float m_rate = 0.f;     // %/min
    double m_gflops = 0.0;

    std::vector<float> m_times; // minutes
    std::vector<float> m_perc;  // %, same length as m_times
public:
    bool eng_lang;
};
