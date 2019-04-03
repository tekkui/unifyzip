#include "Error.h"


#include <stdio.h>
#define ANSI            /* UNIX バージョンではコメント アウトしてください。 */
#ifdef ANSI             /* ANSI 互換バージョン */
#include <stdarg.h>
int average( int first, ... );
#else                   /* UNIX 互換バージョン */
#include <varargs.h>
int average( va_list );
#endif

#include <Windows.h>

#include <time.h>		// debug用に日付時刻の出力をするので

#include <tchar.h>



bool bOutputErrorlog = 
#ifdef _DEBUG
	true;  // デバッグモードではデフォルトでtrue
#else
	false; // リリースモードではデフォルトでfalse
#endif

TCHAR szErrorMessage[256];



void EnableOutputErrorlog(bool b)
{
	bOutputErrorlog = b;
}



void InnerLog(LPCTSTR szFormat, ...)
{
	TCHAR szDate[256];
	TCHAR szTime[256];
	TCHAR szText[256];

	_tstrdate(szDate);// 日付
	_tstrtime(szTime);// 時刻

	va_list marker;	// 可変個引数リストの先頭を示す
	va_start(marker, szFormat);
	_vstprintf(szText, szFormat, marker);
	va_end(marker);

	_stprintf(szErrorMessage,_T("%s %s %s\n"), szDate, szTime, szText);

	if (!bOutputErrorlog) return;

	//デバッガに文字を出力	
	OutputDebugString(szErrorMessage);
}



// エラーメッセージの表示
void SDKGetLastErrorMessage()
{
	// 文字列を表示する。
	MessageBox(NULL, szErrorMessage, _T("Error"), MB_OK | MB_ICONINFORMATION);
}



// エラーメッセージの表示
void GetLastErrorMssage()
{
	LPVOID lpMsgBuf;

	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // 既定の言語
		(LPTSTR) &lpMsgBuf,
		0,
		NULL
		);

	// 文字列を表示する。
	MessageBox(NULL, (LPCTSTR)lpMsgBuf, _T("Error"), MB_OK | MB_ICONINFORMATION);

	// バッファを解放する。
	LocalFree(lpMsgBuf);
}
