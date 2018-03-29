#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "curl/curl.h"
#include "pcre/include/pcre.h"

#ifdef __GNUC__
	#include <alloca.h>
#else
	#ifdef WATCOM
		#include <malloc.h>
	#else
		#define NO_ALLOCA
	#endif
#endif

/* Maximum depth of scraped links to follow */
#define SSC_DEPTH_MAX 3

/* Maximum number of redirects to follow (per page) */
#define SSC_REDIRECTS_MAX 10

/* Cookies will be stored if this is defined */
#define SSC_ENABLE_COOKIES

/* The first character of almost any HTML webpage */
#define SSC_HTML_MAGIC '<'

/* Only HTML pages will be scanned if this is defined */
#define SSC_HTML_ONLY

/* Character indicating level of link recursion */
#define SSC_LEVEL_INDICATOR '|'

/* The maximum length of a domain name */
#define SSC_DOMAIN_MAX 255

/* User Agent string - this is IE11's when it's in compatibility mode */
const char* SSC_USERAGENT = 
	"Mozilla/5.0 (compatible, MSIE 11, Windows NT 6.3; Trident/7.0; rv:11.0)"\
	" like Gecko";
/* Some sites block IE11. Random, unknown User Agents are rarely blocked. */
//const char* SSC_USERAGENT = "Chutiya/Rundi 12.34";

/* Name of the URL list file */
const char* SSC_LIST_PATH = "list.txt";

/* Name of the cookie-jar file (delete to clear cookies) */
const char* SSC_COOKIEJAR_PATH = "cookies.dat";

/* Temporary downloads folder */
const char* SSC_FOLDER = "tempdl/";

/* This is what URLs begin with */
const char* SSC_URLBEGIN = "http://";

/* These characters can mark the end of a URL */
const char* SSC_URLEND = "\"'<>[]() \t\n"; 

/* URLs with this string in them will not be scanned */
const char* SSC_BANNED = "www.w3"; // Prevents scraping of standards pages

/* Regular Expression matching "US" phone numbers */
const char* SSC_REGEX_NUMBER =
"\\D(\\+?1?\\s?\\(?[2-9][0-8]\\d\\)?\\s?-?\\s?[2-9]\\d\\d\\s?-?\\s?\\d\\d\\d\\d)\\D";

/* Regular Expression matching banned numbers */
const char* SSC_REGEX_NUMBER_BANNED = "2147483647|2147483648|4294967295";
				    /*  INT_MAX   INT_MAX+1   UINT_MAX  */

/* Regular Expression to extract domain names from URLs */
const char* SSC_REGEX_DOMAIN = "(?:^|\\s)\\w+:\\/\\/([^\\/]+)\\/";

typedef struct _sscListItem {
	char* pszData;
	struct _sscListItem* pliNext;
} SSCLISTITEM;

void __fastcall SSCTrimEnd(char* str);
void __fastcall SSCMallocFail(size_t nBytes);
void __fastcall SSCListPrint(SSCLISTITEM* pliHead, const char* pszPrefix);
char* __fastcall SSCDownloadPage(const char* pszUrl, CURL* pcCurlObj,
	const char* pszPrefix);
void __fastcall SSCCheckCurlCode(CURLcode c);
size_t __cdecl SSCFWriteCDecl(const void* ptr, size_t size, size_t count,
	FILE* stream);
SSCLISTITEM* __fastcall SSCScrapeURLs(const char* data);
char* __fastcall SSCReadAllText(const char* pszFileName);
void __fastcall SSCListDestroy(SSCLISTITEM* pliHead);
void __fastcall SSCScanList(SSCLISTITEM* pliURLsToScan, size_t nLevelsDown,
	CURL* pcCurlObj, SSCLISTITEM** ppliNumberListTail);
void __fastcall SSCScrapeNumbers(const char* pszData, const char* pszPrefix,
	SSCLISTITEM** ppliNumberListTail, const char* pszSourceNote);
void __fastcall SSCExtractDomain(const char* pszURL, char* pszOutBuf,
	size_t nOutBufBytes);
void __fastcall SSCRegexFail(const char* pszErrorDesc, int iErrorLoc,
	const char* pszRegexString);

char g_szCurlErrString[CURL_ERROR_SIZE] = { 0 };
pcre* g_prxNumberRegex = NULL;
pcre* g_prxNumberBan = NULL;
pcre* g_prxDomain = NULL;

