#pragma once
#include <afxwin.h>
#include <vector>
#include "BatteryHelth.h"
#include "resource.h"


class CBatteryHelthDlg;
// Forward-declared here so the header doesn't need the full struct in .cpp
struct BatteryScore;

class CReportDlg : public CDialogEx
{
    DECLARE_DYNAMIC(CReportDlg)
public:
    CReportDlg(const BatteryReportData& data, CWnd* pParent = nullptr);
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

    // 🔹 Battery Life
    struct BatteryLifeRow {
        CString period;
        double designActive = 0;
        double designConnected = 0;
        double fullActive = 0;
        double fullConnected = 0;
    };

    // 🔹 Scoring result (defined in .cpp, stored here after OnInitDialog)
    //
    //  4 factors:  Health (50%) | CPU drain (20%) | Discharge drain (20%) | Cycles (10%)
    //  Any factor whose data is unavailable is excluded; remaining weights are
    //  redistributed proportionally so the final score is always honest.
    struct ScoreResult {
        double total = 0;    // 0-100, computed from available factors only

        // sub-scores (0-100 each); -1 = data unavailable / excluded
        double f1Health = -1;
        double f4Cpu = -1;
        double f5Dis = -1;
        double f6Cycles = -1;

        // raw values for display
        double healthPct = -1;   // -1 = unknown
        double cpuRatePerMin = -1;   // -1 = not measured
        double disRatePerMin = -1;   // -1 = not measured
        int    cycleCount = -1;   // -1 = not reported by device

        // human-readable note when a factor is missing
        CString missingNote;

        CString  grade;
        COLORREF gradeColor = RGB(128, 128, 128);
        CString  advice;
    };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    virtual BOOL OnInitDialog();
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT nType, int cx, int cy);
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    DECLARE_MESSAGE_MAP()

private:
    std::vector<UsageHistoryRow>    m_rows;
    std::vector<BatteryCapacityRow> m_capacity;
    std::vector<BatteryLifeRow>     m_life;

    ScoreResult m_score;       // ← computed once in OnInitDialog, painted every WM_PAINT

    CString m_htmlCache;
    int     m_basicInfoHeight = 0;

    // Export helpers
    CRect   m_exportBtnRect;
    CString BuildHtmlReport() const;
    void    ExportHtmlReport(const CString& destPath = L"") const;
    bool    ExportPrintToPdf(CString& outPdfPath) const;
    void    RenderReportToDC(HDC hDC, int pageW, int pageH,
        int marginX, int marginY, float scaleX, float scaleY) const;

public:
    BatteryReportData m_reportData;

    
    CBatteryHelthDlg* m_pBattDlg = nullptr;

};