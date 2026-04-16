#include "pch.h"
#include "CSOHDlg.h"
#define NOMINMAX
#include <ctime>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <cmath>
#include <algorithm>
#include <windows.h>

IMPLEMENT_DYNAMIC(CSOHDlg, CDialogEx)

CSOHDlg::CSOHDlg(CWnd* pParent)
    : CDialogEx(IDD_SOH, pParent)
    , m_lastPercent(-1)
    , m_lastDropTime(0)
    , m_isFullyCharged(false)
    , m_isDischarging(false)
    , m_startDischargeTime(0)
    , m_stressRunning(false)
{
}

CSOHDlg::~CSOHDlg()
{
    StopCpuStress();
    if (m_logFile.is_open())
        m_logFile.close();
}

void CSOHDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CSOHDlg, CDialogEx)
    ON_WM_PAINT()
    ON_WM_TIMER()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()

BOOL CSOHDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string exePath(path);
    size_t pos = exePath.find_last_of("\\/");
    std::string folder = exePath.substr(0, pos);
    std::string fullPath = folder + "\\soh_log.txt";
    m_logFile.open(fullPath, std::ios::out | std::ios::app);

    time_t now = time(NULL);
    CString temp;
    temp.Format(_T("%lld"), (long long)now);
    CT2A ascii(temp);
    m_testID = std::string(ascii);

    SYSTEMTIME st;
    GetLocalTime(&st);

    if (m_logFile.is_open())
    {
        m_logFile << "\n=====================================================\n";
        m_logFile << "                NEW SOH TEST\n";
        m_logFile << "TEST_ID: " << m_testID << "\n";
        m_logFile << "Start Time: "
            << st.wYear << "-" << st.wMonth << "-" << st.wDay << " "
            << st.wHour << ":" << st.wMinute << ":" << st.wSecond << "\n";
        m_logFile << "=====================================================\n";
        m_logFile.flush();
    }

    CRect rcClient;
    GetClientRect(&rcClient);

    m_fontHeader.CreatePointFont(160, _T("Segoe UI"));
    m_fontPercent.CreatePointFont(220, _T("Segoe UI"));
    m_fontInstruction.CreatePointFont(90, _T("Segoe UI"));

    CRect rcInstr(rcClient.left + 10, rcClient.top + 10, rcClient.right - 10, rcClient.top + 120);
    m_lblInstruction.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_CENTER, rcInstr, this);
    m_lblInstruction.SetFont(&m_fontInstruction);

    CRect rcProgress(rcClient.left + 40, rcClient.top + 140, rcClient.right - 40, rcClient.top + 190);
    m_progress.Create(WS_CHILD | WS_VISIBLE | PBS_SMOOTH, rcProgress, this, 1001);
    m_progress.SetRange(0, 100);
    m_progress.SetPos(0);

    CRect rcPerc(rcClient.left + 10, rcClient.top + 200, rcClient.right - 10, rcClient.top + 420);
    m_lblPercent.Create(_T("0%"), WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_CENTER, rcPerc, this);
    m_lblPercent.SetFont(&m_fontPercent);

    CRect rcElapsed(rcClient.left + 10, rcClient.bottom - 90, rcClient.right - 10, rcClient.bottom - 10);
    m_lblElapsed.Create(_T(""), WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE | SS_CENTER, rcElapsed, this);
    m_lblElapsed.SetFont(&m_fontInstruction);

    m_instruction = _T("Please charge battery to 100%");
    m_lastDropTime = GetTickCount64();

    SetTimer(1, 1000, NULL);

    m_isFullyCharged = true; // (remove later)

    UpdateUI(0);

    return TRUE;
}

void CSOHDlg::OnTimer(UINT_PTR nIDEvent)
{
    UpdateBatteryStatus();
    Invalidate();
    CDialogEx::OnTimer(nIDEvent);
}

void CSOHDlg::UpdateBatteryStatus()
{
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps))
        return;

    int percent = sps.BatteryLifePercent;
    ULONGLONG now = GetTickCount64();

    if (percent == 100 && sps.ACLineStatus == 1)
    {
        m_isFullyCharged = true;
        UpdateInstruction();
    }

    if (m_isFullyCharged && sps.ACLineStatus == 0)
    {
        if (!m_isDischarging)
        {
            m_isDischarging = true;
            m_lastDropTime = now;
            m_startDischargeTime = now;
            StartCpuStress();
        }
        UpdateInstruction();
    }

    if (m_isDischarging && m_lastPercent != -1 && percent < m_lastPercent)
    {
        ULONGLONG duration = now - m_lastDropTime;
        LogData(percent, duration);
        m_lastDropTime = now;
    }

    UpdateUI(percent);
    m_lastPercent = percent;

    if (percent <= 1 && m_isDischarging)
    {
        KillTimer(1);
        m_instruction = _T("Test completed. Data saved.");
        UpdateInstruction();
        UpdateUI(percent);
        StopCpuStress();
    }

    if (m_isDischarging && sps.ACLineStatus == 1)
    {
        if (m_logFile.is_open())
        {
            SYSTEMTIME st;
            GetLocalTime(&st);
            m_logFile << "[" << m_testID << "] "
                << "TEST STOPPED: Charger plugged in at "
                << st.wHour << ":" << st.wMinute << ":" << st.wSecond
                << " | Percent: " << percent << std::endl;
            m_logFile.flush();
        }

        KillTimer(1);
        m_instruction = _T("Test stopped: Charger plugged in. Closing...");
        UpdateInstruction();
        UpdateUI(percent);
        StopCpuStress();
        PostMessage(WM_CLOSE);
    }
}

