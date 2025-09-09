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
#pragma comment(lib, "pdh.lib")

#pragma comment(lib, "wbemuuid.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif




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

    m_stopCpuLoad.store(false);

    // Get battery info + start timer
    GetBatteryInfo();
    SetTimer(1, 1000, NULL);

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

    // ----------------------------
    // Dialog size & style
    // ----------------------------
    int width = 670;
    int height = 780;
    SetWindowPos(NULL, 0, 0, width, height,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    CenterWindow();

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
        IDC_BATT_HEALTH, IDC_STATIC_VOLTAGE, IDC_BATT_VOLTAGE, IDC_STATIC_TEMP, IDC_BATT_TEMP
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

    return TRUE;
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
    // Clean up scaled fonts
    if (m_scaledFont.GetSafeHandle())
        m_scaledFont.DeleteObject();
    if (m_scaledBoldFont.GetSafeHandle())
        m_scaledBoldFont.DeleteObject();

    // Create scaled fonts
    LOGFONT lf = { 0 };
    lf.lfHeight = (int)(-16 * scale);
    lf.lfWeight = FW_NORMAL;
    _tcscpy_s(lf.lfFaceName, _T("Segoe UI"));
    m_scaledFont.CreateFontIndirect(&lf);

    lf.lfWeight = FW_BOLD;
    m_scaledBoldFont.CreateFontIndirect(&lf);

    // Apply scaled fonts to all controls
    CWnd* pWnd = GetWindow(GW_CHILD);
    while (pWnd)
    {
        pWnd->SetFont(&m_scaledFont);
        pWnd = pWnd->GetNextWindow();
    }

    // Apply bold scaled font to specific controls
    if (m_bb.GetSafeHwnd()) m_bb.SetFont(&m_scaledBoldFont);
    if (m_abt.GetSafeHwnd()) m_abt.SetFont(&m_scaledBoldFont);
    if (m_dh.GetSafeHwnd()) m_dh.SetFont(&m_scaledBoldFont);
    if (m_header.GetSafeHwnd()) m_header.SetFont(&m_scaledBoldFont);

    CWnd* pBtn;
    if ((pBtn = GetDlgItem(IDC_BTN_CPULOAD)) != nullptr)
        pBtn->SetFont(&m_scaledBoldFont);
    if ((pBtn = GetDlgItem(IDC_BTN_DISCHARGE)) != nullptr)
        pBtn->SetFont(&m_scaledBoldFont);
    if ((pBtn = GetDlgItem(IDC_BTN_UPLOADPDF)) != nullptr)
        pBtn->SetFont(&m_scaledBoldFont);
    if ((pBtn = GetDlgItem(IDC_BTN_HISTORY)) != nullptr)
        pBtn->SetFont(&m_scaledBoldFont);
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
    }

    // Force redraw to show font changes
    Invalidate();
    UpdateWindow();
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






HBRUSH CBatteryHelthDlg::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    HBRUSH hbr = CDialogEx::OnCtlColor(pDC, pWnd, nCtlColor);

    // Set background color for the dialog
    if (nCtlColor == CTLCOLOR_DLG)
    {
        return m_brushWhite; // white background
    }

    // Optional: make static text transparent with white background
    if (nCtlColor == CTLCOLOR_STATIC)
    {
        pDC->SetBkMode(TRANSPARENT);
        return m_brushWhite;
    }

    return hbr;
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


