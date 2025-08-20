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
    virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

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


    m_BatteryProgress.SetRange(0, 100);
    m_BatteryProgress.SetPos(0);
    GetBatteryInfo();

    SetTimer(1, 1000, NULL);

    // Add "About..." menu item to system menu.

    // IDM_ABOUTBOX must be in the system command range.
    ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
    ASSERT(IDM_ABOUTBOX < 0xF000);

    CMenu* pSysMenu = GetSystemMenu(FALSE);
    if (pSysMenu != nullptr)
    {
        BOOL bNameValid;
        CString strAboutMenu;
        bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
        ASSERT(bNameValid);
        if (!strAboutMenu.IsEmpty())
        {
            pSysMenu->AppendMenu(MF_SEPARATOR);
            pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
        }
    }

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);			// Set big icon
    SetIcon(m_hIcon, FALSE);		// Set small icon

    // TODO: Add extra initialization here

    return TRUE;  // return TRUE  unless you set the focus to a control
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
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

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
    SYSTEM_POWER_STATUS sps;

    if (!GetSystemPowerStatus(&sps) || sps.BatteryLifePercent == 255)
    {
        AfxMessageBox(L"Cannot read battery percentage.");
        return;
    }

    // Check Battery Saver
    if (sps.SystemStatusFlag & 1) // 1 = Battery saver ON
    {
        AfxMessageBox(L"Turn off your Power Saver.");
        return;
    }



    // Start discharge test
    m_initialBatteryPercent = sps.BatteryLifePercent; // Record starting %
    m_elapsedMinutes = 0;
    m_dischargeTestRunning = true;

    SetTimer(m_dischargeTimerID, 60000, NULL); // Timer every 1 minute
    SetDlgItemText(IDC_BATT_DISCHARGR, L"Discharge Test Running...");

    // Optional: check if test duration already completed (maybe move this to OnTimer)
    if (m_elapsedMinutes >= m_dischargeDurationMinutes)
    {
        KillTimer(m_dischargeTimerID);
        m_dischargeTestRunning = false;
        m_dischargeTestCompleted = true;

        CString finalMsg;
        finalMsg.Format(L"Initial: %d%%\nFinal: %d%%\nDrop: %d%%",
            m_initialBatteryPercent, sps.BatteryLifePercent, m_initialBatteryPercent - sps.BatteryLifePercent);

        m_dischargeResult = finalMsg;
        AfxMessageBox(finalMsg);
    }
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

                AfxMessageBox(L"Discharge Test Completed!");
            }
        }
        else
        {
            SetDlgItemText(IDC_BATT_DISCHARGR, L"Cannot read battery percentage.");
        }
    }

    CDialogEx::OnTimer(nIDEvent);
}


void RunCPULoad(int durationSeconds, std::atomic<long long>* operationCounter, std::atomic<long long>* flopCounter)
{
    ULONGLONG startTime = GetTickCount64();
    volatile double result = 0.0; // Prevent compiler optimization
    long long localOps = 0;
    long long localFlops = 0;

    while ((GetTickCount64() - startTime) < (ULONGLONG)durationSeconds * 1000)
    {
        for (int i = 1; i < 10000; i++)
        {
            double a = sqrt(i);         
            double b = sin(i);          
            double c = cos(i);         
       

            result *= a * b * c;

            localFlops += 6;
            localOps += 6;
        }
    }

    operationCounter->fetch_add(localOps);
    flopCounter->fetch_add(localFlops);
}

// CPU Load Test button handler
void CBatteryHelthDlg::OnBnClickedBtnCpuload()
{
    SYSTEM_POWER_STATUS sps;
    if (!GetSystemPowerStatus(&sps) || sps.BatteryLifePercent == 255)
    {
        AfxMessageBox(L"Cannot read battery percentage.");
        return;
    }

    m_initialBatteryCPUPercent = sps.BatteryLifePercent;
    m_cpuLoadTestRunning = true;
    SetDlgItemText(IDC_BATT_CPULOAD, L"CPU Load Test Running...");

    std::thread([this]()
        {
            int numCores = std::thread::hardware_concurrency();

            std::vector<std::thread> threads;
            std::atomic<long long> totalOperations(0);
            std::atomic<long long> totalFlops(0);

            // Start timing for TOPS and GFLOPS calculation
            ULONGLONG startTime = GetTickCount64();

            for (int i = 0; i < numCores; i++)
                threads.emplace_back(RunCPULoad, m_cpuLoadDurationSeconds, &totalOperations, &totalFlops);

            for (auto& t : threads) t.join();

            // Calculate total execution time
            ULONGLONG endTime = GetTickCount64();
            double actualDurationSeconds = (endTime - startTime) / 1000.0;

            // Get total operations and flops count
            long long ops = totalOperations.load();
            long long flops = totalFlops.load();

            // Calculate TOPS (Tera Operations Per Second) and GFLOPS (Giga Floating Point Operations Per Second)
            double tops = (ops / actualDurationSeconds) / 1e12;
            double gflops = (flops / actualDurationSeconds) / 1e9;

            SYSTEM_POWER_STATUS spsEnd;
            if (GetSystemPowerStatus(&spsEnd) && spsEnd.BatteryLifePercent != 255)
            {
                int drop = m_initialBatteryCPUPercent - spsEnd.BatteryLifePercent;
                double rate = drop / (m_cpuLoadDurationSeconds / 60.0);

                CString msg;
                msg.Format(L"Initial: %d%%\nCurrent: %d%%\nDrop: %d%%\nRate: %.2f%%/min\nTOPS: %.4f\nGFLOPS: %.3f",
                    m_initialBatteryCPUPercent, spsEnd.BatteryLifePercent, drop, rate, tops, gflops);

                // Save result for CSV (including TOPS and GFLOPS)
                m_cpuLoadTestCompleted = true;
                m_cpuLoadResult = msg;

                this->PostMessage(WM_APP + 1, 0, (LPARAM)new CString(msg));
            }
            else
            {
                CString msg = L"Cannot read battery percentage.";

                //Save failure too
                m_cpuLoadTestCompleted = true;
                m_cpuLoadResult = msg;

                this->PostMessage(WM_APP + 1, 0, (LPARAM)new CString(msg));
            }

            m_cpuLoadTestRunning = false;
        }).detach();
}

LRESULT CBatteryHelthDlg::OnCPULoadFinished(WPARAM wParam, LPARAM lParam)
{
    CString* pMsg = (CString*)lParam;
    SetDlgItemText(IDC_BATT_CPULOAD, *pMsg);
    delete pMsg; // free memory
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