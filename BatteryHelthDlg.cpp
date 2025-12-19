// BatteryHelthDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "BatteryHelth.h"
#include "BatteryHelthDlg.h"
#include "afxdialogex.h"

#include <windows.h>
#include <atlbase.h>
#include <comdef.h>
#include <Wbemidl.h>

#include <thread>
#include <vector>
#include <cmath>

#include <fstream> 

#include <memory>
#include <atomic>

#include <immintrin.h> 
#include <chrono>
#include <afxwin.h> 
#include <intrin.h> 

#include <pdh.h>
#include <pdhmsg.h>

#include <set>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")


#pragma comment(lib, "pdh.lib")

#pragma comment(lib, "wbemuuid.lib")

#include <regex>

#include "CTrendDlg.h"

#include "StandByDlg.h"

#include "CPerfDlg.h"

#include "CDischargeDlg.h"

#include "CPredictionDlg.h"

#include "CManipulationDlg.h"

#include "CRateInfoDlg.h"

#include"CSleepDataDlg.h"


#include <string>
#include <algorithm>
#include <cwchar>      // _wcsnicmp

#include <psapi.h>      // EnumProcesses, QueryFullProcessImageNameW
#include <winver.h>     // GetFileVersionInfo*, VerQueryValue
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "Version.lib")


#ifdef _DEBUG
#define new DEBUG_NEW
#endif


#include <wingdi.h>
#pragma comment(lib, "msimg32.lib")


#include <gdiplus.h>
using namespace Gdiplus;
#pragma comment(lib, "gdiplus.lib")


// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
    CAboutDlg();

    // Dialog Data
#ifdef AFX_DESIGN_TIME
    enum { IDD = IDD_ABOUTBOX };
#endif

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

    // Implementation
protected:
    DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// CBatteryHelthDlg dialog



CBatteryHelthDlg::CBatteryHelthDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_BATTERYHELTH_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_ICON);
}

void CBatteryHelthDlg::DoDataExchange(CDataExchange* pDX)
{
    CDialogEx::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_BATT_PROGRESS, m_BatteryProgress);
    DDX_Control(pDX, IDC_STATIC_BBI, m_bb);
    DDX_Control(pDX, IDC_STATIC_ABT, m_abt);
    DDX_Control(pDX, IDC_STATIC_DH, m_dh);
    DDX_Control(pDX, IDC_STATIC_HEADER, m_header);
    DDX_Control(pDX, IDC_PROGRESS4, m_CPU_Progress);

    DDX_Control(pDX, IDC_PROGRESS5, m_discharge_progress);
    DDX_Control(pDX, IDC_BTN_CPULOAD, Button1);
}

BEGIN_MESSAGE_MAP(CBatteryHelthDlg, CDialogEx)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_TIMER()
    ON_BN_CLICKED(IDC_BTN_DISCHARGE, &CBatteryHelthDlg::OnBnClickedBtnDischarge)
    ON_BN_CLICKED(IDC_BTN_CPULOAD, &CBatteryHelthDlg::OnBnClickedBtnCpuload)
    ON_MESSAGE(WM_APP + 1, &CBatteryHelthDlg::OnCPULoadFinished)
    ON_BN_CLICKED(IDC_BTN_UPLOADPDF, &CBatteryHelthDlg::OnBnClickedBtnUploadpdf)
    ON_BN_CLICKED(IDC_BTN_HISTORY, &CBatteryHelthDlg::OnBnClickedBtnHistory)
    ON_STN_CLICKED(IDC_STATIC_HEADER, &CBatteryHelthDlg::OnStnClickedStaticHeader)

    ON_WM_CTLCOLOR()

    ON_WM_DRAWITEM()
    ON_WM_SETCURSOR()

    ON_WM_SIZE()

    ON_WM_ERASEBKGND()

    ON_WM_DESTROY()      // For cleanup

    ON_MESSAGE(WM_DPICHANGED, &CBatteryHelthDlg::OnDpiChanged)

    ON_BN_CLICKED(IDC_BTN_CAPHIS, &CBatteryHelthDlg::OnBnClickedBtnCaphis)
    //ON_BN_CLICKED(IDC_BTN_PREDICTION, &CBatteryHelthDlg::OnBnClickedBtnPrediction)
    ON_BN_CLICKED(IDC_BTN_ACTIVE, &CBatteryHelthDlg::OnBnClickedBtnActive)
    ON_BN_CLICKED(IDC_BTN_STANDBY, &CBatteryHelthDlg::OnBnClickedBtnStandby)

    ON_BN_CLICKED(IDC_BTN_USAGE, &CBatteryHelthDlg::OnBnClickedBtnUsage)
    ON_BN_CLICKED(IDC_BTN_MANIPULATIOIN, &CBatteryHelthDlg::OnBnClickedBtnManipulatioin)
    ON_BN_CLICKED(IDC_BTN_BGAPP, &CBatteryHelthDlg::OnBnClickedBtnBgapp)
    ON_BN_CLICKED(IDC_BTN_RATEINFO, &CBatteryHelthDlg::OnBnClickedBtnRateinfo)
    ON_BN_CLICKED(IDC_BTN_EN, &CBatteryHelthDlg::OnBnClickedBtnEn)
    ON_BN_CLICKED(IDC_BTN_JP, &CBatteryHelthDlg::OnBnClickedBtnJp)
    ON_BN_CLICKED(IDC_BTN_SLEEP, &CBatteryHelthDlg::OnBnClickedBtnSleep)

    ON_WM_POWERBROADCAST()   // main dialog is the single logger
END_MESSAGE_MAP()

// CBatteryHelthDlg message handlers



static void BuildSeries(float initialPct, float currentPct, float ratePctPerMin,
    std::vector<float>& timesMin,
    std::vector<float>& percents)
{
    timesMin.clear(); percents.clear();
    if (ratePctPerMin <= 0.f) {
        timesMin.push_back(0.f);
        percents.push_back(initialPct);
        return;
    }

    const float totalDrop = initialPct;  // drop to 0
    const float totalMin = totalDrop / ratePctPerMin;

    int fullSteps = (int)floorf(totalMin);
    for (int m = 0; m <= fullSteps; ++m) {
        timesMin.push_back((float)m);
        float v = initialPct - ratePctPerMin * m;
        percents.push_back(max(0.f, v));
    }

    // ensure last = 0%
    if (percents.back() > 0.f) {
        timesMin.push_back(totalMin);
        percents.push_back(0.f);
    }
}



void CBatteryHelthDlg::CalculateDPIScale()
    {
        // 1) Get DPI for THIS window
        UINT dpi = 96;
        if (m_hWnd) {
            HMODULE hUser32 = ::GetModuleHandleW(L"user32.dll");
            if (hUser32) {
                using GetDpiForWindow_t = UINT(WINAPI*)(HWND);
                auto pGetDpiForWindow = reinterpret_cast<GetDpiForWindow_t>(
                    ::GetProcAddress(hUser32, "GetDpiForWindow"));
                if (pGetDpiForWindow) {
                    dpi = pGetDpiForWindow(m_hWnd);
                }
                else {
                    HDC hdc = ::GetDC(m_hWnd);
                    if (hdc) {
                        dpi = static_cast<UINT>(::GetDeviceCaps(hdc, LOGPIXELSX));
                        ::ReleaseDC(m_hWnd, hdc);
                    }
                }
            }
        }
        if (dpi == 0) dpi = 96;
        m_dpiScaleFactor = static_cast<double>(dpi) / 96.0;

        // 2) Get screen dimensions
        RECT workArea;
        SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);
        int screenWidth = workArea.right - workArea.left;
        int screenHeight = workArea.bottom - workArea.top;

        // 3) Get dialog base size
        CRect rcClient;
        GetClientRect(&rcClient);
        int w = rcClient.Width();
        int h = rcClient.Height();
        if (w <= 0) w = 1;
        if (h <= 0) h = 1;

        if (m_baseWidth <= 0 || m_baseHeight <= 0) {
            m_baseWidth = w;
            m_baseHeight = h;
        }

        // 4) Calculate scaled dimensions
        int scaledWidth = static_cast<int>(m_baseWidth * m_dpiScaleFactor);
        int scaledHeight = static_cast<int>(m_baseHeight * m_dpiScaleFactor);

        // 5) Apply adaptive strategy based on screen size
        const int SAFE_MARGIN = 80; // For window borders, taskbar, etc.
        int availableWidth = screenWidth - SAFE_MARGIN;
        int availableHeight = screenHeight - SAFE_MARGIN;

        bool needsAdjustment = false;
        double adjustedScale = m_dpiScaleFactor;

        // Strategy 1: If dialog is much larger than screen, reduce DPI scaling
        if (scaledHeight > availableHeight || scaledWidth > availableWidth)
        {
            double heightScale = (double)availableHeight / m_baseHeight;
            double widthScale = (double)availableWidth / m_baseWidth;
            double maxFitScale = min(heightScale, widthScale);

            // Only reduce DPI scale, never increase it beyond original
            if (maxFitScale < m_dpiScaleFactor)
            {
                adjustedScale = maxFitScale;
                needsAdjustment = true;

                CString msg;
                msg.Format(_T("Screen too small. Adjusted DPI from %.2f to %.2f\n"),
                    m_dpiScaleFactor, adjustedScale);
                OutputDebugString(msg);
            }
        }

        // Strategy 2: For very small screens (< 800x600), use minimum scale
        if (screenHeight < 600 || screenWidth < 800)
        {
            adjustedScale = min(adjustedScale, 1);
            needsAdjustment = false;

            OutputDebugString(_T("Very small screen detected. Using minimum scale.\n"));
        }

        // Strategy 3: Clamp to reasonable bounds
        adjustedScale = max(0.5, min(adjustedScale, 1.5));

        // Apply adjusted scale
        if (needsAdjustment)
        {
            m_dpiScaleFactor = adjustedScale;

            // Show user-friendly message
            if (screenHeight < 768) // Small screen detected
            {
                CString userMsg;
                userMsg.Format(
                    _T("Small screen detected (%dx%d).\n")
                    _T("Application has been scaled to fit.\n")
                    _T("Some controls may appear smaller than intended."),
                    screenWidth, screenHeight);

                // Optional: Show this message once per session
                // MessageBox(userMsg, _T("Display Notice"), MB_OK | MB_ICONINFORMATION);
                OutputDebugString(userMsg);
            }
        }
    }

int CBatteryHelthDlg::ScaleDPI(int value)
{
    return static_cast<int>(value * m_dpiScaleFactor);
}

// Create fonts with scaled sizes
void CBatteryHelthDlg::CreateScaledFonts()
{
    // Delete old fonts if they exist
    if (m_fontNormal.m_hObject) m_fontNormal.DeleteObject();
    if (m_fontBold.m_hObject) m_fontBold.DeleteObject();
    if (m_fontHeader.m_hObject) m_fontHeader.DeleteObject();
    if (m_fontSmall.m_hObject) m_fontSmall.DeleteObject();

    // Define your base font sizes (from your original design at 100% scale)
    m_baseFontSize = 16;        // Main text
    m_baseHeaderFontSize = 18;  // Headers
    m_baseSmallFontSize = 10;   // Small text

    // Calculate scaled font sizes
    int scaledNormalSize = ScaleDPI(m_baseFontSize);
    int scaledHeaderSize = ScaleDPI(m_baseHeaderFontSize);
    int scaledSmallSize = ScaleDPI(m_baseSmallFontSize);

    // Create Normal Font
    m_fontNormal.CreateFont(
        scaledNormalSize,           // Height (scaled)
        0,                          // Width
        0,                          // Escapement
        0,                          // Orientation
        FW_NORMAL,                  // Weight
        FALSE,                      // Italic
        FALSE,                      // Underline
        0,                          // StrikeOut
        DEFAULT_CHARSET,            // CharSet
        OUT_DEFAULT_PRECIS,         // OutPrecision
        CLIP_DEFAULT_PRECIS,        // ClipPrecision
        CLEARTYPE_QUALITY,          // Quality
        DEFAULT_PITCH | FF_SWISS,   // PitchAndFamily
        _T("Segoe UI")              // Facename
    );

    // Create Bold Font
    m_fontBold.CreateFont(
        scaledNormalSize,
        0, 0, 0,
        FW_BOLD,                    // Bold weight
        FALSE, FALSE, 0,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        _T("Segoe UI")
    );

    // Create Header Font (larger and bold)
    m_fontHeader.CreateFont(
        scaledHeaderSize,
        0, 0, 0,
        FW_BOLD,
        FALSE, FALSE, 0,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        _T("Segoe UI")
    );

    // Create Small Font
    m_fontSmall.CreateFont(
        scaledSmallSize,
        0, 0, 0,
        FW_NORMAL,
        FALSE, FALSE, 0,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_SWISS,
        _T("Segoe UI")
    );
}

// Apply scaled fonts to controls
void CBatteryHelthDlg::ApplyScaledFonts()
{
    // Apply header font to header controls
    if (CWnd* pWnd = GetDlgItem(IDC_STATIC_HEADER))
        pWnd->SetFont(&m_fontHeader);

    // Apply bold font to labels
    UINT boldIds[] = {
        IDC_BATT_STATUS, IDC_BATT_TIME, IDC_BATT_PERCENTAGE,
            IDC_BATT_CAPACITY, IDC_BATT_NAME, IDC_BATT_DCAPACITY,
            IDC_BATT_MANUFAC, IDC_BATT_CYCLE, IDC_BATT_HEALTH,
            IDC_BATT_VOLTAGE, IDC_BATT_TEMP, IDC_BATT_CURRCAPACITY,
            IDC_STATIC_DH, IDC_STATIC_ABT, IDC_STATIC_BBI, 
    };

    for (UINT id : boldIds)
    {
        if (CWnd* pWnd = GetDlgItem(id))
            pWnd->SetFont(& m_fontBold);
    }

    // Apply normal font to value displays
    UINT normalIds[] = {
        IDC_STATIC_STATUS, IDC_STATIC_TIME, IDC_STATIC_PERCENTAGE,
        IDC_STATIC_CAPACITY, IDC_STATIC_NAME, IDC_STATIC_DCAPACITY,
        IDC_STATIC_MANUFAC, IDC_STATIC_CYCLE, IDC_STATIC_HEALTH,
        IDC_STATIC_VOLTAGE, IDC_STATIC_TEMP, IDC_STATIC_CURRCAPACITY,
        IDC_BATT_DID
    };

    for (UINT id : normalIds)
    {
        if (CWnd* pWnd = GetDlgItem(id))
            pWnd->SetFont(&m_fontNormal);
    }

    // Apply normal font to buttons
    UINT buttonIds[] = {
        IDC_BTN_CPULOAD, IDC_BTN_DISCHARGE,
        IDC_BTN_HISTORY, IDC_BTN_UPLOADPDF
    };

    for (UINT id : buttonIds)
    {
        if (CWnd* pWnd = GetDlgItem(id))
            pWnd->SetFont(&m_fontNormal);
    }

    // Apply small font to any small text controls (if you have any)
    // if (CWnd* pWnd = GetDlgItem(IDC_SOME_SMALL_TEXT))
    //     pWnd->SetFont(&m_fontSmall);
}

void CBatteryHelthDlg::ScaleDialog()
{
    // Calculate new dimensions
    int newWidth = ScaleDPI(m_baseWidth);
    int newHeight = ScaleDPI(m_baseHeight);

    // Resize the dialog
    SetWindowPos(NULL, 0, 0, newWidth, newHeight,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    // Center the dialog
    CenterWindow();

    // Scale all child controls
    UINT ids[] = {
        IDC_BTN_CPULOAD, IDC_BTN_DISCHARGE, IDC_BTN_HISTORY, IDC_BTN_UPLOADPDF,
        IDC_STATIC_STATUS, IDC_BATT_STATUS, IDC_STATIC_TIME, IDC_BATT_TIME,
        IDC_BATT_DISCHARGR, IDC_STATIC_DH, IDC_BATT_CPULOAD, IDC_STATIC_ABT,
        IDC_PROGRESS4, IDC_STATIC_BBI, IDC_STATIC_HEADER, IDC_BATT_DID,
        IDC_STATIC_PERCENTAGE, IDC_BATT_PROGRESS, IDC_BATT_PERCENTAGE,
        IDC_STATIC_CAPACITY, IDC_BATT_CAPACITY, IDC_STATIC_NAME, IDC_BATT_NAME,
        IDC_STATIC_DCAPACITY, IDC_BATT_DCAPACITY, IDC_STATIC_MANUFAC,
        IDC_BATT_MANUFAC, IDC_STATIC_CYCLE, IDC_BATT_CYCLE, IDC_STATIC_HEALTH,
        IDC_BATT_HEALTH, IDC_STATIC_VOLTAGE, IDC_BATT_VOLTAGE, IDC_STATIC_TEMP,
        IDC_BATT_TEMP, IDC_PROGRESS5, IDC_STATIC_CURRCAPACITY, IDC_BATT_CURRCAPACITY,
		IDC_BTN_CAPHIS, IDC_BTN_ACTIVE, IDC_BTN_STANDBY, IDC_BTN_MANIPULATIOIN,
		IDC_BTN_RATEINFO, IDC_BTN_BGAPP, IDC_BTN_USAGE, IDC_BTN_SLEEP
    };

    for (auto id : ids)
    {
        CWnd* pWnd = GetDlgItem(id);
        if (pWnd && pWnd->GetSafeHwnd())
        {
            // Find original position
            for (const auto& pos : m_origPositions)
            {
                if (pos.id == id)
                {
                    CRect newRect;
                    newRect.left = ScaleDPI(pos.rect.left);
                    newRect.top = ScaleDPI(pos.rect.top);
                    newRect.right = ScaleDPI(pos.rect.right);
                    newRect.bottom = ScaleDPI(pos.rect.bottom);

                    pWnd->MoveWindow(&newRect);
                    break;
                }
            }
        }
    }
}



LRESULT CBatteryHelthDlg::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
    UINT newDpi = LOWORD(wParam);
    m_dpiScaleFactor = static_cast<double>(newDpi) / 96.0;

    // Recreate fonts and rescale controls relative to the same base
    CreateScaledFonts();
    ApplyScaledFonts();
    ScaleDialog();   // uses m_baseWidth/Height as design reference



    return 0;
}
 


static void UpdateLabel(CWnd* pDlg, int ctrlId, const CString& text)
{
    if (!pDlg) return;

    CWnd* pCtl = pDlg->GetDlgItem(ctrlId);
    if (!pCtl || !::IsWindow(pCtl->GetSafeHwnd())) return;

    // -------------------------------
    // 1) Determine base (design) size
    // -------------------------------
    static SIZE s_base = { 0, 0 };
    CSize base(0, 0);

    if (s_base.cx == 0 || s_base.cy == 0)
    {
        CRect rc0; pDlg->GetClientRect(&rc0);
        s_base.cx = rc0.Width();
        s_base.cy = rc0.Height();
        if (s_base.cx <= 0) s_base.cx = 1;
        if (s_base.cy <= 0) s_base.cy = 1;
    }
    base.cx = s_base.cx;
    base.cy = s_base.cy;

    // -------------------------------
    // 2) Compute uniform scale factor
    // -------------------------------
    CRect rcNow; pDlg->GetClientRect(&rcNow);
    double scaleX = static_cast<double>(rcNow.Width()) / static_cast<double>(base.cx);
    double scaleY = static_cast<double>(rcNow.Height()) / static_cast<double>(base.cy);

    double scale = (scaleX < scaleY) ? scaleX : scaleY;

    if (scale < 0.75) scale = 0.75;
    if (scale > 1.50) scale = 1.50;

    // -------------------------------
    // 3) Build/apply scaled font
    // -------------------------------
    const bool needsScaledSemiBold =
        (ctrlId == IDC_BATT_DISCHARGR) || (ctrlId == IDC_BATT_CPULOAD);

    if (needsScaledSemiBold)
    {
        const int baseHeight = -13;
        int targetHeight = static_cast<int>(::lround(baseHeight * scale));
        if (targetHeight > -12) targetHeight = -12;
        if (targetHeight < -28) targetHeight = -28;

        static CFont s_fontCpuLoad;         static int s_hCpuLoad = 0;
        static CFont s_fontDischarge;       static int s_hDischarge = 0;

        CFont* pFontObj = nullptr;
        int* pLastH = nullptr;

        if (ctrlId == IDC_BATT_CPULOAD) {
            pFontObj = &s_fontCpuLoad;      pLastH = &s_hCpuLoad;
        }
        else {
            pFontObj = &s_fontDischarge;    pLastH = &s_hDischarge;
        }

        if (!pFontObj->GetSafeHandle() || *pLastH != targetHeight)
        {
            LOGFONT lf; ::ZeroMemory(&lf, sizeof(lf));
            if (CFont* pOld = pCtl->GetFont()) pOld->GetLogFont(&lf);
            else ::GetObject(::GetStockObject(DEFAULT_GUI_FONT), sizeof(lf), &lf);

            lf.lfHeight = targetHeight;
            lf.lfWeight = 600;

            pFontObj->DeleteObject();
            pFontObj->CreateFontIndirect(&lf);
            *pLastH = targetHeight;
        }

        pCtl->SetFont(pFontObj, FALSE);
    }

    // -------------------------------
    // 4) Ensure WS_EX_TRANSPARENT style (one-time setup)
    // -------------------------------
    static BOOL s_cpuLoadTransparent = FALSE;
    static BOOL s_dischargeTransparent = FALSE;

    BOOL* pTransFlag = nullptr;
    if (ctrlId == IDC_BATT_CPULOAD)
        pTransFlag = &s_cpuLoadTransparent;
    else if (ctrlId == IDC_BATT_DISCHARGR)
        pTransFlag = &s_dischargeTransparent;

    if (pTransFlag && !(*pTransFlag))
    {
        pCtl->ModifyStyleEx(0, WS_EX_TRANSPARENT);
        *pTransFlag = TRUE;
    }

    // -------------------------------
    // 5) Check if text actually changed
    // -------------------------------
    CString cur;
    pCtl->GetWindowText(cur);
    if (cur == text) return;

    // -------------------------------
    // 6) Update text with aggressive background clearing
    // -------------------------------
    // Get control rectangle BEFORE hiding
    CRect rcCtl;
    pCtl->GetWindowRect(&rcCtl);
    pDlg->ScreenToClient(&rcCtl);

    // Expand the invalidation area to cover any antialiasing
    CRect rcInvalidate = rcCtl;
    rcInvalidate.InflateRect(5, 5);

    // Hide control temporarily
    pCtl->ShowWindow(SW_HIDE);

    // Force parent dialog to redraw the area (this redraws your gradient)
    pDlg->InvalidateRect(&rcInvalidate, TRUE);
    pDlg->UpdateWindow(); // Force immediate redraw

    // Update the text while hidden
    pCtl->SetWindowText(text);

    // Show and invalidate the control
    pCtl->ShowWindow(SW_SHOW);
    pCtl->Invalidate(TRUE); // TRUE = erase background
    pCtl->UpdateWindow();
}




namespace {
    constexpr UINT  IDT_NOTIFY_LONGRUN = 2001;
    constexpr UINT  NOTIFY_INTERVAL_MS = 30*60000;             // every 30*1min = 30 mins
    constexpr ULONGLONG MIN_UPTIME_SEC = 15ULL * 60ULL;     // 15 minutes (requirement)
    constexpr ULONGLONG IDLE_THRESHOLD_SEC = 5ULL * 60ULL;  // 5 minutes = idle
}

// ----------------- helpers -----------------
struct Row {
    std::wstring name;
    DWORD        pid{};
    FILETIME     startFT{};
    ULONGLONG    uptimeSec{};
    std::wstring title;
    HWND         mainWindow{};
    ULONGLONG    idleTimeSec{};
};

static bool StartsWithI(const std::wstring& s, const std::wstring& prefix) {
    if (s.size() < prefix.size()) return false;
    return _wcsnicmp(s.c_str(), prefix.c_str(), static_cast<unsigned>(prefix.size())) == 0;
}

static inline ULONGLONG FTtoU64(const FILETIME& ft) {
    ULARGE_INTEGER u; u.LowPart = ft.dwLowDateTime; u.HighPart = ft.dwHighDateTime; return u.QuadPart;
}

static CString FormatTimespan(ULONGLONG totalSec) {
    ULONGLONG d = totalSec / 86400; totalSec %= 86400;
    ULONGLONG h = totalSec / 3600;  totalSec %= 3600;
    ULONGLONG m = totalSec / 60;    ULONGLONG s = totalSec % 60;
    CString out;
    if (d) out.Format(L"%llud %02llu:%02llu:%02llu", d, h, m, s);
    else   out.Format(L"%02llu:%02llu:%02llu", h, m, s);
    return out;
}

static CString FormatFileTimeLocal(const FILETIME& ftUtc) {
    SYSTEMTIME u{}, l{}; FileTimeToSystemTime(&ftUtc, &u); SystemTimeToTzSpecificLocalTime(nullptr, &u, &l);
    CString s; s.Format(L"%02u/%02u/%04u %02u:%02u:%02u", l.wMonth, l.wDay, l.wYear, l.wHour, l.wMinute, l.wSecond);
    return s;
}

