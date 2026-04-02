
// BatteryHelth.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'pch.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols
#include <afxstr.h>


// CBatteryHelthApp:
// See BatteryHelth.cpp for the implementation of this class
//


struct BatteryReportData
{
    CString     designCapacity;
    CString     fullChargeCapacity;


    CString health;
    CString cycles;

    CString manufacturer;
    CString name;
    CString uuid;
    CString bid;

    CString status;
    CString percentage;
    CString remainingTime;
    CString     currentCapacity;
    CString voltage;
    CString temperature;


};

class CBatteryHelthApp : public CWinApp
{
public:
	CBatteryHelthApp();

// Overrides
public:
	virtual BOOL InitInstance();

// Implementation


	DECLARE_MESSAGE_MAP()

public:
	int ExitInstance();
};

extern CBatteryHelthApp theApp;