int main(int argc, char** argv) {
	FILE* pfURLList;
	SSCLISTITEM liURLListHead = { .pszData = NULL, .pliNext = NULL };
	SSCLISTITEM* pliURLListTail;
	SSCLISTITEM* pliNewItem;
	SSCLISTITEM* pliCurItem;
	char szBuf[2048];
	char* pszBufEnd;
	size_t nBytes;
	CURL* pcCurlObj;
	SSCLISTITEM liNumberListHead = { .pszData = NULL, .pliNext = NULL };
	SSCLISTITEM* pliNumberListTail;
	
	pliURLListTail = &liURLListHead;
	pliNumberListTail = &liNumberListHead;
	
	if(curl_global_init(CURL_GLOBAL_DEFAULT)) {
		puts("Failed to initialize CURL.");
		return 1;
	}
	
	pfURLList = fopen(SSC_LIST_PATH, "r");
	if(!pfURLList || ferror(pfURLList)) {
		perror("Unable to open indirect file list");
		printf("Make sure that %s exists in the current directory.\n",
			SSC_LIST_PATH);
		return 2;
	}
	
	// Read links file
	puts("Adding links...");
	while(!ferror(pfURLList) && !feof(pfURLList)) {
		memset(szBuf, 0, sizeof(szBuf));
		fgets(szBuf, sizeof(szBuf), pfURLList);
		nBytes = strlen(szBuf) + 1;
		
		if(nBytes < 2) {
			puts("\tSkipping blank line.");
			continue;
		}
		
		if(nBytes < sizeof(szBuf)) {
			if(szBuf[nBytes - 2] == '\n')
				szBuf[nBytes - 2] = 0;
		} else
			while(fgetc(pfURLList) != '\n');
		
		SSCTrimEnd(szBuf);
		
		printf("\t%s ", szBuf);
		
		pliNewItem = calloc(sizeof(SSCLISTITEM), 1);
		if(!pliNewItem) SSCMallocFail(sizeof(SSCLISTITEM));
		
		nBytes = strlen(szBuf) + 1;
		pliNewItem->pszData = malloc(nBytes);
		if(!pliNewItem->pszData) SSCMallocFail(nBytes);
		memcpy(pliNewItem->pszData, szBuf, nBytes);
		
		pliURLListTail->pliNext = pliNewItem;
		pliURLListTail = pliNewItem;
		
		puts("ADDED");
	}
	puts("");
	
	// Initialize CURL easy mode
	pcCurlObj = curl_easy_init();
	if(!pcCurlObj) {
		puts("Failed to create CURL object.");
		return 3;
	}
	
	// Set options
	curl_easy_setopt(pcCurlObj, CURLOPT_ERRORBUFFER, g_szCurlErrString);
	SSCCheckCurlCode(curl_easy_setopt(pcCurlObj, CURLOPT_FOLLOWLOCATION, 1));
	SSCCheckCurlCode(curl_easy_setopt(pcCurlObj, CURLOPT_MAXREDIRS,
		SSC_REDIRECTS_MAX));
	SSCCheckCurlCode(curl_easy_setopt(pcCurlObj, CURLOPT_USERAGENT,
		SSC_USERAGENT));
	SSCCheckCurlCode(curl_easy_setopt(pcCurlObj, CURLOPT_WRITEFUNCTION,
		SSCFWriteCDecl));
	#ifdef SSC_ENABLE_COOKIES
	SSCCheckCurlCode(curl_easy_setopt(pcCurlObj, CURLOPT_COOKIEJAR,
		SSC_COOKIEJAR_PATH));
	SSCCheckCurlCode(curl_easy_setopt(pcCurlObj, CURLOPT_COOKIEFILE,
		SSC_COOKIEJAR_PATH));
	#endif
	
	// Scan
	SSCScanList(liURLListHead.pliNext, 0, pcCurlObj, &pliNumberListTail);
	puts("Done scanning.");
	puts("");
	
	// Stop CURL easy mode
	curl_easy_cleanup(pcCurlObj);
	
	// Print numbers found
	if(liNumberListHead.pliNext) {
		puts("Possible numbers found: ");
		SSCListPrint(liNumberListHead.pliNext, " => ");
	} else
		puts("No possible numbers found.");
	
	return 0;
}

void __fastcall SSCTrimEnd(char* str) {
	str += strlen(str);
	while(isspace(*str)) {
		*str = 0;
		str--;
	}
}

void __fastcall SSCMallocFail(size_t nBytes) {
	printf("\nFailed to allocate %u bytes. Exiting.\n", nBytes);
	exit(0xAAAA);
}

void __fastcall SSCListPrint(SSCLISTITEM* pliCur, const char* pszPrefix) {
	while(pliCur) {
		printf("%s", pszPrefix);
		if(pliCur->pszData)
			puts(pliCur->pszData);
		else
			puts("[=NULL ELEMENT=]");
		
		pliCur = pliCur->pliNext;
	}
}