// Strip \\?\ or \??\ prefix
static std::wstring StripLongPrefix(const std::wstring& p) {
    if (StartsWithI(p, L"\\\\?\\")) return p.substr(4);
    if (StartsWithI(p, L"\\??\\"))  return p.substr(4);
    return p;
}

// Map NT device path (\Device\HarddiskVolumeX\...) -> DOS drive (X:\...)
static std::wstring NtToDosPath(const std::wstring& pathIn) {
    std::wstring p = StripLongPrefix(pathIn);
    if (p.size() >= 3 && p[1] == L':' && (p[2] == L'\\' || p[2] == L'/')) return p;
    if (!StartsWithI(p, L"\\Device\\")) return p;

    wchar_t drives[512] = {};
    DWORD n = GetLogicalDriveStringsW(_countof(drives) - 1, drives);
    for (wchar_t* d = drives; n && *d; d += wcslen(d) + 1) {
        wchar_t root[3] = { d[0], L':', 0 };
        wchar_t dev[512] = {};
        if (QueryDosDeviceW(root, dev, _countof(dev))) {
            size_t len = wcslen(dev);
            if (len && _wcsnicmp(p.c_str(), dev, (unsigned)len) == 0) {
                std::wstring tail = p.substr(len);
                if (!tail.empty() && tail[0] != L'\\') tail.insert(tail.begin(), L'\\');
                return std::wstring(root) + tail;
            }
        }
    }
    return p;
}

// Visible main window for a PID - returns title and HWND
struct WindowSearchResult { std::wstring title; HWND hwnd; };

static WindowSearchResult GetMainWindowAndTitleByPid(DWORD targetPid) {
    WindowSearchResult result; result.hwnd = nullptr;

    struct ED { DWORD pid; std::wstring* outTitle; HWND* outHwnd; } ed{ targetPid, &result.title, &result.hwnd };
    auto cb = [](HWND hWnd, LPARAM lp)->BOOL {
        auto* d = reinterpret_cast<ED*>(lp);
        if (!IsWindowVisible(hWnd)) return TRUE;
        DWORD pid = 0; GetWindowThreadProcessId(hWnd, &pid);
        if (pid != d->pid) return TRUE;

        HWND root = GetAncestor(hWnd, GA_ROOTOWNER);
        if (root && root != hWnd) return TRUE;

        wchar_t buf[512] = {};
        int len = GetWindowTextW(hWnd, buf, _countof(buf));
        if (len <= 0) return TRUE;

        d->outTitle->assign(buf, buf + len);
        *(d->outHwnd) = hWnd;
        return FALSE;
        };
    EnumWindows(cb, reinterpret_cast<LPARAM>(&ed));
    return result;
}



BOOL CBatteryHelthDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Initialize COM
    HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres))
    {
        AfxMessageBox(L"Failed to initialize COM.");
        return FALSE;
    }

    hres = CoInitializeSecurity(
        NULL, -1, NULL, NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE, NULL
    );

    if (FAILED(hres))
    {
        AfxMessageBox(L"Failed to initialize COM security.");
        CoUninitialize();
        return FALSE;
    }

    // Battery progress
    m_BatteryProgress.SetRange(0, 100);
    m_BatteryProgress.SetPos(0);

    // CPU progress
    m_CPU_Progress.SetRange(0, 100);
    m_CPU_Progress.SetPos(0);
    m_CPU_Progress.SetStep(1);
    m_CPU_Progress.ModifyStyle(0, PBS_SMOOTH);
    ::SetWindowTheme(m_CPU_Progress.GetSafeHwnd(), L"", L"");
    m_CPU_Progress.SetBarColor(RGB(0, 122, 204));
    m_CPU_Progress.SetBkColor(RGB(220, 220, 220));
    m_CPU_Progress.ShowWindow(SW_HIDE);

    // Discharge progress
    m_discharge_progress.SetRange(0, 100);
    m_discharge_progress.SetPos(0);
    m_discharge_progress.SetStep(1);
    m_discharge_progress.ModifyStyle(0, PBS_SMOOTH);
    ::SetWindowTheme(m_discharge_progress.GetSafeHwnd(), L"", L"");
    m_discharge_progress.SetBarColor(RGB(0, 122, 204));
    m_discharge_progress.SetBkColor(RGB(220, 220, 220));
    m_discharge_progress.ShowWindow(SW_HIDE);

    m_stopCpuLoad.store(false);



 /*   UpdateLabel(this, IDC_BATT_CPULOAD, L"CPU Load Not Tested");
    UpdateLabel(this, IDC_BATT_DISCHARGR, L"Discharge Not Tested");*/


	GetStaticBatteryInfo(); // Initial fetch

    // Get battery info + start timer
    GetBatteryInfo();
    SetTimer(2, 1000, NULL);

    // Add "About..." menu item
    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != nullptr)
    {
        CString strAboutMenu;
        if (strAboutMenu.LoadString(IDS_ABOUTBOX) && !strAboutMenu.IsEmpty())
        {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    SetIcon(m_hIcon, TRUE);
    SetIcon(m_hIcon, FALSE);

    // Fix window style BEFORE scaling
    LONG style = GetWindowLong(this->m_hWnd, GWL_STYLE);
    style &= ~WS_THICKFRAME;
    style |= WS_MINIMIZEBOX;
    style |= WS_SYSMENU;
    SetWindowLong(this->m_hWnd, GWL_STYLE, style);

    // Save original positions BEFORE scaling
    CRect dialogRect;
    GetClientRect(&dialogRect);
    m_origDialogSize = CSize(dialogRect.Width(), dialogRect.Height());

    UINT ids[] = {
        IDC_BTN_CPULOAD, IDC_BTN_DISCHARGE, IDC_BTN_HISTORY, IDC_BTN_UPLOADPDF,
        IDC_STATIC_STATUS, IDC_BATT_STATUS, IDC_STATIC_TIME, IDC_BATT_TIME,
        IDC_BATT_DISCHARGR, IDC_STATIC_DH, IDC_BATT_CPULOAD, IDC_STATIC_ABT,
        IDD_BATTERYHELTH_DIALOG, IDC_PROGRESS4, IDC_STATIC_BBI, IDC_STATIC_HEADER,
        IDC_BATT_DID, IDC_STATIC_PERCENTAGE, IDC_BATT_PROGRESS, IDC_BATT_PERCENTAGE,
        IDC_STATIC_CAPACITY, IDC_BATT_CAPACITY, IDC_STATIC_NAME, IDC_BATT_NAME,
        IDC_STATIC_DCAPACITY, IDC_BATT_DCAPACITY, IDC_STATIC_MANUFAC,
        IDC_BATT_MANUFAC, IDC_STATIC_CYCLE, IDC_BATT_CYCLE, IDC_STATIC_HEALTH,
        IDC_BATT_HEALTH, IDC_STATIC_VOLTAGE, IDC_BATT_VOLTAGE, IDC_STATIC_TEMP,
        IDC_BATT_TEMP, IDC_PROGRESS5, IDC_STATIC_CURRCAPACITY, IDC_BATT_CURRCAPACITY,
		IDC_BTN_CAPHIS, IDC_BTN_ACTIVE, IDC_BTN_STANDBY,IDC_BTN_USAGE,IDC_BTN_MANIPULATIOIN,
		IDC_BTN_RATEINFO, IDC_BTN_BGAPP, IDC_BTN_EN, IDC_BTN_JP, IDC_BTN_SLEEP
    };

    for (auto id : ids)
    {
        CWnd* pWnd = GetDlgItem(id);
        if (pWnd && pWnd->GetSafeHwnd())
        {
            CRect rc;
            pWnd->GetWindowRect(&rc);
            ScreenToClient(&rc);
            m_origPositions.push_back({ id, rc });
        }
    }
    m_origPositionsSaved = true;

    // *** CALCULATE DPI AND SCALE EVERYTHING ***
    CalculateDPIScale();
    ScaleDialog();

    // Create and apply SCALED fonts AFTER scaling
    CreateScaledFonts();  // <-- Use new function
    ApplyScaledFonts();   // <-- Use new function

    m_brushWhite.CreateSolidBrush(RGB(255, 255, 255));

    // Apply frame changes
    SetWindowPos(NULL, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    // Change title bar color
    COLORREF titleBarColor = RGB(70, 80, 185);
    DwmSetWindowAttribute(m_hWnd, DWMWA_CAPTION_COLOR, &titleBarColor, sizeof(titleBarColor));

    // Mark buttons as owner-draw
    for (UINT id : m_buttonIds) {
        if (CWnd* p = GetDlgItem(id)) {
            p->ModifyStyle(0, BS_OWNERDRAW);
            m_hover[id] = FALSE;
        }
    }

    SetTimer(1, 100, NULL);
    InitToolTips();


    // Initialize last input check time (kept from your code)
    m_lastInputCheck = GetTickCount64();

    // Tray + first notification immediately
    EnsureTrayIcon();
    CheckAndNotifyTopLongRunning();

    // Then keep checking every 10 seconds
    m_timerId = SetTimer(IDT_NOTIFY_LONGRUN, NOTIFY_INTERVAL_MS, nullptr);


    ///Toggle button
    // === Make the two buttons owner-drawn so we can color them ===
    if (CWnd* p = GetDlgItem(IDC_BTN_JP)) p->ModifyStyle(0, BS_OWNERDRAW);
    if (CWnd* p = GetDlgItem(IDC_BTN_EN)) p->ModifyStyle(0, BS_OWNERDRAW);

    // Initial visual state (default EN active)
    RedrawToggleButtons();

    //SetDlgItemTextW(IDC_STATIC_LN, L"言語: 日本語");

    SetDlgItemTextW(IDC_STATIC_HEADER, L"Battery Health and Performance Monitor");

	// Register for display on/off/dim notifications (sleep-awake features)
    m_hDispNotify = RegisterPowerSettingNotification(
        m_hWnd,
        &GUID_CONSOLE_DISPLAY_STATE,
        DEVICE_NOTIFY_WINDOW_HANDLE);


    return TRUE;
}


void CBatteryHelthDlg::InitToolTips()
{
    if (!m_toolTip.GetSafeHwnd())
    {
        m_toolTip.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX /*| TTS_BALLOON*/);
        m_toolTip.SetMaxTipWidth(400);
        m_toolTip.Activate(TRUE);
        m_toolTip.SetDelayTime(TTDT_INITIAL, 200);
        m_toolTip.SetDelayTime(TTDT_RESHOW, 100);
        m_toolTip.SetDelayTime(TTDT_AUTOPOP, 8000);
    }

    struct Tip
    {
        UINT id;
        const wchar_t* text_en;
        const wchar_t* text_jp;
    };

    const Tip tips[] =
    {
        { IDC_BTN_CPULOAD,
          L"Start CPU Load Test",
          L"CPU負荷テストを開始します" },

        { IDC_BTN_DISCHARGE,
          L"Start Discharge Test",
          L"放電試験を開始する" },

        { IDC_BTN_HISTORY,
          L"Charge History",
          L"充電履歴" },

        { IDC_BTN_UPLOADPDF,
          L"Export Report To CSV",
          L"レポートをCSVにエクスポート" },

        { IDC_BATT_PERCENTAGE,
          L"Current battery percentage reported by Windows.",
          L"Windows によって報告された現在のバッテリー残量の割合。" },

        { IDC_BATT_CAPACITY,
          L"Full charge vs. design capacity (health estimate).",
          L"フル充電と設計容量（健全性の推定）の比較。" },

        { IDC_BTN_CAPHIS,
          L"Battery Charge Capacity History",
          L"バッテリー充電容量履歴" },

          // { IDC_BTN_PREDICTION,
          //   L"Health Prediction",
          //   L"バッテリー寿命の予測" },

          { IDC_BTN_ACTIVE,
            L"Active Battery Life Trend",
            L"アクティブなバッテリー寿命の傾向" },

          { IDC_BTN_STANDBY,
            L"Standby Battery Life Trend",
            L"スタンバイ時のバッテリー寿命の傾向" },

          { IDC_BTN_USAGE,
            L"Usage History",
            L"利用履歴" },

          { IDC_BTN_MANIPULATIOIN,
            L"Detect Manipulation",
            L"改ざん検出" },

          { IDC_BTN_RATEINFO,
            L"Charge/Discharge Rate Information",
            L"充電/放電レート情報" },

          { IDC_BTN_BGAPP,
            L"Long-Running Background Applications",
            L"長時間実行されるバックグラウンドアプリケーション" },

          { IDC_BTN_EN,
            L"English Language",
            L"英語" },

          { IDC_BTN_JP,
            L"Japanese Language",
            L"日本語" },

          { IDC_BTN_SLEEP,
            L"Sleep Logs",
            L"睡眠ログ" },
    };

    // Common tooltips based on language
    for (size_t i = 0; i < _countof(tips); ++i)
    {
        if (CWnd* p = GetDlgItem(tips[i].id))
        {
            const wchar_t* txt =
                (m_lang == Lang::EN) ? tips[i].text_en : tips[i].text_jp;
            m_toolTip.AddTool(p, txt);
        }
    }

    // Conditional tooltip for discharge button (start/stop)
    if (CWnd* p = GetDlgItem(IDC_BTN_DISCHARGE))
    {
        const wchar_t* start_en = L"Start Discharge Test";
        const wchar_t* start_jp = L"放電試験を開始する";
        const wchar_t* stop_en = L"Stop Discharge Test";
        const wchar_t* stop_jp = L"停止放電テスト";

        const wchar_t* txt = nullptr;
        if (!m_dischargeTestRunning)
        {
            txt = (m_lang == Lang::EN) ? start_en : start_jp;
        }
        else
        {
            txt = (m_lang == Lang::EN) ? stop_en : stop_jp;
        }

        m_toolTip.AddTool(p, txt);
    }
}




BOOL CBatteryHelthDlg::PreTranslateMessage(MSG* pMsg)
{
    if (m_toolTip.GetSafeHwnd())
        m_toolTip.RelayEvent(pMsg);
    return CDialogEx::PreTranslateMessage(pMsg);
}




BOOL CBatteryHelthDlg::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);

    TRIVERTEX vert[2] = {};

    // Start color: Light Sky Blue (#87CEFA ? RGB(135,206,250))
    vert[0].x = rc.left;
    vert[0].y = rc.top;
    vert[0].Red = 135 << 8;
    vert[0].Green = 206 << 8;
    vert[0].Blue = 250 << 8;
    vert[0].Alpha = 0xFFFF;

    // End color: Soft Lavender (#BAA9F5 ? RGB(186,169,245))
    vert[1].x = rc.right;
    vert[1].y = rc.bottom;
    vert[1].Red = 186 << 8;
    vert[1].Green = 169 << 8;
    vert[1].Blue = 245 << 8;
    vert[1].Alpha = 0xFFFF;

    GRADIENT_RECT g = { 0, 1 };

    // Vertical gradient (top ? bottom)
    ::GradientFill(pDC->GetSafeHdc(), vert, 2, &g, 1, GRADIENT_FILL_RECT_V);

    return TRUE; // we drew the background
}





void CBatteryHelthDlg::CreateFonts()
{
    // Clean up existing fonts
    if (m_Font16px.GetSafeHandle())
        m_Font16px.DeleteObject();
    if (m_boldFont.GetSafeHandle())
        m_boldFont.DeleteObject();
    if (m_scaledFont.GetSafeHandle())
        m_scaledFont.DeleteObject();
    if (m_scaledBoldFont.GetSafeHandle())
        m_scaledBoldFont.DeleteObject();

    // Create base fonts
    LOGFONT lf = { 0 };
    lf.lfHeight = -16;
    lf.lfWeight = FW_NORMAL;
    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
    m_Font16px.CreateFontIndirect(&lf);

    lf.lfWeight = FW_BOLD;
    m_boldFont.CreateFontIndirect(&lf);


}

void CBatteryHelthDlg::ApplyFonts(double scale)
{
    // Clean up scaled fonts if they already exist
    if (m_scaledFont.GetSafeHandle())
        m_scaledFont.DeleteObject();
    if (m_scaledBoldFont.GetSafeHandle())
        m_scaledBoldFont.DeleteObject();

    // --- Create scaled normal font ---
    LOGFONT lf = {};
    lf.lfHeight = static_cast<int>(-16 * scale);  // base size -16, scaled
    lf.lfWeight = FW_NORMAL;                      // normal (400)
    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
    m_scaledFont.CreateFontIndirect(&lf);

    // --- Create scaled semi-bold font ---
    lf.lfWeight = 600;                            // semi-bold (between normal 400 and bold 700)
    m_scaledBoldFont.CreateFontIndirect(&lf);

    // --- Apply normal font to ALL child controls by default ---
    CWnd* pWnd = GetWindow(GW_CHILD);
    while (pWnd)
    {
        pWnd->SetFont(&m_scaledFont, FALSE);
        pWnd = pWnd->GetNextWindow();
    }

    // --- Apply bold/semi-bold font to specific controls ---

    // Static members (you said you had m_bb, m_abt, m_dh, m_header as CStatic)
    if (m_bb.GetSafeHwnd())     m_bb.SetFont(&m_scaledBoldFont, FALSE);
    if (m_abt.GetSafeHwnd())    m_abt.SetFont(&m_scaledBoldFont, FALSE);
    if (m_dh.GetSafeHwnd())     m_dh.SetFont(&m_scaledBoldFont, FALSE);
    if (m_header.GetSafeHwnd()) m_header.SetFont(&m_scaledBoldFont, FALSE);

    // Buttons
    CWnd* pBtn;
    if ((pBtn = GetDlgItem(IDC_BTN_CPULOAD)) != nullptr) pBtn->SetFont(&m_scaledBoldFont, FALSE);
    if ((pBtn = GetDlgItem(IDC_BTN_DISCHARGE)) != nullptr) pBtn->SetFont(&m_scaledBoldFont, FALSE);
    if ((pBtn = GetDlgItem(IDC_BTN_UPLOADPDF)) != nullptr) pBtn->SetFont(&m_scaledBoldFont, FALSE);
    if ((pBtn = GetDlgItem(IDC_BTN_HISTORY)) != nullptr) pBtn->SetFont(&m_scaledBoldFont, FALSE);
}


void CBatteryHelthDlg::SetButtonFont(int controlId, bool useScaled)
{
    CWnd* pBtn = GetDlgItem(controlId);
    if (pBtn && pBtn->GetSafeHwnd())
    {
        if (useScaled)
            pBtn->SetFont(&m_scaledBoldFont);  // scaled + bold
        else
            pBtn->SetFont(&m_boldFont);        // normal bold
    }
}


// Handle window resize (maximize/restore)
void CBatteryHelthDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);

    if (!m_origPositionsSaved || cx == 0 || cy == 0)
        return;

    // Calculate scaling factors
    double scaleX = (double)cx / m_origDialogSize.cx;
    double scaleY = (double)cy / m_origDialogSize.cy;

    // Use average scaling for font size
    double windowScale = (scaleX + scaleY) / 2.0;

    // Limit window scaling to reasonable range
    windowScale = max(0.5, min(windowScale, 1.5));

    if (nType == SIZE_MAXIMIZED)
    {
        // Combined scale: DPI scale + window maximize scale
        double combinedScale = m_dpiScaleFactor * windowScale;

        // Recreate fonts with combined scaling
        int scaledNormalSize = static_cast<int>(m_baseFontSize * combinedScale);
        int scaledHeaderSize = static_cast<int>(m_baseHeaderFontSize * combinedScale);
        int scaledSmallSize = static_cast<int>(m_baseSmallFontSize * combinedScale);

        // Delete and recreate fonts
        if (m_fontNormal.m_hObject) m_fontNormal.DeleteObject();
        if (m_fontBold.m_hObject) m_fontBold.DeleteObject();
        if (m_fontHeader.m_hObject) m_fontHeader.DeleteObject();
        if (m_fontSmall.m_hObject) m_fontSmall.DeleteObject();

        m_fontNormal.CreateFont(scaledNormalSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

        m_fontBold.CreateFont(scaledNormalSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, 0,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

        m_fontHeader.CreateFont(scaledHeaderSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, 0,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

        m_fontSmall.CreateFont(scaledSmallSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, 0,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, _T("Segoe UI"));

        // Apply the scaled fonts
        ApplyScaledFonts();

        // Scale control positions and sizes
        for (auto& ctl : m_origPositions)
        {
            CWnd* pWnd = GetDlgItem(ctl.id);
            if (pWnd && pWnd->GetSafeHwnd())
            {
                CRect origRect = ctl.rect;

                // Calculate new dimensions with scaling
                int newWidth = (int)(origRect.Width() * scaleX);
                int newHeight = (int)(origRect.Height() * scaleY);

                // Calculate center point of original control
                int origCenterX = origRect.left + origRect.Width() / 2;
                int origCenterY = origRect.top + origRect.Height() / 2;

                // Calculate new center point with scaling
                int newCenterX = (int)(origCenterX * scaleX);
                int newCenterY = (int)(origCenterY * scaleY);

                // Position the resized control centered on its new center point
                int newLeft = newCenterX - newWidth / 2;
                int newTop = newCenterY - newHeight / 2;

                // Ensure controls don't go outside dialog boundaries
                newLeft = max(0, min(newLeft, cx - newWidth));
                newTop = max(0, min(newTop, cy - newHeight));

                pWnd->MoveWindow(newLeft, newTop, newWidth, newHeight);
            }
        }
    }
    else if (nType == SIZE_RESTORED)
    {
		
        // Restore to DPI-scaled fonts (not 1.0, but m_dpiScaleFactor)
        CreateScaledFonts();
        ApplyScaledFonts();

        // Restore original positions (but keep DPI scaling)
        for (auto& ctl : m_origPositions)
        {
            CWnd* pWnd = GetDlgItem(ctl.id);
            if (pWnd && pWnd->GetSafeHwnd())
            {
                // Scale original positions by DPI factor
                CRect scaledRect;
                scaledRect.left = ScaleDPI(ctl.rect.left);
                scaledRect.top = ScaleDPI(ctl.rect.top);
                scaledRect.right = ScaleDPI(ctl.rect.right);
                scaledRect.bottom = ScaleDPI(ctl.rect.bottom);

                pWnd->MoveWindow(&scaledRect);
            }
        }
    }

    // Force redraw to show changes
    Invalidate();
    UpdateWindow();
}





HBRUSH CBatteryHelthDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
 
    if (nCtlColor == CTLCOLOR_STATIC )
    {
        pDC->SetBkMode(TRANSPARENT);
        pDC->SetBkColor(GetSysColor(COLOR_3DFACE)); 
        return (HBRUSH)GetStockObject(HOLLOW_BRUSH);
    }

    // Tell the dialog not to erase its background (your OnEraseBkgnd paints the gradient)
    if (nCtlColor == CTLCOLOR_DLG)
    {
        return (HBRUSH)GetStockObject(NULL_BRUSH);
    }

    return CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);
}


void CBatteryHelthDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
    if ((nID & 0xFFF0) == IDM_ABOUTBOX)
    {
        CAboutDlg dlgAbout;
        dlgAbout.DoModal();
    }
    else
    {
        CDialogEx::OnSysCommand(nID, lParam);
    }
}



void CBatteryHelthDlg::OnPaint()
{
    if (IsIconic())
    {
        CPaintDC dc(this); // device context for painting

        SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

        // Center icon in client rectangle
        int cxIcon = GetSystemMetrics(SM_CXICON);
        int cyIcon = GetSystemMetrics(SM_CYICON);
        CRect rect;
        GetClientRect(&rect);
        int x = (rect.Width() - cxIcon + 1) / 2;
        int y = (rect.Height() - cyIcon + 1) / 2;

        // Draw the icon
        dc.DrawIcon(x, y, m_hIcon);
    }
    else
    {
        CDialogEx::OnPaint();
    }
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CBatteryHelthDlg::OnQueryDragIcon()
{
    return static_cast<HCURSOR>(m_hIcon);
}




///////////////////////Capacity History Starts//////////////////////////////////////

// ---------- Helpers ----------
static CString GenBatteryReportPath()
{
    TCHAR tempPath[MAX_PATH] = {};
    GetTempPath(MAX_PATH, tempPath);
    CString path; path.Format(_T("%sbattery_report.html"), tempPath);
    return path;
}

static bool RunPowerCfgBatteryReport(const CString& path)
{
    CString cmd; cmd.Format(_T("powercfg /batteryreport /output \"%s\""), path);
    STARTUPINFO si{ sizeof(si) }; PROCESS_INFORMATION pi{};
    CString cmdBuf = cmd;

    if (!CreateProcess(nullptr, cmdBuf.GetBuffer(), nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return false;

    WaitForSingleObject(pi.hProcess, 10000); // wait up to 10s
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

// Robust text loader: UTF-16LE BOM, UTF-8 BOM, UTF-8 no BOM, fallback ANSI
static CString ReadTextAutoEncoding(const CString& path)
{
    CFile f;
    if (!f.Open(path, CFile::modeRead | CFile::shareDenyNone)) return {};
    const ULONGLONG len = f.GetLength();
    if (len == 0 || len > 16ULL * 1024 * 1024) { f.Close(); return {}; }

    std::vector<BYTE> buf((size_t)len);
    f.Read(buf.data(), (UINT)len); f.Close();

    if (len >= 2 && buf[0] == 0xFF && buf[1] == 0xFE) { // UTF-16LE BOM
        return CString((LPCWSTR)(buf.data() + 2), (int)((len - 2) / 2));
    }
    if (len >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) { // UTF-8 BOM
        int n = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data() + 3, (int)len - 3, nullptr, 0);
        CStringW w; LPWSTR p = w.GetBuffer(n);
        MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data() + 3, (int)len - 3, p, n);
        w.ReleaseBuffer(n); return w;
    }
    // Try UTF-8, then ANSI
    int n = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data(), (int)len, nullptr, 0);
    if (n > 0) {
        CStringW w; LPWSTR p = w.GetBuffer(n);
        MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data(), (int)len, p, n);
        w.ReleaseBuffer(n); return w;
    }
    n = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)buf.data(), (int)len, nullptr, 0);
    if (n > 0) {
        CStringW w; LPWSTR p = w.GetBuffer(n);
        MultiByteToWideChar(CP_ACP, 0, (LPCSTR)buf.data(), (int)len, p, n);
        w.ReleaseBuffer(n); return w;
    }
    return {};
}

