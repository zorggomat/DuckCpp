# DuckCpp
This is multifunctional keylogger using WinAPI and libcurl.
It captures all pressed keys using low-level keyboard hook and sends it to email address via libcurl.
If sending fail, it writes log to the file.
DuckCpp copies itself to disk and adds copy to autorun if this is enabled in the settings.
