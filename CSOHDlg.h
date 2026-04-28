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
    DECLARE_MESSAGE_MAP()
private:
    void UpdateBatteryStatus();
    void LogData(int percent, ULONGLONG duration);
    void UpdateInstruction();
    void UpdateUI(int percent);
    void StartCpuStress();
    void StopCpuStress();

    int             m_lastPercent;
    ULONGLONG       m_lastDropTime;
    bool            m_isFullyCharged;
    bool            m_isDischarging;
    CString         m_instruction;
    std::ofstream   m_logFile;
    std::string     m_testID;

    CProgressCtrl   m_progress;
    CStatic         m_lblPercent;
    CStatic         m_lblInstruction;
    CStatic         m_lblElapsed;
    CFont           m_fontHeader;
    CFont           m_fontInstruction;
    CFont           m_fontPercent;

    ULONGLONG       m_startDischargeTime;

    std::vector<std::thread>  m_stressThreads;
    std::atomic<bool>         m_stressRunning;

    afx_msg BOOL OnEraseBkgnd(CDC* pDC);

    bool eng_lang;
};