static CString StripHtml(const CString& in)
{
    std::wstring s(in);
    static const std::wregex reTags(L"<[^>]*>");
    static const std::wregex reNBSP(L"&nbsp;");
    static const std::wregex reWS(L"[ \\t\\r\\n]+");

    s = std::regex_replace(s, reTags, L" ");
    s = std::regex_replace(s, reNBSP, L" ");
    s = std::regex_replace(s, reWS, L" ");
    CString o(s.c_str()); o.Trim(); return o;
}

static int ExtractFirstInt(const CString& s)
{
    std::wsmatch m; std::wstring w = s.GetString();
    static const std::wregex reNum(L"([\\d,]+)");
    if (std::regex_search(w, m, reNum)) {
        CString t(m[1].str().c_str()); t.Remove(','); return _ttoi(t);
    }
    return 0;
}



bool CBatteryHelthDlg::GetBatteryCapacityHistory(std::vector<CBatteryHelthDlg::BatteryCapacityRecord>& out)
{
    out.clear();

    const CString reportPath = GenBatteryReportPath();
    if (!RunPowerCfgBatteryReport(reportPath)) return false;

    const CString html = ReadTextAutoEncoding(reportPath);
    if (html.IsEmpty()) return false;

    // ---- 1) Parse Design Capacity (header section) ----
    int design_mWh = 0;
    {
        std::wsmatch mh;
        std::wstring W = html.GetString();
        std::wregex reDesign(L"DESIGN\\s+CAPACITY[\\s\\S]*?([\\d,]+)", std::regex_constants::icase);
        if (std::regex_search(W, mh, reDesign)) {
            CString tmp(mh[1].str().c_str()); tmp.Remove(','); design_mWh = _ttoi(tmp);
        }
    }

    // ---- 2) Locate the "Battery capacity history" table ----
    std::wsmatch table;
    std::wstring H = html.GetString();
    std::wregex reTable(L"BATTERY\\s*CAPACITY\\s*HISTORY[\\s\\S]*?<table[^>]*>([\\s\\S]*?)</table>",
        std::regex_constants::icase);
    if (!std::regex_search(H, table, reTable)) return false;

    std::wstring tableHtml = table[1].str();

    // ---- 3) Iterate rows/cells ----
    std::wregex reRow(L"<tr[^>]*>([\\s\\S]*?)</tr>", std::regex_constants::icase);
    std::wregex reCell(L"<t[dh][^>]*>([\\s\\S]*?)</t[dh]>", std::regex_constants::icase);

    std::wsregex_iterator it(tableHtml.begin(), tableHtml.end(), reRow), end;

    for (; it != end; ++it) {
        std::wstring row = (*it)[1].str();

        std::vector<CString> cells;
        for (std::wsregex_iterator ic(row.begin(), row.end(), reCell); ic != end; ++ic) {
            CString cell = StripHtml(CString((*ic)[1].str().c_str()));
            if (!cell.IsEmpty()) cells.push_back(cell);
        }
        if (cells.size() < 2) continue;

        // skip header-ish rows
        CString c0U = cells[0]; c0U.MakeUpper();
        CString c1U = cells[1]; c1U.MakeUpper();
        if (c0U.Find(L"DATE") >= 0 || c0U.Find(L"PERIOD") >= 0 || c1U.Find(L"FULL") >= 0)
            continue;

        CBatteryHelthDlg::BatteryCapacityRecord rec;
        rec.dateText = cells[0];
        rec.fullCharge_mWh = ExtractFirstInt(cells[1]);
        rec.design_mWh = design_mWh;
        if (rec.design_mWh > 0 && rec.fullCharge_mWh > 0)
            rec.healthPct = 100.0 * rec.fullCharge_mWh / rec.design_mWh;

        // try date parse
        if (!rec.date.ParseDateTime(rec.dateText))
            rec.date.m_dt = 0; // leave unparsed but keep dateText

        if (rec.fullCharge_mWh > 0)
            out.push_back(rec);
    }

    return !out.empty();
}


//////////////////////////////Capacity History Ends//////////////////////////////////////





CString QueryWmiValue(LPCWSTR wmiNamespace, LPCWSTR wmiClass, LPCWSTR propertyName)
{
    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;
    IEnumWbemClassObject* pEnumerator = NULL;
    IWbemClassObject* pObj = NULL;
    ULONG uReturn = 0;
    VARIANT vtProp;
    CString result = L"Not available";

    HRESULT hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) return result;

    hres = pLoc->ConnectServer(
        _bstr_t(wmiNamespace), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) { pLoc->Release(); return result; }

    hres = CoSetProxyBlanket(
        pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    if (FAILED(hres)) { pSvc->Release(); pLoc->Release(); return result; }

    CString query;
    query.Format(L"SELECT * FROM %s", wmiClass);
    hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t(query),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);
    if (FAILED(hres)) { pSvc->Release(); pLoc->Release(); return result; }

    if (pEnumerator)
    {
        hres = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn);
        if (uReturn != 0)
        {
            hres = pObj->Get(propertyName, 0, &vtProp, 0, 0);
            if (SUCCEEDED(hres) && vtProp.vt != VT_NULL && vtProp.vt != VT_EMPTY)
            {
                if (vtProp.vt == VT_I4 || vtProp.vt == VT_UI4)
                    result.Format(L"%ld", vtProp.lVal);
                else if (vtProp.vt == VT_BSTR)
                    result = vtProp.bstrVal;
            }
            VariantClear(&vtProp);
            pObj->Release();
        }
        pEnumerator->Release();
    }

    pSvc->Release();
    pLoc->Release();

    return result;
}

CString QueryBatteryCycleCount()
{
    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;
    IEnumWbemClassObject* pEnumerator = NULL;
    IWbemClassObject* pObj = NULL;
    ULONG uReturn = 0;
    VARIANT vtProp;
    CString result = L"Not available";

    HRESULT hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) return result;

    // Try ROOT\WMI first for cycle count
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &pSvc);

    if (SUCCEEDED(hres))
    {
        hres = CoSetProxyBlanket(
            pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

        if (SUCCEEDED(hres))
        {
            // Try different WMI classes that might contain cycle count
            const WCHAR* wmiClasses[] = {
                L"BatteryCycleCount",
                L"MSBattery_CycleCount",
                L"BatteryStaticData",
                L"MSBattery_StaticData"
            };

            const WCHAR* propertyNames[] = {
                L"CycleCount",
                L"CycleCount",
                L"CycleCount",
                L"CycleCount"
            };

            for (int i = 0; i < 4; i++)
            {
                CString query;
                query.Format(L"SELECT * FROM %s", wmiClasses[i]);

                hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t(query),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                    NULL, &pEnumerator);

                if (SUCCEEDED(hres) && pEnumerator)
                {
                    hres = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn);
                    if (uReturn != 0)
                    {
                        hres = pObj->Get(propertyNames[i], 0, &vtProp, 0, 0);
                        if (SUCCEEDED(hres) && vtProp.vt != VT_NULL && vtProp.vt != VT_EMPTY)
                        {
                            if (vtProp.vt == VT_I4 || vtProp.vt == VT_UI4)
                            {
                                result.Format(L"%ld", vtProp.lVal);
                                VariantClear(&vtProp);
                                pObj->Release();
                                pEnumerator->Release();
                                pSvc->Release();
                                pLoc->Release();
                                return result;
                            }
                        }
                        VariantClear(&vtProp);
                        pObj->Release();
                    }
                    pEnumerator->Release();
                    pEnumerator = NULL;
                }
            }
        }
        pSvc->Release();
    }

    // If ROOT\WMI didn't work, try ROOT\CIMV2
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);

    if (SUCCEEDED(hres))
    {
        hres = CoSetProxyBlanket(
            pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

        if (SUCCEEDED(hres))
        {
            // Try Win32_Battery class for cycle count
            hres = pSvc->ExecQuery(bstr_t("WQL"),
                bstr_t("SELECT * FROM Win32_Battery"),
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                NULL, &pEnumerator);

            if (SUCCEEDED(hres) && pEnumerator)
            {
                hres = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn);
                if (uReturn != 0)
                {
                    // Try different property names that might exist
                    const WCHAR* cycleProps[] = { L"CycleCount", L"DesignCycleCount", L"MaxCycleCount" };

                    for (int i = 0; i < 3; i++)
                    {
                        hres = pObj->Get(cycleProps[i], 0, &vtProp, 0, 0);
                        if (SUCCEEDED(hres) && vtProp.vt != VT_NULL && vtProp.vt != VT_EMPTY)
                        {
                            if (vtProp.vt == VT_I4 || vtProp.vt == VT_UI4)
                            {
                                result.Format(L"%ld", vtProp.lVal);
                                VariantClear(&vtProp);
                                break;
                            }
                        }
                        VariantClear(&vtProp);
                    }
                    pObj->Release();
                }
                pEnumerator->Release();
            }
        }
        pSvc->Release();
    }

    pLoc->Release();
    return result;
}



CString QueryBatteryTemperature()
{
    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;
    IEnumWbemClassObject* pEnumerator = NULL;
    IWbemClassObject* pObj = NULL;
    ULONG uReturn = 0;
    VARIANT vtProp;
    CString result = L"Not available";

    HRESULT hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) return result;

    // Try ROOT\WMI first for temperature
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &pSvc);

    if (SUCCEEDED(hres))
    {
        hres = CoSetProxyBlanket(
            pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

        if (SUCCEEDED(hres))
        {
            // Try different WMI classes that might contain temperature
            const WCHAR* wmiClasses[] = {
                L"BatteryStatus",
                L"BatteryTemperature",
                L"MSBattery_Status",
                L"MSBattery_Temperature",
                L"BatteryRuntimeInfo",
                L"MSBattery_Information"
            };

            const WCHAR* propertyNames[] = {
                L"Temperature",
                L"Temperature",
                L"Temperature",
                L"Temperature",
                L"Temperature",
                L"Temperature"
            };

            for (int i = 0; i < 6; i++)
            {
                CString query;
                query.Format(L"SELECT * FROM %s", wmiClasses[i]);

                hres = pSvc->ExecQuery(bstr_t("WQL"), bstr_t(query),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                    NULL, &pEnumerator);

                if (SUCCEEDED(hres) && pEnumerator)
                {
                    hres = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn);
                    if (uReturn != 0)
                    {
                        hres = pObj->Get(propertyNames[i], 0, &vtProp, 0, 0);
                        if (SUCCEEDED(hres) && vtProp.vt != VT_NULL && vtProp.vt != VT_EMPTY)
                        {
                            if (vtProp.vt == VT_I4 || vtProp.vt == VT_UI4)
                            {
                                // Temperature is usually in Kelvin (divide by 10) or already in Celsius
                                double tempKelvin = vtProp.lVal / 10.0; // Some systems return in 10ths of Kelvin
                                double tempCelsius = tempKelvin - 273.15; // Convert Kelvin to Celsius

                                // If temperature seems reasonable in Celsius, use it directly
                                if (vtProp.lVal < 100 && vtProp.lVal > 0)
                                {
                                    tempCelsius = vtProp.lVal; // Already in Celsius
                                }

                                result.Format(L"%.1f°C", tempCelsius);
                                VariantClear(&vtProp);
                                pObj->Release();
                                pEnumerator->Release();
                                pSvc->Release();
                                pLoc->Release();
                                return result;
                            }
                        }
                        VariantClear(&vtProp);
                        pObj->Release();
                    }
                    pEnumerator->Release();
                    pEnumerator = NULL;
                }
            }
        }
        pSvc->Release();
    }

    // Try ROOT\CIMV2 namespace
    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);

    if (SUCCEEDED(hres))
    {
        hres = CoSetProxyBlanket(
            pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

        if (SUCCEEDED(hres))
        {
            // Try Win32_Battery for temperature-related properties
            hres = pSvc->ExecQuery(bstr_t("WQL"),
                bstr_t("SELECT * FROM Win32_Battery"),
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                NULL, &pEnumerator);

            if (SUCCEEDED(hres) && pEnumerator)
            {
                hres = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn);
                if (uReturn != 0)
                {
                    // Try different temperature property names
                    const WCHAR* tempProps[] = {
                        L"Temperature",
                        L"BatteryTemperature",
                        L"ThermalState",
                        L"CurrentTemperature"
                    };

                    for (int i = 0; i < 4; i++)
                    {
                        hres = pObj->Get(tempProps[i], 0, &vtProp, 0, 0);
                        if (SUCCEEDED(hres) && vtProp.vt != VT_NULL && vtProp.vt != VT_EMPTY)
                        {
                            if (vtProp.vt == VT_I4 || vtProp.vt == VT_UI4)
                            {
                                double temp = vtProp.lVal;

                                // Handle different temperature formats
                                if (temp > 1000) // Likely in Kelvin * 10
                                {
                                    temp = (temp / 10.0) - 273.15;
                                }
                                else if (temp > 273) // Likely in Kelvin
                                {
                                    temp = temp - 273.15;
                                }
                                // else assume already in Celsius

                                result.Format(L"%.1f°C", temp);
                                VariantClear(&vtProp);
                                break;
                            }
                        }
                        VariantClear(&vtProp);
                    }
                    pObj->Release();
                }
                pEnumerator->Release();
            }
        }
        pSvc->Release();
    }

    // Try alternative method using thermal zone information
    if (result == L"Not available")
    {
        hres = pLoc->ConnectServer(
            _bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);

        if (SUCCEEDED(hres))
        {
            hres = CoSetProxyBlanket(
                pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

            if (SUCCEEDED(hres))
            {
                // Try Win32_TemperatureProbe for system temperature sensors
                hres = pSvc->ExecQuery(bstr_t("WQL"),
                    bstr_t("SELECT * FROM Win32_TemperatureProbe WHERE Description LIKE '%Battery%' OR Name LIKE '%Battery%'"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                    NULL, &pEnumerator);

                if (SUCCEEDED(hres) && pEnumerator)
                {
                    hres = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn);
                    if (uReturn != 0)
                    {
                        hres = pObj->Get(L"CurrentReading", 0, &vtProp, 0, 0);
                        if (SUCCEEDED(hres) && vtProp.vt != VT_NULL && vtProp.vt != VT_EMPTY)
                        {
                            if (vtProp.vt == VT_I4 || vtProp.vt == VT_UI4)
                            {
                                // CurrentReading is usually in tenths of Kelvin
                                double tempKelvin = vtProp.lVal / 10.0;
                                double tempCelsius = tempKelvin - 273.15;
                                result.Format(L"%.1f°C", tempCelsius);
                            }
                        }
                        VariantClear(&vtProp);
                        pObj->Release();
                    }
                    pEnumerator->Release();
                }
            }
            pSvc->Release();
        }
    }

    pLoc->Release();
    return result;
}

bool CBatteryHelthDlg::HasBattery()
{
    IWbemLocator* pLoc = nullptr;
    IWbemServices* pSvc = nullptr;
    IEnumWbemClassObject* pEnum = nullptr;
    IWbemClassObject* pObj = nullptr;

    bool hasBattery = false;

    HRESULT hr = CoCreateInstance(
        CLSID_WbemLocator,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_IWbemLocator,
        (LPVOID*)&pLoc);

    if (FAILED(hr) || !pLoc)
        goto cleanup;

    hr = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"), // namespace
        nullptr,                // user
        nullptr,                // password
        nullptr,                // locale
        0,                      // SECURITY FLAGS (long) ✔ FIXED
        nullptr,                // authority
        nullptr,                // context
        &pSvc);

    if (FAILED(hr) || !pSvc)
        goto cleanup;

    hr = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        nullptr,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        nullptr,
        EOAC_NONE);

    if (FAILED(hr))
        goto cleanup;

    hr = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_Battery"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        nullptr,
        &pEnum);

    if (FAILED(hr) || !pEnum)
        goto cleanup;

    ULONG ret = 0;
    hr = pEnum->Next(WBEM_INFINITE, 1, &pObj, &ret);

    if (SUCCEEDED(hr) && ret > 0 && pObj)
        hasBattery = true;

cleanup:
    if (pObj)  pObj->Release();
    if (pEnum) pEnum->Release();
    if (pSvc)  pSvc->Release();
    if (pLoc)  pLoc->Release();

    return hasBattery;
}



void CBatteryHelthDlg::GetStaticBatteryInfo()
{
    // --- Connect to WMI (ROOT\CIMV2 : Win32_Battery) ---
    IWbemLocator* pLoc = nullptr;
    IWbemServices* pSvc = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hr) || !pLoc) {
        {
            if (m_lang == Lang::EN) {
                UpdateLabel(this, IDC_BATT_DID, L"");
                UpdateLabel(this, IDC_BATT_DID, L"Failed to create IWbemLocator");
            }
            else {
                UpdateLabel(this, IDC_BATT_DID, L"");
                UpdateLabel(this, IDC_BATT_DID, L"IWbemLocator の作成に失敗しました");
            }
        }
        return;
    }

    hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hr) || !pSvc) {
        if (m_lang == Lang::EN) {
            UpdateLabel(this, IDC_BATT_DID, L"");
            UpdateLabel(this, IDC_BATT_DID, L"Could not connect to WMI");
        }
        else {
            UpdateLabel(this, IDC_BATT_DID, L"");
            UpdateLabel(this, IDC_BATT_DID, L"WMI に接続できませんでした");
        }
        
        pLoc->Release();
        return;
    }

    hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE);
    if (FAILED(hr)) {

        if (m_lang == Lang::EN) {
            UpdateLabel(this, IDC_BATT_DID, L"");
            UpdateLabel(this, IDC_BATT_DID, L"Could not set proxy blanket");
        }
        else {
            UpdateLabel(this, IDC_BATT_DID, L"");
            UpdateLabel(this, IDC_BATT_DID, L"プロキシ ブランケットを設定できませんでした");
        }

        
        pSvc->Release(); pLoc->Release();
        return;
    }

    // --- Query Win32_Battery ---
    IEnumWbemClassObject* pEnumerator = nullptr;
    hr = pSvc->ExecQuery(bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_Battery"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);
    if (FAILED(hr) || !pEnumerator) {
        

            if (m_lang == Lang::EN) {
                UpdateLabel(this, IDC_BATT_DID, L"");
                UpdateLabel(this, IDC_BATT_DID, L"WMI query failed");
            }
            else {
                UpdateLabel(this, IDC_BATT_DID, L"");
                UpdateLabel(this, IDC_BATT_DID, L"WMI クエリに失敗しました");
            }

        
        pSvc->Release(); pLoc->Release();
        return;
    }

    IWbemClassObject* pObj = nullptr;
    ULONG uReturn = 0;
    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn);

    if (uReturn == 0 || !pObj) {
        if (m_lang == Lang::EN) {
            UpdateLabel(this, IDC_BATT_DID, L"");
            UpdateLabel(this, IDC_BATT_DID, L"No battery found");
        }
        else {
            UpdateLabel(this, IDC_BATT_DID, L"");
            UpdateLabel(this, IDC_BATT_DID, L"バッテリーが見つかりません");
        }
        pEnumerator->Release(); pSvc->Release(); pLoc->Release();
        return;
    }

 
    VARIANT vt{};
  

    // ---------------------------------------
    // 2) Full Charge Capacity (current WMI)
    // ---------------------------------------
    CString fullCapStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryFullChargedCapacity", L"FullChargedCapacity");
    int     fullCap_mWh = 0;

    if (fullCapStr != L"Not available") {
        fullCap_mWh = _wtoi(fullCapStr);
        fullCapStr += L" mWh";
    }
    else {
        if(m_lang == Lang::EN) {
            fullCapStr = L"Unknown";
        }
        else {
            fullCapStr = L"不明";
		}
    }
    UpdateLabel(this, IDC_BATT_CAPACITY, fullCapStr);

    // ---------------------------------------
    // 3) Design Capacity (current WMI)
    // ---------------------------------------
    CString designCapStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryStaticData", L"DesignedCapacity");
    int     designCapWmi_mWh = 0;

    if (designCapStr != L"Not available") {
        designCapWmi_mWh = _wtoi(designCapStr);
        designCapStr += L" mWh";
    }
    else {
        if(m_lang == Lang::EN) {
            designCapStr = L"Unknown";
        }
        else {
            designCapStr = L"不明";
        }
    }

    // -------------------------------------------------
    // 4) History fallback for Design Capacity (report)
    //    Prefer the larger of first/last design value.
    // -------------------------------------------------
    int     designCapHist_mWh = 0;
    CString designCapHistPretty;

    {
        std::vector<BatteryCapacityRecord> hist;
        if (GetBatteryCapacityHistory(hist) && !hist.empty()) {
            const auto& first = hist.front();
            const auto& last = hist.back();
            int d1 = first.design_mWh;
            int d2 = last.design_mWh;
            if (d1 > 0 || d2 > 0) {
                designCapHist_mWh = max(d1, d2);
                designCapHistPretty.Format(L"%d mWh", designCapHist_mWh);
            }
        }
    }

    // Choose which Design Capacity to DISPLAY
    if (designCapWmi_mWh > 0 && fullCap_mWh != designCapWmi_mWh) {
        UpdateLabel(this, IDC_BATT_DCAPACITY, designCapStr);            // WMI preferred
    }
    else if (designCapHist_mWh > 0) {
        UpdateLabel(this, IDC_BATT_DCAPACITY, designCapHistPretty);      // history fallback
    }
    else {
        if(m_lang == Lang::EN) {
            UpdateLabel(this, IDC_BATT_DCAPACITY, L"");
            UpdateLabel(this, IDC_BATT_DCAPACITY, L"Unknown");
        }
        else {
            UpdateLabel(this, IDC_BATT_DCAPACITY, L"");
            UpdateLabel(this, IDC_BATT_DCAPACITY, L"不明");
		}
    }

 

    // --------------------------
    // 6) Battery Health (%)
    // --------------------------
    {
        // Prefer WMI design; if missing, use history design
        int designForHealth_mWh = (designCapWmi_mWh > 0 && fullCap_mWh != designCapWmi_mWh) ? designCapWmi_mWh : designCapHist_mWh;

        if (fullCap_mWh > 0 && designForHealth_mWh > 0) {
            double health = (static_cast<double>(fullCap_mWh) / designForHealth_mWh) * 100.0;
            CString healthStr; healthStr.Format(L"%.2f %%", health);
            UpdateLabel(this, IDC_BATT_HEALTH, healthStr);
        }
        else {
            if(m_lang == Lang::EN) {
                UpdateLabel(this, IDC_BATT_HEALTH, L"");
                UpdateLabel(this, IDC_BATT_HEALTH, L"Unknown");
            }
            else {
                UpdateLabel(this, IDC_BATT_HEALTH, L"");
                UpdateLabel(this, IDC_BATT_HEALTH, L"不明");
            }
        }
    }

  
    // --------------------------
    // 8) DeviceID / Name
    // --------------------------
    VARIANT vtDev{};
    if (SUCCEEDED(pObj->Get(L"DeviceID", 0, &vtDev, 0, 0))) {
        CString dev = (vtDev.vt != VT_NULL && vtDev.vt != VT_EMPTY) ? vtDev.bstrVal : L"Unknown";
        UpdateLabel(this, IDC_BATT_MANUFAC, dev);
        VariantClear(&vtDev);
    }
    else {
        if(m_lang == Lang::EN) {
            UpdateLabel(this, IDC_BATT_MANUFAC, L"");
            UpdateLabel(this, IDC_BATT_MANUFAC, L"Unknown");
        }
        else {
            UpdateLabel(this, IDC_BATT_MANUFAC, L"");
			UpdateLabel(this, IDC_BATT_MANUFAC, L"不明");
        }
    }

    if (SUCCEEDED(pObj->Get(L"Name", 0, &vt, 0, 0))) {
        CString name = (vt.vt != VT_NULL && vt.vt != VT_EMPTY) ? vt.bstrVal : L"Unknown";
        UpdateLabel(this, IDC_BATT_NAME, name);
        VariantClear(&vt);
    }
    else {
        if(m_lang == Lang::EN) {
            UpdateLabel(this, IDC_BATT_NAME, L"");
            UpdateLabel(this, IDC_BATT_NAME, L"Unknown");
        }
        else {
            UpdateLabel(this, IDC_BATT_NAME, L"");
            UpdateLabel(this, IDC_BATT_NAME, L"不明");
        }
    }

    // --------------------------
    // 9) Cycle Count (vendor/WMI)
    // --------------------------
    {
        CString cycles = QueryBatteryCycleCount(); // your helper
        if (cycles != L"Not available" && cycles != L"0")
        {
            if(m_lang == Lang::EN) {
                cycles += L" cycles";
            }
            else {
                cycles += L" サイクル";
			}
        }
        else
        {
            if(m_lang == Lang::EN) {
                cycles = L"Unknown";
            }
            else {
				cycles = L"不明";
            }
        }
        UpdateLabel(this, IDC_BATT_CYCLE, cycles);

    }

    // --------------------------
    // 11) System UUID
    // --------------------------
    {
        IEnumWbemClassObject* pEnumUUID = nullptr;
        if (SUCCEEDED(pSvc->ExecQuery(bstr_t("WQL"),
            bstr_t("SELECT UUID FROM Win32_ComputerSystemProduct"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
            NULL, &pEnumUUID)) && pEnumUUID)
        {
            IWbemClassObject* pUUID = nullptr; ULONG ret = 0;
            if (pEnumUUID->Next(WBEM_INFINITE, 1, &pUUID, &ret) == S_OK) {
                VARIANT v{};
                if (SUCCEEDED(pUUID->Get(L"UUID", 0, &v, 0, 0))) {
                    CString uuid = (v.vt != VT_NULL && v.vt != VT_EMPTY) ? v.bstrVal : L"Not available";
                    UpdateLabel(this, IDC_BATT_DID, L"ID - " + uuid);
                    VariantClear(&v);
                }
                pUUID->Release();
            }
            else {
                if(m_lang == Lang::EN) {
                    UpdateLabel(this, IDC_BATT_DID, L"");
                    UpdateLabel(this, IDC_BATT_DID, L"Not available");
                }
                else {
                    UpdateLabel(this, IDC_BATT_DID, L"");
                    UpdateLabel(this, IDC_BATT_DID, L"利用不可");
				}
            }
            pEnumUUID->Release();
        }
        else {
            if(m_lang == Lang::EN) {
                UpdateLabel(this, IDC_BATT_DID, L"");
                    UpdateLabel(this, IDC_BATT_DID, L"Not available");
                }
                else {
                UpdateLabel(this, IDC_BATT_DID, L"");
                    UpdateLabel(this, IDC_BATT_DID, L"利用不可");
				}
        }
    }

    // cleanup
    pObj->Release();
    pEnumerator->Release();
    pSvc->Release();
    pLoc->Release();

    // your post-actions
    UpdateDischargeButtonStatus();
    CheckBatteryTransition();

	Invalidate();

}