void CSOHDlg::LogData(int percent, ULONGLONG duration)
{
    if (!m_logFile.is_open())
        return;

    ULONGLONG totalSec = duration / 1000;
    int hrs = static_cast<int>(totalSec / 3600);
    int mins = static_cast<int>((totalSec % 3600) / 60);
    int secs = static_cast<int>(totalSec % 60);

    SYSTEMTIME st;
    GetLocalTime(&st);

    m_logFile << "[" << m_testID << "] "
        << "Time: "
        << st.wHour << ":" << st.wMinute << ":" << st.wSecond
        << " | Percent: " << percent
        << " | Duration(ms): " << duration
        << " | Duration(h:m:s): "
        << hrs << ":" << mins << ":" << secs
        << std::endl;
    m_logFile.flush();
}

void CSOHDlg::UpdateInstruction()
{
    if (!m_isFullyCharged)
        m_instruction = _T("Please charge battery to 100%");
    else if (!m_isDischarging)
        m_instruction = _T("FULLY CHARGED.\nUNPLUG CHARGER AND LET IT DISCHARGE.");
    else
        m_instruction = _T("Discharging... Recording data.");

    if (m_lblInstruction.GetSafeHwnd())
        m_lblInstruction.SetWindowTextW(m_instruction);
}

void CSOHDlg::UpdateUI(int percent)
{
    if (m_progress.GetSafeHwnd())
    {
        int clamped = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
        m_progress.SetPos(clamped);
    }

    if (m_lblPercent.GetSafeHwnd())
    {
        CString pct;
        pct.Format(_T("%d%%"), (percent < 0 ? 0 : percent));
        m_lblPercent.SetWindowTextW(pct);
    }

    if (m_lblElapsed.GetSafeHwnd())
    {
        if (m_isDischarging && m_startDischargeTime != 0)
        {
            ULONGLONG now = GetTickCount64();
            ULONGLONG totalSec = (now - m_startDischargeTime) / 1000;
            int hrs = static_cast<int>(totalSec / 3600);
            int mins = static_cast<int>((totalSec % 3600) / 60);
            int secs = static_cast<int>(totalSec % 60);

            CString elapsed;
            elapsed.Format(_T("Elapsed: %02d:%02d:%02d"), hrs, mins, secs);
            m_lblElapsed.SetWindowTextW(elapsed);
        }
        else
        {
            m_lblElapsed.SetWindowTextW(_T("Not discharging"));
        }
    }

    if (m_lblInstruction.GetSafeHwnd())
        m_lblInstruction.SetWindowTextW(m_instruction);
}

void CSOHDlg::OnPaint()
{
    CPaintDC dc(this);
    CRect rect;
    GetClientRect(&rect);
    CBrush brTop(RGB(245, 247, 250));
    dc.FillRect(&rect, &brTop);
}

void CSOHDlg::OnClose()
{
    KillTimer(1);
    StopCpuStress();
    if (m_logFile.is_open())
        m_logFile.close();
    CDialogEx::OnClose();
}

void CSOHDlg::OnDestroy()
{
    KillTimer(1);
    StopCpuStress();
    if (m_logFile.is_open())
        m_logFile.close();
    CDialogEx::OnDestroy();
}

// ============================================================
//  CPU STRESS  —  100% usage, UI never hangs
//
//  KEY INSIGHT:
//  Stress threads run THREAD_PRIORITY_IDLE (= -15, the absolute
//  floor).  The MFC UI thread runs at THREAD_PRIORITY_NORMAL.
//  Windows preemptive scheduler ALWAYS gives the UI its quantum
//  whenever it needs one, regardless of how many IDLE threads
//  are spinning.  So the infinite loop below is safe.
// ============================================================
void CSOHDlg::StartCpuStress()
{
    if (m_stressRunning.load(std::memory_order_acquire))
        return;

    unsigned int cores = std::thread::hardware_concurrency();
    if (cores == 0) cores = 1;

    m_stressRunning.store(true, std::memory_order_release);
    m_stressThreads.clear();
    m_stressThreads.reserve(cores);

    for (unsigned int i = 0; i < cores; ++i)
    {
        std::thread t([this]()
            {
                // Absolute lowest priority — UI (NORMAL) always preempts us
                ::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_IDLE);

                // Pin to one logical processor so every core shows 100%
                // (affinity is also set outside after the thread is created)

                // Volatile accumulator — compiler cannot eliminate this loop
                volatile double acc = 1.0;

                while (m_stressRunning.load(std::memory_order_relaxed))
                {
                    // Pure FP burn — no sleep, no yield, no clock calls
                    // This is genuinely 100% busy on this core.
                    for (int j = 0; j < 100000; ++j)
                    {
                        acc += std::sqrt(acc + static_cast<double>(j) + 1.0000001);
                        if (acc > 1e15) acc = 1.0;   // stay finite, keep FPU hot
                    }
                    // Only memory-order_relaxed read — cheapest possible flag check
                    // happens roughly every ~few ms of real work, not every iteration
                }
            });

        // Distribute each thread to its own logical processor
        DWORD_PTR mask = static_cast<DWORD_PTR>(1) << (i % (sizeof(DWORD_PTR) * 8));
        SetThreadAffinityMask(t.native_handle(), mask);

        m_stressThreads.emplace_back(std::move(t));
    }
}

void CSOHDlg::StopCpuStress()
{
    if (!m_stressRunning.load(std::memory_order_acquire))
        return;

    m_stressRunning.store(false, std::memory_order_release);

    for (auto& t : m_stressThreads)
    {
        if (t.joinable())
        {
            try { t.join(); }
            catch (...) {}
        }
    }

    m_stressThreads.clear();
}