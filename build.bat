@echo off

set "CCOPTS=%CCOPTS% -DCURL_STATICLIB -DPCRE_STATIC -DWATCOM -ecc"
set "LOPTS=%LOPTS% OPTION MAP"

REM echo.
REM echo *** Compiling regular expression system
REM wcc386 %CCOPTS% tiny-regex\re.c

echo.
echo *** Compiling regular expression test file
wcc386 %CCOPTS% re_test.c

echo.
echo *** Compiling ScamScan main file
wcc386 %CCOPTS% scamscan.c

echo.
echo *** Linking ScamScan
wlink SYSTEM NT %LOPTS% FILE scamscan LIB curl\bin\libcurl.lib,pcre\lib\pcre.lib

echo.
echo *** Linking regular expression test program
wlink SYSTEM NT %LOPTS% FILE re_test LIB pcre\lib\pcre.lib

pause
