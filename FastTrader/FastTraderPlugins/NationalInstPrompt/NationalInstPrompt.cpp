// NationalInstPrompt.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "NationalInstPrompt.h"
#include "NationalInstPromptDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

HWND g_hWnd_FT=NULL;

// CNationalInstPromptApp

BEGIN_MESSAGE_MAP(CNationalInstPromptApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CNationalInstPromptApp construction

CNationalInstPromptApp::CNationalInstPromptApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}


// The one and only CNationalInstPromptApp object

CNationalInstPromptApp theApp;


// CNationalInstPromptApp initialization

BOOL CNationalInstPromptApp::InitInstance()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();

	AfxEnableControlContainer();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Local AppWizard-Generated Applications"));

    if(__argc>1)
    {
        g_hWnd_FT = (HWND)atol(__argv[1]);
        if(!::IsWindow(g_hWnd_FT))
            g_hWnd_FT=NULL;
    }

#ifdef _DEBUG
    {
        if(::MessageBox(NULL, 
            "����Ҫ��ʾ���No��ť��\n"
            "��Ҫ�������Yes��ť�������Ϊ������һ��ASSERT��Ȼ������\"����\"��\n"
            "Ȼ��ѡ�д�\"Module-BookOrder\"���̵�VC2008IDE����IDE��׽�����Ǻ�\n"
            "��ʾ\"Break\"����\"Continue\"������ѡ��\"Continue\"��", 
            "���� ��ʾ", 
            MB_YESNO|MB_ICONWARNING|MB_APPLMODAL)==IDYES) 
        {
            ASSERT(FALSE);
        }
    }
#endif


    CNationalInstPromptDlg dlg;
	m_pMainWnd = &dlg;
	INT_PTR nResponse = dlg.DoModal();
	if (nResponse == IDOK)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with OK
	}
	else if (nResponse == IDCANCEL)
	{
		// TODO: Place code here to handle when the dialog is
		//  dismissed with Cancel
	}

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}