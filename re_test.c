#include <stdio.h>
#include <string.h>
#include "pcre/include/pcre.h"

// unescaped: \D(\+?1?\s?\(?[2-9][0-8]\d\)?\s?-?\s?[2-9]\d\d\s?-?\s?\d\d\d\d)\D
const char* TEST_REGEX =
"\\D(\\+?1?\\s?\\(?[2-9][0-8]\\d\\)?\\s?-?\\s?[2-9]\\d\\d\\s?-?\\s?\\d\\d\\d\\d)\\D";
const char* TEST_DATA = "<html><head><title>zeus wirus</title></head>"\
	"<body style='background-color:blue;color:white;font-family:monospace;'>"\
	"<h1>YOUR PC HAS OF THE ZEUS WIRUS</h1>"\
	"<p>0x34952903458486045 ERROR ZEUS WIRUS DETECT</p>"\
	"<p>LEVIS LOCKER ALSO PRESENT ON SYSTEM</p>"\
	"<p>CALL MICROSOFT CERTIFIED TEKNEEKAL SUPPOWT: 1 234 567 8901</p>"\
	"<p>24/7 SUPPOWT GROUP: +1 (567)-890-1234</p>"\
	"<p>SUPPOWT FOR BUSYNESS: 2234567890</p>"\
	"<p>WE DO NOT OFFER SUPPOWT FOR WEE M WARES OR WIRTUAL BOXES.</p>"\
	"</body></html>";

#define REGEX_OUT_MAX 30
#define REGEX_MATCH_MAX 64
	
int main(int argc, char** argv) {
	pcre* prxToTest;
	int iRegexOutput[REGEX_OUT_MAX];
	char* pszRegexError;
	int iRegexErrorLoc, nMatchedThisRound, i;
	char szMatchedData[REGEX_MATCH_MAX];
	const char* pszTestDataCur;
	
	pszTestDataCur = TEST_DATA;
	
	printf("Regex string: %s\n", TEST_REGEX);
	printf("Test data: %s\n", TEST_DATA);
	
	prxToTest = pcre_compile(TEST_REGEX, PCRE_CASELESS,
		(char const **)&pszRegexError, &iRegexErrorLoc, NULL);
	if(!prxToTest) {
		printf("Regex compile failed: %s\n", pszRegexError);
		printf("Error at position %d in regex string.\n", iRegexErrorLoc);
		return 1;
	}
	puts("Regex compiled.");
	
	while(nMatchedThisRound = pcre_exec(prxToTest, NULL, pszTestDataCur,
		strlen(pszTestDataCur), 0, 0, iRegexOutput, REGEX_OUT_MAX)) {
		if(nMatchedThisRound < 2) break;
		pcre_copy_substring(pszTestDataCur, iRegexOutput, 2, 1, szMatchedData,
			REGEX_MATCH_MAX);
		if(!strlen(szMatchedData)) break;
		printf("Matched data: %s\n", szMatchedData);
		pszTestDataCur += iRegexOutput[1];
	}
	
	return 0;
}