void CBatteryHelthDlg::GetBatteryInfo()
{

    // Initialize COM
    HRESULT hres;
    //hres = CoInitializeEx(0, COINIT_MULTITHREADED);
    //if (FAILED(hres)) { AfxMessageBox(L"Failed to initialize COM."); return; }

    //// Initialize security
    //hres = CoInitializeSecurity(
    //    NULL,
    //    -1,
    //    NULL,
    //    NULL,
    //    RPC_C_AUTHN_LEVEL_DEFAULT,
    //    RPC_C_IMP_LEVEL_IMPERSONATE,
    //    NULL,
    //    EOAC_NONE,
    //    NULL);
    //if (FAILED(hres)) { AfxMessageBox(L"Failed to initialize security."); CoUninitialize(); return; }

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

            //// Manufacturer
            //hr = pClsObj->Get(L"Manufacturer", 0, &vtProp, 0, 0);
            //SetDlgItemText(IDC_STATIC_MANUFACTURER, vtProp.bstrVal);
            //VariantClear(&vtProp);

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
            SetDlgItemText(IDC_BATT_STATUS, status);
            VariantClear(&vtProp);

            // cycle_count
            /* hr = pClsObj->Get(L"CycleCount", 0, &vtProp, 0, 0);
             CString cycle;
             if (vtProp.vt != VT_NULL && vtProp.vt != VT_EMPTY)
             {
                 cycle.Format(L"%d", vtProp.intVal);
             }
             else
             {
                 cycle = L"Not available";
             }
             SetDlgItemText(IDC_STATIC_MANUFACTURER, cycle);
             VariantClear(&vtProp);*/

             // Battery Percentage
            /* hr = pClsObj->Get(L"EstimatedChargeRemaining", 0, &vtProp, 0, 0);
             CString percentage;
             if (vtProp.vt != VT_NULL && vtProp.vt != VT_EMPTY)
             {
                 percentage.Format(L"%d%%", vtProp.intVal);
             }
             else
             {
                 percentage = L"Not available";
             }
             SetDlgItemText(IDC_STATIC_MANUFACTURER, percentage);
             VariantClear(&vtProp);*/

             //battery percentage using win32api
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

                SetDlgItemText(IDC_BATT_PERCENTAGE, percentage);
            }
            else
            {
                SetDlgItemText(IDC_BATT_PERCENTAGE, L"Not available");
            }

            // Estimated Time Remaining
            //hr = pClsObj->Get(L"EstimatedRunTime", 0, &vtProp, 0, 0);
            //CString remaining;
            //if (vtProp.vt != VT_NULL && vtProp.vt != VT_EMPTY)
            //{
            //    int minutes = vtProp.intVal;
            //    if (minutes == 0xFFFFFFFF) // Unknown or unlimited
            //        remaining = L"Unknown";
            //    else
            //    {
            //        int hours = minutes / 60;
            //        int mins = minutes % 60;
            //        remaining.Format(L"%d hr %d min", hours, mins);
            //    }
            //}
            //else
            //{
            //    remaining = L"Not available";
            //}
            //SetDlgItemText(IDC_STATIC_TIMEREMAINING, remaining);
            //VariantClear(&vtProp);

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
            SetDlgItemText(IDC_BATT_TIME, remainingTime);

            // Full Charge Capacity
            CString fullCap = QueryWmiValue(L"ROOT\\WMI", L"BatteryFullChargedCapacity", L"FullChargedCapacity");
            if (fullCap != L"Not available")
                fullCap += L" mWh";
            SetDlgItemText(IDC_BATT_CAPACITY, fullCap);

            // Design Capacity
            CString designCap = QueryWmiValue(L"ROOT\\WMI", L"BatteryStaticData", L"DesignedCapacity");
            if (designCap != L"Not available")
                designCap += L" mWh";
            SetDlgItemText(IDC_BATT_DCAPACITY, designCap);


            // --- Battery Health Calculation ---
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
                    SetDlgItemText(IDC_BATT_HEALTH, healthStr);
                }
                else
                {
                    SetDlgItemText(IDC_BATT_HEALTH, L"Not available");
                }
            }
            else
            {
                SetDlgItemText(IDC_BATT_HEALTH, L"Not available");
            }


            // Voltage
            CString voltage = QueryWmiValue(L"ROOT\\WMI", L"BatteryStatus", L"Voltage");
            if (voltage != L"Not available")
            {
                double volts = _wtoi(voltage) / 1000.0; // convert mV ? V
                CString voltStr;
                voltStr.Format(L"%.2f V", volts);
                SetDlgItemText(IDC_BATT_VOLTAGE, voltStr);
            }
            else
            {
                SetDlgItemText(IDC_BATT_VOLTAGE, L"Not available");
            }

            // Get Battery DeviceID
            VARIANT vtDeviceID;
            HRESULT hr = pClsObj->Get(L"DeviceID", 0, &vtDeviceID, 0, 0);
            CString deviceID;
            if (vtDeviceID.vt != VT_NULL && vtDeviceID.vt != VT_EMPTY)
            {
                deviceID = vtDeviceID.bstrVal;
            }
            else
            {
                deviceID = L"Not available";
            }
            SetDlgItemText(IDC_BATT_MANUFAC, deviceID);
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
            SetDlgItemText(IDC_BATT_NAME, batteryName);
            VariantClear(&vtProp);

            // Battery Cycle Count - NEW SECTION
            CString cycleCount = QueryBatteryCycleCount();
            if (cycleCount != L"Not available")
            {
                cycleCount += L" cycles";
            }
            SetDlgItemText(IDC_BATT_CYCLE, cycleCount);

            // --------- ?? NEW: System UUID (from Win32_ComputerSystemProduct) ---------
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
                            SetDlgItemText(IDC_BATT_DID, uuid); // make sure you add IDC_DEVICE_UUID in your dialog
                            VariantClear(&vtUUID);
                        }
                        pObjUUID->Release();
                    }
                    pEnumUUID->Release();
                }
                else
                {
                    SetDlgItemText(IDC_BATT_DID, L"Not available");
                }
            }



            pClsObj->Release();
        }

        pEnumerator->Release();
    }

    pSvc->Release();
    pLoc->Release();
    /*   CoUninitialize();*/

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

    if (m_cpuLoadTestRunning ) {
        GetDlgItem(IDC_BTN_DISCHARGE)->EnableWindow(FALSE);
        GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(FALSE);

        return;
    }

    if (m_dischargeTestRunning ) {
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

            SetDlgItemText(IDC_BATT_DISCHARGR, L"Discharge Test Stopped");
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
        SetDlgItemText(IDC_BATT_DISCHARGR, L"Discharge Test Stopped!");

        GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(TRUE);
        return;
    }

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
    m_dischargeTestRunning = true;

    GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(FALSE);
    SetDlgItemText(IDC_BTN_DISCHARGE, L"Stop Discharge Test");

    SetTimer(m_dischargeTimerID, 60000, NULL); // 1-minute interval

    CString imsg;
    imsg.Format(L"Discharge Test Running...\nTime Elapsed: %d min\nInitial: %d%%\nCurrent: %d%%\nDrop: %d%%\nDrain Rate: %.2f %%/min",
        0, m_initialBatteryPercent, m_initialBatteryPercent, 0, 0.0);
    SetDlgItemText(IDC_BATT_DISCHARGR, imsg);
}



void CBatteryHelthDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1)
    {
        GetBatteryInfo();

    }
    else if (nIDEvent == m_dischargeTimerID && m_dischargeTestRunning)
    {
        m_elapsedMinutes++;

        SYSTEM_POWER_STATUS sps;
        if (GetSystemPowerStatus(&sps) && sps.BatteryLifePercent != 255)
        {
            int drop = m_initialBatteryPercent - sps.BatteryLifePercent; // Calculate drop

            // Calculate drain rate (% per minute)
            double drainRate = 0.0;
            if (m_elapsedMinutes > 0)
                drainRate = static_cast<double>(drop) / m_elapsedMinutes;

            CString msg;
            msg.Format(L"Time elapsed: %d min\nInitial: %d%%\nCurrent: %d%%\nDrop: %d%%\nDrain Rate: %.2f %%/min",
                m_elapsedMinutes, m_initialBatteryPercent, sps.BatteryLifePercent, drop, drainRate);

            SetDlgItemText(IDC_BATT_DISCHARGR, msg);

            // Stop the test after duration
            if (m_elapsedMinutes >= m_dischargeDurationMinutes)
            {
                KillTimer(m_dischargeTimerID);
                m_dischargeTestRunning = false;

                // UPDATE m_dischargeResult here including drain rate
                m_dischargeResult.Format(L"Initial: %d%%, Final: %d%%, Drop: %d%%, Drain Rate: %.2f %%/min",
                    m_initialBatteryPercent, sps.BatteryLifePercent, drop, drainRate);

                GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(TRUE);
                GetDlgItem(IDC_BTN_DISCHARGE)->EnableWindow(TRUE);
         
                return;

                AfxMessageBox(L"Discharge Test Completed!"); 
            }
        }
        else
        {
            SetDlgItemText(IDC_BATT_DISCHARGR, L"Cannot read battery percentage.");
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
        msg.Format(L"Please wait 2 minutes to complete.");



        
        SetDlgItemText(IDC_BATT_CPULOAD, msg);

        m_CPU_Progress.SetPos(percentDone);


        // Stop if requested
        if (m_stopCpuLoad.load())
        {
            m_cpuLoadTestRunning = false;
            KillTimer(m_cpuLoadTimerID);
            SetDlgItemText(IDC_BTN_CPULOAD, L"CPU Load Test");
            SetDlgItemText(IDC_BATT_CPULOAD, L"CPU Load Test Stopped!");
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




void RunCPULoadFP64(int durationSeconds,
    std::atomic<long long>* operationCounter,
    std::atomic<long long>* flopCounter,
    std::atomic<bool>* stopFlag)
{
    auto startTime = std::chrono::high_resolution_clock::now();
    long long localOps = 0;
    long long localFlops = 0;
    const long long iterationsPerBatch = 1000000;

    int batchCounter = 0;
    const int timeBatchSize = 100;

    bool useAVX512 = CheckAVX512Support();
    bool useAVX2 = !useAVX512 && CheckAVX2Support();
    bool useSSE = !useAVX512 && !useAVX2 && CheckSSESupport();

    if (useAVX512)
    {
        __m512d a1 = _mm512_set1_pd(1.5);
        __m512d b1 = _mm512_set1_pd(2.3);
        __m512d c1 = _mm512_set1_pd(0.0);
        __m512d a2 = _mm512_set1_pd(1.7);
        __m512d b2 = _mm512_set1_pd(2.1);
        __m512d c2 = _mm512_set1_pd(0.0);
        __m512d a3 = _mm512_set1_pd(1.9);
        __m512d b3 = _mm512_set1_pd(2.5);
        __m512d c3 = _mm512_set1_pd(0.0);
        __m512d a4 = _mm512_set1_pd(1.3);
        __m512d b4 = _mm512_set1_pd(2.7);
        __m512d c4 = _mm512_set1_pd(0.0);

        while (true)
        {
            if ((batchCounter % timeBatchSize) == 0)
            {
                if (stopFlag->load()) break;
                auto now = std::chrono::high_resolution_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count()
                    >= durationSeconds * 1000)
                    break;
            }

            // Process in batches - each batch does iterationsPerBatch/4 actual loop iterations
            // because original code incremented i += 4
            for (long long i = 0; i < iterationsPerBatch; i += 4)
            {
                c1 = _mm512_fmadd_pd(a1, b1, c1);  // 8 doubles × 2 ops = 16 FLOPs
                c2 = _mm512_fmadd_pd(a2, b2, c2);  // 8 doubles × 2 ops = 16 FLOPs
                c3 = _mm512_fmadd_pd(a3, b3, c3);  // 8 doubles × 2 ops = 16 FLOPs
                c4 = _mm512_fmadd_pd(a4, b4, c4);  // 8 doubles × 2 ops = 16 FLOPs
                // Total per loop iteration: 64 FLOPs, 64 operations

                localFlops += 64;   // 4 FMA × 16 FLOPs each
                localOps += 64;     // Same as FLOPs for floating point operations
            }
            batchCounter++;
        }

        double result[8];
        _mm512_storeu_pd(result, _mm512_add_pd(_mm512_add_pd(c1, c2), _mm512_add_pd(c3, c4)));
        volatile double sink = result[0];
    }
    else if (useAVX2)
    {
        __m256d a1 = _mm256_set1_pd(1.5);
        __m256d b1 = _mm256_set1_pd(2.3);
        __m256d c1 = _mm256_set1_pd(0.0);
        __m256d a2 = _mm256_set1_pd(1.7);
        __m256d b2 = _mm256_set1_pd(2.1);
        __m256d c2 = _mm256_set1_pd(0.0);
        __m256d a3 = _mm256_set1_pd(1.9);
        __m256d b3 = _mm256_set1_pd(2.5);
        __m256d c3 = _mm256_set1_pd(0.0);
        __m256d a4 = _mm256_set1_pd(1.3);
        __m256d b4 = _mm256_set1_pd(2.7);
        __m256d c4 = _mm256_set1_pd(0.0);

        while (true)
        {
            if ((batchCounter % timeBatchSize) == 0)
            {
                if (stopFlag->load()) break;
                auto now = std::chrono::high_resolution_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count()
                    >= durationSeconds * 1000)
                    break;
            }

            for (long long i = 0; i < iterationsPerBatch; i += 4)
            {
                c1 = _mm256_fmadd_pd(a1, b1, c1);  // 4 doubles × 2 ops = 8 FLOPs
                c2 = _mm256_fmadd_pd(a2, b2, c2);  // 4 doubles × 2 ops = 8 FLOPs
                c3 = _mm256_fmadd_pd(a3, b3, c3);  // 4 doubles × 2 ops = 8 FLOPs
                c4 = _mm256_fmadd_pd(a4, b4, c4);  // 4 doubles × 2 ops = 8 FLOPs
                // Total per loop iteration: 32 FLOPs, 32 operations

                localFlops += 32;   // 4 FMA × 8 FLOPs each
                localOps += 32;     // Same as FLOPs
            }
            batchCounter++;
        }

        double result[4];
        _mm256_storeu_pd(result, _mm256_add_pd(_mm256_add_pd(c1, c2), _mm256_add_pd(c3, c4)));
        volatile double sink = result[0];
    }
    else if (useSSE)
    {
        __m128d a1 = _mm_set1_pd(1.5);
        __m128d b1 = _mm_set1_pd(2.3);
        __m128d c1 = _mm_set1_pd(0.0);
        __m128d a2 = _mm_set1_pd(1.7);
        __m128d b2 = _mm_set1_pd(2.1);
        __m128d c2 = _mm_set1_pd(0.0);
        __m128d a3 = _mm_set1_pd(1.9);
        __m128d b3 = _mm_set1_pd(2.5);
        __m128d c3 = _mm_set1_pd(0.0);
        __m128d a4 = _mm_set1_pd(1.3);
        __m128d b4 = _mm_set1_pd(2.7);
        __m128d c4 = _mm_set1_pd(0.0);

        while (true)
        {
            if ((batchCounter % timeBatchSize) == 0)
            {
                if (stopFlag->load()) break;
                auto now = std::chrono::high_resolution_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count()
                    >= durationSeconds * 1000)
                    break;
            }

            for (long long i = 0; i < iterationsPerBatch; i += 4)
            {
                __m128d tmp1 = _mm_mul_pd(a1, b1);  // 2 doubles × 1 op = 2 FLOPs
                c1 = _mm_add_pd(tmp1, c1);          // 2 doubles × 1 op = 2 FLOPs
                __m128d tmp2 = _mm_mul_pd(a2, b2);  // 2 FLOPs
                c2 = _mm_add_pd(tmp2, c2);          // 2 FLOPs
                __m128d tmp3 = _mm_mul_pd(a3, b3);  // 2 FLOPs
                c3 = _mm_add_pd(tmp3, c3);          // 2 FLOPs
                __m128d tmp4 = _mm_mul_pd(a4, b4);  // 2 FLOPs
                c4 = _mm_add_pd(tmp4, c4);          // 2 FLOPs
                // Total per loop iteration: 16 FLOPs, 16 operations

                localFlops += 16;   // 8 instructions × 2 FLOPs each
                localOps += 16;     // Same as FLOPs
            }
            batchCounter++;
        }

        double result[2];
        _mm_storeu_pd(result, _mm_add_pd(_mm_add_pd(c1, c2), _mm_add_pd(c3, c4)));
        volatile double sink = result[0];
    }
    else
    {
        // Scalar fallback
        double a1 = 1.5, b1 = 2.3, c1 = 0.0;
        double a2 = 1.7, b2 = 2.1, c2 = 0.0;
        double a3 = 1.9, b3 = 2.5, c3 = 0.0;
        double a4 = 1.3, b4 = 2.7, c4 = 0.0;

        while (true)
        {
            if ((batchCounter % timeBatchSize) == 0)
            {
                if (stopFlag->load()) break;
                auto now = std::chrono::high_resolution_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count()
                    >= durationSeconds * 1000)
                    break;
            }

            for (long long i = 0; i < iterationsPerBatch; i += 4)
            {
                c1 = a1 * b1 + c1;  // 1 multiply + 1 add = 2 FLOPs
                c2 = a2 * b2 + c2;  // 2 FLOPs
                c3 = a3 * b3 + c3;  // 2 FLOPs
                c4 = a4 * b4 + c4;  // 2 FLOPs
                // Total per loop iteration: 8 FLOPs, 8 operations

                localFlops += 8;    // 4 operations × 2 FLOPs each
                localOps += 8;      // Same as FLOPs
            }
            batchCounter++;
        }

        volatile double sink = c1 + c2 + c3 + c4;
    }

    operationCounter->fetch_add(localOps, std::memory_order_relaxed);
    flopCounter->fetch_add(localFlops, std::memory_order_relaxed);
}