//void CBatteryHelthDlg::GetBatteryInfo()
//{
//    // --- Connect to WMI (ROOT\CIMV2 : Win32_Battery) ---
//    IWbemLocator* pLoc = nullptr;
//    IWbemServices* pSvc = nullptr;
//
//    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
//        IID_IWbemLocator, (LPVOID*)&pLoc);
//    if (FAILED(hr) || !pLoc) {
//        UpdateLabel(this, IDC_BATT_DID, L"Failed to create IWbemLocator");
//        return;
//    }
//
//    hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
//    if (FAILED(hr) || !pSvc) {
//        UpdateLabel(this, IDC_BATT_DID, L"Could not connect to WMI");
//        pLoc->Release();
//        return;
//    }
//
//    hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
//        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
//        NULL, EOAC_NONE);
//    if (FAILED(hr)) {
//        UpdateLabel(this, IDC_BATT_DID, L"Could not set proxy blanket");
//        pSvc->Release(); pLoc->Release();
//        return;
//    }
//
//    // --- Query Win32_Battery ---
//    IEnumWbemClassObject* pEnumerator = nullptr;
//    hr = pSvc->ExecQuery(bstr_t("WQL"),
//        bstr_t("SELECT * FROM Win32_Battery"),
//        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
//        NULL, &pEnumerator);
//    if (FAILED(hr) || !pEnumerator) {
//        UpdateLabel(this, IDC_BATT_DID, L"WMI query failed");
//        pSvc->Release(); pLoc->Release();
//        return;
//    }
//
//    IWbemClassObject* pObj = nullptr;
//    ULONG uReturn = 0;
//    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn);
//
//    if (uReturn == 0 || !pObj) {
//        UpdateLabel(this, IDC_BATT_DID, L"No battery found");
//        pEnumerator->Release(); pSvc->Release(); pLoc->Release();
//        return;
//    }
//
//    // ----------------------------
//    // 1) Status / Percent / Time
//    // ----------------------------
//    VARIANT vt{};
//    CString statusText = L"Unknown";
//    int batteryStatus = 0;
//
//    if (SUCCEEDED(pObj->Get(L"BatteryStatus", 0, &vt, 0, 0))) {
//        batteryStatus = vt.intVal;
//        switch (vt.intVal) {
//        case 1: statusText = L"Discharging";   break;
//        case 2: statusText = L"Charging";      break;
//        case 3: statusText = L"Fully Charged"; break;
//        default: statusText = L"Unknown";      break;
//        }
//        VariantClear(&vt);
//    }
//    UpdateLabel(this, IDC_BATT_STATUS, statusText);
//
//    SYSTEM_POWER_STATUS sps{};
//    if (GetSystemPowerStatus(&sps)) {
//        CString pct;
//        if (sps.BatteryLifePercent != 255) {
//            pct.Format(L"%d%%", sps.BatteryLifePercent);
//            m_BatteryProgress.SetPos(sps.BatteryLifePercent);
//        }
//        else {
//            pct = L"Unknown";
//            m_BatteryProgress.SetPos(0);
//        }
//        UpdateLabel(this, IDC_BATT_PERCENTAGE, pct);
//
//        CString remain;
//
//        // Check if AC power is connected
//        if (sps.ACLineStatus == 1) { // Plugged in
//            // Calculate time to full charge
//            if (batteryStatus == 2 && sps.BatteryLifePercent != 255 && sps.BatteryLifePercent < 100) {
//                // Get current and design capacity
//                CString currCapStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"RemainingCapacity");
//                CString designCapStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryFullChargedCapacity", L"FullChargedCapacity");
//                CString chargeRateStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"ChargeRate");
//
//                if (currCapStr != L"Not available" && designCapStr != L"Not available" && chargeRateStr != L"Not available") {
//                    int currentCap = _wtoi(currCapStr);
//                    int fullCap = _wtoi(designCapStr);
//                    int chargeRate = _wtoi(chargeRateStr);
//
//                    // ChargeRate is typically in mW, need to calculate time
//                    if (chargeRate > 0 && fullCap > currentCap) {
//                        int remainingCap = fullCap - currentCap;
//                        // Time in hours = remaining capacity / charge rate
//                        double timeHours = (double)remainingCap / (double)chargeRate;
//                        int hours = (int)timeHours;
//                        int mins = (int)((timeHours - hours) * 60);
//
//                        if (hours > 0 || mins > 0) {
//                            remain.Format(L"%d hr %d min", hours, mins);
//                        }
//                        else {
//                            remain = L"Calculating...";
//                        }
//                    }
//                    else {
//                        remain = L"Calculating...";
//                    }
//                }
//                else {
//                    remain = L"Calculating...";
//                }
//            }
//            else if (batteryStatus == 3 || sps.BatteryLifePercent == 100) {
//                remain = L"Fully Charged";
//            }
//            else {
//                remain = L"Plugged In";
//            }
//        }
//        else { // On battery
//            if (sps.BatteryLifeTime != (DWORD)-1) {
//                int hours = static_cast<int>(sps.BatteryLifeTime / 3600);
//                int mins = static_cast<int>((sps.BatteryLifeTime % 3600) / 60);
//                remain.Format(L"%d hr %d min", hours, mins);
//            }
//            else {
//                remain = L"Unknown";
//            }
//        }
//
//        UpdateLabel(this, IDC_BATT_TIME, remain);
//    }
//    else {
//        UpdateLabel(this, IDC_BATT_PERCENTAGE, L"Unknown");
//        UpdateLabel(this, IDC_BATT_TIME, L"Unknown");
//    }
//
//    // ---------------------------------------
//    // 5) Current (Remaining) Capacity
//    // ---------------------------------------
//    CString currCapStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"RemainingCapacity");
//    if (currCapStr != L"Not available") {
//        currCapStr += L" mWh";
//    }
//    else {
//        currCapStr = L"Unknown";
//    }
//    UpdateLabel(this, IDC_BATT_CURRCAPACITY, currCapStr);
//
//    // --------------------------
//    // 7) Voltage
//    // --------------------------
//    CString voltageStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"Voltage");
//    if (voltageStr != L"Not available") {
//        double volts = _wtoi(voltageStr) / 1000.0; // mV -> V
//        CString out; out.Format(L"%.2f V", volts);
//        UpdateLabel(this, IDC_BATT_VOLTAGE, out);
//    }
//    else {
//        UpdateLabel(this, IDC_BATT_VOLTAGE, L"Unknown");
//    }
//
//    // --------------------------
//    // 10) Temperature (if avail)
//    // --------------------------
//    {
//        CString t = QueryBatteryTemperature(); // your helper
//        if (t == L"Not available") t = L"Unknown";
//        UpdateLabel(this, IDC_BATT_TEMP, t);
//    }
//
//    // cleanup
//    pObj->Release();
//    pEnumerator->Release();
//    pSvc->Release();
//    pLoc->Release();
//
//    // your post-actions
//    UpdateDischargeButtonStatus();
//    CheckBatteryTransition();
//}

//void CBatteryHelthDlg::GetBatteryInfo()
//{
//    // --- Connect to WMI (ROOT\CIMV2 : Win32_Battery) ---
//    IWbemLocator* pLoc = nullptr;
//    IWbemServices* pSvc = nullptr;
//
//    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
//        IID_IWbemLocator, (LPVOID*)&pLoc);
//    if (FAILED(hr) || !pLoc) {
//        if (m_lang == Lang::EN) {
//            UpdateLabel(this, IDC_BATT_DID, L"Failed to create IWbemLocator");
//        }
//        else {
//			UpdateLabel(this, IDC_BATT_DID, L"IWbemLocator の作成に失敗しました");
//        }
//        return;
//    }
//
//    hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
//    if (FAILED(hr) || !pSvc) {
//        if(m_lang == Lang::EN) {
//            UpdateLabel(this, IDC_BATT_DID, L"Could not connect to WMI");
//        }
//        else {
//            UpdateLabel(this, IDC_BATT_DID, L"WMI に接続できませんでした");
//        }
//        pLoc->Release();
//        return;
//    }
//
//    hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
//        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
//        NULL, EOAC_NONE);
//    if (FAILED(hr)) {
//        if (m_lang == Lang::EN) {
//            UpdateLabel(this, IDC_BATT_DID, L"Could not set proxy blanket");
//        }
//        else{
//            UpdateLabel(this, IDC_BATT_DID, L"プロキシ ブランケットを設定できませんでした");
//        }
//        pSvc->Release(); pLoc->Release();
//        return;
//    }
//
//    // --- Query Win32_Battery ---
//    IEnumWbemClassObject* pEnumerator = nullptr;
//    hr = pSvc->ExecQuery(bstr_t("WQL"),
//        bstr_t("SELECT * FROM Win32_Battery"),
//        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
//        NULL, &pEnumerator);
//    if (FAILED(hr) || !pEnumerator) {
//        if (m_lang == Lang::EN) {
//UpdateLabel(this, IDC_BATT_DID, L"WMI query failed");
//        }
//        else {
//            UpdateLabel(this, IDC_BATT_DID, L"WMI クエリに失敗しました");
//        }
//        
//        pSvc->Release(); pLoc->Release();
//        return;
//    }
//
//    IWbemClassObject* pObj = nullptr;
//    ULONG uReturn = 0;
//    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn);
//
//    if (uReturn == 0 || !pObj) {
//
//        if(m_lang == Lang::EN) {
//            UpdateLabel(this, IDC_BATT_DID, L"No battery found");
//        }
//        else {
//            UpdateLabel(this, IDC_BATT_DID, L"バッテリーが見つかりません");
//		}
//
//        pEnumerator->Release(); pSvc->Release(); pLoc->Release();
//        return;
//    }
//
//    // ----------------------------
//    // 1) Status / Percent / Time (rate-based ETA + 5s gate + percent-lock)
//    // ----------------------------
//    VARIANT vt{};
//    CString statusText;
//    if(m_lang == Lang::EN) {
//        statusText = L"Unknown";
//    }
//    else {
//        statusText = L"不明";
//	}
//    
//    int batteryStatus = 0;
//
//    if (SUCCEEDED(pObj->Get(L"BatteryStatus", 0, &vt, 0, 0))) {
//        batteryStatus = vt.intVal;
//        if (m_lang == Lang::EN) {
//            switch (vt.intVal) {
//            case 1: statusText = L"Discharging";   break;
//            case 2: statusText = L"Charging";      break;
//            case 3: statusText = L"Fully Charged"; break;
//            default: statusText = L"Unknown";      break;
//            }
//        }
//        else {
//            switch (vt.intVal) {
//            case 1: statusText = L"放電中";   break;
//            case 2: statusText = L"充電中";   break;
//            case 3: statusText = L"満充電";   break;
//            default: statusText = L"不明";    break;
//            }
//        }
//
//        VariantClear(&vt);
//    }
//    UpdateLabel(this, IDC_BATT_STATUS, statusText);
//
//    SYSTEM_POWER_STATUS sps{};
//    if (GetSystemPowerStatus(&sps)) {
//        // Battery % + progress
//        CString pct;
//        int pctNow = -1;
//        if (sps.BatteryLifePercent != 255) {
//            pctNow = (int)sps.BatteryLifePercent;
//            pct.Format(L"%d%%", pctNow);
//            m_BatteryProgress.SetPos(pctNow);
//        }
//        else {
//            if (m_lang == Lang::EN)
//            {
//                pct = L"Unknown";
//            }
//            else {
//                pct = L"不明";
//            }
//            m_BatteryProgress.SetPos(0);
//        }
//        UpdateLabel(this, IDC_BATT_PERCENTAGE, pct);
//
//        // Pre-fetch WMI values (ROOT\WMI)
//        CString remCapStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"RemainingCapacity");
//        CString fullCapStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryFullChargedCapacity", L"FullChargedCapacity");
//        CString chargeRateStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"ChargeRate");
//        CString dischargeRateStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"DischargeRate");
//
//        auto asInt = [](const CString& s)->int {
//            if (s.IsEmpty() || s == L"Not available") return 0;
//            return _wtoi(s);
//            };
//        const int remaining_mWh = asInt(remCapStr);
//        const int full_mWh = asInt(fullCapStr);
//        const int charge_mW = asInt(chargeRateStr);
//        const int discharge_mW = asInt(dischargeRateStr);
//
//        /*auto fmtHM = [](double hours)->CString {
//            CString out;
//            if (hours <= 0.0) { out = L"Calculating..."; return out; }
//            int h = (int)hours;
//            int m = (int)((hours - h) * 60.0);
//            if (h > 0 || m > 0) out.Format(L"%d hr %d min", h, m);
//            else out = L"Calculating...";
//            return out;
//            };*/
//
//
//        auto fmtHM = [this](double hours)->CString {
//            CString out;
//
//            if (hours <= 0.0) {
//                if (m_lang == Lang::EN)
//                    out = L"Calculating...";
//                else
//                    out = L"計算中...";
//                return out;
//            }
//
//            int h = (int)hours;
//            int m = (int)((hours - h) * 60.0);
//
//            if (h > 0 || m > 0) {
//                if (m_lang == Lang::EN)
//                {
//                    out.Format(L"%d hr %d min", h, m);
//                }
//                else {
//                    out.Format(L"%d 時間 %d 分", h, m);
//
//                }
//                    
//            }
//            else {
//                if (m_lang == Lang::EN)
//                    out = L"Calculating...";
//                else
//                    out = L"計算中...";
//            }
//
//            return out;
//            };
//
//
//        // Percent-lock + 5s-gate state (function-local statics)
//        static int        s_lastPct = -1;
//        static int        s_lastMode = 2;
//        static double     s_lockedEtaHours = -1.0; // numeric ETA in hours
//        static ULONGLONG  s_readyAtMs = 0;         // when ETA may first appear after mode/first run gate
//
//        CString remain;      // final output (localized)
//        double  etaHours = -1.0; // computed ETA based on rate (in hours), -1 = unknown
//
//        const bool onAC = (sps.ACLineStatus == 1);
//        const bool fully = (batteryStatus == 3 || pctNow == 100);
//
//        int curMode = 2; // default not-lockable
//
//        if (onAC) {
//            if (fully) {
//                remain = (m_lang == Lang::EN) ? L"Fully Charged" : L"満充電";
//                curMode = 2;
//            }
//            else if ((batteryStatus == 2 || pctNow < 100) && full_mWh > 0 && remaining_mWh >= 0) {
//                const int missing_mWh = max(0, full_mWh - remaining_mWh);
//                if (charge_mW > 0 && missing_mWh > 0) {
//                    double hours = (double)missing_mWh / (double)charge_mW;
//                    if (pctNow >= 80 && pctNow < 100) {
//                        hours *= (1.0 + (pctNow - 80) / 100.0); // up to +0.20x
//                    }
//                    etaHours = hours;
//                }
//                else {
//                    etaHours = -1.0;
//                }
//                curMode = 1; // charging
//            }
//            else {
//                remain = (m_lang == Lang::EN) ? L"Plugged In" : L"差し込まれた";
//                curMode = 2;
//            }
//        }
//        else {
//            // discharging
//            if (remaining_mWh > 0 && discharge_mW > 0) {
//                double hours = (double)remaining_mWh / (double)discharge_mW;
//                etaHours = hours;
//            }
//            else {
//                etaHours = -1.0;
//            }
//            curMode = 0; // discharging
//        }
//
//        // === 5s gate + percent-locked display (using numeric ETA) ===
//        ULONGLONG nowMs = GetTickCount64();
//        bool      lockable = ((curMode == 0 || curMode == 1) && pctNow >= 0);
//        bool      modeChanged = (curMode != s_lastMode);
//
//        if (lockable) {
//            // Start (or restart) 5s gate on first entry or mode change
//            if (modeChanged || s_lastMode == 2 || s_readyAtMs == 0) {
//                s_readyAtMs = nowMs + 5000;  // gate opens after 5s
//                s_lockedEtaHours = -1.0;
//                s_lastPct = -1;            // force capture after gate
//            }
//
//            if (nowMs < s_readyAtMs) {
//                // Gate not yet open -> show placeholder (localized)
//                remain = (m_lang == Lang::EN) ? L"Calculating..." : L"計算中...";
//                s_lastMode = curMode;
//            }
//            else {
//                // Gate open -> apply percent-lock on numeric ETA
//                if (s_lastPct < 0 || modeChanged || pctNow != s_lastPct) {
//                    s_lockedEtaHours = etaHours;  // may be -1.0 if unknown; that's fine
//                    s_lastPct = pctNow;
//                    s_lastMode = curMode;
//                }
//
//                double displayHours = (s_lockedEtaHours >= 0.0) ? s_lockedEtaHours : etaHours;
//
//                if (displayHours > 0.0) {
//                    remain = fmtHM(displayHours); // <-- formatted *now* with current m_lang
//                }
//                else {
//                    remain = (m_lang == Lang::EN) ? L"Calculating..." : L"計算中...";
//                }
//            }
//        }
//        else {
//            // Not lockable states: reset gate + lock, use existing remain or fallback
//            s_lastMode = 2;
//            s_readyAtMs = 0;
//            s_lockedEtaHours = -1.0;
//            s_lastPct = pctNow;
//
//            if (remain.IsEmpty()) {
//                remain = (m_lang == Lang::EN) ? L"Calculating..." : L"計算中...";
//            }
//        }
//
//        UpdateLabel(this, IDC_BATT_TIME, remain);
//
//
//        // ----- Current (Remaining) Capacity -----
//        CString currCapOut;
//        if (remaining_mWh > 0) currCapOut.Format(L"%d mWh", remaining_mWh);
//        else {
//            if(m_lang == Lang::EN) {
//                currCapOut = L"Unknown";
//            }
//            else {
//                currCapOut = L"不明";
//            }
//        };
//        UpdateLabel(this, IDC_BATT_CURRCAPACITY, currCapOut);
//    }
//    else {
//        if(m_lang == Lang::EN) {
//            UpdateLabel(this, IDC_BATT_PERCENTAGE, L"Unknown");
//            UpdateLabel(this, IDC_BATT_TIME, L"Unknown");
//        }
//        else {
//            UpdateLabel(this, IDC_BATT_PERCENTAGE, L"不明");
//            UpdateLabel(this, IDC_BATT_TIME, L"不明");
//        }
//        
//    }
//
//    // --------------------------
//    // 7) Voltage
//    // --------------------------
//    {
//        CString voltageStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"Voltage");
//        if (voltageStr != L"Not available") {
//            double volts = _wtoi(voltageStr) / 1000.0; // mV -> V
//            CString out; out.Format(L"%.2f V", volts);
//            UpdateLabel(this, IDC_BATT_VOLTAGE, out);
//        }
//        else {
//            if(m_lang == Lang::EN) {
//                UpdateLabel(this, IDC_BATT_VOLTAGE, L"Unknown");
//            }
//            else {
//                UpdateLabel(this, IDC_BATT_VOLTAGE, L"不明");
//            }
//        }
//    }
//
//    // --------------------------
//    // 10) Temperature (if avail)
//    // --------------------------
//    {
//        CString t = QueryBatteryTemperature(); // your helper
//        if (t == L"Not available") {
//            if(m_lang == Lang::EN) {
//                t = L"Unknown";
//            }
//            else {
//                t = L"不明";
//			}
//        };
//        UpdateLabel(this, IDC_BATT_TEMP, t);
//    }
//
//    // cleanup
//    pObj->Release();
//    pEnumerator->Release();
//    pSvc->Release();
//    pLoc->Release();
//
//    // your post-actions
//    UpdateDischargeButtonStatus();
//    CheckBatteryTransition();
//
//
//}




