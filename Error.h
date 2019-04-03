/*------------------------------------------------------------------------------
	File:	Error.h
												2003.02.20 (c)YOSHIDA Kurima
===============================================================================
 Content:
	エラーログ出力関数群

 Notes:
	_DEBUGが定義されているときのみ有効だが
	EnableOutputErrorlog()でRelease時も出力可能

 Sample:

------------------------------------------------------------------------------*/

#ifndef __ERROR_H__
#define __ERROR_H__



// Releaseビルドでもエラーの出力を有効にする
void EnableOutputErrorlog(bool b);

// メッセージに日付時刻を加えてデバッガに出力
void InnerLog(const char* szFormat, ...);

// InnerLog()で最後に出力した文字列をMessageBox()で表示
void SDKGetLastErrorMssage();

// GetLastError()を実行しエラーコードに対応するエラー文字列をMessageBox()で表示
void GetLastErrorMssage();



#endif //__ERROR_H__