// CPU Load Test button handler
void CBatteryHelthDlg::OnBnClickedBtnCpuload()
{
    // Stop if running
    if (m_cpuLoadTestRunning)
    {
        m_stopCpuLoad.store(true);
        m_cpuLoadTestRunning = false;

        SetDlgItemText(IDC_BTN_CPULOAD, L"Start CPU Load Test");
        SetDlgItemText(IDC_BATT_CPULOAD, L"CPU Load Not Tested...");

        return;
    }

    // Check current CPU usage before starting the test
    double usage = GetCurrentCPUUsage();
    if (usage >= 10)
    {
        CString msg;
        msg.Format(L"CPU usage is currently above 10%%. Please close background applications first and try again.\n(Current: %.0f%%)", usage);
        ::MessageBox(this->m_hWnd, msg, L"CPU Usage Warning", MB_OK | MB_ICONWARNING);
        return;
    }

    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps) || sps.BatteryLifePercent == 255)
    {
        AfxMessageBox(L"Cannot read battery percentage.");
        return;
    }

    m_initialBatteryCPUPercent = sps.BatteryLifePercent;
    m_cpuLoadTestRunning = true;
    m_stopCpuLoad.store(false);

    SetDlgItemText(IDC_BATT_CPULOAD, L"CPU Load Test Running...");

   /* SetDlgItemText(IDC_BTN_CPULOAD, L"Stop CPU Load Test");*/
    GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(FALSE);

    GetDlgItem(IDC_BTN_DISCHARGE)->EnableWindow(FALSE);

    m_cpuLoadElapsed = 0;
    SetTimer(m_cpuLoadTimerID, 1000, NULL);

    std::thread([this]()
        {
            // Use ALL logical cores for maximum stress
            int numCores = std::thread::hardware_concurrency();
            if (numCores == 0) numCores = 1;

            std::vector<std::thread> threads;
            std::atomic<long long> totalOperations(0);
            std::atomic<long long> totalFlops(0);

            // Set high priority for maximum CPU usage
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

            auto startTime = std::chrono::high_resolution_clock::now();

            // Launch one thread per logical core (including hyperthreading)
            for (int i = 0; i < numCores; i++)
            {
                threads.emplace_back([this, &totalOperations, &totalFlops, i]()
                    {
                        // Set high priority for worker threads
                        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                        // Set thread affinity to specific core for better performance
                        DWORD_PTR affinityMask = 1ULL << i;
                        SetThreadAffinityMask(GetCurrentThread(), affinityMask);

                        RunCPULoadFP64(m_cpuLoadDurationSeconds, &totalOperations, &totalFlops, &m_stopCpuLoad);
                    });
            }

            // Join threads
            for (auto& t : threads)
                t.join();

            auto endTime = std::chrono::high_resolution_clock::now();
            double actualDurationSeconds = std::chrono::duration<double>(endTime - startTime).count();

            long long ops = totalOperations.load();
            long long flops = totalFlops.load();

            double tops = (ops / actualDurationSeconds) / 1e12;
            double gflops = (flops / actualDurationSeconds) / 1e9;

            SYSTEM_POWER_STATUS spsEnd;
            if (GetSystemPowerStatus(&spsEnd) && spsEnd.BatteryLifePercent != 255)
            {
                int drop = m_initialBatteryCPUPercent - spsEnd.BatteryLifePercent;
                double rate = drop / (m_cpuLoadDurationSeconds / 60.0);
                CString msg;
                msg.Format(L"Initial: %d%%\nCurrent: %d%%\nDrop: %d%%\nRate: %.2f%%/min\nTOPS: %.4f\nGFLOPS: %.3f\nCores Used: %d",
                    m_initialBatteryCPUPercent, spsEnd.BatteryLifePercent, drop, rate, tops, gflops, numCores);
                m_cpuLoadTestCompleted = true;
                m_cpuLoadResult = msg;
                this->PostMessage(WM_APP + 1, 0, (LPARAM)new CString(msg));
            }
            else
            {
                CString msg = L"Cannot read battery percentage.";
                m_cpuLoadTestCompleted = true;
                m_cpuLoadResult = msg;
                this->PostMessage(WM_APP + 1, 0, (LPARAM)new CString(msg));
            }
            m_cpuLoadTestRunning = false;
        }).detach();
}






