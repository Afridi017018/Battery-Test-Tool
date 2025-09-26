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

#include <immintrin.h> // For AVX-512 intrinsics
#include <chrono>
#include <afxwin.h> // MFC headers
#include <intrin.h> 

#include <pdh.h>
#include <pdhmsg.h>

#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")


#pragma comment(lib, "pdh.lib")

#pragma comment(lib, "wbemuuid.lib")

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
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
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

END_MESSAGE_MAP()

// CBatteryHelthDlg message handlers

BOOL CBatteryHelthDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // Initialize COM once
    HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    if (FAILED(hres))
    {
        AfxMessageBox(L"Failed to initialize COM.");
        return FALSE;
    }

    hres = CoInitializeSecurity(
        NULL,
        -1,
        NULL,
        NULL,
        RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE,
        NULL
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
    m_CPU_Progress.SetPos(0);   // int, no double!
    m_CPU_Progress.SetStep(1);

    m_CPU_Progress.ModifyStyle(0, PBS_SMOOTH);
    ::SetWindowTheme(m_CPU_Progress.GetSafeHwnd(), L"", L"");
    m_CPU_Progress.SetBarColor(RGB(0, 122, 204));
    m_CPU_Progress.SetBkColor(RGB(220, 220, 220));
    m_CPU_Progress.ShowWindow(SW_HIDE); // Hide initially


    ////Discharge progress
    m_discharge_progress.SetRange(0, 100);
    m_discharge_progress.SetPos(0);   // int, no double!
    m_discharge_progress.SetStep(1);

    m_discharge_progress.ModifyStyle(0, PBS_SMOOTH);
    ::SetWindowTheme(m_discharge_progress.GetSafeHwnd(), L"", L"");
    m_discharge_progress.SetBarColor(RGB(0, 122, 204));
    m_discharge_progress.SetBkColor(RGB(220, 220, 220));
    m_discharge_progress.ShowWindow(SW_HIDE); // Hide initially

    m_stopCpuLoad.store(false);

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

    SetIcon(m_hIcon, TRUE);   // big icon
    SetIcon(m_hIcon, FALSE);  // small icon



    // ----------------------------
    // Fonts - Create base fonts
    // ----------------------------
    CreateFonts();


    // Apply fonts initially
    ApplyFonts(1.0); // normal scale

    m_brushWhite.CreateSolidBrush(RGB(255, 255, 255));

    // Fix window style
    LONG style = GetWindowLong(this->m_hWnd, GWL_STYLE);
    style &= ~WS_THICKFRAME;   // no resize
    //style &= ~WS_MAXIMIZEBOX;  // no maximize
    style |= WS_MINIMIZEBOX;   // keep minimize
    style |= WS_SYSMENU;       // system menu required for minimize

    SetWindowLong(this->m_hWnd, GWL_STYLE, style);
    SetWindowPos(NULL, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);



    //// ----------------------------
    //// Dialog size & style
    //// ----------------------------
    //int width = 670;
    //int height = 780;
    //SetWindowPos(NULL, 0, 0, width, height,
    //    SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    //CenterWindow();



    // Save original positions and dialog size
    CRect dialogRect;
    GetClientRect(&dialogRect);
    m_origDialogSize = CSize(dialogRect.Width(), dialogRect.Height());

    UINT ids[] = { IDC_BTN_CPULOAD, IDC_BTN_DISCHARGE, IDC_BTN_HISTORY, IDC_BTN_UPLOADPDF,
                   IDC_STATIC_STATUS, IDC_BATT_STATUS, IDC_STATIC_TIME, IDC_BATT_TIME,
                   IDC_BATT_DISCHARGR, IDC_STATIC_DH, IDC_BATT_CPULOAD, IDC_STATIC_ABT,
        IDD_BATTERYHELTH_DIALOG, IDC_PROGRESS4, IDC_STATIC_BBI, IDC_STATIC_HEADER, IDC_BATT_DID,
        IDC_STATIC_PERCENTAGE,IDC_BATT_PROGRESS,IDC_BATT_PERCENTAGE,IDC_STATIC_CAPACITY,
        IDC_BATT_CAPACITY, IDC_STATIC_NAME, IDC_BATT_NAME, IDC_STATIC_DCAPACITY, IDC_BATT_DCAPACITY,
        IDC_STATIC_MANUFAC, IDC_BATT_MANUFAC, IDC_STATIC_CYCLE, IDC_BATT_CYCLE, IDC_STATIC_HEALTH,
        IDC_BATT_HEALTH, IDC_STATIC_VOLTAGE, IDC_BATT_VOLTAGE, IDC_STATIC_TEMP, IDC_BATT_TEMP,
        IDC_PROGRESS5
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

    /* m_bgBrush.CreateSolidBrush(RGB(255, 0, 0));*/


     // Change title bar color (Windows 10 build 22000+)
    COLORREF titleBarColor = RGB(70, 80, 185); // Your desired color
    DwmSetWindowAttribute(m_hWnd, DWMWA_CAPTION_COLOR, &titleBarColor, sizeof(titleBarColor));






    // Mark every listed button as owner-draw
    for (UINT id : m_buttonIds) {
        if (CWnd* p = GetDlgItem(id)) {
            p->ModifyStyle(0, BS_OWNERDRAW);
            m_hover[id] = FALSE; // init hover state
        }
    }

    // Smooth hover timer
    SetTimer(1, 100, NULL); // tune as you like


    InitToolTips();

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

    struct Tip { UINT id; const wchar_t* text; };
    const Tip tips[] = {
        { IDC_BTN_CPULOAD,     L"Start CPU Load Test" },
        { IDC_BTN_DISCHARGE,   L"Start Discharge Test" },
        { IDC_BTN_HISTORY,     L"Charge History" },
        { IDC_BTN_UPLOADPDF,   L"Export Report To CSV" },
        { IDC_BATT_PERCENTAGE, L"Current battery percentage reported by Windows." },
        { IDC_BATT_CAPACITY,   L"Full charge vs. design capacity (health estimate)." },
    };

    for (size_t i = 0; i < _countof(tips); ++i)
    {
        if (CWnd* p = GetDlgItem(tips[i].id))
            m_toolTip.AddTool(p, tips[i].text);
    }

    // Conditional tooltip: only add if test is NOT running
    if (!m_dischargeTestRunning)
    {
        if (CWnd* p = GetDlgItem(IDC_BTN_DISCHARGE))
            m_toolTip.AddTool(p, L"Start Discharge Test");
    }
    else {
        if (CWnd* p = GetDlgItem(IDC_BTN_DISCHARGE))
            m_toolTip.AddTool(p, L"Stop Discharge Test");
    }

 
}



BOOL CBatteryHelthDlg::PreTranslateMessage(MSG* pMsg)
{
    if (m_toolTip.GetSafeHwnd())
        m_toolTip.RelayEvent(pMsg);
    return CDialogEx::PreTranslateMessage(pMsg);
}


static void UpdateLabel(CWnd* pDlg, int ctrlId, const CString& text)
{
    CWnd* pCtl = pDlg->GetDlgItem(ctrlId);
    if (!pCtl) return;

    // Hide the control temporarily
    pCtl->ShowWindow(SW_HIDE);

    // Update text
    pCtl->SetWindowTextW(text);






    if (ctrlId == IDC_BATT_CPULOAD) {
        CFont* pOldFont = pCtl->GetFont();
        LOGFONT lf = {};
        if (pOldFont)
        {
            pOldFont->GetLogFont(&lf);
        }
        else
        {
            ::GetObject(GetStockObject(DEFAULT_GUI_FONT), sizeof(LOGFONT), &lf);
        }

        // Slightly smaller than -16
        lf.lfHeight = -16;

        // Semi-bold weight
        lf.lfWeight = 700;   // FW_SEMIBOLD (~600)

        // Create persistent font
        static CFont semiBoldFont;
        semiBoldFont.DeleteObject();
        semiBoldFont.CreateFontIndirect(&lf);

        // Apply
        pCtl->SetFont(&semiBoldFont);
    }
    






    // Get control rectangle and invalidate a larger area
    CRect rc;
    pCtl->GetWindowRect(&rc);
    pDlg->ScreenToClient(&rc);
    rc.InflateRect(5, 5); // Add some padding

    // Force complete redraw of the area
    pDlg->InvalidateRect(&rc, TRUE);
    pDlg->UpdateWindow();

    // Show the control again
    pCtl->ShowWindow(SW_SHOW);
    pCtl->InvalidateRect(NULL, TRUE);
    pCtl->UpdateWindow();
}





BOOL CBatteryHelthDlg::OnEraseBkgnd(CDC* pDC)
{
    CRect rc;
    GetClientRect(&rc);

    // Start color: Light Sky Blue (#87CEFA)
    TRIVERTEX vert[2] = {};
    vert[0].x = rc.left;
    vert[0].y = rc.top;
    vert[0].Red = 135 << 8;  // R = 135
    vert[0].Green = 206 << 8;  // G = 206
    vert[0].Blue = 250 << 8;  // B = 250
    vert[0].Alpha = 0xFFFF;

    // End color: White (#FFFFFF)
    vert[1].x = rc.right;
    vert[1].y = rc.bottom;
    vert[1].Red = 255 << 8;
    vert[1].Green = 255 << 8;
    vert[1].Blue = 255 << 8;
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

void CBatteryHelthDlg::OnSize(UINT nType, int cx, int cy)
{
    CDialogEx::OnSize(nType, cx, cy);

    if (!m_origPositionsSaved || cx == 0 || cy == 0)
        return;

    // Calculate scaling factors
    double scaleX = (double)cx / m_origDialogSize.cx;
    double scaleY = (double)cy / m_origDialogSize.cy;

    // Use average scaling for font size
    double fontScale = (scaleX + scaleY) / 2.0;

    // Limit font scaling to reasonable range
    fontScale = max(0.5, min(fontScale, 1.3));

    if (nType == SIZE_MAXIMIZED)
    {
        // Apply scaled fonts for maximized state
        ApplyFonts(fontScale);

        // Make IDC_BATT_CPULOAD bold + scaled
        if (m_cpuLoadClick) {
            SetButtonFont(IDC_BATT_CPULOAD, true); // scaled bold
        }

        if (m_dischargeClick) {
            SetButtonFont(IDC_BATT_DISCHARGR, true);
        }



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
        // Restore original fonts for normal state
        ApplyFonts(1.0);

        // Restore original positions
        for (auto& ctl : m_origPositions)
        {
            CWnd* pWnd = GetDlgItem(ctl.id);
            if (pWnd && pWnd->GetSafeHwnd())
            {
                pWnd->MoveWindow(&ctl.rect);
            }
        }

        // Make IDC_BATT_CPULOAD bold but unscaled
        if (m_cpuLoadClick) {
            SetButtonFont(IDC_BATT_CPULOAD, false); // normal bold
        }

        if (m_dischargeClick) {
            SetButtonFont(IDC_BATT_DISCHARGR, false); // normal bold
        }
    }


    // Force redraw to show font changes
    Invalidate();
    UpdateWindow();
}






HBRUSH CBatteryHelthDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    // Let STATIC texts (labels), group boxes, checkboxes, radios, etc. be transparent
    if (nCtlColor == CTLCOLOR_STATIC || nCtlColor == CTLCOLOR_BTN)
    {
        pDC->SetBkMode(TRANSPARENT);
        // Optional: tweak text color if needed
        // pDC->SetTextColor(RGB(60, 60, 60));
        return (HBRUSH)GetStockObject(NULL_BRUSH);
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

// If you add a minimize button to your dialog, you will need the code below
// to draw the icon.  For MFC applications using the document/view model,
// this is automatically done for you by the framework.

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





void CBatteryHelthDlg::GetBatteryInfo()
{
    // Initialize COM
    HRESULT hres;

    // Connect to WMI
    IWbemLocator* pLoc = NULL;
    hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
        IID_IWbemLocator, (LPVOID*)&pLoc);
    if (FAILED(hres)) { AfxMessageBox(L"Failed to create IWbemLocator."); CoUninitialize(); return; }

    IWbemServices* pSvc = NULL;

    hres = pLoc->ConnectServer(
        _bstr_t(L"ROOT\\CIMV2"),
        NULL, NULL, 0, NULL, 0, 0, &pSvc);
    if (FAILED(hres)) { AfxMessageBox(L"Could not connect to WMI."); pLoc->Release(); CoUninitialize(); return; }

    // Set security levels
    hres = CoSetProxyBlanket(
        pSvc,
        RPC_C_AUTHN_WINNT,
        RPC_C_AUTHZ_NONE,
        NULL,
        RPC_C_AUTHN_LEVEL_CALL,
        RPC_C_IMP_LEVEL_IMPERSONATE,
        NULL,
        EOAC_NONE);
    if (FAILED(hres)) { AfxMessageBox(L"Could not set proxy blanket."); pSvc->Release(); pLoc->Release(); CoUninitialize(); return; }

    // Query Battery Information
    IEnumWbemClassObject* pEnumerator = NULL;
    hres = pSvc->ExecQuery(
        bstr_t("WQL"),
        bstr_t("SELECT * FROM Win32_Battery"),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
        NULL,
        &pEnumerator);

    if (FAILED(hres)) { AfxMessageBox(L"WMI query failed."); pSvc->Release(); pLoc->Release(); CoUninitialize(); return; }

    IWbemClassObject* pClsObj = NULL;
    ULONG uReturn = 0;

    if (pEnumerator)
    {
        HRESULT hr = pEnumerator->Next(WBEM_INFINITE, 1, &pClsObj, &uReturn);
        if (uReturn == 0) { AfxMessageBox(L"No battery found."); }
        else
        {
            VARIANT vtProp;

            // Charging Status
            hr = pClsObj->Get(L"BatteryStatus", 0, &vtProp, 0, 0);
            CString status;
            switch (vtProp.intVal)
            {
            case 1: status = L"Discharging"; break;
            case 2: status = L"Charging"; break;
            case 3: status = L"Fully Charged"; break;
            default: status = L"Unknown"; break;
            }
            /*SetDlgItemText(IDC_BATT_STATUS, status);*/
            UpdateLabel(this, IDC_BATT_STATUS, status);

            VariantClear(&vtProp);

            // Battery percentage using win32api
            SYSTEM_POWER_STATUS sps;
            if (GetSystemPowerStatus(&sps))
            {
                CString percentage;
                if (sps.BatteryLifePercent != 255) // 255 = unknown
                {
                    percentage.Format(L"%d%%", sps.BatteryLifePercent);
                    m_BatteryProgress.SetPos(sps.BatteryLifePercent);
                }
                else
                {
                    percentage = L"Not available";
                    m_BatteryProgress.SetPos(0);
                }
             /*   SetDlgItemText(IDC_BATT_PERCENTAGE, percentage);*/
                
                UpdateLabel(this, IDC_BATT_PERCENTAGE, percentage);



            }
            else
            {
                UpdateLabel(this, IDC_BATT_PERCENTAGE, L"Not available");
            }

            // Estimated Time Remaining using Win32 API
            DWORD batteryLifeSeconds = sps.BatteryLifeTime; // in seconds
            CString remainingTime;
            if (batteryLifeSeconds != (DWORD)-1) // -1 = unknown
            {
                int hours = batteryLifeSeconds / 3600;
                int minutes = (batteryLifeSeconds % 3600) / 60;
                remainingTime.Format(L"%d hr %d min", hours, minutes);
            }
            else
            {
                remainingTime = L"Unknown";
            }
            /*SetDlgItemText(IDC_BATT_TIME, remainingTime);*/
            UpdateLabel(this, IDC_BATT_TIME, remainingTime);

            // Full Charge Capacity
            CString fullCap = QueryWmiValue(L"ROOT\\WMI", L"BatteryFullChargedCapacity", L"FullChargedCapacity");
            if (fullCap != L"Not available")
                fullCap += L" mWh";
            UpdateLabel(this, IDC_BATT_CAPACITY, fullCap);

            // Design Capacity
            CString designCap = QueryWmiValue(L"ROOT\\WMI", L"BatteryStaticData", L"DesignedCapacity");
            if (designCap != L"Not available")
                designCap += L" mWh";
            UpdateLabel(this, IDC_BATT_DCAPACITY, designCap);

            // Battery Health Calculation
            CString fullCapStr = fullCap;
            CString designCapStr = designCap;

            if (fullCapStr != L"Not available" && designCapStr != L"Not available")
            {
                int fullCap = _wtoi(fullCapStr);
                int designCap = _wtoi(designCapStr);
                if (designCap > 0)
                {
                    double health = (static_cast<double>(fullCap) / designCap) * 100.0;
                    CString healthStr;
                    healthStr.Format(L"%.2f %%", health);
                    UpdateLabel(this, IDC_BATT_HEALTH, healthStr);
                }
                else
                {
                    UpdateLabel(this, IDC_BATT_HEALTH, L"Not available");
                }
            }
            else
            {
                UpdateLabel(this, IDC_BATT_HEALTH, L"Not available");
            }

            // Voltage
            CString voltage = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"Voltage");
            if (voltage != L"Not available")
            {
                double volts = _wtoi(voltage) / 1000.0; // convert mV to V
                CString voltStr;
                voltStr.Format(L"%.2f V", volts);
                UpdateLabel(this, IDC_BATT_VOLTAGE, voltStr);
            }
            else
            {
                UpdateLabel(this, IDC_BATT_VOLTAGE, L"Not available");
            }

            // Get Battery DeviceID
            VARIANT vtDeviceID;
            hr = pClsObj->Get(L"DeviceID", 0, &vtDeviceID, 0, 0);
            CString deviceID;
            if (vtDeviceID.vt != VT_NULL && vtDeviceID.vt != VT_EMPTY)
            {
                deviceID = vtDeviceID.bstrVal;
            }
            else
            {
                deviceID = L"Not available";
            }
            UpdateLabel(this, IDC_BATT_MANUFAC, deviceID);
            VariantClear(&vtDeviceID);

            // Battery Name
            hr = pClsObj->Get(L"Name", 0, &vtProp, 0, 0);
            CString batteryName;
            if (vtProp.vt != VT_NULL && vtProp.vt != VT_EMPTY)
            {
                batteryName = vtProp.bstrVal;
            }
            else
            {
                batteryName = L"Not available";
            }
            UpdateLabel(this, IDC_BATT_NAME, batteryName);
            VariantClear(&vtProp);

            // Battery Cycle Count
            CString cycleCount = QueryBatteryCycleCount();
            if (cycleCount != L"Not available")
            {
                cycleCount += L" cycles";
            }
            UpdateLabel(this, IDC_BATT_CYCLE, cycleCount);

            // Battery Temperature - NEW ADDITION
            CString temperature = QueryBatteryTemperature();
            UpdateLabel(this, IDC_BATT_TEMP, temperature);

            // System UUID
            {
                IEnumWbemClassObject* pEnumUUID = NULL;
                HRESULT hresUUID = pSvc->ExecQuery(
                    bstr_t("WQL"),
                    bstr_t("SELECT UUID FROM Win32_ComputerSystemProduct"),
                    WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                    NULL,
                    &pEnumUUID);

                if (SUCCEEDED(hresUUID) && pEnumUUID)
                {
                    IWbemClassObject* pObjUUID = NULL;
                    ULONG uReturnUUID = 0;
                    if (pEnumUUID->Next(WBEM_INFINITE, 1, &pObjUUID, &uReturnUUID) == S_OK)
                    {
                        VARIANT vtUUID;
                        if (SUCCEEDED(pObjUUID->Get(L"UUID", 0, &vtUUID, 0, 0)))
                        {
                            CString uuid = (vtUUID.vt != VT_NULL && vtUUID.vt != VT_EMPTY) ? vtUUID.bstrVal : L"Not available";
                            UpdateLabel(this, IDC_BATT_DID, uuid);
                            VariantClear(&vtUUID);
                        }
                        pObjUUID->Release();
                    }
                    pEnumUUID->Release();
                }
                else
                {
                    UpdateLabel(this, IDC_BATT_DID, L"Not available");
                }
            }

            pClsObj->Release();
        }

        pEnumerator->Release();
    }

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
        GetDlgItem(IDC_BTN_DISCHARGE)->EnableWindow(FALSE);
        /*   GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(FALSE);*/

           // If discharge test is running, stop it immediately
        if (m_dischargeTestRunning)
        {
            KillTimer(m_dischargeTimerID);
            m_dischargeTestRunning = false;

            /*  SetDlgItemText(IDC_BATT_DISCHARGR, L"Discharge Test Stopped");*/
            UpdateLabel(this, IDC_BATT_DISCHARGR, L"Discharge Test Stopped");
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

    // If already running 
    if (m_dischargeTestRunning)
    {
        m_dischargeTestRunning = false;
        KillTimer(m_dischargeTimerID);

        SetDlgItemText(IDC_BTN_DISCHARGE, L"Start Discharge Test");
        /*  SetDlgItemText(IDC_BATT_DISCHARGR, L"Discharge Test Stopped!");*/

        UpdateLabel(this, IDC_BATT_DISCHARGR, L"Discharge Test Stopped!");

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

    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps) || sps.BatteryLifePercent == 255)
    {
        AfxMessageBox(L"Cannot read battery percentage.");
        return;
    }

    // Check Battery Saver
    if (sps.SystemStatusFlag & 1) // Battery saver ON
    {
        CString msg;
        msg = L"Turn off your Battery Saver.\n\n";
        msg += L"=> WINDOWS:\n";
        msg += L"  Settings -> System -> Power & Battery -> Battery saver -> Turn Off\n\n";

        ::MessageBox(this->m_hWnd, msg, L"Battery Saver Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    // Start discharge test
    m_initialBatteryPercent = sps.BatteryLifePercent;

    m_elapsedMinutes = 0;
    m_elapsedSeconds = 0;

    m_dischargeTestRunning = true;
    InitToolTips();


    GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(FALSE);
    SetDlgItemText(IDC_BTN_DISCHARGE, L"Stop Discharge Test");

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

            msg.Format(L"Please wait 5 minutes to complete. (%.0f%%)", progressPercent);


            /*SetDlgItemText(IDC_BATT_DISCHARGR, msg);*/
            UpdateLabel(this, IDC_BATT_DISCHARGR, msg);

            m_discharge_progress.SetPos(progressPercent);






            // Stop the test after duration
            if (m_elapsedSeconds >= totalSeconds)
            {
                KillTimer(m_dischargeTimerID);
                m_dischargeTestRunning = false;

                m_dischargeClick = true;
                m_discharge_progress.ShowWindow(SW_HIDE);

                // Store final result
                m_dischargeResult.Format(L"Initial: %d%%, Final: %d%%, Drop: %d%%, Drain Rate: %.2f %%/min",
                    m_initialBatteryPercent, sps.BatteryLifePercent, drop, drainRate);

                msg.Format(L"Time elapsed: %d sec\nInitial: %d%%\nCurrent: %d%%\nDrop: %d%%\nDrain Rate: %.2f %%/min\nProgress: %.0f%%",
                    m_elapsedSeconds, m_initialBatteryPercent, sps.BatteryLifePercent, drop, drainRate, progressPercent);

                /*SetDlgItemText(IDC_BATT_DISCHARGR, msg);*/
                UpdateLabel(this, IDC_BATT_DISCHARGR, msg);

                GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(TRUE);
                SetDlgItemText(IDC_BTN_DISCHARGE, L"Start Discharge Test");

                AfxMessageBox(L"Discharge Test Completed!");
            }
        }
        else
        {
            /*SetDlgItemText(IDC_BATT_DISCHARGR, L"Cannot read battery percentage.");*/
            UpdateLabel(this, IDC_BATT_DISCHARGR, L"Cannot read battery percentage.");
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
        msg.Format(L"Please wait 1 minute to complete. (%.0f%%)", percentDone);




        /*SetDlgItemText(IDC_BATT_CPULOAD, msg);*/
        UpdateLabel(this, IDC_BATT_CPULOAD, msg);

        m_CPU_Progress.SetPos(percentDone);


        // Stop if requested
        if (m_stopCpuLoad.load())
        {
            m_cpuLoadTestRunning = false;
            KillTimer(m_cpuLoadTimerID);
            SetDlgItemText(IDC_BTN_CPULOAD, L"CPU Load Test");

            /*SetDlgItemText(IDC_BATT_CPULOAD, L"CPU Load Test Stopped!");*/
            UpdateLabel(this, IDC_BATT_CPULOAD, L"CPU Load Test Stopped!");
            m_CPU_Progress.ShowWindow(SW_HIDE);
        }


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



    // Stop if running
    if (m_cpuLoadTestRunning)
    {
        m_stopCpuLoad.store(true);
        m_cpuLoadTestRunning = false;

        SetDlgItemText(IDC_BTN_CPULOAD, L"Start CPU Load Test");

        UpdateLabel(this, IDC_BATT_CPULOAD, L"CPU Load Not Tested...");
        return;
    }


    // Check current CPU usage before starting
    double usage = GetCurrentCPUUsage();
    if (usage >= 90)
    {
        CString msg;
        msg.Format(L"CPU usage is currently too high (%.0f%%). Please close background applications first.", usage);
        ::MessageBox(this->m_hWnd, msg, L"CPU Usage Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    // Get initial battery percentage
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps) || sps.BatteryLifePercent == 255)
    {
        AfxMessageBox(L"Cannot read battery percentage.");
        return;
    }

    m_initialBatteryCPUPercent = sps.BatteryLifePercent;
    m_cpuLoadTestRunning = true;
    m_stopCpuLoad.store(false);

    /* SetDlgItemText(IDC_BATT_CPULOAD, L"CPU Load Test Running...");*/
    UpdateLabel(this, IDC_BATT_CPULOAD, L"CPU Load Test Running...");

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

                msg.Format(L"Initial: %d%%\nCurrent: %d%%\nDrop: %d%%\nRate: %.2f%%/min\nTOPS: %.4f\nGFLOPS: %.3f",
                    m_initialBatteryCPUPercent,
                    spsEnd.BatteryLifePercent,
                    drop,
                    rate,
                    tops,
                    gflops);


            }
            else
            {
                msg = L"Cannot read battery percentage.";
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

    CString* pMsg = (CString*)lParam;
    //SetDlgItemText(IDC_BATT_CPULOAD, *pMsg);

    UpdateLabel(this, IDC_BATT_CPULOAD, *pMsg);

    if (m_cpuLoadClick) {
        SetButtonFont(IDC_BATT_CPULOAD, true);
    }


    delete pMsg; // free memory



    // Re-enable the CPU Load button after finishing
    GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(TRUE);

    // Re-enable the Discharge Test button after finishing
    GetDlgItem(IDC_BTN_DISCHARGE)->EnableWindow(TRUE);

    m_CPU_Progress.ShowWindow(SW_HIDE);



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
    // Ask user where to save
    CFileDialog fileDlg(FALSE, L"csv", L"BatteryData.csv", OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT, L"CSV Files (*.csv)|*.csv||");
    if (fileDlg.DoModal() != IDOK)
        return;

    CString filePath = fileDlg.GetPathName();

    // Open file
    std::ofstream csvFile(filePath.GetString());
    if (!csvFile.is_open())
    {
        AfxMessageBox(L"Failed to open file for writing.");
        return;
    }

    // Helper lambda to convert CString to CSV-safe string
    auto toCsvField = [](const CString& str) -> std::string {
        CT2CA pszConvertedAnsiString(str);
        std::string s(pszConvertedAnsiString);

        // Replace line breaks with spaces
        for (auto& c : s)
        {
            if (c == '\n' || c == '\r') c = ' ';
        }

        // Enclose in quotes
        return "\"" + s + "\"";
        };

    // Write CSV header with Cycle Count
    csvFile << "Battery Percentage,Status,Voltage (V),Full Capacity (mWh),Design Capacity (mWh),Health,Device ID,Battery Name,Estimated Time,Cycle Count,CPU Load Test,Discharge Test\n";

    // Get current values from controls
    CString battPercent, battStatus, voltage, fullCap, designCap, health, deviceID, battName, estTime, cycleCount;

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

    csvFile << toCsvField(battPercent) << ","
        << toCsvField(battStatus) << ","
        << toCsvField(voltage) << ","
        << toCsvField(fullCap) << ","
        << toCsvField(designCap) << ","
        << toCsvField(health) << ","
        << toCsvField(deviceID) << ","
        << toCsvField(battName) << ","
        << toCsvField(estTime) << ","
        << toCsvField(cycleCount) << ","
        << toCsvField(m_cpuLoadResult) << ","
        << toCsvField(m_dischargeResult) << "\n";

    csvFile.close();

    AfxMessageBox(L"Battery data exported successfully!");
}


void CBatteryHelthDlg::OnBnClickedBtnHistory()
{
    CString table;
    // Column headers
    table = L"Start       End         Total Charge Time\n";
    table += L"----------------------------------------\n";

    CTime startTime;
    bool hasStart = false;

    for (const auto& evt : m_batteryHistory)
    {
        if (evt.charging) // Charging started
        {
            startTime = evt.timestamp;
            hasStart = true;
        }
        else // Discharging started
        {
            if (hasStart)
            {
                CTimeSpan total = evt.timestamp - startTime;
                double totalMinutes = total.GetTotalSeconds() / 60.0;

                CString startStr = startTime.Format(L"%H:%M:%S");
                CString endStr = evt.timestamp.Format(L"%H:%M:%S");
                CString totalStr;
                totalStr.Format(L"%.2f min", totalMinutes);

                // Pad strings to fixed width
                startStr = startStr + CString(' ', 10 - startStr.GetLength());
                endStr = endStr + CString(' ', 10 - endStr.GetLength());
                totalStr = totalStr + CString(' ', 12 - totalStr.GetLength());

                table += startStr + endStr + totalStr + L"\n";

                hasStart = false;
            }
        }
    }

    // Case where charging started but not yet ended
    if (hasStart)
    {
        CString startStr = startTime.Format(L"%H:%M:%S");
        CString endStr = L"null";
        CString totalStr = L"0 min";

        startStr = startStr + CString(' ', 10 - startStr.GetLength());
        endStr = endStr + CString(' ', 10 - endStr.GetLength());
        totalStr = totalStr + CString(' ', 12 - totalStr.GetLength());

        table += startStr + endStr + totalStr + L"\n";
    }

    if (m_batteryHistory.empty())
        table += L"No history recorded yet.";

    AfxMessageBox(table, MB_OK | MB_ICONINFORMATION);
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




    CDialogEx::OnDrawItem(nIDCtl, lpDrawItemStruct);
}




BOOL CBatteryHelthDlg::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
    // Check if the cursor is over your button
    if (pWnd->GetDlgCtrlID() == IDC_BTN_CPULOAD || pWnd->GetDlgCtrlID() == IDC_BTN_DISCHARGE || pWnd->GetDlgCtrlID() == IDC_BTN_HISTORY || pWnd->GetDlgCtrlID() == IDC_BTN_UPLOADPDF)
    {
        ::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
        return TRUE; // handled
    }

    return CDialogEx::OnSetCursor(pWnd, nHitTest, message);
}




void CBatteryHelthDlg::OnDestroy()
{
    KillTimer(1); // Stop hover detection timer
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

