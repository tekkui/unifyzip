//文字列に関する関数群

/*
日本語文字コード（Shift-JIS JIS第3、第4水準）の場合
第1バイトの範囲は 0x81 ～ 0x9F、0xE0 ～ 0xFC
第2バイトの範囲は 0x40 ～ 0xFC

ここで注目すべき点は
正しい文字列においては第1バイト、第2バイトはいずれも
制御コード（タブやスペース含む）や
一部の記号（!"#$%&'()*+,-./0123456789:;<=>?@）
と同じにならないということ
*/

#ifndef __STRFUNC_H__
#define __STRFUNC_H__

#include "types.h"

namespace KSDK {

LPCTSTR strchrex(LPCTSTR pszSrc, TCHAR c);
LPCTSTR strrchrex(LPCTSTR pszSrc, TCHAR c);
LPCTSTR strstrex(LPCTSTR pszString1, LPCTSTR pszString2);

// 文字列の中から文字列が最後に見つかった位置を返す
LPTSTR strrstrex(LPCTSTR pszString1, LPCTSTR pszString2);

// 0xFFだとかC言語の数値定数をintに変換
int StringToInteger(LPCTSTR psz);

// ２バイト文字の１バイト目かどうか判定する
// （IsDBCSLeadByte()で代用できるらしい）
inline bool IsKanji(TCHAR c)
{
#ifdef _UNICODE
	return false;
#else
	UINT8 n = c;
	return (0x81 <= n && n <= 0x9F) || (0xE0 <= n && n <= 0xFC);
#endif
}

// アルファベットかどうか判定する
// ライブラリの isalpha() は最上位ビットの値によっては挙動がおかしいので自作
inline bool IsAlpha(TCHAR c)
{
	return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

// 数字かどうか判定する
inline bool IsDigit(TCHAR c)
{
	return ('0' <= c && c <= '9');
}

// 英数字かどうか判定する
inline bool IsAlNum(TCHAR c)
{
	return (IsAlpha(c) || IsDigit(c));
}

bool IsUpper(LPCTSTR psz);
void MakeUpper(LPTSTR psz);

} // end of namespace KSDK

#endif //__STRFUNC_H__