void CBatteryHelthDlg::GetBatteryInfo()
{
    // --- Connect to WMI (ROOT\CIMV2 : Win32_Battery) ---
    IWbemLocator* pLoc = nullptr;
    IWbemServices* pSvc = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hr) || !pLoc) {
        if (m_lang == Lang::EN) {
            UpdateLabel(this, IDC_BATT_DID, L"Failed to create IWbemLocator");
        }
        else {
            UpdateLabel(this, IDC_BATT_DID, L"IWbemLocator の作成に失敗しました");
        }
        return;
    }
    hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hr) || !pSvc) {
        if (m_lang == Lang::EN) {
            UpdateLabel(this, IDC_BATT_DID, L"Could not connect to WMI");
        }
        else {
            UpdateLabel(this, IDC_BATT_DID, L"WMI に接続できませんでした");
        }
        pLoc->Release();
        return;
    }
    hr = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL, EOAC_NONE);
    if (FAILED(hr)) {
        if (m_lang == Lang::EN) {
            UpdateLabel(this, IDC_BATT_DID, L"Could not set proxy blanket");
        }
        else {
            UpdateLabel(this, IDC_BATT_DID, L"プロキシ ブランケットを設定できませんでした");
        }
        pSvc->Release(); pLoc->Release();
        return;
    }
    // --- Query Win32_Battery ---
    IEnumWbemClassObject* pEnumerator = nullptr;
    hr = pSvc->ExecQuery(bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_Battery"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL, &pEnumerator);
    if (FAILED(hr) || !pEnumerator) {
        if (m_lang == Lang::EN) {
            UpdateLabel(this, IDC_BATT_DID, L"WMI query failed");
        }
        else {
            UpdateLabel(this, IDC_BATT_DID, L"WMI クエリに失敗しました");
        }

        pSvc->Release(); pLoc->Release();
        return;
    }
    IWbemClassObject* pObj = nullptr;
    ULONG uReturn = 0;
    hr = pEnumerator->Next(WBEM_INFINITE, 1, &pObj, &uReturn);
    if (uReturn == 0 || !pObj) {
        if (m_lang == Lang::EN) {
            UpdateLabel(this, IDC_BATT_DID, L"No battery found");
        }
        else {
            UpdateLabel(this, IDC_BATT_DID, L"バッテリーが見つかりません");
        }
        pEnumerator->Release(); pSvc->Release(); pLoc->Release();
        return;
    }
    // ----------------------------
    // 1) Status / Percent / Time (rate-based ETA + 5s gate + percent-lock)
    // ----------------------------
    VARIANT vt{};
    CString statusText;
    if (m_lang == Lang::EN) {
        statusText = L"Unknown";
    }
    else {
        statusText = L"不明";
    }

    int batteryStatus = 0;
    if (SUCCEEDED(pObj->Get(L"BatteryStatus", 0, &vt, 0, 0))) {
        batteryStatus = vt.intVal;
        if (m_lang == Lang::EN) {
            switch (vt.intVal) {
            case 1: statusText = L"Discharging";   break;
            case 2: statusText = L"Charging";      break;
            case 3: statusText = L"Fully Charged"; break;
            default: statusText = L"Unknown";      break;
            }
        }
        else {
            switch (vt.intVal) {
            case 1: statusText = L"放電中";   break;
            case 2: statusText = L"充電中";   break;
            case 3: statusText = L"満充電";   break;
            default: statusText = L"不明";    break;
            }
        }
        VariantClear(&vt);
    }
    UpdateLabel(this, IDC_BATT_STATUS, statusText);
    SYSTEM_POWER_STATUS sps{};
    if (GetSystemPowerStatus(&sps)) {
        // Battery % + progress
        CString pct;
        int pctNow = -1;
        if (sps.BatteryLifePercent != 255) {
            pctNow = (int)sps.BatteryLifePercent;
            pct.Format(L"%d%%", pctNow);
            m_BatteryProgress.SetPos(pctNow);
        }
        else {
            if (m_lang == Lang::EN)
            {
                pct = L"Unknown";
            }
            else {
                pct = L"不明";
            }
            m_BatteryProgress.SetPos(0);
        }
        UpdateLabel(this, IDC_BATT_PERCENTAGE, pct);
        // Pre-fetch WMI values (ROOT\WMI)
        CString remCapStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"RemainingCapacity");
        CString fullCapStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryFullChargedCapacity", L"FullChargedCapacity");
        CString chargeRateStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"ChargeRate");
        CString dischargeRateStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"DischargeRate");
        auto asInt = [](const CString& s)->int {
            if (s.IsEmpty() || s == L"Not available") return 0;
            return _wtoi(s);
            };
        const int remaining_mWh = asInt(remCapStr);
        const int full_mWh = asInt(fullCapStr);
        const int charge_mW = asInt(chargeRateStr);
        const int discharge_mW = asInt(dischargeRateStr);

        auto fmtHM = [this](double hours)->CString {
            CString out;
            if (hours <= 0.0) {
                if (m_lang == Lang::EN)
                    out = L"Calculating...";
                else
                    out = L"計算中...";
                return out;
            }
            int h = (int)hours;
            int m = (int)((hours - h) * 60.0);
            if (h > 0 || m > 0) {
                if (m_lang == Lang::EN)
                {
                    out.Format(L"%d hr %d min", h, m);
                }
                else {
                    out.Format(L"%d 時間 %d 分", h, m);
                }
            }
            else {
                if (m_lang == Lang::EN)
                    out = L"Calculating...";
                else
                    out = L"計算中...";
            }
            return out;
            };

        // Percent-lock + 5s-gate state (function-local statics)
        static int        s_lastPct = -1;
        static int        s_lastMode = 2;
        static double     s_lockedEtaHours = -1.0;
        static ULONGLONG  s_readyAtMs = 0;

        CString remain;
        double  etaHours = -1.0;
        const bool onAC = (sps.ACLineStatus == 1);
        const bool fully = (batteryStatus == 3 || pctNow == 100);
        int curMode = 2;

        if (onAC) {
            if (fully) {
                remain = (m_lang == Lang::EN) ? L"Fully Charged" : L"満充電";
                curMode = 2;
            }
            else if ((batteryStatus == 2 || pctNow < 100) && full_mWh > 0 && remaining_mWh >= 0) {
                const int missing_mWh = max(0, full_mWh - remaining_mWh);

                // Primary: Try charge rate
                if (charge_mW > 0 && missing_mWh > 0) {
                    double hours = (double)missing_mWh / (double)charge_mW;
                    if (pctNow >= 80 && pctNow < 100) {
                        hours *= (1.0 + (pctNow - 80) / 100.0);
                    }
                    etaHours = hours;
                }
                // Fallback 1: Use capacity percentage with historical average
                else if (pctNow > 0 && pctNow < 100 && full_mWh > 0) {
                    // Estimate based on percentage remaining to charge
                    // Assume average charging takes ~2-3 hours for 0-100%
                    double pctRemaining = (100.0 - pctNow) / 100.0;
                    etaHours = pctRemaining * 2.5; // rough estimate
                }
                // Fallback 2: Try to get EstimatedRunTime from Win32_Battery
                else if (pctNow > 0 && pctNow < 100) {
                    // Get EstimatedRunTime from Win32_Battery (when charging, some systems provide charge time)
                    VARIANT vtEstTime{};
                    if (SUCCEEDED(pObj->Get(L"EstimatedRunTime", 0, &vtEstTime, 0, 0))) {
                        UINT estMinutes = vtEstTime.uintVal;
                        VariantClear(&vtEstTime);

                        // EstimatedRunTime is in minutes
                        // Value 71582788 means unknown/unavailable
                        if (estMinutes != 71582788 && estMinutes > 0 && estMinutes < 10000) {
                            etaHours = estMinutes / 60.0;
                        }
                        else {
                            // Last resort: simple percentage-based estimate
                            double pctRemaining = (100.0 - pctNow) / 100.0;
                            etaHours = pctRemaining * 2.5;
                        }
                    }
                    else {
                        // Fallback to percentage estimate
                        double pctRemaining = (100.0 - pctNow) / 100.0;
                        etaHours = pctRemaining * 2.5;
                    }
                }
                else {
                    etaHours = -1.0;
                }
                curMode = 1;
            }
            else {
                remain = (m_lang == Lang::EN) ? L"Plugged In" : L"差し込まれた";
                curMode = 2;
            }
        }
        else {
            // Discharging
            // Primary: Try discharge rate
            if (remaining_mWh > 0 && discharge_mW > 0) {
                double hours = (double)remaining_mWh / (double)discharge_mW;
                etaHours = hours;
            }
            // Fallback 1: Use capacity percentage
            else if (pctNow > 0 && full_mWh > 0 && remaining_mWh > 0) {
                // Estimate based on current percentage
                // Assume average discharge takes ~4-8 hours for 100-0%
                double pctRemaining = pctNow / 100.0;
                etaHours = pctRemaining * 6.0; // rough estimate
            }
            // Fallback 2: Try Win API estimated time
            else if (sps.BatteryLifeTime != (DWORD)-1 && sps.BatteryLifeTime > 0) {
                // BatteryLifeTime is in seconds, represents time remaining
                etaHours = sps.BatteryLifeTime / 3600.0;
            }
            else {
                etaHours = -1.0;
            }
            curMode = 0;
        }

        // === 5s gate + percent-locked display ===
        ULONGLONG nowMs = GetTickCount64();
        bool      lockable = ((curMode == 0 || curMode == 1) && pctNow >= 0);
        bool      modeChanged = (curMode != s_lastMode);

        if (lockable) {
            if (modeChanged || s_lastMode == 2 || s_readyAtMs == 0) {
                s_readyAtMs = nowMs + 5000;
                s_lockedEtaHours = -1.0;
                s_lastPct = -1;
            }
            if (nowMs < s_readyAtMs) {
                remain = (m_lang == Lang::EN) ? L"Calculating..." : L"計算中...";
                s_lastMode = curMode;
            }
            else {
                if (s_lastPct < 0 || modeChanged || pctNow != s_lastPct) {
                    s_lockedEtaHours = etaHours;
                    s_lastPct = pctNow;
                    s_lastMode = curMode;
                }
                double displayHours = (s_lockedEtaHours >= 0.0) ? s_lockedEtaHours : etaHours;
                if (displayHours > 0.0) {
                    remain = fmtHM(displayHours);
                }
                else {
                    remain = (m_lang == Lang::EN) ? L"Unknown" : L"不明";
                }
            }
        }
        else {
            s_lastMode = 2;
            s_readyAtMs = 0;
            s_lockedEtaHours = -1.0;
            s_lastPct = pctNow;
            if (remain.IsEmpty()) {
                remain = (m_lang == Lang::EN) ? L"Calculating..." : L"計算中...";
            }
        }
        UpdateLabel(this, IDC_BATT_TIME, remain);

        // ----- Current (Remaining) Capacity -----
        CString currCapOut;
        if (remaining_mWh > 0) currCapOut.Format(L"%d mWh", remaining_mWh);
        else {
            if (m_lang == Lang::EN) {
                currCapOut = L"Unknown";
            }
            else {
                currCapOut = L"不明";
            }
        }
        UpdateLabel(this, IDC_BATT_CURRCAPACITY, currCapOut);
    }
    else {
        if (m_lang == Lang::EN) {
            UpdateLabel(this, IDC_BATT_PERCENTAGE, L"Unknown");
            UpdateLabel(this, IDC_BATT_TIME, L"Unknown");
        }
        else {
            UpdateLabel(this, IDC_BATT_PERCENTAGE, L"不明");
            UpdateLabel(this, IDC_BATT_TIME, L"不明");
        }
    }

    // --------------------------
    // 7) Voltage
    // --------------------------
    {
        CString voltageStr = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"Voltage");
        if (voltageStr != L"Not available") {
            double volts = _wtoi(voltageStr) / 1000.0;
            CString out; out.Format(L"%.2f V", volts);
            UpdateLabel(this, IDC_BATT_VOLTAGE, out);
        }
        else {
            if (m_lang == Lang::EN) {
                UpdateLabel(this, IDC_BATT_VOLTAGE, L"Unknown");
            }
            else {
                UpdateLabel(this, IDC_BATT_VOLTAGE, L"不明");
            }
        }
    }

    // --------------------------
    // 10) Temperature (if avail)
    // --------------------------
    {
        CString t = QueryBatteryTemperature();
        if (t == L"Not available") {
            if (m_lang == Lang::EN) {
                t = L"Unknown";
            }
            else {
                t = L"不明";
            }
        }
        UpdateLabel(this, IDC_BATT_TEMP, t);
    }

    // cleanup
    pObj->Release();
    pEnumerator->Release();
    pSvc->Release();
    pLoc->Release();

    UpdateDischargeButtonStatus();
    CheckBatteryTransition();
}


void CBatteryHelthDlg::UpdateDischargeButtonStatus()
{
    SYSTEM_POWER_STATUS sps = {};
    if (!GetSystemPowerStatus(&sps))
    {
        // Could not read battery status
        GetDlgItem(IDC_BTN_DISCHARGE)->EnableWindow(FALSE);
       
        /*     GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(FALSE);*/
        return;
    }

    if (m_cpuLoadTestRunning) {
        GetDlgItem(IDC_BTN_DISCHARGE)->EnableWindow(FALSE);
        GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(FALSE);

        return;
    }

    if (m_dischargeTestRunning) {
        GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(FALSE);
        //GetDlgItem(IDC_BTN_DISCHARGE)->EnableWindow(FALSE);

        return;
    }


    if (sps.ACLineStatus == 1) // Charging
    {
       /* GetDlgItem(IDC_BTN_DISCHARGE)->EnableWindow(FALSE);*/
        /*   GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(FALSE);*/
        m_discharge_progress.ShowWindow(SW_HIDE);

       /* UpdateLabel(this, IDC_BATT_DISCHARGR, L"Unplug the charger");*/
       

           // If discharge test is running, stop it immediately
        if (m_dischargeTestRunning)
        {
            KillTimer(m_dischargeTimerID);
            m_dischargeTestRunning = false;

            m_discharge_progress.ShowWindow(SW_HIDE);
            /*  SetDlgItemText(IDC_BATT_DISCHARGR, L"Discharge Test Stopped");*/
      /*      UpdateLabel(this, IDC_BATT_DISCHARGR, L"Unplug the charger");*/
        }

    }
    else // Not charging
    {
        GetDlgItem(IDC_BTN_DISCHARGE)->EnableWindow(TRUE);
        /*   GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow();*/
    }

    
}





void CBatteryHelthDlg::OnBnClickedBtnDischarge()
{

    if(HasBattery() == false) {
        if(m_lang == Lang::EN) {
            AfxMessageBox(L"No battery detected.");
        }
        else {
            AfxMessageBox(L"バッテリーが検出されません。");
        }
        
        return;
	}


    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps) || sps.BatteryLifePercent == 255)
    {
        if(m_lang == Lang::EN) {
            AfxMessageBox(L"Cannot read battery percentage.");
        }
        else {
            AfxMessageBox(L"バッテリー残量を読み取ることができません。");
        }

        
        return;
    }

    if (sps.ACLineStatus == 1) // Charging
    {
        if(m_lang == Lang::EN) {
            AfxMessageBox(L"Please unplug the charger to start the discharge test.", MB_OK | MB_ICONWARNING);
        }
        else {
            AfxMessageBox(L"放電テストを開始するには充電器を抜いてください。", MB_OK | MB_ICONWARNING);
        }

        

        return;
    }


    // Check Battery Saver
    if (sps.SystemStatusFlag & 1) // Battery saver ON
    {
        m_discharge_progress.ShowWindow(SW_HIDE);

        /*UpdateLabel(this, IDC_BATT_DISCHARGR, L"Discharge Test Stopped!");*/

  /*      UpdateLabel(this, IDC_BATT_DISCHARGR, L"Unplug the charger!");*/

        KillTimer(m_dischargeTimerID);
        m_dischargeTestRunning = false;

        CString msg;
        
        if(m_lang == Lang::EN) {
            msg = L"Turn off your Battery Saver.\n\n=> WINDOWS:\n  Settings -> System -> Power & Battery -> Battery saver -> Turn Off\n\n";
        }
        else {
            msg = L"バッテリーセーバーをオフにしてください。\n\n = > Windows:\n  設定->システム->電源とバッテリー->バッテリーセーバー->オフ\n\n";
        }


        ::MessageBox(this->m_hWnd, msg, L"Battery Saver Warning", MB_OK | MB_ICONWARNING);



        return;
    }

    // If already running 
    if (m_dischargeTestRunning)
    {
        m_dischargeTestRunning = false;
        m_discharge_progress.ShowWindow(SW_HIDE);
        KillTimer(m_dischargeTimerID);

        SetDlgItemText(IDC_BTN_DISCHARGE, L"Start Discharge Test");
        /*  SetDlgItemText(IDC_BATT_DISCHARGR, L"Discharge Test Stopped!");*/

        /*UpdateLabel(this, IDC_BATT_DISCHARGR, L"Unplug the charger!");*/


        if (sps.ACLineStatus == 1) {
            if(m_lang == Lang::EN) {
                UpdateLabel(this, IDC_BATT_DISCHARGR, L"Unplug the charger!");
            }
            else {
                UpdateLabel(this, IDC_BATT_DISCHARGR, L"充電器を抜いてください!");
            }

            
        }
        else
        {

            if (m_lang == Lang::EN) {
                UpdateLabel(this, IDC_BATT_DISCHARGR, L"Discharge Test Stopped");
				AfxMessageBox(L"Discharge test stopped.");
            }
            else {
                UpdateLabel(this, IDC_BATT_DISCHARGR, L"");
                AfxMessageBox(L"放電テストを停止しました。");
            }

            
        }

        GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(TRUE);

        /* m_dischargeClick = false;*/

		InitToolTips();

        return;
    }


    if (m_dischargeClick) {
        SetButtonFont(IDC_BATT_DISCHARGR, true);
    }

    GetDlgItem(IDC_BATT_DISCHARGR)->ShowWindow(SW_HIDE);
    m_discharge_progress.ShowWindow(SW_SHOW);

   



    // Start discharge test
    m_initialBatteryPercent = sps.BatteryLifePercent;

    m_elapsedMinutes = 0;
    m_elapsedSeconds = 0;

    m_dischargeTestRunning = true;
    InitToolTips();


    GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(FALSE);

    if (m_lang == Lang::EN) {
        SetDlgItemText(IDC_BTN_DISCHARGE, L"Stop Discharge Test");

    }
    else {
        SetDlgItemText(IDC_BTN_DISCHARGE, L"放電テスト停止");
    }

    

    SetTimer(m_dischargeTimerID, 1000, NULL); // 1-second interval

    //CString imsg;
    //imsg.Format(L"\n\nDischarge Test Running...\nTime Elapsed: %d min\nInitial: %d%%\nCurrent: %d%%\nDrop: %d%%\nDrain Rate: %.2f %%/min",
    //    0, m_initialBatteryPercent, m_initialBatteryPercent, 0, 0.0);
    //SetDlgItemText(IDC_BATT_DISCHARGR, imsg);
}



void CBatteryHelthDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1 || nIDEvent == 2)
    {
       if (nIDEvent == 2) {
            GetBatteryInfo();
	   }


        if (nIDEvent == 1) {
            CPoint mouse; GetCursorPos(&mouse);

            for (UINT id : m_buttonIds) {
                CWnd* p = GetDlgItem(id);
                if (!p || !::IsWindow(p->GetSafeHwnd())) continue;

                CRect rc; p->GetWindowRect(&rc);
                BOOL over = rc.PtInRect(mouse);

                BOOL& last = m_hover[id]; // creates entry if missing
                if (over != last) {
                    last = over;
                    p->Invalidate(FALSE); // repaint only this button
                }
            }
        }
        CDialogEx::OnTimer(nIDEvent);


    }
    else if (nIDEvent == m_dischargeTimerID && m_dischargeTestRunning)
    { 
        m_elapsedSeconds++; // count seconds

        SYSTEM_POWER_STATUS sps;
        if (GetSystemPowerStatus(&sps) && sps.BatteryLifePercent != 255)
        {
            int drop = m_initialBatteryPercent - sps.BatteryLifePercent;

            // Calculate progress in percentage
            int totalSeconds = m_dischargeDurationMinutes * 60;
            double progressPercent = 100.0 * m_elapsedSeconds / totalSeconds;
            if (progressPercent > 100) progressPercent = 100;

            // Calculate drain rate per minute (optional)
            double drainRate = 0.0;
            if (m_elapsedSeconds > 0)
                drainRate = static_cast<double>(drop) / (m_elapsedSeconds / 60.0); // % per min

            CString msg;
            //msg.Format(L"Time elapsed: %d sec\nInitial: %d%%\nCurrent: %d%%\nDrop: %d%%\nDrain Rate: %.2f %%/min\nProgress: %.0f%%",
            //    m_elapsedSeconds, m_initialBatteryPercent, sps.BatteryLifePercent, drop, drainRate, progressPercent);

            if(m_lang == Lang::EN) {
                msg.Format(L"Please wait 5 minutes to complete. (%.0f%%)", progressPercent);
            }
            else {
                    msg.Format(L"完了まで 5 分お待ちください。（ %.0f%%）", progressPercent);
			}


            /*SetDlgItemText(IDC_BATT_DISCHARGR, msg);*/
            UpdateLabel(this, IDC_BATT_DISCHARGR, msg);

            m_discharge_progress.SetPos(progressPercent);




            if (sps.ACLineStatus == 1) {
                KillTimer(m_dischargeTimerID);
                m_dischargeTestRunning = false;
                UpdateLabel(this, IDC_BATT_DISCHARGR, L"");
                
                if(m_lang == Lang::EN) {
                    AfxMessageBox(L"Charger connected. Discharge test stopped.");
                }
                else {
					AfxMessageBox(L"充電器を接続しました。放電テストを停止しました。");
                }

                
                GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(TRUE);
				m_discharge_progress.ShowWindow(SW_HIDE);
            }

            else if (sps.SystemStatusFlag & 1) // Battery saver ON
            {
                m_discharge_progress.ShowWindow(SW_HIDE);

               /* UpdateLabel(this, IDC_BATT_DISCHARGR, L"Unplug the charger!");*/

                KillTimer(m_dischargeTimerID);
                m_dischargeTestRunning = false;
                
                UpdateLabel(this, IDC_BATT_DISCHARGR, L"");

                CString msg;
                
                if(m_lang == Lang::EN) {
                    msg = L"Turn off your Battery Saver.\n\n=> WINDOWS:\n  Settings -> System -> Power & Battery -> Battery saver -> Turn Off\n\n";
                }
                else {
					msg = L"バッテリーセーバーをオフにしてください。\n\n = > Windows:\n  設定->システム->電源とバッテリー->バッテリーセーバー->オフ\n\n";
                }



                ::MessageBox(this->m_hWnd, msg, L"Battery Saver Warning", MB_OK | MB_ICONWARNING);

                GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(TRUE);


                return;
            }
            // Stop the test after duration
            else if (m_elapsedSeconds >= totalSeconds )
            {
                KillTimer(m_dischargeTimerID);
                m_dischargeTestRunning = false;

                m_dischargeClick = true;
                m_discharge_progress.ShowWindow(SW_HIDE);

                // Store final result
                if(m_lang == Lang::EN) {
                    m_dischargeResult.Format(L"Initial Charge: %d%%, Final Charge: %d%%, Drop: %d%%, Drain Rate: %.2f %%/min",
                        m_initialBatteryPercent, sps.BatteryLifePercent, drop, drainRate);
                }
                else {
                    m_dischargeResult.Format(L"初期チャージ: %d%%、最終チャージ: %d%%、ドロップ: %d%%、ドレイン率: %.2f %%/分",
                        m_initialBatteryPercent, sps.BatteryLifePercent, drop, drainRate);
                }

            

                //msg.Format(L"Time elapsed: %d sec\nInitial Charge: %d%%\nCurrent Charge: %d%%\nDrop: %d%%\nDrain Rate: %.2f %%/min\nProgress: %.0f%%",
                //    m_elapsedSeconds, m_initialBatteryPercent, sps.BatteryLifePercent, drop, drainRate, progressPercent);

                if (m_lang == Lang::EN) {
                    msg.Format(L"Initial Charge: %d%%\nCurrent Charge: %d%%\nDrop: %d%%\nDrain Rate: %.2f %%/min",
                        m_initialBatteryPercent, sps.BatteryLifePercent, drop, drainRate);
                }
                else {
                    msg.Format(L"初期充電: %d%%\n現在の充電: %d%%\nドロップ: %d%%\n排出速度: %.2f %%/分",
                        m_initialBatteryPercent, sps.BatteryLifePercent, drop, drainRate);
                }

                {
                    // ---- Static values for learning/demo ----
                    float initial = static_cast<float>(m_initialBatteryPercent);
                    float current = static_cast<float>(sps.BatteryLifePercent);
                    float rate = static_cast<float>(drainRate);     // %/min
    

                    // Build (time, %) series
                    std::vector<float> tt, yy;
                    BuildSeries(initial, current, rate, tt, yy);

                    // Show the graph dialog
                    CDischargeDlg dlg;

                    if (m_lang == Lang::EN)
                    {
                        dlg.eng_lang = true;
                    }
                    else {
                        dlg.eng_lang = false;
                    }

                    dlg.SetData(initial, current, rate, tt, yy);
                    dlg.DoModal();
                }

                /*SetDlgItemText(IDC_BATT_DISCHARGR, msg);*/
             /*   UpdateLabel(this, IDC_BATT_DISCHARGR, msg);*/

                GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(TRUE);
                
                if(m_lang == Lang::EN) {
                    SetDlgItemText(IDC_BTN_DISCHARGE, L"Start Discharge Test");
                }
                else {
                    SetDlgItemText(IDC_BTN_DISCHARGE, L"放電試験を開始する");
                }

                

               /* AfxMessageBox(L"Discharge Test Completed!");*/
            }
        }
        else
        {
            /*SetDlgItemText(IDC_BATT_DISCHARGR, L"Cannot read battery percentage.");*/
            if(m_lang == Lang::EN) {
                UpdateLabel(this, IDC_BATT_DISCHARGR, L"Cannot read battery percentage.");
            }
            else {
            }
            UpdateLabel(this, IDC_BATT_DISCHARGR, L"バッテリー残量を読み取ることができません。");

        }
    }


    if (nIDEvent == m_cpuLoadTimerID && m_cpuLoadTestRunning)
    {
        m_CPU_Progress.ShowWindow(SW_SHOW);

        m_cpuLoadElapsed++;

        //Count up instead of down
        double percentDone = 100.0 * ((double)m_cpuLoadElapsed / m_cpuLoadDurationSeconds);
        if (percentDone > 100) percentDone = 100;

        CString msg;
        /*msg.Format(L"CPU Load Test Running... %.0f%% completed", percentDone);*/

        

            if(m_lang == Lang::EN) {
                msg.Format(L"Please wait 1 minute to complete. (%.0f%%)", percentDone);
            }
            else {
                msg.Format(L"完了するまで 1 分ほどお待ちください。(%.0f%%)", percentDone);
			}

        


        /*SetDlgItemText(IDC_BATT_CPULOAD, msg);*/
        UpdateLabel(this, IDC_BATT_CPULOAD, msg);

        m_CPU_Progress.SetPos(percentDone);


        // Stop if requested
        if (m_stopCpuLoad.load())
        {
            m_cpuLoadTestRunning = false;
            KillTimer(m_cpuLoadTimerID);

            if(m_lang == Lang::EN) {
                SetDlgItemText(IDC_BTN_CPULOAD, L"CPU Load Test");
            }
            else {
                SetDlgItemText(IDC_BTN_CPULOAD, L"CPU負荷テスト");
            }

       

            /*SetDlgItemText(IDC_BATT_CPULOAD, L"CPU Load Test Stopped!");*/
            
            if(m_lang == Lang::EN) {
                UpdateLabel(this, IDC_BATT_CPULOAD, L"CPU Load Test Stopped!");
            }
            else {
                UpdateLabel(this, IDC_BATT_CPULOAD, L"CPU負荷テストが停止しました！");
            }

            m_CPU_Progress.ShowWindow(SW_HIDE);
        }


    }

    if (nIDEvent == IDT_NOTIFY_LONGRUN) {
        CheckAndNotifyTopLongRunning();
    }

    CDialogEx::OnTimer(nIDEvent);
}


