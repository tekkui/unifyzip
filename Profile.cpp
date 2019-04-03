/*------------------------------------------------------------------------------
	File:	Profile.cpp
												2003.10.19 (c)YOSHIDA Kurima
================================================================================
------------------------------------------------------------------------------*/

#include "Profile.h"

#include <tchar.h>

#include "PathFunc.h"

namespace KSDK {

Profile::Profile()
{
}

Profile::Profile(LPCTSTR pszFilename)
{
	Open(pszFilename);
}

Profile::~Profile()
{
	Close();
}

void Profile::Open(LPCTSTR pszFilename)
{
	m_Filename = pszFilename;

	// フルパスでないと失敗するようなのでフルパスに変換
	GetFullPath(m_Filename);
}

void Profile::Close() {
	m_Filename.Empty();
}


bool Profile::WriteInt(LPCTSTR pszAppName, LPCTSTR pszKeyName, int n)
{
	TCHAR szKey[16];
	_itot_s(n, szKey, 10);
	return WriteString(pszAppName, pszKeyName, szKey);
}

bool Profile::WriteString(LPCTSTR pszAppName, LPCTSTR pszKeyName, LPCTSTR pszString)
{
	// 書き込みに失敗したらファイル属性を調べてリトライ
	if(WritePrivateProfileString(pszAppName, pszKeyName, pszString, m_Filename.c_str()) != 0){
		return true;
	}else{
		DWORD dwAttribute;
		if((dwAttribute = GetFileAttributes(m_Filename.c_str())) == 0xFFFFFFFF) return false;
		if((dwAttribute & FILE_ATTRIBUTE_READONLY) == 0) return false;
		SetFileAttributes(m_Filename.c_str(), dwAttribute & ~FILE_ATTRIBUTE_READONLY);
		return (WritePrivateProfileString(pszAppName, pszKeyName, pszString, m_Filename.c_str()) != 0);
	}
}

int Profile::GetInt(LPCTSTR pszAppName, LPCTSTR pszKeyName, int nDefault)
{
	return GetPrivateProfileInt(pszAppName, pszKeyName, nDefault, m_Filename.c_str());
}

// サイズ制限なしで結果を String に返す GetPrivateProfileString()
// pszAppName や pszKeyName を NULL 指定する使い方は想定していない
void Profile::GetString(LPCTSTR pszAppName, LPCTSTR pszKeyName, LPCTSTR pszDefault, String& returnedString)
{
	DWORD dwSize;
	DWORD dwRet;
	LPTSTR pszString;
	dwSize = 256;
	for(;;){
		pszString = new TCHAR [dwSize];
		// 関数が成功するとバッファに格納された文字数が返る (終端の NULL 文字は含まない) 
		// バッファのサイズが小さすぎると文字列は途中で切り捨てられ
		// 文字列の終端は NULL 文字になり 戻り値は nSize-1 になる
		dwRet = GetPrivateProfileString(pszAppName, pszKeyName, pszDefault, pszString, dwSize, m_Filename.c_str());
		if(dwRet == dwSize - 1){
			delete pszString;
			dwSize*=2;
		}else{
			break;
		}
	}
	returnedString.Copy(pszString);
	delete [] pszString;
}

} // end of namespace KSDK


