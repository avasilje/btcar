cmd /c dir
cd %WIRESHARK_BASE_DIR%\wireshark\plugins\dbglog
nmake SCD_DWARF_CFG=%1 /D /P -f Makefile.nmake all
xcopy /Y dbglog.dll %APPDATA%\Wireshark\plugins