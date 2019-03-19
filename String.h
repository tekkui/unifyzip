/*------------------------------------------------------------------------------
	File:	String.h
												2003.08.30 (c)YOSHIDA Kurima
================================================================================
 Content:
	自作文字列クラス
	
 Notes:
	MakeMid()を文字列の移動をせずに
	文字列の先頭のポインタを変える事でやっているので高速なのが特徴

	またUnicode・ANSI両対応であり
	SHIFT-JIS の２バイト目に関する対応もなされている

	長さの単位は全て文字数単位
	マルチバイト文字は2文字として扱われるので注意

	TODO:
		MakeLeft -> Leftのチェック
		format に Boost を用いる？



 Sample:

------------------------------------------------------------------------------*/

#ifndef __STRING_H__
#define __STRING_H__

#include <windows.h>
#include <memory.h>
#include <string.h>
#include "types.h"

namespace KSDK {

class String {
public:
	String();
	String(LPCTSTR pszSrc);
	String(const String& another);

	~String();

	String& Copy(LPCTSTR pszString);
	String& Copy(int n);
	String& Copy(TCHAR c);

	String& NCopy(LPCTSTR pszString, int n);

	String& Cat(LPCTSTR pszString);
	String& Cat(int n);
	String& Cat(TCHAR c);

	String& NCat(LPCTSTR pszString, int n);

	void Empty(void);
	bool Reserve(int nSize);

	LPCTSTR c_str(void) const {return (LPCTSTR)m_pszStr;}	// 文字列へのポインタを返す
	bool IsEmpty(void) const {return m_nStrLen == 0;}		// 文字列が空か
	int GetLength(void) const {return m_nStrLen;}			// 文字列長さ（文字数単位）

	// 文字配列
	TCHAR GetAt(int nIndex) const {return *(m_pszStr + nIndex);}

	// 抽出
	int MakeMid(int nFirst);
	int MakeMid(int nFirst, int nCount);
	int MakeLeft(int nCount);

	// その他の変換	
	void MakeUpper();// 大文字化
	void MakeLower();
	int Replace( TCHAR chOld, TCHAR chNew );	// 置換
	int Replace(LPCTSTR pszOld, LPCTSTR pszNew);
	int Insert( int nIndex, TCHAR ch );			// 挿入
	int Insert( int nIndex, LPCTSTR pstr );
	int Delete( int nIndex, int nCount = 1 );	// 削除
	void Format( LPCTSTR lpszFormat, ...);
	void FormatV( LPCTSTR lpszFormat, va_list argList);
	void TrimLeft( TCHAR chTarget );
	void TrimLeft( LPCTSTR lpszTargets );
	void TrimRight( TCHAR chTarget );
	void TrimRight( LPCTSTR lpszTargets );


	// 検索
	int Find( TCHAR ch, int nStart = 0) const;
	int Find( LPCTSTR lpszSub, int nStart = 0) const;
	int ReverseFind( TCHAR ch) const;
	int FindOneOf( LPCTSTR lpszCharSet ) const;

	// バッファ アクセス
	LPTSTR GetBuffer(int nMinBufLength);
	void ReleaseBuffer(int nNewLength = -1);

	// オペレータ

	// 自動変換
	//operator LPCTSTR() const {return (LPCTSTR)m_pszStr;}

	String& operator=(const String& String){return Copy(String.c_str());}
	String& operator=(LPCTSTR pszString){return Copy(pszString);}
	String& operator=(int n){return Copy(n);}
	String& operator=(TCHAR c){return Copy(c);}

	// 「[]」演算子
	TCHAR operator []( int nIndex ) const{return GetAt(nIndex);}

	// 「+=」演算子
	const String& operator +=( const String& String ){return Cat(String.c_str());}
	const String& operator +=( LPCTSTR pszString ){return Cat(pszString);}
	const String& operator +=( int n ){return Cat(n);}
	const String& operator +=( TCHAR ch ){return Cat(ch);}

	// 比較演算子
	bool operator==(LPCTSTR pszString ) const {return m_pszStr && lstrcmp( m_pszStr, pszString ) == 0; }
	bool operator!=(LPCTSTR pszString ) const {return m_pszStr && lstrcmp( m_pszStr, pszString ) != 0; }
	bool operator< (LPCTSTR pszString ) const {return m_pszStr && lstrcmp( m_pszStr, pszString ) < 0; }
	bool operator> (LPCTSTR pszString ) const {return m_pszStr && lstrcmp( m_pszStr, pszString ) > 0; }
	bool operator<=(LPCTSTR pszString ) const {return m_pszStr && lstrcmp( m_pszStr, pszString ) <= 0; }
	bool operator>=(LPCTSTR pszString ) const {return m_pszStr && lstrcmp( m_pszStr, pszString ) >= 0; }

	bool operator==(const String& String ) const {return m_pszStr && lstrcmp( m_pszStr, String.c_str() ) == 0;}
	bool operator!=(const String& String ) const {return m_pszStr && lstrcmp( m_pszStr, String.c_str() ) != 0;}
	bool operator< (const String& String ) const {return m_pszStr && lstrcmp( m_pszStr, String.c_str() ) < 0;}
	bool operator> (const String& String ) const {return m_pszStr && lstrcmp( m_pszStr, String.c_str() ) > 0;}
	bool operator<=(const String& String ) const {return m_pszStr && lstrcmp( m_pszStr, String.c_str() ) <= 0;}
	bool operator>=(const String& String ) const {return m_pszStr && lstrcmp( m_pszStr, String.c_str() ) >= 0;}


	// UNICODE
	String& Utf8Copy(const char * utf8str);
	LPCTSTR utf8_str(void);		// UTF8文字列へのポインタを返す
	int GetUtf8Length(void);	// UTF8文字列の長さ（バイト単位）
	LPCSTR ansi_str(void);		// ANSI文字列へのポインタを返す
	int GetAnsiLength(void);	// ANSI文字列の長さ（バイト単位）

protected:
	// 実際には終端文字があるので m_nBufLen - 1 文字だけ
	// メモリを拡張せずに格納できることになる

	LPTSTR m_pszBuf;	// 確保しているメモリへのポインタ
	LPTSTR m_pszStr;	// 文字列へのポインタ
	LPTSTR m_utf8Str;	// UTF8文字列へのポインタ
	LPSTR m_ansiStr;	// ANSI文字列へのポインタ
	int m_nMaxBufLen;	// 確保しているメモリ量（文字数単位）
	int m_nBufLen;		// 文字列が使用できるメモリ量（文字数単位）
	int m_nStrLen;		// 文字列の長さ（文字数単位）
};

} // end of namespace KSDK

#endif //__STRING_H__