char* __fastcall SSCDownloadPage(const char* pszUrl, CURL* pcCurlObj,
	const char* pszPrefix) {
	char* pszTempPath;
	char* pszCur;
	size_t nBytes;
	size_t nFolderBytes;
	size_t nUrlBytes;
	FILE* pfTempFile;
	CURLcode ccDlCode;
	
	nFolderBytes = strlen(SSC_FOLDER);
	nUrlBytes = strlen(pszUrl);
	nBytes = nFolderBytes + nUrlBytes + 1;
	pszTempPath = malloc(nBytes);
	memcpy(pszTempPath, SSC_FOLDER, nFolderBytes);
	memcpy(pszTempPath + nFolderBytes, pszUrl, nUrlBytes + 1);
	
	pszCur = pszTempPath + nFolderBytes;
	while(*pszCur) {
		if(!isalnum(*pszCur)) *pszCur = '_';
		pszCur++;
	}
	
	pfTempFile = fopen(pszTempPath, "w");
	if(!pfTempFile || ferror(pfTempFile)) {
		printf("%sError opening %s.\n", pszPrefix, pszTempPath);
		perror(NULL);
		return NULL;
	}
	
	SSCCheckCurlCode(curl_easy_setopt(pcCurlObj, CURLOPT_URL, pszUrl));
	SSCCheckCurlCode(curl_easy_setopt(pcCurlObj, CURLOPT_WRITEDATA, 
		pfTempFile));
	
	printf("%sDownloading %s\n%s	 to %s\n", pszPrefix, pszUrl,
		pszPrefix, pszTempPath);
	
	SSCCheckCurlCode(ccDlCode = curl_easy_perform(pcCurlObj));
	if(ccDlCode == CURLE_OK)
		printf("%s	    Downloaded.\n", pszPrefix);
	
	if(!ferror(pfTempFile)) fclose(pfTempFile);
	
	return pszTempPath;
}

void __fastcall SSCCheckCurlCode(CURLcode ccErrCode) {
	char cFirstInputChar = 0;
	if(ccErrCode != CURLE_OK) {
		printf("CURL error %u: %s\n", ccErrCode, g_szCurlErrString);
		
		while(!((cFirstInputChar == 'Y') || (cFirstInputChar == 'N'))) {
			printf("Continue (y/n)? ");
			cFirstInputChar = toupper(getchar());
			while(getchar() != '\n');
		}
		
		if(cFirstInputChar == 'N')
			exit(0xBBBB);
	}
}

/* A verson of fwrite() that will always be __cdecl, for CURL to use.
   The standard library's fwrite() can vary in calling convention.    */
size_t __cdecl SSCFWriteCDecl(const void* ptr, size_t size, size_t count,
	FILE* stream) {
	return fwrite(ptr, size, count, stream);
}

SSCLISTITEM* __fastcall SSCScrapeURLs(const char* pszData) {
	char szUrlBuf[1024];
	const char* pszDataCur;
	char* pszUrlBufCur;
	SSCLISTITEM liListTail = { .pszData = NULL, .pliNext = NULL };
	SSCLISTITEM* pliListHead;
	SSCLISTITEM* pliNewItem;
	size_t nCharsCopied;
	
	#ifdef SSC_HTML_ONLY
	if(*pszData != SSC_HTML_MAGIC) return NULL;
	#endif
	
	pszDataCur = pszData;
	pliListHead = &liListTail;
	
	while(pszDataCur = strstr(pszDataCur, SSC_URLBEGIN)) {
		pszUrlBufCur = szUrlBuf;
		nCharsCopied = 0;
		while((!strchr(SSC_URLEND, *pszDataCur)) && (nCharsCopied < sizeof(szUrlBuf) - 1)) {
			*pszUrlBufCur = *pszDataCur;
			pszDataCur++;
			pszUrlBufCur++;
			nCharsCopied++;
		}
		*pszUrlBufCur = 0;
		
		if(!nCharsCopied) continue;
		
		if(strstr(szUrlBuf, SSC_BANNED)) continue;
		
		pliNewItem = calloc(sizeof(SSCLISTITEM), 1);
		if(!pliNewItem) SSCMallocFail(sizeof(SSCLISTITEM));
		
		nCharsCopied++; // null char
		pliNewItem->pszData = malloc(nCharsCopied);
		if(!pliNewItem->pszData) SSCMallocFail(nCharsCopied);
		memcpy(pliNewItem->pszData, szUrlBuf, nCharsCopied);
		
		pliListHead->pliNext = pliNewItem;
		pliListHead = pliNewItem;
	}
	
	return liListTail.pliNext;
}

