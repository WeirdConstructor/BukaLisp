cd build
IF NOT DEFINED VCINSTALLDIR call "%VS140COMNTOOLS%\..\..\VC\vcvarsall.bat" amd64_x86
C:\local\cmake\bin\cmake.exe ..
C:\local\cmake\bin\cmake.exe --build . --config Debug
set LOCAL_ERR=%ERRORLEVEL%
cd ..
"C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\gflags.exe" -p /enable bklisp.exe /full
REM cmd /C "C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\windbg.exe" bklisp.exe -i tests\c2g_tests.bkl
REM cmd /C "C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\windbg.exe" bklisp.exe -b etst.bkl
cmd /C "C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\windbg.exe" bklisp.exe apps\retrodb\retrodb_webservice.bkl
cmd /c exit %LOCAL_ERR%
