/*------------------------------------------------------------------------------
	File:	Profile.h
												2003.10.19 (c)YOSHIDA Kurima
================================================================================
 Content:
	Profileクラス
	
 Notes:

 Sample:

------------------------------------------------------------------------------*/

#ifndef __PROFILE_H__
#define __PROFILE_H__



#include "String.h"

namespace KSDK {

class Profile {
public:
	Profile();						// デフォルトコンストラクタ
	Profile(LPCTSTR pszFilename);	// コンストラクタ
	virtual ~Profile();			// デストラクタ

	void Open(LPCTSTR pszFilename);
	void Close();

	bool WriteInt(LPCTSTR pszAppName, LPCTSTR pszKeyName, int n);
	bool WriteString(LPCTSTR pszAppName, LPCTSTR pszKeyName, LPCTSTR pszString);

	int GetInt(LPCTSTR pszAppName, LPCTSTR pszKeyName, int nDefault);
	void GetString(LPCTSTR pszAppName, LPCTSTR pszKeyName, LPCTSTR pszDefault, String& returnedString);

private:
	Profile(const Profile &Profile);					// コピーコンストラクタを呼べないように private 宣言
	const Profile& operator=(const Profile& Profile);	// 代入演算を呼べないように private 宣言

	String m_Filename;
};

} // end of namespace KSDK

#endif //__PROFILE_H__