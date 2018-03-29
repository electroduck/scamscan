ScamScan: Automatically scan for scan for tech support scammer numbers.

Give it a list of commonly mistyped website URLs, and it will scrape the
pages that come up for both phone numbers and additional URLs, traversing
through the link tree while collecting numbers along the way.

Dependencies: CURL and PCRE.

To build: Compile scamscan.c, linking it with LIBCURL.LIB and PCRE.LIB.
	Make sure that <curl/curl.h> and <pcre/include/pcre.h> can be found,
	or modify scamscan.c to find the headers in the right way for your
	system.  Define the CURL_STATICLIB and PCRE_STATIC macros to ensure
	that the CURL and PCRE functions can be found.
If you are building on Windows, simply install Open Watcom and run build.bat,
then copy PCRE.DLL and LIBCURL.DLL into the same folder as scamscan.exe.

To customize: Check out the macros and constants at the top of scamscan.c.

To run: Enter the desired URLs into list.txt (or whatever you changed
	SSC_LIST_PATH to) and run the scamscan executable.  Also, on Windows,
	the installer gives Start Menu and Desktop shortcuts for editing the list
	and running the program.

To build installer: After building scamscan (including moving the DLLs),
	compile inst.nsi with NSIS.  The installer is Windows only.

To Do list:
 - Change URL scraping over to a regex system
 - Change banned URL detection over to a regex system
 - Add Linux build script (Makefile)
 - Add MSVC build script (and/or VS solution)
 - Decrease false-positive rate on phone numbers by requiring some spacing
 - Find a way to automatically look for possible redirector sites