char* __fastcall SSCReadAllText(const char* pszFileName) {
	FILE* pfToReadFrom;
	size_t nFileBytes, nRead;
	char* pszFileData;
	
	pfToReadFrom = fopen(pszFileName, "r");
	if(!pfToReadFrom || ferror(pfToReadFrom)) return NULL;
	
	if(fseek(pfToReadFrom, 0, SEEK_END)) {
		if(!ferror(pfToReadFrom)) fclose(pfToReadFrom);
		return NULL;
	}
	
	nFileBytes = (size_t)ftell(pfToReadFrom);
	
	pszFileData = calloc(1, nFileBytes);
	if(!pszFileData) {
		if(!ferror(pfToReadFrom)) fclose(pfToReadFrom);
		return NULL;
	}
	
	if(fseek(pfToReadFrom, 0, SEEK_SET)) {
		if(!ferror(pfToReadFrom)) fclose(pfToReadFrom);
		return NULL;
	}
	
	nRead = fread(pszFileData, 1, nFileBytes, pfToReadFrom);
	if(!ferror(pfToReadFrom)) fclose(pfToReadFrom);
	/*if(nRead != nFileBytes) {
		free(pszFileData);
		if(!ferror(pfToReadFrom)) fclose(pfToReadFrom);
		return NULL;
	}*/
	
	return pszFileData;
}

void __fastcall SSCListDestroy(SSCLISTITEM* pliCur) {
	SSCLISTITEM* pliNext;
	
	while(pliCur) {
		pliNext = pliCur->pliNext;
		free(pliCur->pszData);
		free(pliCur);
		pliCur = pliNext;
	}
}

void __fastcall SSCScanList(SSCLISTITEM* pliCurItem, size_t nLevelsDown,
	CURL* pcCurlObj, SSCLISTITEM** ppliNumberListTail) {
	char* pszDlFileName;
	char* pszDlFileData;
	SSCLISTITEM* pliNewURLList;
	size_t i;
	char* pszPrefix;
	char szDomain[SSC_DOMAIN_MAX];
	
	// Create prefix
	#ifdef NO_ALLOCA
	pszPrefix = malloc(nLevelsDown + 2);
	#else
	pszPrefix = alloca(nLevelsDown + 2);
	#endif
	if(!pszPrefix) SSCMallocFail(nLevelsDown + 2);
	memset(pszPrefix, SSC_LEVEL_INDICATOR, nLevelsDown);
	pszPrefix[nLevelsDown] = ' ';
	pszPrefix[nLevelsDown + 1] = 0;
	
	// Print links
	if(pliCurItem) {
		puts("URLs to scan: ");
		SSCListPrint(pliCurItem, pszPrefix);
	} else
		puts("No URLs to scan.");
	printf("");
	
	// Try to download
	while(pliCurItem) {
		if(pliCurItem->pszData) {
			SSCExtractDomain(pliCurItem->pszData, szDomain, SSC_DOMAIN_MAX);
			pszDlFileName = SSCDownloadPage(pliCurItem->pszData, pcCurlObj,
				pszPrefix);
			if(pszDlFileName) {
				printf("%sReading %s... ", pszPrefix, pszDlFileName);
				pszDlFileData = SSCReadAllText(pszDlFileName);
				if(pszDlFileData) {
					puts("File read.");
					printf("%sScanning for numbers...\n", pszPrefix);
					SSCScrapeNumbers(pszDlFileData, pszPrefix,
						ppliNumberListTail, szDomain);
					printf("%sScanning for URLs... ", pszPrefix);
					pliNewURLList = SSCScrapeURLs(pszDlFileData);
					if(pliNewURLList) {
						puts(" URLs found: ");
						SSCListPrint(pliNewURLList, pszPrefix);
						if(nLevelsDown < SSC_DEPTH_MAX)
							SSCScanList(pliNewURLList, nLevelsDown + 1,
								pcCurlObj, ppliNumberListTail);
						SSCListDestroy(pliNewURLList);
					} else
						puts("No URLs found.");
				} else
					puts("Unable to read file.");
			} else 
				printf("%s was not downloaded.\n", pliCurItem->pszData);
		}
			
		pliCurItem = pliCurItem->pliNext;
	}
	
	#ifdef NO_ALLOCA
	free(pszPrefix);
	#endif
}