//void RunCPULoadFP64(int durationSeconds,
//    std::atomic<long long>* operationCounter,
//    std::atomic<long long>* flopCounter,
//    std::atomic<bool>* stopFlag)
//{
//    auto startTime = std::chrono::high_resolution_clock::now();
//    long long localOps = 0;
//    long long localFlops = 0;
//    const long long iterationsPerLoop = 100000; // large enough for timing
//
//    bool useAVX512 = CheckAVX512Support();
//    bool useAVX2 = !useAVX512 && CheckAVX2Support();
//    bool useSSE = !useAVX512 && !useAVX2 && CheckSSESupport();
//
//    if (useAVX512)
//    {
//        // AVX-512 FP64: 8 doubles per vector, FMA = 16 FLOPs
//        __m512d a = _mm512_set1_pd(1.5);
//        __m512d b = _mm512_set1_pd(2.3);
//        __m512d c = _mm512_set1_pd(0.0);
//        long long flopsPerIteration = 16;
//        long long opsPerIteration = flopsPerIteration + 10;
//
//        while (true)
//        {
//            if (stopFlag->load()) break; // <- stop requested
//            auto now = std::chrono::high_resolution_clock::now();
//            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count()
//                >= durationSeconds * 1000)
//                break;
//
//            for (long long i = 0; i < iterationsPerLoop; i++)
//            {
//                if (stopFlag->load()) break; // <- check inside loop too
//                c = _mm512_fmadd_pd(a, b, c);
//                localFlops += flopsPerIteration;
//                localOps += opsPerIteration;
//            }
//        }
//        double result[8];
//        _mm512_storeu_pd(result, c);
//        volatile double sink = result[0];
//    }
//    else if (useAVX2)
//    {
//        __m256d a = _mm256_set1_pd(1.5);
//        __m256d b = _mm256_set1_pd(2.3);
//        __m256d c = _mm256_set1_pd(0.0);
//        long long flopsPerIteration = 8;
//        long long opsPerIteration = flopsPerIteration + 10;
//
//        while (true)
//        {
//            if (stopFlag->load()) break;
//            auto now = std::chrono::high_resolution_clock::now();
//            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count()
//                >= durationSeconds * 1000)
//                break;
//
//            for (long long i = 0; i < iterationsPerLoop; i++)
//            {
//                if (stopFlag->load()) break;
//                c = _mm256_fmadd_pd(a, b, c);
//                localFlops += flopsPerIteration;
//                localOps += opsPerIteration;
//            }
//        }
//        double result[4];
//        _mm256_storeu_pd(result, c);
//        volatile double sink = result[0];
//    }
//    else if (useSSE)
//    {
//        __m128d a = _mm_set1_pd(1.5);
//        __m128d b = _mm_set1_pd(2.3);
//        __m128d c = _mm_set1_pd(0.0);
//        long long flopsPerIteration = 4;
//        long long opsPerIteration = flopsPerIteration + 10;
//
//        while (true)
//        {
//            if (stopFlag->load()) break;
//            auto now = std::chrono::high_resolution_clock::now();
//            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count()
//                >= durationSeconds * 1000)
//                break;
//
//            for (long long i = 0; i < iterationsPerLoop; i++)
//            {
//                if (stopFlag->load()) break;
//                __m128d tmp = _mm_mul_pd(a, b);
//                c = _mm_add_pd(tmp, c);
//                localFlops += flopsPerIteration;
//                localOps += opsPerIteration;
//            }
//        }
//        double result[2];
//        _mm_storeu_pd(result, c);
//        volatile double sink = result[0];
//    }
//    else
//    {
//        // Scalar fallback
//        double a = 1.5, b = 2.3, c = 0.0;
//        long long flopsPerIteration = 2;
//        long long opsPerIteration = 2 + 10;
//
//        while (true)
//        {
//            if (stopFlag->load()) break;
//            auto now = std::chrono::high_resolution_clock::now();
//            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count()
//                >= durationSeconds * 1000)
//                break;
//
//            for (long long i = 0; i < iterationsPerLoop; i++)
//            {
//                if (stopFlag->load()) break;
//                c = a * b + c;
//                localFlops += flopsPerIteration;
//                localOps += opsPerIteration;
//            }
//        }
//        volatile double sink = c;
//    }
//
//    operationCounter->fetch_add(localOps, std::memory_order_relaxed);
//    flopCounter->fetch_add(localFlops, std::memory_order_relaxed);
//}
//
//
//void RunCPULoad(int durationSeconds, std::atomic<long long>* operationCounter, std::atomic<long long>* flopCounter) {
//    auto startTime = std::chrono::high_resolution_clock::now();
//    long long localOps = 0;
//    long long localFlops = 0;
//    const long long iterationsPerLoop = 10000;
//
//    // Determine instruction set and set FLOP/operation counts
//    bool useAVX512 = CheckAVX512Support();
//    bool useAVX2 = !useAVX512 && CheckAVX2Support();
//    bool useSSE = !useAVX512 && !useAVX2 && CheckSSESupport();
//    long long flopsPerIteration = 0;
//    long long opsPerIteration = 0;
//
//    if (useAVX512) {
//        // AVX-512: 16 FP32 elements, FMA = 32 FLOPs
//        __m512 a = _mm512_set1_ps(1.5f);
//        __m512 b = _mm512_set1_ps(2.3f);
//        __m512 c = _mm512_set1_ps(0.0f);
//        flopsPerIteration = 32; // 16 elements * 2 FLOPs (FMA)
//        opsPerIteration = 32 + 10; // FLOPs + estimated integer/control ops
//
//        while (true) {
//            auto currentTime = std::chrono::high_resolution_clock::now();
//            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
//            if (elapsed >= durationSeconds * 1000) break;
//
//            for (long long i = 0; i < iterationsPerLoop; i++) {
//                c = _mm512_fmadd_ps(a, b, c); // AVX-512 FMA
//                localFlops += flopsPerIteration;
//                localOps += opsPerIteration;
//            }
//        }
//        float result[16];
//        _mm512_storeu_ps(result, c);
//        volatile float sink = result[0]; // Prevent optimization
//    }
//    else if (useAVX2) {
//        // AVX2: 8 FP32 elements, FMA = 16 FLOPs
//        __m256 a = _mm256_set1_ps(1.5f);
//        __m256 b = _mm256_set1_ps(2.3f);
//        __m256 c = _mm256_set1_ps(0.0f);
//        flopsPerIteration = 16; // 8 elements * 2 FLOPs (FMA)
//        opsPerIteration = 16 + 10;
//
//        while (true) {
//            auto currentTime = std::chrono::high_resolution_clock::now();
//            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
//            if (elapsed >= durationSeconds * 1000) break;
//
//            for (long long i = 0; i < iterationsPerLoop; i++) {
//                c = _mm256_fmadd_ps(a, b, c); // AVX2 FMA
//                localFlops += flopsPerIteration;
//                localOps += opsPerIteration;
//            }
//        }
//        float result[8];
//        _mm256_storeu_ps(result, c);
//        volatile float sink = result[0];
//    }
//    else if (useSSE) {
//        // SSE: 4 FP32 elements, FMA (emulated) = 8 FLOPs
//        __m128 a = _mm_set1_ps(1.5f);
//        __m128 b = _mm_set1_ps(2.3f);
//        __m128 c = _mm_set1_ps(0.0f);
//        flopsPerIteration = 8; // 4 elements * 2 FLOPs (mul + add)
//        opsPerIteration = 8 + 10;
//
//        while (true) {
//            auto currentTime = std::chrono::high_resolution_clock::now();
//            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
//            if (elapsed >= durationSeconds * 1000) break;
//
//            for (long long i = 0; i < iterationsPerLoop; i++) {
//                __m128 temp = _mm_mul_ps(a, b); // SSE multiply
//                c = _mm_add_ps(temp, c); // SSE add (emulate FMA)
//                localFlops += flopsPerIteration;
//                localOps += opsPerIteration;
//            }
//        }
//        float result[4];
//        _mm_storeu_ps(result, c);
//        volatile float sink = result[0];
//    }
//    else {
//        // Scalar fallback: 2 FLOPs per iteration (mul + add)
//        float a = 1.5f, b = 2.3f, c = 0.0f;
//        flopsPerIteration = 2; // 1 mul + 1 add
//        opsPerIteration = 2 + 10;
//
//        while (true) {
//            auto currentTime = std::chrono::high_resolution_clock::now();
//            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count();
//            if (elapsed >= durationSeconds * 1000) break;
//
//            for (long long i = 0; i < iterationsPerLoop; i++) {
//                c = a * b + c; // Scalar FMA-like operation
//                localFlops += flopsPerIteration;
//                localOps += opsPerIteration;
//            }
//        }
//        volatile float sink = c;
//    }
//
//    operationCounter->fetch_add(localOps, std::memory_order_relaxed);
//    flopCounter->fetch_add(localFlops, std::memory_order_relaxed);
//}
//
//
//
//
double CBatteryHelthDlg::GetCurrentCPUUsage()
{
    static PDH_HQUERY cpuQuery;
    static PDH_HCOUNTER cpuTotal;
    static bool initialized = false;

    if (!initialized)
    {
        PdhOpenQuery(NULL, 0, &cpuQuery);
        PdhAddCounter(cpuQuery, L"\\Processor(_Total)\\% Processor Time", 0, &cpuTotal);
        PdhCollectQueryData(cpuQuery);
        initialized = true;
        Sleep(100); // short delay to get valid initial reading
    }

    // Collect CPU data
    PdhCollectQueryData(cpuQuery);

    PDH_FMT_COUNTERVALUE counterVal;
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, NULL, &counterVal);

    return counterVal.doubleValue; 
}
//
//
//
//
//// CPU Load Test button handler
//void CBatteryHelthDlg::OnBnClickedBtnCpuload()
//{
//
//
//    // Stop if running
//    if (m_cpuLoadTestRunning)
//    {
//        m_stopCpuLoad.store(true);
//        m_cpuLoadTestRunning = false;
//
//        SetDlgItemText(IDC_BTN_CPULOAD, L"Start CPU Load Test");
//        SetDlgItemText(IDC_BATT_CPULOAD, L"CPU Load Not Tested...");
//
//        return;
//    }
//
//
//    // Check current CPU usage before starting the test
//    double usage = GetCurrentCPUUsage();
//    if (usage >= 10)
//    {
//        CString msg;
//        msg.Format(L"CPU usage is currently above 10%%. Please close background applications first and try again.\n(Current: %.0f%%)", usage);
//        //AfxMessageBox(msg);
//        ::MessageBox(this->m_hWnd, msg, L"CPU Usage Warning", MB_OK | MB_ICONWARNING);
//        
//        return;
//    }
//    
//    SYSTEM_POWER_STATUS sps;
//    if (!GetSystemPowerStatus(&sps) || sps.BatteryLifePercent == 255)
//    {
//        AfxMessageBox(L"Cannot read battery percentage.");
//        return;
//    }
//    m_initialBatteryCPUPercent = sps.BatteryLifePercent;
//    m_cpuLoadTestRunning = true;
//    m_stopCpuLoad.store(false);
//
//    SetDlgItemText(IDC_BATT_CPULOAD, L"CPU Load Test Running...");
//    SetDlgItemText(IDC_BTN_CPULOAD, L"Stop CPU Load Test");
//
//    // Disable the CPU Load button
//    //GetDlgItem(IDC_BTN_CPULOAD)->EnableWindow(FALSE);
//
//    // Disable the Discharge Test button
//    GetDlgItem(IDC_BTN_DISCHARGE)->EnableWindow(FALSE);
//
//    m_cpuLoadElapsed = 0;
//    SetTimer(m_cpuLoadTimerID, 1000, NULL); // tick every 1 sec
//
//
//    std::thread([this]()
//        {
//            // Use physical cores, estimated as half of hardware_concurrency if Hyper-Threading is enabled
//            int numCores = std::thread::hardware_concurrency() / 2;
//            if (numCores == 0) numCores = 1; // Fallback for single-core systems
//            std::vector<std::thread> threads;
//            std::atomic<long long> totalOperations(0);
//            std::atomic<long long> totalFlops(0);
//
//            // Start timing
//            auto startTime = std::chrono::high_resolution_clock::now();
//
//            // Launch one thread per core
//            for (int i = 0; i < numCores; i++)
//                threads.emplace_back(RunCPULoadFP64, m_cpuLoadDurationSeconds, &totalOperations, &totalFlops, &m_stopCpuLoad);
//
//            // Join threads
//            for (auto& t : threads) t.join();
//
//            // Calculate total execution time
//            auto endTime = std::chrono::high_resolution_clock::now();
//            double actualDurationSeconds = std::chrono::duration<double>(endTime - startTime).count();
//
//            // Get total operations and flops count
//            long long ops = totalOperations.load();
//            long long flops = totalFlops.load();
//
//            // Calculate TOPS and GFLOPS
//            double tops = (ops / actualDurationSeconds) / 1e12;
//            double gflops = (flops / actualDurationSeconds) / 1e9;
//
//            SYSTEM_POWER_STATUS spsEnd;
//            if (GetSystemPowerStatus(&spsEnd) && spsEnd.BatteryLifePercent != 255)
//            {
//                int drop = m_initialBatteryCPUPercent - spsEnd.BatteryLifePercent;
//                double rate = drop / (m_cpuLoadDurationSeconds / 60.0);
//                CString msg;
//                msg.Format(L"Initial: %d%%\nCurrent: %d%%\nDrop: %d%%\nRate: %.2f%%/min\nTOPS: %.4f\nGFLOPS: %.3f",
//                    m_initialBatteryCPUPercent, spsEnd.BatteryLifePercent, drop, rate, tops, gflops);
//                m_cpuLoadTestCompleted = true;
//                m_cpuLoadResult = msg;
//                this->PostMessage(WM_APP + 1, 0, (LPARAM)new CString(msg));
//            }
//            else
//            {
//                CString msg = L"Cannot read battery percentage.";
//                m_cpuLoadTestCompleted = true;
//                m_cpuLoadResult = msg;
//                this->PostMessage(WM_APP + 1, 0, (LPARAM)new CString(msg));
//            }
//            m_cpuLoadTestRunning = false;
//        }).detach();
//}

