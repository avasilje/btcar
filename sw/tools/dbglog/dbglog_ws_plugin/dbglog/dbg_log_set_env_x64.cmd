set CYGWIN=nodosfilewarning
set WIRESHARK_BASE_DIR=C:\MyWorks
set WIRESHARK_TARGET_PLATFORM=win64
set QT5_BASE_DIR=C:\Qt\5.3\msvc2013_64

set WIRESHARK_VERSION_EXTRA=-dbglog_edition
set INCLUDE=%INCLUDE%;C:\Program Files (x86)\Microsoft SDKs\Windows\v7.1A\Include
cd  C:\MyWorks\BTCar\sw\tools\dbglog
%comspec% /C ""C:\Program Files (x86)\Microsoft Visual Studio 12.0\VC\vcvarsall.bat"" amd64

cmd /K ""C:\Program Files (x86)\Microsoft Visual Studio 12.0\Common7\IDE\devenv.exe""