#define SSC_NUMSCAN_REGOUTMAX 30
#define SSC_NUMSCAN_MATCHMAX 64
#define SSC_NUMSCAN_MAXNOTE 64
void __fastcall SSCScrapeNumbers(const char* pszData, const char* pszPrefix,
	SSCLISTITEM** ppliNumberListTail, const char* pszSourceNote) {
	const char* pszRegexError;
	int iRegexErrorLoc;
	int iRegexOutput[SSC_NUMSCAN_REGOUTMAX];
	int iBanRegexOutput[SSC_NUMSCAN_REGOUTMAX];
	char szMatchedData[SSC_NUMSCAN_MATCHMAX];
	SSCLISTITEM* pliNewItem;
	size_t nMatchedBytes, nOutputBytes, nNoteBytes;
	
	nNoteBytes = strlen(pszSourceNote);
	if(nNoteBytes > SSC_NUMSCAN_MAXNOTE)
		nNoteBytes = SSC_NUMSCAN_MAXNOTE;
	
	if(!g_prxNumberRegex) {
		g_prxNumberRegex = pcre_compile(SSC_REGEX_NUMBER, PCRE_CASELESS,
			(char const**)&pszRegexError, &iRegexErrorLoc, NULL);
		if(!g_prxNumberRegex)
			SSCRegexFail(pszRegexError, iRegexErrorLoc, SSC_REGEX_NUMBER);
	}
	
	if(!g_prxNumberBan) {
		g_prxNumberBan = pcre_compile(SSC_REGEX_NUMBER_BANNED, PCRE_CASELESS,
			(char const**)&pszRegexError, &iRegexErrorLoc, NULL);
		if(!g_prxNumberBan)
			SSCRegexFail(pszRegexError, iRegexErrorLoc,
				SSC_REGEX_NUMBER_BANNED);
	}
	
	while(pcre_exec(g_prxNumberRegex, NULL, pszData, strlen(pszData), 0, 0,
		iRegexOutput, SSC_NUMSCAN_REGOUTMAX) > 1) {
		pcre_copy_substring(pszData, iRegexOutput, 2, 1, szMatchedData,
			SSC_NUMSCAN_MATCHMAX);
		nMatchedBytes = strlen(szMatchedData);
		if(!nMatchedBytes) break;
		
		if(pcre_exec(g_prxNumberBan, NULL, szMatchedData, nMatchedBytes, 0, 0,
			iBanRegexOutput, SSC_NUMSCAN_REGOUTMAX) > 1) {
			printf("%sSKIPPED NUMBER: %s\n", pszPrefix, szMatchedData);
			continue;
		}
		
		printf("%sGOT NUMBER: %s\n", pszPrefix, szMatchedData);
		pszData += iRegexOutput[1];
		
		pliNewItem = calloc(sizeof(SSCLISTITEM), 1);
		
		nOutputBytes = nMatchedBytes + nNoteBytes + 1 + (nNoteBytes > 0 ? 3 : 0);
		pliNewItem->pszData = malloc(nOutputBytes);
		if(!pliNewItem->pszData) SSCMallocFail(nOutputBytes);
		snprintf(pliNewItem->pszData, nOutputBytes, "%s : %s", szMatchedData,
			pszSourceNote);
		
		(*ppliNumberListTail)->pliNext = pliNewItem;
		*ppliNumberListTail = pliNewItem;
	}
}

#define SSC_DOMEX_REGOUTMAX 15
void __fastcall SSCExtractDomain(const char* pszURL, char* pszOutBuf,
	size_t nOutBufBytes) {
	const char* pszRegexError;
	int iRegexErrorLoc;
	int iRegexOutput[SSC_DOMEX_REGOUTMAX];
	
	if(!g_prxDomain) {
		g_prxDomain = pcre_compile(SSC_REGEX_DOMAIN, PCRE_CASELESS,
			(char const**)&pszRegexError, &iRegexErrorLoc, NULL);
		if(!g_prxDomain)
			SSCRegexFail(pszRegexError, iRegexErrorLoc, SSC_REGEX_DOMAIN);
	}
	
	if(!nOutBufBytes) return;
	
	if(pcre_exec(g_prxDomain, NULL, pszURL, strlen(pszURL), 0, 0,
		iRegexOutput, SSC_NUMSCAN_REGOUTMAX) > 1) {
		pcre_copy_substring(pszURL, iRegexOutput, 2, 1, pszOutBuf,
			nOutBufBytes);
	} else
		pszOutBuf[0] = 0;
}

void __fastcall SSCRegexFail(const char* pszErrorDesc, int iErrorLoc,
	const char* pszRegexString) {
	printf("Regex compile failed: %s\n", pszErrorDesc);
	printf("Error at position %d in regex.\n", iErrorLoc);
	printf("Regex string: %s.\n", pszRegexString);
	exit(0x9999);
}




