LRESULT CBatteryHelthDlg::OnCPULoadFinished(WPARAM wParam, LPARAM lParam)
{

    if (m_cpuLoadTimerID != 0)
    {
        KillTimer(m_cpuLoadTimerID);
    }

    CString* pMsg = (CString*)lParam;
    SetDlgItemText(IDC_BATT_CPULOAD, *pMsg);
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


void CBatteryHelthDlg::OnDrawItem(int nIDCtl, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
    if (nIDCtl == IDC_BTN_CPULOAD || nIDCtl == IDC_BTN_DISCHARGE || nIDCtl == IDC_BTN_UPLOADPDF || nIDCtl == IDC_BTN_HISTORY) 
    {
        CDC dc;
        dc.Attach(lpDrawItemStruct->hDC);

        CRect rc = lpDrawItemStruct->rcItem;

        // Draw subtle shadow by offsetting a darker rectangle behind
        CRect shadowRect = rc;
        shadowRect.OffsetRect(2, 2); // shadow offset
        dc.FillSolidRect(shadowRect, RGB(200, 200, 200)); // shadow color

        // Determine button state
        if (lpDrawItemStruct->itemState & ODS_DISABLED)
        {
            dc.FillSolidRect(rc, RGB(245, 245, 245)); // Disabled background
            dc.SetTextColor(RGB(160, 160, 160));      // Gray text
        }
        else if (lpDrawItemStruct->itemState & ODS_SELECTED)
        {
            dc.FillSolidRect(rc, RGB(200, 200, 200)); // Pressed
            dc.SetTextColor(RGB(0, 0, 0));           // Black text
        }
        else
        {
            dc.FillSolidRect(rc, RGB(230, 230, 230)); // Normal
            dc.SetTextColor(RGB(0, 0, 0));           // Black text
        }

        // Draw border (optional)
        dc.DrawEdge(rc, EDGE_RAISED, BF_RECT);

        // Draw button text
        CString text;
        GetDlgItem(nIDCtl)->GetWindowText(text);

        dc.SetBkMode(TRANSPARENT);
        dc.DrawText(text, rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        dc.Detach();
        return; // handled
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

