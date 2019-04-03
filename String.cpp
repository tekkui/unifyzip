/*------------------------------------------------------------------------------
	File:	String.cpp
												2003.08.30 (c)YOSHIDA Kurima
================================================================================
------------------------------------------------------------------------------*/
#include "String.h"
#include "StrFunc.h"

#include <stdio.h>
#include <strsafe.h>
#include <tchar.h>

namespace KSDK {

const int MIN_BUF_LEN = 16;	// 最低メモリ確保量（文字数単位）
const int MAX_INT_LEN = 10;	// int(32bit)を文字列に直したときに必要なメモリ量（文字数単位）

//コンストラクタ
String::String() :	m_pszBuf(NULL), m_pszStr(NULL), m_utf8Str(NULL), m_ansiStr(NULL),
						m_nMaxBufLen(0), m_nBufLen(0), m_nStrLen(0)
{
	Reserve(MIN_BUF_LEN);
}

// 文字列で初期化
String::String(LPCTSTR pszString) :	m_pszBuf(NULL), m_pszStr(NULL), m_utf8Str(NULL), m_ansiStr(NULL),
										m_nMaxBufLen(0), m_nBufLen(0), m_nStrLen(0)
{
	Copy(pszString);
}

// コピーコンストラクタ
String::String(const String& another) :	m_pszBuf(NULL), m_pszStr(NULL), m_utf8Str(NULL), m_ansiStr(NULL),
											m_nMaxBufLen(0), m_nBufLen(0), m_nStrLen(0)
{
	Copy(another.c_str());
}

// デストラクタ
String::~String(){
	if(m_pszBuf != NULL) free(m_pszBuf);
	if(m_utf8Str != NULL) delete[] m_utf8Str;
	if(m_ansiStr != NULL) delete[] m_ansiStr;
}

// 文字列を代入
String& String::Copy(LPCTSTR pszString)
{
	int nLen;

	// 自分自身への代入はしない
	if(m_pszStr == pszString) return *this;
	
	nLen = lstrlen(pszString);

	Empty();

	if(nLen >= m_nBufLen) Reserve(nLen + 1);

	_tcscpy(m_pszStr, pszString);

	m_nStrLen = nLen;

	return *this;
}


// 数値を文字列に変換して代入
String& String::Copy(int n)
{
	Empty();

	if (MAX_INT_LEN >= m_nBufLen) Reserve(MAX_INT_LEN + 1);

	_itot(n, m_pszStr, 10);

	m_nStrLen = lstrlen(m_pszStr);

	return *this;
}

// 一文字代入
String& String::Copy(TCHAR c)
{
	Empty();

	if(1 >= m_nBufLen) Reserve(1 + 1);

	*m_pszStr = c;
	*(m_pszStr+1) = _TCHAR('\0');

	m_nStrLen = 1;

	return *this;
}

// 長さを指定して文字列を代入（終端に '\0' が付く）
String& String::NCopy(LPCTSTR pszString, int n)
{
	// 自分自身への代入はしない
	if(m_pszStr == pszString) return *this;

	Empty();

	// コピーする文字数
	int nLen = n;

	// 代入したい文字列の長さが指定長さより短ければそこまでを代入
	LPCTSTR p = pszString;
	for (int i=0; i<n; ++i) {
		if (*p == _TCHAR('\0')) {
			nLen = i;
			break;
		}

		// 次の文字へ
		// 2バイト文字には '\0' は含まれないであろうから
		// 特別扱いしない
		p++;
	}

	if (nLen >= m_nBufLen) {
		Reserve(nLen + 1);
	}

	memcpy(m_pszStr, pszString, sizeof(TCHAR) * nLen);
	*(m_pszStr + nLen) = _TCHAR('\0');

	m_nStrLen = nLen;

	return *this;
}

// 文字列を追加
String& String::Cat(LPCTSTR pszString)
{
	int nLen;
	nLen = lstrlen(pszString);

	if(m_nStrLen + nLen >= m_nBufLen) Reserve(m_nStrLen + nLen + 1);

	_tcscat(m_pszStr, pszString);

	m_nStrLen += nLen;

	return *this;
}

// 数値を文字列に変換して追加
String& String::Cat(int n)
{
	LPTSTR psz;

	if(m_nStrLen + MAX_INT_LEN >= m_nBufLen) Reserve(m_nStrLen + MAX_INT_LEN + 1);

	psz = m_pszStr + m_nStrLen;
	_itot(n, psz, 10);

	m_nStrLen += lstrlen(psz);

	return *this;
}

// 一文字追加
String& String::Cat(TCHAR c)
{
	LPTSTR psz;

	if(m_nStrLen + 1 >= m_nBufLen) Reserve(m_nStrLen + 1 + 1);

	psz = m_pszStr + m_nStrLen;
	*psz = c;
	*(psz+1) = _TCHAR('\0');

	m_nStrLen++;

	return *this;
}

// 長さを指定して文字列を追加
String& String::NCat(LPCTSTR pszString, int n)
{
	// コピーする文字数
	int nLen = n;

	// 追加したい文字列の長さが指定長さより短ければそこまでを追加
	LPCTSTR p = pszString;
	for (int i=0; i<n; ++i) {
		if (*p == _TCHAR('\0')) {
			nLen = i;
			break;
		}
		// 次の文字へ
		// 2バイト文字には '\0' は含まれないであろうから
		// 特別扱いしない
		p++;
	}

	if (m_nStrLen + nLen >= m_nBufLen) {
		Reserve(m_nStrLen + nLen + 1);
	}

	memcpy(m_pszStr + m_nStrLen, pszString, sizeof(TCHAR) * nLen);
	*(m_pszStr + m_nStrLen + nLen) = _TCHAR('\0');

	m_nStrLen += nLen;

	return *this;
}

// メモリ領域（m_nBufLen）を指定サイズ以上に拡大
// 文字列の移動は行わない
bool String::Reserve(int nSize)
{
	if(m_nBufLen >= nSize) return true;

	int n;
	int nOffset;

	if(m_pszBuf != NULL){
		// すでにメモリを確保している場合

		// メモリの確保量を決める
		nOffset = (int)(m_pszStr - m_pszBuf);
		nSize += nOffset;
		n = m_nMaxBufLen;
		while(n < nSize) n*=2;
		if((m_pszBuf = (LPTSTR)realloc(m_pszBuf, sizeof(TCHAR) * n)) == NULL) return false;

		m_pszStr = m_pszBuf + nOffset;

		m_nMaxBufLen = n;
		m_nBufLen = n - nOffset;
	}else{
		// メモリ未確保の場合

		// メモリの確保量を決める
		n = MIN_BUF_LEN;
		while(n < nSize) n*=2;

		if((m_pszBuf = (LPTSTR)malloc(sizeof(TCHAR) * n)) == NULL) return false;

		// 空の文字列を設定
		m_pszStr = m_pszBuf;
		*m_pszStr = _TCHAR('\0');

		m_nMaxBufLen = n;
		m_nBufLen = n;
		m_nStrLen = 0;
	}
	return true;
}

void String::Empty(void)
{
	if(m_pszBuf != NULL){
		m_pszStr = m_pszBuf;
		*m_pszStr = _TCHAR('\0');
		m_nBufLen = m_nMaxBufLen;
		m_nStrLen = 0;
	}
}

// 終端文字を打って文字の長さを指定の長さ以下にする
int String::MakeLeft(int nCount)
{
	if(m_nStrLen <= nCount) return m_nStrLen;
	m_pszStr[nCount] = _TCHAR('\0');
	m_nStrLen = nCount;
	return m_nStrLen;
}

// 文字列の指定の位置を取り出す
// nStart = 0 だと文字の最初から取り出す
// 残った文字列の文字数を返す
int String::MakeMid(int nFirst)
{
	if(nFirst <= 0){
		return m_nStrLen;
	}else if(nFirst >= m_nStrLen){
		Empty();
		return m_nStrLen;
	}
	m_pszStr += nFirst;
	m_nBufLen -= nFirst;
	m_nStrLen -= nFirst;
	return m_nStrLen;
}

// 文字列の指定の位置を取り出す
// nStart = 0 だと文字の最初から取り出す
int String::MakeMid(int nFirst, int nCount)
{
	if (nCount < 0) nCount = 0;

	if(nFirst < 0){
		return m_nStrLen;
	}else if(nFirst == 0){
		return MakeLeft(nCount);
	}else if(nFirst >= m_nStrLen){
		Empty();
		return m_nStrLen;
	}
	if(nFirst + nCount >= m_nStrLen){
		return MakeMid(nFirst);
	}else{
		m_pszStr += nFirst;
		m_nBufLen -= nFirst;
		m_pszStr[nCount] = _TCHAR('\0');
		m_nStrLen = nCount;
	}
	return m_nStrLen;
}

// 大文字化
void String::MakeUpper()
{
	KSDK::MakeUpper(m_pszStr);
}

// 文字列の大文字半角アルファベットを小文字にする
void String::MakeLower()
{
	LPTSTR psz;
	TCHAR c;
	bool bKanji;

	psz = m_pszStr;
	bKanji = false;

	while((c = *psz) != _TCHAR('\0')){
		if(bKanji){
			bKanji = false;
		}else{
			if(IsKanji(c)){
				bKanji = true;
			}else if(c >= _TCHAR('A') && c <= _TCHAR('Z')){
				*psz = c + (_TCHAR('a') - _TCHAR('A'));
			}
		}
		psz++;
	}
}

// 置換
int String::Replace( TCHAR chOld, TCHAR chNew ) {
	LPCTSTR psz;
	int n = 0;
	psz = strchrex(m_pszStr, chOld);
	while (psz != NULL) {
		*(const_cast<LPTSTR>(psz)) = chNew;
		n++;
		psz = strchrex(psz + 1, chOld);
	}
	return n;
}

// 置換
int String::Replace(LPCTSTR pszOld, LPCTSTR pszNew) {
	int oldLen = lstrlen(pszOld);
	int newLen = lstrlen(pszNew);

	if (oldLen == newLen) {
		int i = 0;
		int start = 0;
		for (int pos = Find(pszOld, start); pos != -1; ++i, pos = Find(pszOld, start)) {
			if (pos == -1) break;
			lstrcpyn(m_pszStr + pos, pszNew, newLen + 1);
			start = pos + newLen;
		}
		return i;
	} else if (oldLen < newLen) {
		int i = 0;
		int start = 0;
		for (int pos = Find(pszOld, start); pos != -1; ++i, pos = Find(pszOld, start)) {
			lstrcpyn(m_pszStr + pos, pszNew, oldLen + 1);
			Insert(pos + oldLen, pszNew + oldLen);
			start = pos + newLen;
		}
		return i;
	} else {
		int i = 0;
		int start = 0;
		for (int pos = Find(pszOld, start); pos != -1; ++i, pos = Find(pszOld, start)) {
			lstrcpyn(m_pszStr + pos, pszNew, newLen + 1);
			memmove(m_pszStr + pos + newLen, 
				m_pszStr + pos + oldLen, 
				m_nStrLen - pos - oldLen + 1);
			m_nStrLen -= (oldLen - newLen);
			start = pos + newLen;
		}
		return i;
	}
}


// 挿入
int String::Insert( int nIndex, TCHAR ch )
{
	// nIndexが大きい場合はCat()
	if(nIndex >= m_nStrLen){
		Cat(ch);
		return m_nStrLen;
	}

	if(m_pszStr > m_pszBuf){
		memmove(m_pszStr-1, m_pszStr, (nIndex) * sizeof(TCHAR));
		m_pszStr--;
		m_nBufLen++;
	}else{
		Reserve(m_nStrLen + 1 + 1);
		memmove(m_pszStr + nIndex + 1, m_pszStr + nIndex, (m_nStrLen - nIndex + 1) * sizeof(TCHAR));
	}
	*(m_pszStr+nIndex) = ch;
	m_nStrLen++;

	return m_nStrLen;
}

int String::Insert( int nIndex, LPCTSTR pstr )
{
	// nIndexが大きい場合はCat()
	if(nIndex >= m_nStrLen){
		Cat(pstr);
		return m_nStrLen;
	}

	int n = lstrlen(pstr);
	int r = (int)(m_pszStr - m_pszBuf);
	int d = r - n;

	if(d>0){
		// 文字列の前にあるメモリで間に合う
		memmove(m_pszStr-n, m_pszStr, sizeof(TCHAR) * nIndex);
		m_pszStr-=n;
		m_nBufLen+=n;
	}else{
		// 文字列の前にあるメモリは使わない
		Reserve(m_nStrLen + n + 1);
		memmove(m_pszStr + nIndex + n, m_pszStr + nIndex, sizeof(TCHAR) * (m_nStrLen - nIndex + 1));
	}
	memcpy(m_pszStr + nIndex, pstr, sizeof(TCHAR) * n);
	m_nStrLen+=n;

	return m_nStrLen;
}

// 削除
int String::Delete( int nIndex, int nCount )
{
	// 何も削除しない
	if(nCount <= 0) return m_nStrLen;

	if(nIndex < 0){
		return m_nStrLen;
	}else if(nIndex == 0){
		return MakeMid(nCount);
	}

	if(nIndex + nCount >= m_nStrLen){
		return MakeLeft(nIndex);
	}else{
		memmove(m_pszStr + nIndex,
				m_pszStr + nIndex + nCount, 
				(m_nStrLen - (nIndex + nCount) + 1) * sizeof(TCHAR));
		m_nStrLen -= nCount;
	}
	return m_nStrLen;
}



#include <malloc.h>
/////////////////////////////////////////////////////////////////////////////
// String formatting

#define TCHAR_ARG   TCHAR
#define WCHAR_ARG   WCHAR
#define CHAR_ARG    char

#ifdef _X86_
	//#define DOUBLE_ARG  _AFX_DOUBLE
	#define DOUBLE_ARG  double
#else
	#define DOUBLE_ARG  double
#endif

#define FORCE_ANSI      0x10000
#define FORCE_UNICODE   0x20000
#define FORCE_INT64     0x40000

void String::FormatV(LPCTSTR lpszFormat, va_list argList)
{
	//ASSERT(AfxIsValidString(lpszFormat));

	va_list argListSave = argList;

	// make a guess at the maximum length of the resulting string
	int nMaxLen = 0;
	for (LPCTSTR lpsz = lpszFormat; *lpsz != '\0'; lpsz = _tcsinc(lpsz))
	{
		// handle '%' character, but watch out for '%%'
		if (*lpsz != '%' || *(lpsz = _tcsinc(lpsz)) == '%')
		{
			nMaxLen += (int)_tclen(lpsz);
			continue;
		}

		int nItemLen = 0;

		// handle '%' character with format
		int nWidth = 0;
		for (; *lpsz != '\0'; lpsz = _tcsinc(lpsz))
		{
			// check for valid flags
			if (*lpsz == '#')
				nMaxLen += 2;   // for '0x'
			else if (*lpsz == '*')
				nWidth = va_arg(argList, int);
			else if (*lpsz == '-' || *lpsz == '+' || *lpsz == '0' ||
				*lpsz == ' ')
				;
			else // hit non-flag character
				break;
		}
		// get width and skip it
		if (nWidth == 0)
		{
			// width indicated by
			nWidth = _ttoi(lpsz);
			for (; *lpsz != '\0' && _istdigit(*lpsz); lpsz = _tcsinc(lpsz))
				;
		}
		//ASSERT(nWidth >= 0);

		int nPrecision = 0;
		if (*lpsz == '.')
		{
			// skip past '.' separator (width.precision)
			lpsz = _tcsinc(lpsz);

			// get precision and skip it
			if (*lpsz == '*')
			{
				nPrecision = va_arg(argList, int);
				lpsz = _tcsinc(lpsz);
			}
			else
			{
				nPrecision = _ttoi(lpsz);
				for (; *lpsz != '\0' && _istdigit(*lpsz); lpsz = _tcsinc(lpsz))
					;
			}
			//ASSERT(nPrecision >= 0);
		}

		// should be on type modifier or specifier
		int nModifier = 0;
		if (_tcsncmp(lpsz, _T("I64"), 3) == 0)
		{
			lpsz += 3;
			nModifier = FORCE_INT64;
#if !defined(_X86_) && !defined(_ALPHA_)
			// __int64 is only available on X86 and ALPHA platforms
			//ASSERT(FALSE);
#endif
		}
		else
		{
			switch (*lpsz)
			{
			// modifiers that affect size
			case 'h':
				nModifier = FORCE_ANSI;
				lpsz = _tcsinc(lpsz);
				break;
			case 'l':
				nModifier = FORCE_UNICODE;
				lpsz = _tcsinc(lpsz);
				break;

			// modifiers that do not affect size
			case 'F':
			case 'N':
			case 'L':
				lpsz = _tcsinc(lpsz);
				break;
			}
		}

		// now should be on specifier
		switch (*lpsz | nModifier)
		{
		// single characters
		case 'c':
		case 'C':
			nItemLen = 2;
			va_arg(argList, TCHAR_ARG);
			break;
		case 'c'|FORCE_ANSI:
		case 'C'|FORCE_ANSI:
			nItemLen = 2;
			va_arg(argList, CHAR_ARG);
			break;
		case 'c'|FORCE_UNICODE:
		case 'C'|FORCE_UNICODE:
			nItemLen = 2;
			va_arg(argList, WCHAR_ARG);
			break;

		// strings
		case 's':
			{
				LPCTSTR pstrNextArg = va_arg(argList, LPCTSTR);
				if (pstrNextArg == NULL)
				   nItemLen = 6;  // "(null)"
				else
				{
				   nItemLen = lstrlen(pstrNextArg);
				   nItemLen = max(1, nItemLen);
				}
			}
			break;

		case 'S':
			{
#ifndef _UNICODE
				LPWSTR pstrNextArg = va_arg(argList, LPWSTR);
				if (pstrNextArg == NULL)
				   nItemLen = 6;  // "(null)"
				else
				{
				   nItemLen = (int)wcslen(pstrNextArg);
				   nItemLen = max(1, nItemLen);
				}
#else
				LPCSTR pstrNextArg = va_arg(argList, LPCSTR);
				if (pstrNextArg == NULL)
				   nItemLen = 6; // "(null)"
				else
				{
				   nItemLen = lstrlenA(pstrNextArg);
				   nItemLen = max(1, nItemLen);
				}
#endif
			}
			break;

		case 's'|FORCE_ANSI:
		case 'S'|FORCE_ANSI:
			{
				LPCSTR pstrNextArg = va_arg(argList, LPCSTR);
				if (pstrNextArg == NULL)
				   nItemLen = 6; // "(null)"
				else
				{
				   nItemLen = lstrlenA(pstrNextArg);
				   nItemLen = max(1, nItemLen);
				}
			}
			break;

		case 's'|FORCE_UNICODE:
		case 'S'|FORCE_UNICODE:
			{
				LPWSTR pstrNextArg = va_arg(argList, LPWSTR);
				if (pstrNextArg == NULL)
				   nItemLen = 6; // "(null)"
				else
				{
				   nItemLen = (int)wcslen(pstrNextArg);
				   nItemLen = max(1, nItemLen);
				}
			}
			break;
		}

		// adjust nItemLen for strings
		if (nItemLen != 0)
		{
			if (nPrecision != 0)
				nItemLen = min(nItemLen, nPrecision);
			nItemLen = max(nItemLen, nWidth);
		}
		else
		{
			switch (*lpsz)
			{
			// integers
			case 'd':
			case 'i':
			case 'u':
			case 'x':
			case 'X':
			case 'o':
				if (nModifier & FORCE_INT64)
					va_arg(argList, __int64);
				else
					va_arg(argList, int);
				nItemLen = 32;
				nItemLen = max(nItemLen, nWidth+nPrecision);
				break;

			case 'e':
			case 'g':
			case 'G':
				va_arg(argList, DOUBLE_ARG);
				nItemLen = 128;
				nItemLen = max(nItemLen, nWidth+nPrecision);
				break;

			case 'f':
				{
					double f;
					LPTSTR pszTemp;

					// 312 == strlen("-1+(309 zeroes).")
					// 309 zeroes == max precision of a double
					// 6 == adjustment in case precision is not specified,
					//   which means that the precision defaults to 6
					pszTemp = (LPTSTR)_alloca(max(nWidth, 312+nPrecision+6));

					f = va_arg(argList, double);
					_stprintf( pszTemp, _T( "%*.*f" ), nWidth, nPrecision+6, f );
					nItemLen = lstrlen(pszTemp);
				}
				break;

			case 'p':
				va_arg(argList, void*);
				nItemLen = 32;
				nItemLen = max(nItemLen, nWidth+nPrecision);
				break;

			// no output
			case 'n':
				va_arg(argList, int*);
				break;

			//default:
				//ASSERT(FALSE);  // unknown formatting option
			}
		}

		// adjust nMaxLen for output nItemLen
		nMaxLen += nItemLen;
	}

	//GetBuffer(nMaxLen);
	//VERIFY(_vstprintf(m_pchData, lpszFormat, argListSave) <= GetAllocLength());
	//ReleaseBuffer();

// ↓加えました
	Reserve(nMaxLen + 1);
	_vstprintf(m_pszStr, lpszFormat, argListSave);
	m_nStrLen = lstrlen(m_pszStr);
// ↑ここまで

	va_end(argListSave);
}

// formatting (using wsprintf style formatting)
void /*AFX_CDECL*/ String::Format(LPCTSTR lpszFormat, ...)
{
	//ASSERT(AfxIsValidString(lpszFormat));

	va_list argList;
	va_start(argList, lpszFormat);
	FormatV(lpszFormat, argList);
	va_end(argList);
}
/*
void AFX_CDECL String::Format(UINT nFormatID, ...)
{
	String strFormat;
	VERIFY(strFormat.LoadString(nFormatID) != 0);

	va_list argList;
	va_start(argList, nFormatID);
	FormatV(strFormat, argList);
	va_end(argList);
}

// formatting (using FormatMessage style formatting)
void AFX_CDECL String::FormatMessage(LPCTSTR lpszFormat, ...)
{
	// format message into temporary buffer lpszTemp
	va_list argList;
	va_start(argList, lpszFormat);
	LPTSTR lpszTemp;

	if (::FormatMessage(FORMAT_MESSAGE_FROM_STRING|FORMAT_MESSAGE_ALLOCATE_BUFFER,
		lpszFormat, 0, 0, (LPTSTR)&lpszTemp, 0, &argList) == 0 ||
		lpszTemp == NULL)
	{
		AfxThrowMemoryException();
	}

	// assign lpszTemp into the resulting string and free the temporary
	*this = lpszTemp;
	LocalFree(lpszTemp);
	va_end(argList);
}

void AFX_CDECL String::FormatMessage(UINT nFormatID, ...)
{
	// get format string from string table
	String strFormat;
	VERIFY(strFormat.LoadString(nFormatID) != 0);

	// format message into temporary buffer lpszTemp
	va_list argList;
	va_start(argList, nFormatID);
	LPTSTR lpszTemp;
	if (::FormatMessage(FORMAT_MESSAGE_FROM_STRING|FORMAT_MESSAGE_ALLOCATE_BUFFER,
		strFormat, 0, 0, (LPTSTR)&lpszTemp, 0, &argList) == 0 ||
		lpszTemp == NULL)
	{
		AfxThrowMemoryException();
	}

	// assign lpszTemp into the resulting string and free lpszTemp
	*this = lpszTemp;
	LocalFree(lpszTemp);
	va_end(argList);
}
*/



// 左に指定の文字が続く限り削除
void String::TrimLeft( TCHAR chTarget )
{
	LPCTSTR psz;
	for (psz = m_pszStr ; *psz == chTarget; psz++) {;}
	Delete(0, (int)(psz-m_pszStr));
}

// 左に指定の文字が続く限り削除
void String::TrimLeft( LPCTSTR lpszTargets )
{
	LPCTSTR psz, pszt;
	TCHAR c, ct;
	
	for(psz = m_pszStr; ; psz++){
		c = *psz;
		if(c == _TCHAR('\0')){
			Delete(0, (int)(psz-m_pszStr));
			return;
		}else if(IsKanji(c)){
			for(pszt = lpszTargets; ; pszt++){
				ct = *pszt;
				if(ct == _TCHAR('\0')){
					Delete(0, (int)(psz-m_pszStr));
					return;
				}else if(IsKanji(ct)){
					if(_tcsncmp(psz, pszt, 2) == 0) break;
					pszt++;
				}
			}
			psz++;
		}else{
			for(pszt = lpszTargets; ; pszt++){
				ct = *pszt;
				if(ct == _TCHAR('\0')){
					Delete(0, (int)(psz-m_pszStr));
					return;
				}else if(IsKanji(ct)){
					pszt++;
				}else{
					if(c == ct) break;
				}
			}
		}
	}
}



void String::TrimRight( TCHAR chTarget )
{
	while(strrchrex(m_pszStr, chTarget) == m_pszStr + m_nStrLen - 1){
		MakeLeft(m_nStrLen-1);
	}
}

void String::TrimRight( LPCTSTR lpszTargets )
{
	LPCTSTR pszt;
	TCHAR ct;

	while(m_nStrLen > 0){
		for(pszt = lpszTargets; ; pszt++){
			ct = *pszt;
			if(ct == _TCHAR('\0')){
				return;
			}else if(IsKanji(ct)){
				TCHAR szChar[3];
				szChar[0] = *pszt++;
				szChar[1] = *pszt;
				szChar[2] = _TCHAR('\0');
				if(strrstrex(m_pszStr, szChar) == m_pszStr + m_nStrLen - 2){
					MakeLeft(m_nStrLen-2);
					break;
				}
			}else{
				if(strrchrex(m_pszStr, ct) == m_pszStr + m_nStrLen - 1){
					MakeLeft(m_nStrLen-1);
					break;
				}
			}
		}
	}
}

// 検索
int String::Find( TCHAR ch, int nStart) const
{
	LPCTSTR psz;
	if (nStart >= m_nStrLen - 1) return -1;
	psz = strchrex(m_pszStr + nStart, ch);
	if (psz == NULL) return -1;
	return (int)(psz - m_pszStr);
}

int String::Find( LPCTSTR lpszSub, int nStart ) const {
	LPCTSTR psz;
	if (nStart >= m_nStrLen - 1) return -1;
	psz = strstrex(m_pszStr + nStart, lpszSub);
	if (psz == NULL) return -1;
	return (int)(psz - m_pszStr);
}

// 検索
int String::ReverseFind( TCHAR ch) const
{
	LPCTSTR psz;
	psz = strrchrex(m_pszStr, ch);
	if (psz == NULL) return -1;
	return (int)(psz - m_pszStr);
}



int String::FindOneOf( LPCTSTR lpszCharSet ) const
{
	LPCTSTR psz, pszt;
	TCHAR c, ct;
	
	for(psz = m_pszStr; ; psz++){
		c = *psz;
		if(c == _TCHAR('\0')){
			return -1;
		}else if(IsKanji(c)){
			for(pszt = lpszCharSet; ; pszt++){
				ct = *pszt;
				if(ct == _TCHAR('\0')){
					break;
				}else if(IsKanji(ct)){
					if(_tcsncmp(psz, pszt, 2) == 0) {
						return (int)(psz - m_pszStr);
					}
					pszt++;
				}
			}
			psz++;
		}else{
			for(pszt = lpszCharSet; ; pszt++){
				ct = *pszt;
				if(ct == _TCHAR('\0')){
					break;
				}else if(IsKanji(ct)){
					pszt++;
				}else{
					if(c == ct) {
						return (int)(psz - m_pszStr);
					}
				}
			}
		}
	}
}

LPTSTR String::GetBuffer(int nMinBufLength) {
	Reserve(nMinBufLength+1);
	return m_pszStr;
}

void String::ReleaseBuffer(int nNewLength) {
	if (nNewLength == -1) {
		m_nStrLen = lstrlen(m_pszStr);
	} else {
		m_nStrLen = nNewLength;
		m_pszStr[m_nStrLen] = _TCHAR('\0');
	}
}

#if defined(_UNICODE)
// 文字列を代入
String& String::Utf8Copy(const char *utf8str)
{
	size_t utf8Size = 0;
	int utf8Length = 0;
	StringCchLengthA(utf8str, INT_MAX, &utf8Size);
	if (utf8Size != 0) {
		utf8Size++;
		utf8Length = static_cast<int>(utf8Size);
		int utf16Length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8str, utf8Length, NULL, 0);
		if (utf16Length != 0) {
			TCHAR *utf16str = new TCHAR[utf16Length];
			MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8str, utf8Length, utf16str, utf16Length);
			Copy(utf16str);
			delete[] utf16str;
		}
	}
	return *this;
}

LPCTSTR String::utf8_str(void) {
	int size = WideCharToMultiByte(CP_UTF8, 0, m_pszStr, -1, NULL, 0, NULL, NULL);
	if (size != 0) {
		m_utf8Str = new TCHAR[size];
		WideCharToMultiByte(CP_UTF8, 0, m_pszStr, -1, (LPSTR)m_utf8Str, size, NULL, NULL);
	}
	return m_utf8Str;
}

int String::GetUtf8Length(void) {
	return WideCharToMultiByte(CP_UTF8, 0, m_pszStr, -1, NULL, 0, NULL, NULL);
}

LPCSTR String::ansi_str(void) {
	int size = WideCharToMultiByte(CP_ACP, 0, m_pszStr, -1, NULL, 0, NULL, NULL);
	if (size != 0) {
		m_ansiStr = new char[size];
		WideCharToMultiByte(CP_ACP, 0, m_pszStr, -1, m_ansiStr, size, NULL, NULL);
	}
	return m_ansiStr;
}

int String::GetAnsiLength(void) {
	return WideCharToMultiByte(CP_ACP, 0, m_pszStr, -1, NULL, 0, NULL, NULL);
}
#endif

} // end of namespace KSDK
