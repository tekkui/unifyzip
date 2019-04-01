#include "StrFunc.h"

#include <tchar.h>

namespace KSDK {

// ２バイト文字を含む文字列の中から指定の文字を検索、位置を返す。
LPCTSTR strchrex(LPCTSTR pszSrc, TCHAR c)
{
#ifdef _UNICODE
	// Unicode だとライブラリ関数が使える
	return _tcschr(pszSrc, c);
#else
	for ( ; *pszSrc; pszSrc++) {
		if (IsKanji(*pszSrc)) {
			pszSrc++;
			continue;
		}
		if (*pszSrc == c) {
			return (char*)pszSrc;
		}
	}
	return NULL;
#endif
}



// ２バイト文字を含む文字列の中から指定の文字を後ろから検索、位置を返す。
LPCTSTR strrchrex(LPCTSTR pszSrc, TCHAR c)
{
	// しかし、実際は後ろから探すことは不可能
	// 2バイト文字の1バイト目は2バイト文字の2バイト目にも使われることがあるので。
	// よって、前方から検索していき、最後に見つかったのを返すしかない。
#ifdef _UNICODE
	// Unicode だとライブラリ関数が使える
	return _tcsrchr(pszSrc, c);
#else
	LPTSTR pszHit = NULL;
	for ( ; *pszSrc; pszSrc++) {
		if (IsKanji(*pszSrc)) {
			pszSrc++;
			continue;
		}
		if (*pszSrc == c) {
			pszHit = (LPTSTR)pszSrc;
		}
	}
	return pszHit;
#endif
}



// ２バイト文字を含む文字列の中から指定の文字列を検索、位置を返す。
LPCTSTR strstrex(LPCTSTR pszString1, LPCTSTR pszString2) {
#ifdef _UNICODE
	// Unicode だとライブラリ関数が使える
	return _tcsstr(pszString1, pszString2);
#else
	int nLen1 = (int)_tcslen(pszString1);
	int nLen2 = (int)_tcslen(pszString2);
	int nSub = nLen1-nLen2;

	for(int i=0; i<=nSub; i++){
		if(strncmp(pszString1+i,pszString2,nLen2) == 0){
			return (char*)(pszString1+i);
		}else{
			if(IsKanji(*(pszString1+i))) i++;
		}
	}
	return NULL;
#endif
}



// 文字列の中から文字列が最後に見つかった位置を返す
LPTSTR strrstrex(LPCTSTR pszString1, LPCTSTR pszString2) {
	// しかし、実際は後ろから探すことは不可能
	// 2バイト文字の1バイト目は2バイト文字の2バイト目にも使われることがあるので。
	// よって、前方から検索していき、最後に見つかったのを返すしかない。
	LPTSTR  pszHit = NULL;
	for ( ; *pszString1; pszString1++) {
		if (*pszString1 == *pszString2 && _tcscmp(pszString1, pszString2) == 0) {
			pszHit = (LPTSTR)pszString1;
		}
		if (IsKanji(*pszString1)) pszString1++;
	}
	return pszHit;
}



// 0xFFだとかC言語の数値定数をintに変換
int StringToInteger(LPCTSTR psz)
{
	TCHAR c;
	int n;

	n = 0;
	c = *psz;

	if(!IsDigit(c)) return 0;

	// 定数
	if(c == _TCHAR('0')){
		// 0 か 8進数 か 16進数
		
		c = *(psz+1);
		if(_TCHAR('0') <= c && c <= _TCHAR('7')){
			for(;;){
				c = *psz++;
				if(_TCHAR('0') <= c && c <= _TCHAR('7')){
					n *= 8;
					n += (c - _TCHAR('0'));
				}else{
					return n;
				}
			}
		}else if(c == _TCHAR('x') || c == _TCHAR('X')){
			psz+=2;// pszは0xの次をさす
			for(;;){
				c = *psz++;
				if(isxdigit(c)){
					n *= 16;
					if(IsDigit(c)){
						n += (c - _TCHAR('0'));
					}else{
						if(_istupper(c)){
							n += ((c - _TCHAR('A')) + 10);
						}else{
							n += ((c - _TCHAR('a')) + 10);
						}
					}
				}else{
					return n;
				}
			}
		}else{
			return 0;
		}
	}else{
		// 10進数
		for(;;){
			c = *psz++;
			if(IsDigit(c)){
				n *= 10;
				n += (c - _TCHAR('0'));
			}else{
				return n;
			}
		}
	}
}

// 文字列の最初の文字が小文字かどうかチェックする
// 全角のアルファベットもチェックする
// Shift-JIS用
bool IsLower(LPCTSTR psz)
{
	TCHAR c;
	c = *psz;

#ifdef _UNICODE
	return (_TCHAR('a') <= c && c <= _TCHAR('z')) || (_TCHAR('ａ') <= c && c <= _TCHAR('ｚ'));
#else
	if(IsKanji(c)){
		if(c == 0x82){
			c = *(psz + 1);
			return (0x81 <= c && c <= 0x9a);
		}
		return false;
	}else{
		return ('a' <= c && c <= 'z');
	}
#endif
}

// 要動作確認
void MakeUpper(LPTSTR psz)
{
	for(;;){
		if(*psz == '\0') return;
		if(IsLower(psz)){
#ifdef _UNICODE
			if (_TCHAR('a') <= (*psz) && (*psz) <= _TCHAR('z')) {
				*(++psz) += (_TCHAR('A') - _TCHAR('a'));
			} else if (_TCHAR('ａ') <= (*psz) && (*psz) <= _TCHAR('ｚ')) {
				*(++psz) += (_TCHAR('Ａ') - _TCHAR('ａ'));
			}
#else
			if(IsKanji(*psz)){
				*(++psz) += (0x81 - 0x60);
			}else{
				*psz += ('A' - 'a');
			}
#endif
		}
		psz++;
	}
}

} // end of namespace KSDK