// Function to check CPU instruction set support
bool CheckAVX512Support() {
    int cpuInfo[4];
    __cpuid(cpuInfo, 0x7); // CPUID leaf 7 for extended features
    return (cpuInfo[1] & (1 << 16)) != 0; // Check AVX-512F bit
}

bool CheckAVX2Support() {
    int cpuInfo[4];
    __cpuid(cpuInfo, 0x7); // CPUID leaf 7 for extended features
    return (cpuInfo[1] & (1 << 5)) != 0; // Check AVX2 bit
}

bool CheckSSESupport() {
    int cpuInfo[4];
    __cpuid(cpuInfo, 1); // CPUID leaf 1 for basic features
    return (cpuInfo[3] & (1 << 25)) != 0; // Check SSE bit
}




double CBatteryHelthDlg::GetCurrentCPUUsage()
{
    static PDH_HQUERY cpuQuery = NULL;
    static PDH_HCOUNTER cpuTotal = NULL;
    static bool initialized = false;
    static DWORD lastCallTime = 0;
    static double lastValue = 0.0;

    DWORD currentTime = GetTickCount();

    if (!initialized)
    {
        if (PdhOpenQuery(NULL, 0, &cpuQuery) == ERROR_SUCCESS)
        {
            if (PdhAddCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", 0, &cpuTotal) == ERROR_SUCCESS)
            {
                PdhCollectQueryData(cpuQuery);
                initialized = true;
                lastCallTime = currentTime;
                Sleep(100); // Initial delay for accurate reading
                return 0.0;
            }
        }
        return 0.0; // Return 0 if initialization failed
    }

    // Don't query too frequently (minimum 250ms interval)
    if (currentTime - lastCallTime < 250)
    {
        return lastValue;
    }

    // Collect CPU data
    if (PdhCollectQueryData(cpuQuery) == ERROR_SUCCESS)
    {
        PDH_FMT_COUNTERVALUE counterVal;
        if (PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal) == ERROR_SUCCESS)
        {
            lastValue = counterVal.doubleValue;
            lastCallTime = currentTime;
            return lastValue;
        }
    }

    return lastValue; // Return last known value if query failed
}



void RunCPULoadRemaining(
    CBatteryHelthDlg* dlg,
    int durationSeconds,
    std::atomic<long long>* flopCounter,
    std::atomic<bool>* stopFlag)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    double localFlops = 0.0;

    const int burnIters = 100000;  // Adjust for desired workload

    while (true)
    {
        if (stopFlag->load()) break;

        auto now = std::chrono::high_resolution_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(now - startTime).count() >= durationSeconds)
            break;

        // Initialize 4 double vectors (4 doubles each)
        __m256d a = _mm256_set1_pd(1.1);
        __m256d b = _mm256_set1_pd(2.2);
        __m256d c = _mm256_set1_pd(3.3);
        __m256d d = _mm256_set1_pd(4.4);

        for (int i = 0; i < burnIters; i++)
        {
            // Vector multiply and add: 4 multiplications + 4 additions per vector = 8 FLOPs
            a = _mm256_add_pd(_mm256_mul_pd(b, c), a);
            b = _mm256_add_pd(_mm256_mul_pd(c, d), b);
            c = _mm256_add_pd(_mm256_mul_pd(d, a), c);
            d = _mm256_add_pd(_mm256_mul_pd(a, b), d);

            localFlops += 32;  // 4 vectors x 8 FLOPs per vector
        }

        std::this_thread::yield();
    }

    flopCounter->fetch_add(static_cast<long long>(localFlops), std::memory_order_relaxed);
}





// Modified CPU Load Test button handler
// CPU Load Test button handler
// CPU Load Test button handler
void CBatteryHelthDlg::OnBnClickedBtnCpuload()
{

    if(HasBattery() == false) {
        
        if(m_lang == Lang::EN) {
            AfxMessageBox(L"No battery detected. CPU Load Test requires a battery.");
        }
        else {
            AfxMessageBox(L"バッテリーが検出されません。CPU負荷テストにはバッテリーが必要です。");
        }
        
        return;
	}

    UpdateLabel(this,IDC_BATT_DISCHARGR, L"");

    // Stop if running
    if (m_cpuLoadTestRunning)
    {
        m_stopCpuLoad.store(true);
        m_cpuLoadTestRunning = false;

        if (m_lang == Lang::EN) {
            SetDlgItemText(IDC_BTN_CPULOAD, L"Start CPU Load Test");
        }
        else {
            SetDlgItemText(IDC_BTN_CPULOAD, L"CPU負荷テストの開始");
        }

       /* UpdateLabel(this, IDC_BATT_CPULOAD, L"CPU Load Not Tested...");*/
        return;
    }


    // Check current CPU usage before starting
    double usage = GetCurrentCPUUsage();
    if (usage >= 90)
    {
        CString msg;

        //en2jp

        if(m_lang== Lang::EN) {
            msg.Format(L"CPU usage is currently too high (%.0f%%). Please close background applications first.", usage);
        }
        else {
            msg.Format(L"現在CPU使用率が高すぎます（ % .0f % %）。まずバックグラウンドアプリケーションを閉じてください。", usage);
        }

        
        ::MessageBox(this->m_hWnd, msg, L"CPU Usage Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    // Get initial battery percentage
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps) || sps.BatteryLifePercent == 255)
    {
        
            if(m_lang == Lang::EN) {
                AfxMessageBox(L"Cannot read battery percentage.");
            }
            else {
                AfxMessageBox(L"バッテリー残量を読み取ることができません。");
            }
        return;
    }

    m_initialBatteryCPUPercent = sps.BatteryLifePercent;
    m_cpuLoadTestRunning = true;
    m_stopCpuLoad.store(false);

    /* SetDlgItemText(IDC_BATT_CPULOAD, L"CPU Load Test Running...");*/

    //en2jp
    if(m_lang == Lang::EN) {
        UpdateLabel(this, IDC_BATT_CPULOAD, L"CPU Load Test Running...");
    }
    else {
        UpdateLabel(this, IDC_BATT_CPULOAD, L"CPU 負荷テストを実行中...");
	}

    


    GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(FALSE);
    GetDlgItem(IDC_BTN_DISCHARGE)->EnableWindow(FALSE);

    m_cpuLoadElapsed = 0;
    SetTimer(m_cpuLoadTimerID, 1000, NULL);

    // Launch CPU load threads
    std::thread([this]()
        {
            int numCores = std::thread::hardware_concurrency();
            if (numCores == 0) numCores = 1;

            std::vector<std::thread> threads;
            std::atomic<long long> totalFlops(0);

            auto startTime = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < numCores; i++)
            {
                threads.emplace_back([this, &totalFlops, i]()
                    {
                        // Low priority
                        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);

                        // Pin to core
                        DWORD_PTR affinityMask = 1ULL << i;
                        SetThreadAffinityMask(GetCurrentThread(), affinityMask);

                        RunCPULoadRemaining(
                            this,
                            m_cpuLoadDurationSeconds,
                            &totalFlops,
                            &m_stopCpuLoad
                        );
                    });
            }

            for (auto& t : threads)
                t.join();

            auto endTime = std::chrono::high_resolution_clock::now();
            double durationSec = std::chrono::duration<double>(endTime - startTime).count();

            long long flops = totalFlops.load();
            double gflops = flops / durationSec / 1e9;   // GFLOPS
            double tops = flops / durationSec / 1e12;  // TOPS

            // Battery drop calculation
            SYSTEM_POWER_STATUS spsEnd;
            CString msg;
            if (GetSystemPowerStatus(&spsEnd) && spsEnd.BatteryLifePercent != 255)
            {
                int drop = m_initialBatteryCPUPercent - spsEnd.BatteryLifePercent;
                double rate = drop / (m_cpuLoadDurationSeconds / 60.0); // %/min

                //en2jp-

                if (drop == 0) {
                    msg.Format(L"Initial Charge: %d%%\nCurrent Charge: %d%%\nDrop: %d%%\nRate: %.2f%%/min\nGFLOPS: %.3f",
                        m_initialBatteryCPUPercent,
                        spsEnd.BatteryLifePercent,
                        drop,
                        0,
                        gflops);
                }
            
                //en2jp-
                else {
                    msg.Format(L"Initial Charge: %d%%\nCurrent Charge: %d%%\nDrop: %d%%\nRate: %.2f%%/min\nGFLOPS: %.3f",
                        m_initialBatteryCPUPercent,
                        spsEnd.BatteryLifePercent,
                        drop,
                        rate,
                        gflops);
                }


            }
            else
            {
                if(m_lang == Lang::EN) {
                    msg = L"Cannot read battery percentage.";
                }
                else {
                    msg = L"バッテリー残量を読み取ることができません。";
                }
               
            }

            m_cpuLoadTestCompleted = true;
            m_cpuLoadResult = msg;
            this->PostMessage(WM_APP + 1, 0, (LPARAM)new CString(msg));

            m_cpuLoadTestRunning = false;
            m_cpuLoadClick = true;


        }).detach();
}



// Enhanced CPU usage monitoring function

// Enhanced CPU usage monitoring function



CString CBatteryHelthDlg::QueryBatteryCapacityHistory()
{
    HRESULT hres;
    IWbemLocator* pLoc = NULL;
    IWbemServices* pSvc = NULL;
    CString result = L"Not available";

    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) return result;

    hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\WMI"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) { pLoc->Release(); return result; }

    hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
        RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
    if (FAILED(hres)) { pSvc->Release(); pLoc->Release(); return result; }

    // Try BatteryRuntime class for more detailed info
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(bstr_t("WQL"),
        bstr_t("SELECT * FROM BatteryRuntime"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);

    if (SUCCEEDED(hres) && pEnumerator)
    {
        IWbemClassObject* pClsObj = NULL;
        ULONG uReturn = 0;

        if (pEnumerator->Next(WBEM_INFINITE, 1, &pClsObj, &uReturn) == S_OK && uReturn > 0)
        {
            VARIANT vtProp;

            // Try to get EstimatedRunTime or other capacity-related properties
            if (SUCCEEDED(pClsObj->Get(L"EstimatedRunTime", 0, &vtProp, 0, 0)))
            {
                if (vtProp.vt != VT_NULL && vtProp.vt != VT_EMPTY)
                {
                    result.Format(L"%d", vtProp.intVal);
                }
                VariantClear(&vtProp);
            }
            pClsObj->Release();
        }
        pEnumerator->Release();
    }

    pSvc->Release();
    pLoc->Release();
    return result;
}





LRESULT CBatteryHelthDlg::OnCPULoadFinished(WPARAM wParam, LPARAM lParam)
{

    if (m_cpuLoadTimerID != 0)
    {
        KillTimer(m_cpuLoadTimerID);

    }


    m_CPU_Progress.ShowWindow(SW_HIDE);

    CString* pMsg = (CString*)lParam;
    //SetDlgItemText(IDC_BATT_CPULOAD, *pMsg);

    //UpdateLabel(this, IDC_BATT_CPULOAD, *pMsg);
     UpdateLabel(this, IDC_BATT_CPULOAD, L"");

	

    // ---- Parse dynamic values from *pMsg and show the graph ----
    {
        // Convert CString -> std::wstring for regex
        std::wstring w = pMsg->GetString();

        auto grab = [&](const std::wregex& re, double defVal) -> double {
            std::wsmatch m;
            if (std::regex_search(w, m, re) && m.size() >= 2) {
                try { return std::stod(m[1].str()); }
                catch (...) { /* fall through */ }
            }
            return defVal;
            };

        // Patterns tolerate spaces and integer/float values
        // Examples expected:
        //   "Initial Charge: 100%"
        //   "Current Charge: 90%"
        //   "Drop: 10%"                      (we won't actually need it for plotting)
        //   "Rate: 5.00%/min"
        //   "GFLOPS: 234.122"
        const std::wregex reInit(LR"(Initial\s*Charge:\s*([0-9]+(?:\.[0-9]+)?)\s*%)", std::regex::icase);
        const std::wregex reCurr(LR"(Current\s*Charge:\s*([0-9]+(?:\.[0-9]+)?)\s*%)", std::regex::icase);
        const std::wregex reRate(LR"(Rate:\s*([0-9]+(?:\.[0-9]+)?)\s*%/min)", std::regex::icase);
        const std::wregex reGflp(LR"(GFLOPS:\s*([0-9]+(?:\.[0-9]+)?))", std::regex::icase);

        const double initD = grab(reInit, 100.0);
        const double currD = grab(reCurr, initD);
        const double rateD = grab(reRate, 0.0);
        const double gflops = grab(reGflp, 0.0);

        float  initial = static_cast<float>(initD);
        float  current = static_cast<float>(currD);
        float  rate = static_cast<float>(rateD);

        // Build series: your BuildSeries() draws out to 0% using 'rate'
        std::vector<float> tt, yy;
        BuildSeries(initial, current, rate, tt, yy);

        CPerfDlg dlg;

        if (m_lang == Lang::EN)
        {
            dlg.eng_lang = true;
        }
        else {
            dlg.eng_lang = false;
        }

        dlg.SetData(initial, current, rate, gflops, tt, yy);
        dlg.DoModal();
    }



	//AfxMessageBox(*pMsg);

    //if (m_cpuLoadClick) {
    //    SetButtonFont(IDC_BATT_CPULOAD, true);
    //}


    delete pMsg; // free memory

    // Re-enable the CPU Load button after finishing
    GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(TRUE);

    // Re-enable the Discharge Test button after finishing
    GetDlgItem(IDC_BTN_DISCHARGE)->EnableWindow(TRUE);


    return 0;
}


void CBatteryHelthDlg::CheckBatteryTransition()
{
    SYSTEM_POWER_STATUS sps = {};
    if (!GetSystemPowerStatus(&sps))
        return;

    bool charging = (sps.ACLineStatus == 1);

    if (charging != m_lastChargingState) // Transition occurred
    {
        BatteryEvent evt;
        evt.charging = charging;
        evt.timestamp = CTime::GetCurrentTime();
        m_batteryHistory.push_back(evt);

        m_lastChargingState = charging;
    }
}

void CBatteryHelthDlg::OnBnClickedBtnUploadpdf()
{

    if (HasBattery() == false) {
        if (m_lang == Lang::EN) {
            AfxMessageBox(L"No battery detected.");
        }
        else {
            AfxMessageBox(L"バッテリーが検出されません。");
        }

        return;
    }

    // Ask user where to save
    CFileDialog fileDlg(FALSE, L"csv", L"BatteryData.csv", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, L"CSV Files (*.csv)|*.csv||");
    if (fileDlg.DoModal() != IDOK)
        return;

    CString filePath = fileDlg.GetPathName();

    // Open file in Binary mode to strictly control the BOM writing
    std::ofstream csvFile(filePath.GetString(), std::ios::out | std::ios::binary);

    if (!csvFile.is_open())
    {
        if (m_lang == Lang::EN) {
            AfxMessageBox(L"Failed to open file for writing.");
        }
        else {
            AfxMessageBox(L"書き込み用にファイルを開けませんでした。");
        }
        return;
    }

    // 1. WRITE THE BOM (Byte Order Mark) for UTF-8
    // This tells Excel that the file contains UTF-8 characters
    const unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    csvFile.write(reinterpret_cast<const char*>(bom), sizeof(bom));

    // Helper lambda to convert CString (UTF-16) to std::string (UTF-8)
    auto toCsvField = [](const CString& str) -> std::string {
        // CP_UTF8 is the key here. It converts Wide Char to UTF-8
        CW2A pszConvertedUTF8(str, CP_UTF8);
        std::string s(pszConvertedUTF8);

        // Replace line breaks with spaces
        for (auto& c : s)
        {
            if (c == '\n' || c == '\r') c = ' ';
        }

        // Enclose in quotes
        return "\"" + s + "\"";
        };

    // 2. PREPARE HEADERS (Use Wide Strings L"" and convert to UTF-8)
    CString headers;
    if (m_lang == Lang::EN) {
        headers = L"Device ID,Battery Percentage,Battery Status,Voltage (mV),Temperature (°C),Full Charge Capacity (mWh),Design Capacity (mWh),Current Capacity (mWh),Health (%),Manufacturer,Battery Name,Estimated Run Time (min),Cycle Count";
    }
    else {
        headers = L"デバイスID,バッテリーの割合,バッテリー状態,電圧（mV）,温度（°C）,フル充電容量（mWh）,設計容量（mWh）,現在の容量（mWh）,状態（％）,メーカー,バッテリー名,推定稼働時間（分）,サイクル数";
    }

    // Write Headers (Convert the whole line to UTF-8)
    CW2A headerUTF8(headers, CP_UTF8);
    csvFile << headerUTF8 << "\r\n";

    // Get current values from controls
    CString did, battPercent, battStatus, voltage, fullCap, designCap, health, deviceID, battName, estTime, cycleCount, temperature, currentCap;

    GetDlgItemText(IDC_BATT_DID, did);
    GetDlgItemText(IDC_BATT_PERCENTAGE, battPercent);
    GetDlgItemText(IDC_BATT_STATUS, battStatus);
    GetDlgItemText(IDC_BATT_VOLTAGE, voltage);
    GetDlgItemText(IDC_BATT_CAPACITY, fullCap);
    GetDlgItemText(IDC_BATT_DCAPACITY, designCap);
    GetDlgItemText(IDC_BATT_HEALTH, health);
    GetDlgItemText(IDC_BATT_MANUFAC, deviceID);
    GetDlgItemText(IDC_BATT_NAME, battName);
    GetDlgItemText(IDC_BATT_TIME, estTime);
    GetDlgItemText(IDC_BATT_CYCLE, cycleCount);
    GetDlgItemText(IDC_BATT_TEMP, temperature);
    GetDlgItemText(IDC_BATT_CURRCAPACITY, currentCap);

    // Write Data Row
    csvFile << toCsvField(did) << ","
        << toCsvField(battPercent) << ","
        << toCsvField(battStatus) << ","
        << toCsvField(voltage) << ","
        << toCsvField(temperature) << ","
        << toCsvField(fullCap) << ","
        << toCsvField(designCap) << ","
        << toCsvField(currentCap) << ","
        << toCsvField(health) << ","
        << toCsvField(deviceID) << ","
        << toCsvField(battName) << ","
        << toCsvField(estTime) << ","
        << toCsvField(cycleCount) << "\r\n";

    csvFile.close();

    if (m_lang == Lang::EN) {
        AfxMessageBox(L"Battery data exported successfully.");
    }
    else {
        AfxMessageBox(L"バッテリーデータが正常にエクスポートされました。");
    }
}




void CBatteryHelthDlg::OnBnClickedBtnHistory()
{

    if (HasBattery() == false) {
        if (m_lang == Lang::EN) {
            AfxMessageBox(L"No battery detected.");
        }
        else {
            AfxMessageBox(L"バッテリーが検出されません。");
        }

        return;
    }

    CString table;
    CString okButtonText;

    // Column headers
    if (m_lang == Lang::EN) {
        table = L"Start       End         Total Charge Time\n";
        table += L"----------------------------------------\n";
        okButtonText = L"OK";
    }
    else {
        table = L"始める       終わり         合計充電時間\n";
        table += L"----------------------------------------\n";
        okButtonText = L"オーケー"; 
    }

    CTime startTime;
    bool hasStart = false;
    for (const auto& evt : m_batteryHistory)
    {
        if (evt.charging)
        {
            startTime = evt.timestamp;
            hasStart = true;
        }
        else
        {
            if (hasStart)
            {
                CTimeSpan total = evt.timestamp - startTime;
                double totalMinutes = total.GetTotalSeconds() / 60.0;
                CString startStr = startTime.Format(L"%H:%M:%S");
                CString endStr = evt.timestamp.Format(L"%H:%M:%S");
                CString totalStr;

                if (m_lang == Lang::EN) {
                    totalStr.Format(L"%.2f min", totalMinutes);
                }
                else {
                    totalStr.Format(L"%.2f 分", totalMinutes);
                }

                startStr = startStr + CString(' ', 10 - startStr.GetLength());
                endStr = endStr + CString(' ', 10 - endStr.GetLength());
                totalStr = totalStr + CString(' ', 12 - totalStr.GetLength());
                table += startStr + endStr + totalStr + L"\n";
                hasStart = false;
            }
        }
    }

    if (hasStart)
    {
        CString startStr = startTime.Format(L"%H:%M:%S");
        CString endStr, totalStr;
        if (m_lang == Lang::EN) {
            endStr = L"null";
            totalStr = L"0 min";
        }
        else {
            endStr = L"ヌル";
            totalStr = L"0 分";
        }
        startStr = startStr + CString(' ', 10 - startStr.GetLength());
        endStr = endStr + CString(' ', 10 - endStr.GetLength());
        totalStr = totalStr + CString(' ', 12 - totalStr.GetLength());
        table += startStr + endStr + totalStr + L"\n";
    }

    if (m_batteryHistory.empty())
    {
        if (m_lang == Lang::EN) {
            table += L"No history recorded yet.";
        }
        else {
            table += L"まだ履歴は記録されていません。\n";
        }
    }

    // Use TaskDialog for custom button text
    CString title = (m_lang == Lang::EN) ? L"Battery History" : L"バッテリー履歴";

    TASKDIALOGCONFIG config = { 0 };
    config.cbSize = sizeof(config);
    config.hwndParent = m_hWnd;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
    config.dwCommonButtons = TDCBF_OK_BUTTON;
    config.pszWindowTitle = title;
    config.pszContent = table;
    config.pszMainIcon = TD_INFORMATION_ICON;

    // Custom button (not using common button to have full control)
    TASKDIALOG_BUTTON buttons[] = {
        { IDOK, okButtonText }
    };
    config.pButtons = buttons;
    config.cButtons = 1;
    config.dwCommonButtons = 0; // Don't use common buttons

    int nButton;
    TaskDialogIndirect(&config, &nButton, NULL, NULL);
}


void CBatteryHelthDlg::OnStnClickedStaticDid()
{
    // TODO: Add your control notification handler code here
}

void CBatteryHelthDlg::OnStnClickedStaticHeader()
{
    // TODO: Add your control notification handler code here
}


void DrawPngOwnerButtonCommon(LPDRAWITEMSTRUCT lp, BOOL bHover, UINT pngResId)
{
    CDC dc; dc.Attach(lp->hDC);
    CRect rc = lp->rcItem;

    // Background by state
    if (lp->itemState & ODS_SELECTED)      dc.FillSolidRect(&rc, RGB(200, 200, 200));
    else if (bHover)                        dc.FillSolidRect(&rc, RGB(220, 230, 255));
    else                                    dc.FillSolidRect(&rc, RGB(240, 240, 240));

    // Load-and-draw PNG from resource (simple version; see caching note below)
    HRSRC hRsrc = FindResource(AfxGetResourceHandle(), MAKEINTRESOURCE(pngResId), _T("PNG"));
    if (hRsrc) {
        DWORD len = SizeofResource(AfxGetResourceHandle(), hRsrc);
        HGLOBAL hMem = LoadResource(AfxGetResourceHandle(), hRsrc);
        LPVOID lpData = LockResource(hMem);

        HGLOBAL hBuffer = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hBuffer) {
            void* pBuffer = GlobalLock(hBuffer);
            memcpy(pBuffer, lpData, len);
            GlobalUnlock(hBuffer);

            IStream* pStream = nullptr;
            if (CreateStreamOnHGlobal(hBuffer, TRUE, &pStream) == S_OK) {
                Graphics g(dc.GetSafeHdc());
                Image img(pStream);
                g.DrawImage(&img, rc.left, rc.top, rc.Width(), rc.Height());

                // Hover overlay (only when not pressed)
                if (bHover && !(lp->itemState & ODS_SELECTED)) {
                    Color overlay(80, 100, 150, 255); // A,R,G,B  (80 ? 31% opacity)
                    SolidBrush brush(overlay);
                    g.FillRectangle(&brush, rc.left, rc.top, rc.Width(), rc.Height());
                }
                pStream->Release();
            }
        }
    }

    // Border
    if (lp->itemState & ODS_SELECTED) dc.DrawEdge(&rc, EDGE_SUNKEN, BF_RECT);
    else
        dc.DrawEdge(&rc, EDGE_RAISED, BF_RECT);

    dc.Detach();
}


void CBatteryHelthDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    // Only handle IDs we registered
    if (std::find(m_buttonIds.begin(), m_buttonIds.end(), (UINT)nIDCtl) != m_buttonIds.end()) {
        // Resolve PNG to use (fallback: one shared PNG if you prefer)
        UINT pngId = IDB_PNG1;
        if (!m_pngById.empty()) {
            auto it = m_pngById.find(nIDCtl);
            if (it != m_pngById.end())
                pngId = it->second;
        }

        BOOL hover = FALSE;
        auto hit = m_hover.find(nIDCtl);
        if (hit != m_hover.end())
            hover = hit->second;

        // Use the correct parameter name here
        DrawPngOwnerButtonCommon(lpDrawItemStruct, hover, pngId);
        return;
    }



    if (!lpDrawItemStruct)
    {
        CDialogEx::OnDrawItem(nIDCtl, lpDrawItemStruct);
        return;
    }

    const bool isJP = (nIDCtl == IDC_BTN_JP);
    const bool isEN = (nIDCtl == IDC_BTN_EN);

    if (isJP || isEN)
    {
        const bool active = (isJP && m_lang == Lang::JP) || (isEN && m_lang == Lang::EN);
        DrawToggleButton(lpDrawItemStruct, active, isJP ? L"JP" : L"EN");
        return; // handled
    }


    CDialogEx::OnDrawItem(nIDCtl, lpDrawItemStruct);
}




BOOL CBatteryHelthDlg::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    // Check if the cursor is over your button
    if (pWnd->GetDlgCtrlID() == IDC_BTN_CPULOAD || pWnd->GetDlgCtrlID() == IDC_BTN_DISCHARGE || pWnd->GetDlgCtrlID() == IDC_BTN_HISTORY 
        || pWnd->GetDlgCtrlID() == IDC_BTN_UPLOADPDF || pWnd->GetDlgCtrlID() == IDC_BTN_CAPHIS|| pWnd->GetDlgCtrlID() == IDC_BTN_ACTIVE 
        || pWnd->GetDlgCtrlID() == IDC_BTN_STANDBY || pWnd->GetDlgCtrlID() == IDC_BTN_USAGE || pWnd->GetDlgCtrlID() == IDC_BTN_MANIPULATIOIN
		|| pWnd->GetDlgCtrlID() == IDC_BTN_RATEINFO || pWnd->GetDlgCtrlID() == IDC_BTN_BGAPP || pWnd->GetDlgCtrlID() == IDC_BTN_JP 
        || pWnd->GetDlgCtrlID() == IDC_BTN_EN || pWnd->GetDlgCtrlID() == IDC_BTN_SLEEP
        )
    {
        ::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
        return TRUE; // handled
    }

    return CDialogEx::OnSetCursor(pWnd, nHitTest, message);
}




void CBatteryHelthDlg::OnDestroy()
{
    KillTimer(1); // Stop hover detection timer

    if (m_timerId) { KillTimer(m_timerId); m_timerId = 0; }
    RemoveTrayIcon();

    if (m_hDispNotify) {
        UnregisterPowerSettingNotification(m_hDispNotify);
        m_hDispNotify = nullptr;
    }

    CDialogEx::OnDestroy();
}


// Destructor cleanup (add this to your destructor)
void CBatteryHelthDlg::CleanupFonts()
{
    if (m_Font16px.GetSafeHandle())
        m_Font16px.DeleteObject();
    if (m_boldFont.GetSafeHandle())
        m_boldFont.DeleteObject();
    if (m_scaledFont.GetSafeHandle())
        m_scaledFont.DeleteObject();
    if (m_scaledBoldFont.GetSafeHandle())
        m_scaledBoldFont.DeleteObject();
}



static bool SaveCsvUtf8AndOpen(const std::wstring& csv, CString& outPath)
{
    // 1) Build a fixed path in %TEMP% (no timestamp -> overwrite each time)
    TCHAR tempDir[MAX_PATH] = { 0 };
    if (!GetTempPath(MAX_PATH, tempDir)) return false;

    // Use a fixed filename so it gets replaced each time
    CString fname = L"battery_history.csv";

    CString full = CString(tempDir) + fname;
    outPath = full;

    // 2) Convert wide string to UTF-8
    int needed = WideCharToMultiByte(CP_UTF8, 0,
        csv.c_str(), static_cast<int>(csv.size()),
        nullptr, 0, nullptr, nullptr);
    if (needed <= 0) return false;

    std::string utf8;
    utf8.resize(needed);
    if (WideCharToMultiByte(CP_UTF8, 0,
        csv.c_str(), static_cast<int>(csv.size()),
        &utf8[0], needed, nullptr, nullptr) <= 0) {
        return false;
    }

    // 3) Write BOM + content (CREATE_ALWAYS overwrites existing file)
    HANDLE h = CreateFile(full, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    const BYTE bom[3] = { 0xEF, 0xBB, 0xBF };
    DWORD written = 0;
    BOOL ok = WriteFile(h, bom, 3, &written, nullptr);
    if (!ok) { CloseHandle(h); return false; }

    ok = WriteFile(h, utf8.data(), static_cast<DWORD>(utf8.size()), &written, nullptr);
    CloseHandle(h);
    if (!ok) return false;

    // 4) Open with default handler
    HINSTANCE hInst = ShellExecute(nullptr, L"open", full, nullptr, nullptr, SW_SHOWNORMAL);
    return ((INT_PTR)hInst > 32);
}


void CBatteryHelthDlg::OnBnClickedBtnCaphis()
{

    if (HasBattery() == false) {
        if (m_lang == Lang::EN) {
            AfxMessageBox(L"No battery detected.");
        }
        else {
            AfxMessageBox(L"バッテリーが検出されません。");
        }

        return;
    }

    std::vector<CBatteryHelthDlg::BatteryCapacityRecord> hist;
    if (!GetBatteryCapacityHistory(hist) || hist.empty()) {
        
        if(m_lang == Lang::EN) {
            AfxMessageBox(L"Could not read Battery Capacity History.");
        }
        else {
            AfxMessageBox(L"バッテリー容量履歴を読み取ることができませんでした。");
        }

        return;
    }

    // Build CSV content (header + rows)
    std::wstring csv;
    
    if(m_lang == Lang::EN) {
        csv += L"Period ,FullChargeCapacity,DesignCapacity,Health_%\r\n";
    }
    else {
        csv = L"期間,フル充電容量,設計容量,健全性_%\r\n";
	}

    for (const auto& r : hist) {
        // If you also want ISO date, you can format COleDateTime here; we’ll stick to dateText.
        CString line;
        line.Format(L"%s,%d,%d,%.2f\r\n",
            r.dateText.GetString(),
            r.fullCharge_mWh,
            r.design_mWh,
            r.healthPct);
        csv += line.GetString();
    }

    // Save to %TEMP%\battery_capacity_history_YYYYMMDD_HHMMSS.csv and open it
    CString outPath;
    if (!SaveCsvUtf8AndOpen(csv, outPath)) {
        if(m_lang == Lang::EN) {
            AfxMessageBox(L"Failed to write/open CSV file.");
        }
        else {
            AfxMessageBox(L"CSV ファイルの書き込み / オープンに失敗しました。");
            
        }
        return;
    }

    // Optional toast
    CString msg;
    if(m_lang == Lang::EN) {
        msg.Format(L"CSV exported:\n%s", outPath.GetString());
    }
    else {
        msg.Format(L"CSV エクスポート:\n%s", outPath.GetString());
    }
    AfxMessageBox(msg);
}



// ---- Utilities: safe date diff in days
// Simple day difference
static double DaysBetween(const COleDateTime& a, const COleDateTime& b)
{
    COleDateTimeSpan span = a - b;
    return span.GetTotalDays();
}

// Linear regression of Health% vs. time (days since t0)
struct LinearFit {
    bool ok = false;
    double a = 0.0;   // intercept: health at day 0 (t0)
    double b = 0.0;   // slope: health change per day (usually negative)
    COleDateTime t0;  // reference date (first valid record)
    CString note;
};

static LinearFit FitHealthVsDays(const std::vector<CBatteryHelthDlg::BatteryCapacityRecord>& hist)
{
    LinearFit fit;

    // Collect (t, y) pairs with valid dates
    std::vector<double> t; t.reserve(hist.size());
    std::vector<double> y; y.reserve(hist.size());

    // Find first valid date
    COleDateTime t0;
    bool t0Set = false;

    for (const auto& r : hist) {
        if (r.date.m_dt == 0) continue;       // skip invalid dates
        if (r.healthPct <= 0.0) continue;     // skip bogus
        if (!t0Set) { t0 = r.date; t0Set = true; }
        t.push_back(DaysBetween(r.date, t0));  // days since t0
        y.push_back(r.healthPct);
    }

    if (t.size() < 2) {
        fit.ok = false;
        fit.note = L"Not enough dated points to fit a trend.";
        return fit;
    }

    // Ordinary least squares: y = a + b*t
    const size_t n = t.size();
    double sumT = 0, sumY = 0, sumTT = 0, sumTY = 0;
    for (size_t i = 0; i < n; ++i) {
        sumT += t[i];
        sumY += y[i];
        sumTT += t[i] * t[i];
        sumTY += t[i] * y[i];
    }
    double denom = n * sumTT - sumT * sumT;
    if (fabs(denom) < 1e-9) {
        fit.ok = false;
        fit.note = L"Time points are degenerate.";
        return fit;
    }

    fit.b = (n * sumTY - sumT * sumY) / denom;           // slope (% per day)
    fit.a = (sumY - fit.b * sumT) / static_cast<double>(n); // intercept
    fit.t0 = t0;
    fit.ok = true;
    return fit;
}

static bool PredictDateForTarget(const LinearFit& fit, double targetPct, COleDateTime& outDate)
{
    if (!fit.ok) return false;
    // y = a + b*t  ->  t = (y - a)/b
    if (fabs(fit.b) < 1e-9) return false;  // flat line, can't predict
    double tDays = (targetPct - fit.a) / fit.b;
    if (tDays < 0) return false;           // already below target
    outDate = fit.t0 + COleDateTimeSpan(tDays, 0, 0, 0);
    return true;
}



//void CBatteryHelthDlg::OnBnClickedBtnPrediction()
//{
//    CPredictionDlg dlg(this);
//
//	dlg.DoModal(); // open modal window
//}

void CBatteryHelthDlg::OnBnClickedBtnActive()
{
    if(HasBattery() == false)
    {
        if (m_lang == Lang::EN) {
            AfxMessageBox(L"No battery detected.");
        }
        else {
            AfxMessageBox(L"バッテリーが検出されません。");
        }
        return;
	}


    CTrendDlg dlg(this);

    // Optional: pass custom data instead of defaults
    // std::vector<CString> labels = { L"08-01", ... };
    // std::vector<float> full = { ... };
    // std::vector<float> design = { ... };
    // dlg.SetData(labels, full, design);
    // TODO: Add your control notification handler code here
    if (m_lang == Lang::EN)
    {
		dlg.eng_lang = true;
    }
    else{
        dlg.eng_lang = false;
    }
    dlg.DoModal(); // open modal window
}

void CBatteryHelthDlg::OnBnClickedBtnStandby()
{

    if (HasBattery() == false) {
        if (m_lang == Lang::EN) {
            AfxMessageBox(L"No battery detected.");
        }
        else {
            AfxMessageBox(L"バッテリーが検出されません。");
        }

        return;
    }

    StandByDlg dlg(this);

    // Optional: pass custom data instead of defaults
    // std::vector<CString> labels = { L"08-01", ... };
    // std::vector<float> full = { ... };
    // std::vector<float> design = { ... };
    // dlg.SetData(labels, full, design);

    if (m_lang == Lang::EN)
    {
        dlg.eng_lang = true;
    }
    else {
        dlg.eng_lang = false;
    }

    dlg.DoModal(); // open modal window
}





// ---------- Usage History row model ----------
struct UsageHistoryRow {
    CString period;        // e.g. "2025-08-01 - 2025-08-04"
    double battActiveHrs = 0.0;  // Battery ACTIVE
    double battStandbyHrs = 0.0;  // Battery CONNECTED STANDBY
    double acActiveHrs = 0.0;  // AC ACTIVE
    double acStandbyHrs = 0.0;  // AC CONNECTED STANDBY
};

// ---------- Small helpers (local linkage) ----------
static CString StripHtmlSimple(const CString& in)
{
    std::wstring s(in);
    static const std::wregex reTags(L"<[^>]*>", std::regex_constants::icase);
    static const std::wregex reNBSP(L"&nbsp;", std::regex_constants::icase);
    static const std::wregex reWS(L"[ \t\r\n]+");
    s = std::regex_replace(s, reTags, L" ");
    s = std::regex_replace(s, reNBSP, L" ");
    s = std::regex_replace(s, reWS, L" ");
    CString o(s.c_str()); o.Trim(); return o;
}

// Read text file (UTF-16LE BOM / UTF-8 BOM / UTF-8 / ANSI)
static CString ReadTextAutoEncodingLocal(const CString& path)
{
    CFile f;
    if (!f.Open(path, CFile::modeRead | CFile::shareDenyNone)) return {};
    const ULONGLONG len = f.GetLength();
    if (len == 0 || len > 32ULL * 1024 * 1024) { f.Close(); return {}; }

    std::vector<BYTE> buf((size_t)len);
    f.Read(buf.data(), (UINT)len); f.Close();

    // UTF-16LE BOM
    if (len >= 2 && buf[0] == 0xFF && buf[1] == 0xFE) {
        return CString((LPCWSTR)(buf.data() + 2), (int)((len - 2) / 2));
    }
    // UTF-8 BOM
    if (len >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) {
        int n = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data() + 3, (int)len - 3, nullptr, 0);
        CStringW w; LPWSTR p = w.GetBuffer(n);
        MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data() + 3, (int)len - 3, p, n);
        w.ReleaseBuffer(n); return w;
    }
    // Try UTF-8 (no BOM)
    int n = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data(), (int)len, nullptr, 0);
    if (n > 0) {
        CStringW w; LPWSTR p = w.GetBuffer(n);
        MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)buf.data(), (int)len, p, n);
        w.ReleaseBuffer(n); return w;
    }
    // ANSI
    n = MultiByteToWideChar(CP_ACP, 0, (LPCSTR)buf.data(), (int)len, nullptr, 0);
    if (n > 0) {
        CStringW w; LPWSTR p = w.GetBuffer(n);
        MultiByteToWideChar(CP_ACP, 0, (LPCSTR)buf.data(), (int)len, p, n);
        w.ReleaseBuffer(n); return w;
    }
    return {};
}

static float HmsToHoursLocal(int h, int m, int s) {
    return (float)h + (float)m / 60.f + (float)s / 3600.f;
}

// Returns first two H:MM:SS matches found in text
static std::vector<CString> ExtractHms2(const std::wstring& text)
{
    std::vector<CString> out;
    std::wregex re(LR"((\d{1,6}):([0-5]\d):([0-5]\d))");
    for (std::wsregex_iterator it(text.begin(), text.end(), re), end; it != end; ++it) {
        out.emplace_back(it->str().c_str());
        if (out.size() == 2) break;
    }
    return out;
}


static CString ExtractFirstHms(const CString& s)
{
    std::wregex re(LR"((\d{1,6}):([0-9]{2}):([0-9]{2}))");
    std::wstring ws = s.GetString();
    std::wsmatch m;
    if (std::regex_search(ws, m, re)) {
        return CString(m[0].str().c_str());
    }
    return L"";
}

static double HmsTextToHours(const CString& hms)
{
    if (hms.IsEmpty() || hms == L"-") return 0.0;
    int h = 0, m = 0, s = 0;
    if (swscanf_s(hms.GetString(), L"%d:%d:%d", &h, &m, &s) == 3) {
        if (h > 100000) return 0.0; // guard for glitches
        return static_cast<double>(h) + m / 60.0 + s / 3600.0;
    }
    return 0.0;
}

// Extract *all* H:MM:SS occurrences from a string (keeps order)
static std::vector<CString> ExtractAllHms(const CString& s)
{
    std::vector<CString> out;
    std::wregex re(LR"((\d{1,6}):([0-9]{2}):([0-9]{2}))");
    std::wstring ws = s.GetString();
    for (std::wsregex_iterator it(ws.begin(), ws.end(), re), end; it != end; ++it) {
        out.emplace_back(CString(it->str().c_str()));
    }
    return out;
}


// Parse Usage History table from battery_report.html
bool GetUsageHistory(std::vector<UsageHistoryRow>& outRows)
{
    outRows.clear();

    // Fresh report (best effort)
    auto GenBatteryReportPath = []() -> CString {
        TCHAR tempPath[MAX_PATH] = {};
        GetTempPath(MAX_PATH, tempPath);
        CString path; path.Format(_T("%sbattery_report.html"), tempPath);
        return path;
        };
    auto RunPowerCfgBatteryReport = [](const CString& path) -> bool {
        CString cmd; cmd.Format(_T("powercfg /batteryreport /output \"%s\""), path);
        STARTUPINFO si{ sizeof(si) }; PROCESS_INFORMATION pi{};
        CString cmdBuf = cmd;
        if (!CreateProcess(nullptr, cmdBuf.GetBuffer(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) return false;
        WaitForSingleObject(pi.hProcess, 10000);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        return true;
        };

    const CString reportPath = GenBatteryReportPath();
    RunPowerCfgBatteryReport(reportPath);

    CString html = ReadTextAutoEncoding(reportPath);
    if (html.IsEmpty()) return false;

    std::wstring H = html.GetString();

    // Find the Usage history table
    std::wsmatch mh;
    std::wregex reTable(
        LR"(USAGE\s*HISTORY[\s\S]*?<table[^>]*>([\s\S]*?)</table>)",
        std::regex_constants::icase
    );
    if (!std::regex_search(H, mh, reTable)) {
        std::wregex reTableLoose(
            LR"(Usage\s*history[\s\S]*?<table[^>]*>([\s\S]*?)</table>)",
            std::regex_constants::icase
        );
        if (!std::regex_search(H, mh, reTableLoose)) return false;
    }
    std::wstring tableHtml = mh[1].str();

    // Row/cell regex
    std::wregex reRow(L"<tr[^>]*>([\\s\\S]*?)</tr>", std::regex_constants::icase);
    std::wregex reCell(L"<t[dh][^>]*>([\\s\\S]*?)</t[dh]>", std::regex_constants::icase);

    for (std::wsregex_iterator it(tableHtml.begin(), tableHtml.end(), reRow), end; it != end; ++it) {
        std::wstring rowHtml = (*it)[1].str();

        // Gather cells (stripped)
        std::vector<CString> cells;
        for (std::wsregex_iterator ic(rowHtml.begin(), rowHtml.end(), reCell); ic != end; ++ic) {
            CString cell = StripHtml(CString((*ic)[1].str().c_str()));
            cell.Trim();
            if (!cell.IsEmpty()) cells.push_back(cell);
        }
        if (cells.size() < 2) continue;

        // Skip header rows like "PERIOD ACTIVE CONNECTED STANDBY ..."
        CString up0 = cells[0]; up0.MakeUpper();
        if (up0.Find(L"PERIOD") >= 0) continue;

        // We expect a layout like:
        // PERIOD | (Battery) ACTIVE | (Battery) CONNECTED STANDBY | (AC) ACTIVE | (AC) CONNECTED STANDBY
        // Some builds show "-" in some columns.
        UsageHistoryRow row;
        row.period = cells[0];

        // Strategy: try to read exactly from columns if present,
        // else parse all H:MM:SS in order as a fallback (0..3 = battA, battS, acA, acS).
        auto hmsOrDash = [&](int idx) -> double {
            if ((int)cells.size() > idx) {
                CString c = cells[idx];
                if (c == L"-") return 0.0;
                // If it's already H:MM:SS, convert; otherwise, find first time inside it
                auto times = ExtractAllHms(c);
                if (!times.empty()) return HmsTextToHours(times[0]);
            }
            return 0.0;
            };

        if (cells.size() >= 5) {
            row.battActiveHrs = hmsOrDash(1);
            row.battStandbyHrs = hmsOrDash(2);
            row.acActiveHrs = hmsOrDash(3);
            row.acStandbyHrs = hmsOrDash(4);
        }
        else {
            // Fallback: scan the whole row for four H:MM:SS in order
            CString joined;
            for (auto& c : cells) { joined += c; joined += L" "; }
            auto times = ExtractAllHms(joined);
            if (times.size() >= 1) row.battActiveHrs = HmsTextToHours(times[0]);
            if (times.size() >= 2) row.battStandbyHrs = HmsTextToHours(times[1]);
            if (times.size() >= 3) row.acActiveHrs = HmsTextToHours(times[2]);
            if (times.size() >= 4) row.acStandbyHrs = HmsTextToHours(times[3]);
        }

        // Basic sanity check (drop absurd glitches)
        auto ok = [](double v) { return v >= 0.0 && v < 100000.0; };
        if (!ok(row.battActiveHrs) || !ok(row.battStandbyHrs) ||
            !ok(row.acActiveHrs) || !ok(row.acStandbyHrs)) {
            continue;
        }

        outRows.push_back(std::move(row));
    }

    return !outRows.empty();
}



void CBatteryHelthDlg::OnBnClickedBtnUsage()
{

    if (HasBattery() == false) {
        if (m_lang == Lang::EN) {
            AfxMessageBox(L"No battery detected.");
        }
        else {
            AfxMessageBox(L"バッテリーが検出されません。");
        }

        return;
    }

    std::vector<UsageHistoryRow> rows;
    if (!GetUsageHistory(rows) || rows.empty()) {
        
        if(m_lang == Lang::EN) {
            AfxMessageBox(L"Could not read Usage History from the battery report.");
        }
        else {
            AfxMessageBox(L"バッテリーレポートから使用履歴を読み取ることができませんでした。");
		}
        return;
    }

    std::wstring csv;
    
    if(m_lang == Lang::EN) {
        csv += L",Battery Duration,Battery Duration,AC Duration,AC Duration,\r\n";
        csv += L"Period,Battery Active (hrs),Battery Standby (hrs),AC Active (hrs),AC Standby (hrs)\r\n";
    }
    else {
        csv += L",バッテリー持続時間,バッテリー持続時間,AC持続時間,AC持続時間,\r\n";
        csv += L"バッテリー持続時間,バッテリーアクティブ（時間）,バッテリースタンバイ（時間）,ACアクティブ（時間）,ACスタンバイ（時間）\r\n";
    }

    // Rows (skip the summary headers like "BATTERY DURATION" / "AC DURATION")
    for (const auto& r : rows) {
        CString period = r.period; period.Trim();
        if (period.CompareNoCase(L"BATTERY DURATION") == 0 ||
            period.CompareNoCase(L"AC DURATION") == 0)
        {
            continue; // <-- drop that full row
        }

        CString line;
        line.Format(L"%s,%.2f,%.2f,%.2f,%.2f\r\n",
            period.GetString(),
            r.battActiveHrs,
            r.battStandbyHrs,
            r.acActiveHrs,
            r.acStandbyHrs);
        csv += line.GetString();
    }

    CString outPath;
    if (!SaveCsvUtf8AndOpen(csv, outPath)) {
        
        if(m_lang == Lang::EN) {
            AfxMessageBox(L"Failed to write/open CSV file.");
        }
        else {
            AfxMessageBox(L"英語CSVファイルの書き込み / オープンに失敗しました。");
            
		}

        return;
    }

    CString msg;
    if(m_lang == Lang::EN) {
        msg.Format(L"CSV exported:\n%s", outPath.GetString());
    }
    else {
        msg.Format(L"使用履歴（バッテリーとAC）がエクスポートされました:\n%s", outPath.GetString());
    }
    AfxMessageBox(msg);
}


void CBatteryHelthDlg::OnBnClickedBtnManipulatioin()
{

    if (HasBattery() == false) {
        if (m_lang == Lang::EN) {
            AfxMessageBox(L"No battery detected.");
        }
        else {
            AfxMessageBox(L"バッテリーが検出されません。");
        }

        return;
    }

	CManipulationDlg dlg(this);

    if (m_lang == Lang::EN)
    {
        dlg.eng_lang = true;
    }
    else {
        dlg.eng_lang = false;
    }

	dlg.DoModal(); 
 
}




void CBatteryHelthDlg::OnBnClickedBtnBgapp()
{

    if (HasBattery() == false) {
        if (m_lang == Lang::EN) {
            AfxMessageBox(L"No battery detected.");
        }
        else {
            AfxMessageBox(L"バッテリーが検出されません。");
        }

        return;
    }

    // TODO: Add your control notification handler code here
    AfxMessageBox(BuildVisibleAppsReport(), MB_ICONINFORMATION | MB_OK);
}



// ----------------- tray helpers -----------------
void CBatteryHelthDlg::EnsureTrayIcon()
{
    if (m_trayAdded) return;

    ZeroMemory(&m_nid, sizeof(m_nid));
    m_nid.cbSize = sizeof(m_nid);

    m_nid.hWnd = GetSafeHwnd();       // HWND (not CWnd*)
    m_nid.uID = 1;
    m_nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    m_nid.uCallbackMessage = WM_APP + 10;
    m_nid.hIcon = AfxGetApp()->LoadIcon(IDR_ICON);
    lstrcpynW(m_nid.szTip, L"App Uptime Notifier", _countof(m_nid.szTip));

    if (Shell_NotifyIconW(NIM_ADD, &m_nid))
        m_trayAdded = true;
}



void CBatteryHelthDlg::RemoveTrayIcon()
{
    if (!m_trayAdded) return;
    Shell_NotifyIconW(NIM_DELETE, &m_nid);
    m_trayAdded = false;
}

void CBatteryHelthDlg::ShowBalloon(LPCWSTR title, LPCWSTR text, DWORD infoFlags)
{
    if (!m_trayAdded) EnsureTrayIcon();

    NOTIFYICONDATAW nid = m_nid;
    nid.uFlags |= NIF_INFO;
    lstrcpynW(nid.szInfoTitle, title ? title : L"", _countof(nid.szInfoTitle));
    lstrcpynW(nid.szInfo, text ? text : L"", _countof(nid.szInfo));
    nid.dwInfoFlags = infoFlags;

    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

// ----------------- Idle Detection -----------------
ULONGLONG CBatteryHelthDlg::GetGlobalInputIdleTime()
{
    LASTINPUTINFO lii = {};
    lii.cbSize = sizeof(LASTINPUTINFO);

    if (GetLastInputInfo(&lii)) {
        ULONGLONG currentTick = GetTickCount64();
        ULONGLONG lastInputTick = lii.dwTime; // DWORD but compatible within range
        if (currentTick >= lastInputTick) {
            return (currentTick - lastInputTick) / 1000; // seconds
        }
    }
    return 0;
}

bool CBatteryHelthDlg::IsWindowResponding(HWND hWnd)
{
    if (!hWnd || !IsWindow(hWnd)) return false;

    DWORD_PTR result = 0;
    LRESULT res = SendMessageTimeout(hWnd, WM_NULL, 0, 0,
        SMTO_ABORTIFHUNG | SMTO_BLOCK,
        1000, &result);
    return (res != 0);
}

ULONGLONG CBatteryHelthDlg::GetProcessIdleTime(DWORD pid, HWND /*mainWindow*/)
{
    // Foreground process?
    HWND fgWnd = ::GetForegroundWindow();
    DWORD fgPid = 0;
    if (fgWnd) ::GetWindowThreadProcessId(fgWnd, &fgPid);

    // If this process is foreground: use real system idle time
    if (fgPid == pid) {
        return GetGlobalInputIdleTime();
    }

    // If NOT foreground: treat as idle immediately (PowerShell parity)
    // Return a value above the threshold so it qualifies.
    ULONGLONG sysIdle = GetGlobalInputIdleTime();
    ULONGLONG atLeast = IDLE_THRESHOLD_SEC + 1; // ensures >= threshold
    return (sysIdle > atLeast) ? sysIdle : atLeast;
}

//// ----------------- core (report) -----------------
CString CBatteryHelthDlg::BuildVisibleAppsReport()
{
    wchar_t windir[MAX_PATH] = L""; GetWindowsDirectoryW(windir, _countof(windir));
    std::wstring winPrefix = NtToDosPath(windir); if (!winPrefix.empty() && winPrefix.back() != L'\\') winPrefix.push_back(L'\\');

    wchar_t pf[MAX_PATH] = L"", pf86[MAX_PATH] = L"";
    GetEnvironmentVariableW(L"ProgramFiles", pf, _countof(pf));
    GetEnvironmentVariableW(L"ProgramFiles(x86)", pf86, _countof(pf86));

    std::wstring winApps1 = NtToDosPath(pf);
    if (!winApps1.empty() && winApps1.back() != L'\\') winApps1.push_back(L'\\');
    winApps1 += L"WindowsApps\\";

    std::wstring winApps2 = NtToDosPath(pf86);
    if (!winApps2.empty() && winApps2.back() != L'\\') winApps2.push_back(L'\\');
    winApps2 += L"WindowsApps\\";

    DWORD pids[8192] = {}; DWORD cb = 0;
    if (!EnumProcesses(pids, sizeof(pids), &cb)) return L"Failed to enumerate processes.";
    const DWORD count = cb / sizeof(DWORD);

    FILETIME nowFT{}; GetSystemTimeAsFileTime(&nowFT); const ULONGLONG now100 = FTtoU64(nowFT);

    std::vector<Row> rows; rows.reserve(count);

    for (DWORD i = 0; i < count; ++i) {
        DWORD pid = pids[i]; if (!pid) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid); if (!h) continue;

        wchar_t pathBuf[MAX_PATH] = L""; DWORD sz = _countof(pathBuf);
        std::wstring path; if (QueryFullProcessImageNameW(h, 0, pathBuf, &sz)) path.assign(pathBuf);
        if (path.empty()) { CloseHandle(h); continue; }

        std::wstring norm = NtToDosPath(path);
        if (StartsWithI(norm, winPrefix) || StartsWithI(norm, winApps1) || StartsWithI(norm, winApps2)) {
            CloseHandle(h); continue;
        }

        FILETIME ftCreate{}, ftExit{}, ftKernel{}, ftUser{};
        if (!GetProcessTimes(h, &ftCreate, &ftExit, &ftKernel, &ftUser)) { CloseHandle(h); continue; }

        WindowSearchResult wsResult = GetMainWindowAndTitleByPid(pid);
        if (wsResult.title.empty()) { CloseHandle(h); continue; }

        const ULONGLONG start100 = FTtoU64(ftCreate);
        if (now100 <= start100) { CloseHandle(h); continue; }
        const ULONGLONG uptimeSec = (now100 - start100) / 10000000ULL;

        std::wstring name = norm; size_t pos = name.find_last_of(L"\\/");
        if (pos != std::wstring::npos) name = name.substr(pos + 1);

        Row row;
        row.name = name;
        row.pid = pid;
        row.startFT = ftCreate;
        row.uptimeSec = uptimeSec;
        row.title = wsResult.title;
        row.mainWindow = wsResult.hwnd;
        row.idleTimeSec = 0;

        rows.push_back(row);
        CloseHandle(h);
    }


    if (rows.empty()) {
        if (m_lang == Lang::EN) {
            return L"No eligible apps (visible window, non-Windows path).";
        }
        else {
            return L"適格なアプリがありません (表示ウィンドウ、非 Windows パス)。";
        }
    }

    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.uptimeSec > b.uptimeSec; });

    CString out;
    
    if(m_lang == Lang::EN){
        out += L"Name                      StartTime              Uptime\r\n";
        out += L"------------------------- ---------------------- -------------\r\n";
    }
    else {
        out += L"名前                      開始時間              稼働時間\r\n";
        out += L"------------------------- ---------------------- -------------\r\n";
    }

    

        

    for (size_t idx = 0; idx < rows.size(); ++idx) {
        const Row& r = rows[idx];
        CString name = r.name.c_str();
        CString start = FormatFileTimeLocal(r.startFT);
        CString up = FormatTimespan(r.uptimeSec);

        CString line;
        line.Format(L"%-25.25s %-22.22s %-13.13s\r\n",
            name.GetString(), start.GetString(), up.GetString());
        out += line;

        if (out.GetLength() > 60000) { 

            if(m_lang == Lang::EN){
                out += L"... (truncated)\r\n"; 
            }
            else {
                out += L"...（省略）\r\n";
            }
            
            
            break; 
        }
    }

    return out;
}



void CBatteryHelthDlg::CloseBalloon()
{
    if (!m_trayAdded) return;

    NOTIFYICONDATAW nid = m_nid;
    nid.uFlags |= NIF_INFO;
    nid.szInfoTitle[0] = L'\0';
    nid.szInfo[0] = L'\0';
    nid.dwInfoFlags = NIIF_NONE;

    Shell_NotifyIconW(NIM_MODIFY, &nid);
}



// ----------------- periodic notifier WITH IDLE DETECTION -----------------
void CBatteryHelthDlg::CheckAndNotifyTopLongRunning()
{
    wchar_t windir[MAX_PATH] = L""; GetWindowsDirectoryW(windir, _countof(windir));
    std::wstring winPrefix = NtToDosPath(windir); if (!winPrefix.empty() && winPrefix.back() != L'\\') winPrefix.push_back(L'\\');

    wchar_t pf[MAX_PATH] = L"", pf86[MAX_PATH] = L"";
    GetEnvironmentVariableW(L"ProgramFiles", pf, _countof(pf));
    GetEnvironmentVariableW(L"ProgramFiles(x86)", pf86, _countof(pf86));
    std::wstring winApps1 = NtToDosPath(pf); if (!winApps1.empty() && winApps1.back() != L'\\') winApps1.push_back(L'\\'); winApps1 += L"WindowsApps\\";
    std::wstring winApps2 = NtToDosPath(pf86); if (!winApps2.empty() && winApps2.back() != L'\\') winApps2.push_back(L'\\'); winApps2 += L"WindowsApps\\";

    DWORD pids[8192] = {}; DWORD cb = 0;
    if (!EnumProcesses(pids, sizeof(pids), &cb)) return;
    const DWORD count = cb / sizeof(DWORD);

    FILETIME nowFT{}; GetSystemTimeAsFileTime(&nowFT); const ULONGLONG now100 = FTtoU64(nowFT);

    Row* best = nullptr;
    Row  tmp{};

    for (DWORD i = 0; i < count; ++i) {
        DWORD pid = pids[i]; if (!pid) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid); if (!h) continue;

        wchar_t pathBuf[MAX_PATH] = L""; DWORD sz = _countof(pathBuf);
        std::wstring path; if (QueryFullProcessImageNameW(h, 0, pathBuf, &sz)) path.assign(pathBuf);
        if (path.empty()) { CloseHandle(h); continue; }

        std::wstring norm = NtToDosPath(path);
        if (StartsWithI(norm, winPrefix) || StartsWithI(norm, winApps1) || StartsWithI(norm, winApps2)) { CloseHandle(h); continue; }

        FILETIME ftCreate{}, ftExit{}, ftKernel{}, ftUser{};
        if (!GetProcessTimes(h, &ftCreate, &ftExit, &ftKernel, &ftUser)) { CloseHandle(h); continue; }

        WindowSearchResult wsResult = GetMainWindowAndTitleByPid(pid);
        if (wsResult.title.empty()) { CloseHandle(h); continue; }

        const ULONGLONG start100 = FTtoU64(ftCreate);
        if (now100 <= start100) { CloseHandle(h); continue; }
        const ULONGLONG uptimeSec = (now100 - start100) / 10000000ULL;

        std::wstring name = norm; size_t pos = name.find_last_of(L"\\/");
        if (pos != std::wstring::npos) name = name.substr(pos + 1);

        // Compute idle time
        ULONGLONG idleSec = GetProcessIdleTime(pid, wsResult.hwnd);

        // Only consider apps that are IDLE (>= threshold)
        if (idleSec >= IDLE_THRESHOLD_SEC) {
            if (!best || uptimeSec > best->uptimeSec) {
                tmp.name = name;
                tmp.pid = pid;
                tmp.startFT = ftCreate;
                tmp.uptimeSec = uptimeSec;
                tmp.title = wsResult.title;
                tmp.mainWindow = wsResult.hwnd;
                tmp.idleTimeSec = idleSec;
                best = &tmp;
            }
        }

        CloseHandle(h);
    }

    // Only notify if we found an IDLE app
    if (!best) {
   
        return;
    }

    // Require at least 15 minutes of uptime
    if (best->uptimeSec < MIN_UPTIME_SEC) return;

    CString up = FormatTimespan(best->uptimeSec);
    CString idleStr = FormatTimespan(best->idleTimeSec);

    // Close any existing balloon before showing a new one
        CloseBalloon();

    CString title;
    CString body;

    if(m_lang == Lang::EN) {
         title.Format(L"Idle long-running app: %s", best->name.c_str());
         body.Format(L"%s has been running for %s.\nStatus: IDLE",
            best->name.c_str(), up.GetString());
        ShowBalloon(title, body, NIIF_INFO);
        return;
	}
    else {
        title.Format(L"アイドル状態の長時間実行アプリ: %s", best->name.c_str());
        body.Format(L"%s は %s の間実行されています。\nステータス: IDLE",
            best->name.c_str(), up.GetString());
        ShowBalloon(title, body, NIIF_INFO);
        return;
    }

    /* title.Format(L"Idle long-running app: %s", best->name.c_str());
      body.Format(L"%s has been running for %s.\nStatus: IDLE",
        best->name.c_str(), up.GetString());

    ShowBalloon(title, body, NIIF_INFO);*/
}

void CBatteryHelthDlg::OnBnClickedBtnRateinfo()
{

    if (HasBattery() == false) {
        if (m_lang == Lang::EN) {
            AfxMessageBox(L"No battery detected.");
        }
        else {
            AfxMessageBox(L"バッテリーが検出されません。");
        }

        return;
    }

    // TODO: Add your control notification handler code here

	CRateInfoDlg dlg(this); 

    if (m_lang == Lang::EN)
    {
        dlg.eng_lang = true;
    }
    else {
        dlg.eng_lang = false;
    }

	dlg.DoModal();
}



void CBatteryHelthDlg::RedrawToggleButtons()
{
    if (CWnd* p = GetDlgItem(IDC_BTN_JP)) p->Invalidate();
    if (CWnd* p = GetDlgItem(IDC_BTN_EN)) p->Invalidate();
    UpdateWindow();
}



void CBatteryHelthDlg::DrawToggleButton(LPDRAWITEMSTRUCT lp, bool active, const wchar_t* text)
{
    CDC* pDC = CDC::FromHandle(lp->hDC);
    CRect rc(lp->rcItem);

    // Colors
    const COLORREF clrActiveBk = RGB(0, 120, 215);               // Blue
    const COLORREF clrInactiveBk = ::GetSysColor(COLOR_BTNFACE);   // Default face
    const COLORREF clrActiveTx = RGB(255, 255, 255);             // White
    const COLORREF clrInactiveTx = ::GetSysColor(COLOR_BTNTEXT);   // Default text

    // Background fill
    CBrush br(active ? clrActiveBk : clrInactiveBk);
    pDC->FillRect(&rc, &br);

    // 3D edge depending on pressed state
    pDC->DrawEdge(&rc, (lp->itemState & ODS_SELECTED) ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT);

    // Text
    int oldBkMode = pDC->SetBkMode(TRANSPARENT);
    COLORREF oldTx = pDC->SetTextColor(active ? clrActiveTx : clrInactiveTx);

    // Use the control's assigned font (or dialog font)
    CFont* pOldFont = nullptr;
    if (CWnd* pBtn = GetDlgItem(lp->CtlID))
    {
        if (CFont* pFont = pBtn->GetFont())
            pOldFont = pDC->SelectObject(pFont);
    }

    CRect rcText = rc;
    rcText.DeflateRect(4, 2);
    UINT format = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
    pDC->DrawText(text, (int)wcslen(text), &rcText, format);

    // Focus rect (keyboard navigation)
    if ((lp->itemState & ODS_FOCUS) != 0)
    {
        CRect rcFocus = rc; rcFocus.DeflateRect(3, 3);
        pDC->DrawFocusRect(&rcFocus);
    }

    // Restore GDI objects/state
    if (pOldFont) pDC->SelectObject(pOldFont);
    pDC->SetTextColor(oldTx);
    pDC->SetBkMode(oldBkMode);
}
void CBatteryHelthDlg::OnBnClickedBtnEn()
{
    // TODO: Add your control notification handler code here
    if (m_lang != Lang::EN)
    {
        m_lang = Lang::EN;
        /*UpdateLanguageTexts();*/
        RedrawToggleButtons();
        // TODO: apply English strings / resources here if needed


        ///////Empty-----------------

        SetDlgItemTextW(IDC_STATIC_HEADER, L"");
        SetDlgItemTextW(IDC_STATIC_BBI, _T(""));

        SetDlgItemTextW(IDC_STATIC_STATUS, _T(""));
        SetDlgItemTextW(IDC_STATIC_PERCENTAGE, _T(""));
        SetDlgItemTextW(IDC_STATIC_NAME, _T(""));
        SetDlgItemTextW(IDC_STATIC_MANUFAC, _T(""));
        SetDlgItemTextW(IDC_STATIC_VOLTAGE, _T(""));
        SetDlgItemTextW(IDC_STATIC_TEMP, _T(""));
        SetDlgItemTextW(IDC_STATIC_TIME, _T(""));
        SetDlgItemTextW(IDC_STATIC_DCAPACITY, _T(""));
        SetDlgItemTextW(IDC_STATIC_CAPACITY, _T(""));
        SetDlgItemTextW(IDC_STATIC_CURRCAPACITY, _T(""));
        SetDlgItemTextW(IDC_STATIC_CYCLE, _T(""));
        SetDlgItemTextW(IDC_STATIC_HEALTH, _T(""));

		Invalidate();

		//--------------------------------------
        SetDlgItemTextW(IDC_STATIC_HEADER, L"Battery Health & Performance Monitor");
        SetDlgItemTextW(IDC_STATIC_BBI, _T("Basic Battery Info"));

        SetDlgItemTextW(IDC_STATIC_STATUS, _T("Status"));
        SetDlgItemTextW(IDC_STATIC_PERCENTAGE, _T("Percentage"));
        SetDlgItemTextW(IDC_STATIC_NAME, _T("Name"));
        SetDlgItemTextW(IDC_STATIC_MANUFAC, _T("Battery Id"));
        SetDlgItemTextW(IDC_STATIC_VOLTAGE, _T("Voltage"));
        SetDlgItemTextW(IDC_STATIC_TEMP, _T("Temperature"));
        SetDlgItemTextW(IDC_STATIC_TIME, _T("Time Remaining"));
        SetDlgItemTextW(IDC_STATIC_DCAPACITY, _T("Design Capacity"));
        SetDlgItemTextW(IDC_STATIC_CAPACITY, _T("Full Charge Capacity"));
        SetDlgItemTextW(IDC_STATIC_CURRCAPACITY, _T("Current Capacity"));
        SetDlgItemTextW(IDC_STATIC_CYCLE, _T("Cycle Count"));
        SetDlgItemTextW(IDC_STATIC_HEALTH, _T("Health"));


		////////////Advance Info Section/////////////////////

        SetDlgItemTextW(IDC_STATIC_ABT, _T("Advance Battery Info"));

        ///////Data and History Section/////////////////////
        SetDlgItemTextW(IDC_STATIC_DH, _T("Data and History"));


        CloseBalloon();
        CheckAndNotifyTopLongRunning();

		InitToolTips();
        Invalidate();

        GetBatteryInfo();
        GetStaticBatteryInfo();

		
        UpdateWindow();
    }
}

void CBatteryHelthDlg::OnBnClickedBtnJp()
{
    // TODO: Add your control notification handler code here
    if (m_lang != Lang::JP)
    {
        m_lang = Lang::JP;
       /* UpdateLanguageTexts();*/
        RedrawToggleButtons();
        // TODO: apply Japanese strings / resources here if needed

        ///////Empty-----------------

        SetDlgItemTextW(IDC_STATIC_HEADER, L"");
        SetDlgItemTextW(IDC_STATIC_BBI, _T(""));

        SetDlgItemTextW(IDC_STATIC_STATUS, _T(""));
        SetDlgItemTextW(IDC_STATIC_PERCENTAGE, _T(""));
        SetDlgItemTextW(IDC_STATIC_NAME, _T(""));
        SetDlgItemTextW(IDC_STATIC_MANUFAC, _T(""));
        SetDlgItemTextW(IDC_STATIC_VOLTAGE, _T(""));
        SetDlgItemTextW(IDC_STATIC_TEMP, _T(""));
        SetDlgItemTextW(IDC_STATIC_TIME, _T(""));
        SetDlgItemTextW(IDC_STATIC_DCAPACITY, _T(""));
        SetDlgItemTextW(IDC_STATIC_CAPACITY, _T(""));
        SetDlgItemTextW(IDC_STATIC_CURRCAPACITY, _T(""));
        SetDlgItemTextW(IDC_STATIC_CYCLE, _T(""));
        SetDlgItemTextW(IDC_STATIC_HEALTH, _T(""));

        Invalidate();


        //--------------------------------------
    
        SetDlgItemTextW(IDC_STATIC_HEADER, L"バッテリー状態・パフォーマンスモニター");
        SetDlgItemTextW(IDC_STATIC_BBI, _T("基本バッテリー情報"));

        SetDlgItemTextW(IDC_STATIC_STATUS, _T("ステータス"));
        SetDlgItemTextW(IDC_STATIC_PERCENTAGE, _T("残量"));
        SetDlgItemTextW(IDC_STATIC_NAME, _T("名前"));
        SetDlgItemTextW(IDC_STATIC_MANUFAC, _T("バッテリー ID"));
        SetDlgItemTextW(IDC_STATIC_VOLTAGE, _T("電圧"));
        SetDlgItemTextW(IDC_STATIC_TEMP, _T("温度"));
        SetDlgItemTextW(IDC_STATIC_TIME, _T("残り時間"));
        SetDlgItemTextW(IDC_STATIC_DCAPACITY, _T("設計容量"));
        SetDlgItemTextW(IDC_STATIC_CAPACITY, _T("満充電容量"));
        SetDlgItemTextW(IDC_STATIC_CURRCAPACITY, _T("現在の容量"));
        SetDlgItemTextW(IDC_STATIC_CYCLE, _T("サイクル回数"));
        SetDlgItemTextW(IDC_STATIC_HEALTH, _T("ヘルス"));

        ////////////Advance Info Section/////////////////////

        SetDlgItemTextW(IDC_STATIC_ABT, _T("事前のバッテリー情報"));


		///////Data and History Section/////////////////////
        SetDlgItemTextW(IDC_STATIC_DH, _T("データと歴史"));


        GetBatteryInfo();
        GetStaticBatteryInfo();

        CloseBalloon();
        CheckAndNotifyTopLongRunning();

        InitToolTips();
        Invalidate();
    
   /*     UpdateWindow();*/
    }
}





void CBatteryHelthDlg::OnBnClickedBtnSleep()
{

    if (HasBattery() == false) {
        if (m_lang == Lang::EN) {
            AfxMessageBox(L"No battery detected.");
        }
        else {
            AfxMessageBox(L"バッテリーが検出されません。");
        }

        return;
    }

    CSleepDataDlg dlg;

    if(m_lang == Lang::EN){
        dlg.eng_lang = true;
    }
    else {
        dlg.eng_lang = false;
	}

    dlg.DoModal(); // shows persistent log, auto-refreshing
}


//// ====== Single power-broadcast sink(sleep-awake feture) ======
//UINT CBatteryHelthDlg::OnPowerBroadcast(UINT nPowerEvent, LPARAM nEventData)
//{
//    // 1) Handle display on/off/dim (screen-only case -> create a single row like sleep)
//    if (nPowerEvent == PBT_POWERSETTINGCHANGE) {
//        auto* ps = reinterpret_cast<POWERBROADCAST_SETTING*>(nEventData);
//        if (ps && ps->PowerSetting == GUID_CONSOLE_DISPLAY_STATE && ps->DataLength >= sizeof(DWORD)) {
//            DWORD state = *reinterpret_cast<DWORD*>(ps->Data); // 0=Off,1=On,2=Dim
//            CSleepDataDlg::HandleDisplayState(state);
//            return TRUE;
//        }
//    }
//
//    // 2) Normal sleep/hibernate tracking (suspend/resume) with de-dup
//    CSleepDataDlg::HandlePowerBroadcast(nPowerEvent);
//    return TRUE; // handled
//}


// ====== Single power-broadcast sink(sleep-awake feature) ======
UINT CBatteryHelthDlg::OnPowerBroadcast(UINT nPowerEvent, LPARAM nEventData)
{
    // 1) Handle display on/off/dim (screen-only case -> create a single row like sleep)
    if (nPowerEvent == PBT_POWERSETTINGCHANGE) {
        auto* ps = reinterpret_cast<POWERBROADCAST_SETTING*>(nEventData);
        if (ps && ps->PowerSetting == GUID_CONSOLE_DISPLAY_STATE && ps->DataLength >= sizeof(DWORD)) {
            DWORD state = *reinterpret_cast<DWORD*>(ps->Data); // 0=Off,1=On,2=Dim
            CSleepDataDlg::HandleDisplayState(state);
            return TRUE;
        }
    }

    // 2) Normal sleep/hibernate tracking (suspend/resume) with de-dup
    CSleepDataDlg::HandlePowerBroadcast(nPowerEvent);

    return TRUE; // handled
}