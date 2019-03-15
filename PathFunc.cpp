#include "PathFunc.h"

#include <tchar.h>

#include <boost/shared_ptr.hpp>

#include <Shlwapi.h>
#pragma comment(lib,"shlwapi.lib")

#include "String.h"
#include "StrFunc.h"

using namespace std;
using namespace boost;

namespace KSDK {

/*
char* GetFullPath(const char* pszFile, const char* pszDefPath, const char* pszDefExt, char* pszPath)
{
	char* psz;

	*pszPath = _TCHAR('\0');
	if(!IsFullPathEx(pszFile)){
		// ファイルの指定が相対パスの場合デフォルトのパスをつける
		if(pszDefPath != NULL && *pszDefPath != _TCHAR('\0')){
			strcpy(pszPath, pszDefPath);
			// パスの最後に「\」をつける
			if(strrchrex(pszPath, _TCHAR('\\')) != pszPath + strlen(pszPath)-1) strcat(pszPath, _T("\\"));
		}
	}
	strcat(pszPath, pszFile);

	// 拡張子の指定が無い場合はデフォルトの拡張子をつける
	psz = strrchrex(pszFile, _TCHAR('\\'));
	if(psz == NULL){psz = (char*)pszFile;}
	if(strrchrex(psz, _TCHAR('.')) == NULL){
		strcat(pszPath, pszDefExt);
	}

	return pszPath;
}
*/


// 相対パスにカレントディレクトリをつけてフルパスにする
bool GetFullPath(String& Path) {
	String Temp;

	if (IsFullPathEx(Path.c_str())) {
		return true;
	}

	GetCurrentDirectory(Temp);
	CatPath(Temp, Path.c_str());
	Path = Temp;

	return true;
}



bool IsUNCPath(LPCTSTR pszPath)
{
	if(*pszPath == _TCHAR('\0'))
		return false;

	if(pszPath[0] == _TCHAR('\\') && pszPath[1] == _TCHAR('\\'))
		return true;

	return false;
}



bool IsFullPath(LPCTSTR pszPath)
{
	if(*pszPath == _TCHAR('\0'))
		return false;

	if(pszPath[1] == _TCHAR(':')){
		return true;
	}
	return false;
}



bool IsFullPathEx(LPCTSTR pszPath)
{
	return (IsUNCPath(pszPath) || IsFullPath(pszPath));
}

/*
// それがファイルのパスかチェックする
// （単に拡張子があるかチェックするだけ）
bool IsFilePath(LPCTSTR pszPath)
{
	char* psz;

	psz = strrchrex(pszPath, _TCHAR('\\'));

	if(psz == NULL)
		psz = (char*)pszPath;

	if(strchrex(psz, _TCHAR('.')) != NULL)
		return true;

	return false;
}
*/


// 指定のファイルが存在するか
// ディレクトリも指定可能
// フルパスでない場合カレントディレクトリ以下のものとみなされる
// NULL を指定した場合 false を返す
bool FileExists(LPCTSTR pszPath)
{
	return (GetPathAttribute(pszPath) != PATH_INVALID);
}



// 先頭のスペース 末尾のスペースとピリオド を削除
void FixFileName(String& FileName)
{
	FileName.TrimLeft(_TCHAR(' '));
	FileName.TrimRight(_T(" ."));
}



// 正しい形式にする
// （最初に「\」があったら取り除く）
// （ただし2つ以上「\」が続く場合はネットワークパスと考え2つ残す）
// （最後に「\」があったら取り除く ただし 「c:\」の様にルートの場合はそうならない）
void FixPath(String& Path)
{
	int nPos = 0;
	int n = 0;

	for(;;){
		if(Path[nPos++] == _TCHAR('\\')){
			n++;
		}else{
			break;
		}
	}

	// 最初に「\」があったら取り除く
	// ただし2つ以上「\」が続く場合はネットワークパスと考え2つ残す
	if(n == 0 || n == 2){

	}else if(n == 1){
		Path.Delete(0, 1);
	}else{
		Path.Delete(0, n - 2);
	}

	// 最後に「\」があったら取り除く
	// 「c:\」の様にルートの場合はそうならない
	if(Path.ReverseFind(_TCHAR('\\')) == Path.GetLength()-1){
		if(!IsFullPath(Path.c_str()) || Path.GetLength() != (int)_tcslen(_T("C:\\"))){
			Path.Delete(Path.GetLength()-1, 1);
		}
	}
}


// テスト済み
// サイズ制限なしで結果を String に返す GetCurrentDirectory()
void GetCurrentDirectory(String& string)
{
	DWORD dwSize;
	DWORD dwRet;
	LPTSTR pszString;

	dwSize = MAX_PATH;

	for(;;){
		pszString = new TCHAR [dwSize];

		// 関数が成功するとバッファに書き込まれた文字数 (終端の NULL 文字を除く) が返る
		// バッファのサイズが小さかったときは必要なバッファのサイズ (終端の NULL 文字を含む)が返る
		dwRet = ::GetCurrentDirectory(dwSize, pszString);

		if(dwRet == 0){
			// エラー
			string.Empty();
			delete [] pszString;
			return;
		}else if(dwRet > dwSize){
			// バッファが小さかった
			dwSize = dwRet;
			delete [] pszString;
		}else{
			// 成功
			break;
		}
	}
	string = pszString;
	delete [] pszString;
}


// テスト済み
// サイズ制限なしで結果を String に返す GetModuleFileName()
void GetModuleFileName(HMODULE hModule, String& filename)
{
	DWORD dwSize = 256;
	DWORD dwRet;
	LPTSTR pszString;

	for(;;){
		pszString = new TCHAR [dwSize];

		// コピーされた文字列の長さが文字単位（NULL文字を除いた文字数）で返る
		// 結果が長すぎた場合切り捨てられる
		// 切り捨てた場合終端にNULL文字は付かない
		dwRet = ::GetModuleFileName(hModule, pszString, dwSize);

		if(dwRet == 0){
			// エラー
			delete [] pszString;
			filename.Empty();
			return;
		}else if(dwRet == dwSize){
			// バッファが小さかった
			delete [] pszString;
			dwSize*=2;
		}else{
			// 成功
			break;
		}
	}
	filename.Copy(pszString);
	delete [] pszString;
}

// MAX_PATHを超える長さのパスを
// ワイド文字 (W) バージョンの API 関数に渡すときの
// フォーマットに変換する
void FormatTooLongPath(String& String)
{
	if(String.GetLength() <= MAX_PATH) return;

	if(IsUNCPath(String.c_str())){
		String.Delete(0, 1);// 先頭の\をひとつ消す
		String.Insert(0, _T("\\\\?\\UNC"));
	}else{
		String.Insert(0, _T("\\\\?\\"));
	}
}

// 指定のパスがフォルダかファイルかを返す
PathAttribute GetPathAttribute(LPCTSTR pszPath)
{
	String Path;

	if(IsFullPathEx(pszPath)){
		Path = pszPath;
	}else{
		GetCurrentDirectory(Path);
		CatPath(Path, pszPath);
	}

	FormatTooLongPath(Path);
	DWORD dw = GetFileAttributes(Path.c_str());

	if(dw == 0xFFFFFFFF){
		return PATH_INVALID;
	}else if(dw & FILE_ATTRIBUTE_DIRECTORY){
		return PATH_FOLDER;
	}else{
		return PATH_FILE;
	}
}

// ディレクトリの作成（複数階層のディレクトリも作成可能）
// フルパスのみ対応
bool CreateDirectoryEx(LPCTSTR path) 
{
	String temp = path;
	LPCTSTR p;

	// そのディレクトリが既に存在する場合は終了
	if(FileExists(path)) return true;

	// ルートディレクトリから下位ディレクトリに向かって
	// そのディレクトリがあるかチェックなければ作成
	// を繰り返す

	// はじめの「\」の位置を得る
	// 「\」が見つからない場合は不正なパス
	// はじめの「\」はドライブ名の次にあるはずなので2番目以降の\で区切っていく

	if((p = strchrex(path, _TCHAR('\\'))) == NULL) return false;

	temp.NCopy(path, (int)(p-path+1));

	// ドライブの存在を確認
	if(FileExists(temp.c_str()) == false) return false;

	for(;;){
		if((p = strchrex(p+1, _TCHAR('\\'))) == NULL) break;
		temp.NCopy(path, (int)(p-path));
		if(!FileExists(temp.c_str())){
			// 指定したディレクトリが存在しなかったら作成
			// ディレクトリの作成に失敗したら終了
			if(CreateDirectory(temp.c_str(), NULL) == 0) return false;
		}
	}

	// 指定のディレクトリのルートディレクトリまでは作成されたはずなので
	// 最後に指定のディレクトリを作成する
	// ディレクトリの作成に失敗したら終了
	if (CreateDirectory(path, NULL) == 0) {
		return false;
	}

	return true;
}


// パスからファイル名を取り除く
// 有効なパスであるかは確かめない
bool RemoveFileName(String& path)
{
	return (StripPath(path, 1) == 1);
}

// フルパスからファイル名を取り除く
// 有効な（存在する）パスについてのみ有効
bool RemoveFileNameEx(String& path)
{
	switch(GetPathAttribute(path.c_str())){
	case PATH_FILE:
		return RemoveFileName(path);
	case PATH_FOLDER:
		return true;
	}
	return false;
}

// パスから拡張子を取り除く
// 有効なパスであるかは確かめない
bool RemoveExtension(String& path)
{
	int nPos0, nPos1;
	nPos0 = path.ReverseFind(_TCHAR('\\'));

	if(nPos0 == -1){
		nPos0 = 0;
	}
	nPos1 = path.ReverseFind(_TCHAR('.'));
	if(nPos0 < nPos1) path.MakeLeft(nPos1);
	return true;
}

// 拡張子を得る(先頭に'.'を含む)
void GetExtention(LPCTSTR filename, String& ext) {

	String str = filename;

	int nPos0, nPos1;
	nPos0 = str.ReverseFind(_TCHAR('\\'));

	if(nPos0 == -1){
		nPos0 = 0;
	}
	nPos1 = str.ReverseFind(_TCHAR('.'));
	if(nPos0 < nPos1) {
		ext.Copy(str.c_str() + nPos1);
	} else {
		ext.Empty();
	}
}

// カレントディレクトリ設定
// 絶対パスでも、相対パス（空文字列でも可）でも指定可能
// "\\data" とか "C:\\data" といったように指定する
// 絶対パスでない場合exeファイルを含むフォルダからの相対パスとみなす
bool SetCurrentDirectoryEx(LPCTSTR pszDir)
{		
	if(IsFullPathEx(pszDir)){
		return (::SetCurrentDirectory(pszDir) != 0);
	}else{
		String Path;
		GetModuleFileName(NULL, Path);
		RemoveFileName(Path);
		CatPath(Path, pszDir);
		return (::SetCurrentDirectory(Path.c_str()) != 0);
	}
}

/*
// パスの追加
void CatPath(char* pszPath1, const char* pszPath2)
{
	if(*pszPath2 == _TCHAR('\0')) return;

	// 空文字列でなくて最後に「\」がなかったらつける
	DWORD dwLen = strlen(pszPath1);
	if(	*pszPath1 != _TCHAR('\0')
	 && strrchrex(pszPath1, _TCHAR('\\')) != pszPath1 + dwLen-1){
		strcat(pszPath1,"\\"); // 検索するファイル名作成
	}
	strcat(pszPath1, pszPath2);
}
*/

// パスの追加
void CatPath(String& String, LPCTSTR pszPath)
{
	if(*pszPath == _TCHAR('\0')) return;

	// 空文字列でなくて最後に「\」がなかったらつける
	int nLen = lstrlen(String.c_str());
	if(	!String.IsEmpty()
	 && strrchrex(String.c_str(), _TCHAR('\\')) != String.c_str() + (nLen-1) ){
		String += _TCHAR('\\');
	}
	String+=pszPath;
}

// フォルダが空かどうか調べる
// TODO システム属性のファイルは無視するようにする？
bool IsEmptyFolder(LPCTSTR pszPath) {
	// 「指定されたファイルが見つかりません。」で GetLastError() が返す値
	static const DWORD FILE_NOT_FOUND = 0x00000002;

	WIN32_FIND_DATA fFind;
	String Path;
	HANDLE hSearch;
	bool bFind;

	bFind = false;// 指定フォルダ内に何らかのファイル・フォルダを見つけたか

	// 探索パスを作成
	Path = pszPath;
	FormatTooLongPath(Path);
	CatPath(Path, _T("*"));

	hSearch = FindFirstFile(Path.c_str(), &fFind);

	if (hSearch == INVALID_HANDLE_VALUE) {
		if (GetLastError() == FILE_NOT_FOUND) {
			// ルートディレクトリでかつファイルが全く存在しない場合
			return true;
		} else {
			return false;
		}
	}

	do{
		// ディレクトリか、普通のファイルか
		if((fFind.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=0){// ディレクトリであった
			// ルート及びカレントでないフォルダかどうかチェック
			if(	_tcscmp(fFind.cFileName, _T(".")) != 0 &&
				_tcscmp(fFind.cFileName, _T("..")) != 0) {
				bFind = true;
				break;
			}
		}else{// 普通のファイルであった
			bFind = true;
			break;
		}
	} while (FindNextFile(hSearch, &fFind)!=0);

	FindClose(hSearch);

	return  !bFind;
}

// 2つのパスの先頭から共通するパスの長さを返す
// 「共通するパス」なので大文字小文字の違いは無視する
// 全角アルファベットについて考える必要あり
int PathCommonPrefixLen(const char* psz1, const char* psz2)
{
	char *p1, *p2;
	char c1, c2;
	char* pBS;		// 最後の「\」の位置
	bool bKanji;	// 2バイト文字の2文字目か

	bKanji = false;

	if(psz1 == NULL || psz2 == NULL) return 0;

	p1 = (char*)psz1;
	p2 = (char*)psz2;
	pBS = p1;

	for(;;){
		c1 = *p1;
		c2 = *p2;

		if(c1== _TCHAR('\0') || c2 == _TCHAR('\0')){
			if(c1 == c2 || c1 == _TCHAR('\\') || c2 == _TCHAR('\\')) pBS = p1;
			break;
		}

		if(bKanji){
			if(c1 != c2) break;
			bKanji = false;
		}else{
			if(c1 >= _TCHAR('A') && c1 <= _TCHAR('Z')) c1 = c1 + (_TCHAR('a') - _TCHAR('A'));	// c1を小文字化
			if(c2 >= _TCHAR('A') && c2 <= _TCHAR('Z')) c2 = c2 + (_TCHAR('a') - _TCHAR('A'));	// c2を小文字化
			if(c1 != c2) break;
			if(c1 == _TCHAR('\\')) pBS = p1;
			bKanji = IsKanji(c1);
		}
		p1++;
		p2++;
	}
	return (int)(pBS - psz1);
}

/*
// 指定したパスを長いパスに変換する
// UNCパスの場合と
// 残りの文字列が「C:」となったときFindFirstFileに失敗するところを
// 見直す必要がある
bool GetLongPathName(const char* pszShortPath, char* pszLongPath)
{
	WIN32_FIND_DATA wfd;
	HANDLE hFind;
	char szTempShortPath[MAX_PATH];
	char szLongPath[MAX_PATH];
	char szTemp[MAX_PATH];
	LPTSTR pYen;
	int nPos;

	if(!FileExists(pszShortPath)) return false;

	if(IsUNCPath(pszShortPath)){
		// UNCパスの場合はそのまま
		return true;
	}else{
		szLongPath[0] = _TCHAR('\0');

		strcpy(szTempShortPath, pszShortPath);

		hFind = FindFirstFile( szTempShortPath, &wfd );
		if (hFind == INVALID_HANDLE_VALUE) return false;
		FindClose(hFind);

		do{
			pYen = strrchrex( szTempShortPath, _TCHAR('\\') );
			if( pYen != NULL ){
				nPos = pYen - szTempShortPath;
				if(strlen(szLongPath) == 0){
					lstrcpy( szLongPath, wfd.cFileName );
				}else{
					wsprintf( szTemp, "%s\\%s", wfd.cFileName, szLongPath );
					lstrcpy( szLongPath, szTemp );
				}
				szTempShortPath[nPos] = _TCHAR('\0');
				hFind = FindFirstFile( szTempShortPath ,&wfd );
				if (hFind == INVALID_HANDLE_VALUE){
					wsprintf( szTemp, "%s\\%s", szTempShortPath, szLongPath );
					lstrcpy( szLongPath, szTemp );
					break;
				}
				FindClose(hFind);
			}else{
				wsprintf( szTemp, "%s\\%s", szTempShortPath, szLongPath );
				lstrcpy( szLongPath, szTemp );
			}
		}while( pYen != NULL );
	}

	strcpy(pszLongPath, szLongPath);

	return true;
}

// 指定したパスを長いパスに変換する
bool GetLongPathName(char* pszPath)
{
	return (GetLongPathName(pszPath, pszPath));
}
*/


// 指定された文字列がファイル名として正しいか
// （ファイル名に使えない文字が使われていないか）チェック
// ファイルに使えない文字は
// WinXPだと「TAB」「\」「/」「:」「*」「?」「"」「<」「>」「|」
// Win98SEだとさらに「,」「;」が加わる
// （どうやらロングファイルネームにおいては「,」も「;」も使える模様）
// 空の文字列や先頭にピリオドがある場合不正とする
bool IsFileName(LPCTSTR pszFileName)
{
	LPTSTR p;
	TCHAR c;

	// 空の文字列や先頭にピリオドがある場合不正とする
	if(*pszFileName == _TCHAR('\0') || *pszFileName == _TCHAR('.')) return false;

	p = const_cast<LPTSTR>(pszFileName);

	for(;;){
		c = *p++;
		if(c == _TCHAR('\0')){
			break;

#ifndef _UNICODE
		}else if(IsKanji(c)){
			p++;
			continue;
#endif
		//}else if(c==_TCHAR('\t') || c==_TCHAR('\\') || c==_TCHAR('/') || c==_TCHAR(':') 
		//	  || c==_TCHAR(',')  || c==_TCHAR(';')  || c==_TCHAR('*') || c==_TCHAR('?')
		//	  || c==_TCHAR('"')  || c==_TCHAR('<')  || c==_TCHAR('>') || c==_TCHAR('|')){
		//	return false;

		}else if(c==_TCHAR('\t') || c==_TCHAR('\\') || c==_TCHAR('/')  || c==_TCHAR(':')  || c==_TCHAR('*') 
			  || c==_TCHAR('?')  || c==_TCHAR('"')  || c==_TCHAR('<')  || c==_TCHAR('>')  || c==_TCHAR('|')){
			return false;
		}
	}
	return true;
}

// パスを表す文字列からスペースを検索し
// スペース文字が見つかった場合
// 文字列全体をダブルクォーテーションマークで囲む
void QuotePath(String& Path)
{
	if(Path.IsEmpty()) return;

	// スペースを含むか
	if(Path.Find(_TCHAR(' ')) == -1) return;

	Path.Insert(0, _TCHAR('\"'));
	Path += _TCHAR('\"');
}

// パスの最後の'\\'の後を返す
// '\\'が無い場合はそのまま返す
LPCTSTR GetFileName(LPCTSTR Path)
{
	LPCTSTR p;
	p = strrchrex(Path, _TCHAR('\\'));
	if(p == NULL){
		return Path;
	}else{
		return p+1;
	}
}

void GetFileName(LPCTSTR path, String& filename)
{
	LPCTSTR p;
	p = strrchrex(path, _TCHAR('\\'));
	if(p == NULL){
		filename.Copy(path);
	}else{
		filename.Copy(p+1);
	}
}

#include <shellapi.h>
#pragma comment(lib,"shell32.lib")

// ファイルやフォルダの削除
// ごみ箱に送ることもできる
// 読み取り専用のファイルも削除できる
// フォルダはそのフォルダに含まれるファイルごと削除する
// 
bool DeleteFileOrFolder(LPCTSTR filename, bool usesRecycleBin) {

	TCHAR* deletePath = new TCHAR [_tcslen(filename) + 2];

	_tcscpy(deletePath, filename);

	deletePath[_tcslen(deletePath)+1] = '\0';
	SHFILEOPSTRUCT fileOp;
	fileOp.hwnd = NULL;
	fileOp.wFunc = FO_DELETE;
	fileOp.pFrom = deletePath;
	fileOp.pTo = NULL;
	fileOp.fFlags = FOF_NOCONFIRMATION | FOF_SILENT;

	// SHFileOperation() の終了をこの値で知るため
	fileOp.fAnyOperationsAborted = TRUE;

	if (usesRecycleBin) fileOp.fFlags |= FOF_ALLOWUNDO;
	if (SHFileOperation(&fileOp) != 0) {
		// ファイル削除失敗
		delete [] deletePath;
		return false;
	}
	
	while (fileOp.fAnyOperationsAborted == TRUE) {
		MessageBox(NULL, _T("SHFileOperation() の途中です\n\nこのメッセージが出たことを作者までご連絡下さい\nソフトの改善に繋がります"), _T("Notice"), MB_OK);
		Sleep(1);
	}

	delete [] deletePath;

	return true;
}

// パスの比較
// 省略したパスに対応していない
// 大文字小文字や\と/などには対応
// この関数を使ってソートすると
// フォルダの直後にそのフォルダに含まれるファイル・フォルダが続く事が保証される
// （文字列比較の際 '\\' や '/' を '\0' の次に小さいものとして扱うため）
int ComparePath(LPCTSTR path1, LPCTSTR path2) {
	#ifdef _UNICODE
		TCHAR str1[2];
		TCHAR str2[2];
	#else
		TCHAR str1[3];
		TCHAR str2[3];
		str1[2] = _TCHAR('\0');
		str2[2] = _TCHAR('\0');
	#endif

	str1[1] = _TCHAR('\0');
	str2[1] = _TCHAR('\0');

	
	for (int len = 0; ; ) {
		#ifdef _UNICODE
			str1[0] = path1[len];
			str2[0] = path2[len];
			int r = lstrcmpi(str1, str2);
			if (r == 0) {
				if (str1[0] == _TCHAR('\0')) {
					return 0;
				} else {
					len++;
				}
			} else {
				if (str1[0] == _TCHAR('\0') || str2[0] == _TCHAR('\0')) {
					return r;
				}

				// '\\' と '/' を '\0' の次に小さいものとして扱う
				if (str1[0] == _TCHAR('\\') || str1[0] == _TCHAR('/')) {
					return -1;
				}
				if (str2[0] == _TCHAR('\\') || str2[0] == _TCHAR('/')) {
					return 1;
				}
				return r;
			}
		#else
			str1[0] = path1[len];
			str2[0] = path2[len];

			bool b1 = IsKanji(str1[0]);
			bool b2 = IsKanji(str2[0]);

			if (b1) str1[1] = path1[len+1];
			if (b2) str2[1] = path2[len+1];

			int r = lstrcmpi(str1, str2);
			if (r != 0) {
				if (str1[0] == _TCHAR('\0') || str2[0] == _TCHAR('\0')) {
					return r;
				}

				// '\\' と '/' を '\0' の次に小さいものとして扱う
				if (b1 == false && (str1[0] == _TCHAR('\\') || str1[0] == _TCHAR('/'))) {
					return -1;
				}
				if (b2 == false && (str2[0] == _TCHAR('\\') || str2[0] == _TCHAR('/'))) {
					return 1;
				}
				return r;

			} else if (str1[0] == _TCHAR('\0')) {
				return 0;
			} else {
				if (b1) {
					str1[1] = _TCHAR('\0');
					str2[1] = _TCHAR('\0');
					len+=2;
				} else {
					len++;
				}
			}
		#endif
	}
}


// エクスプローラと同じソートを実現するためのパス比較
// フォルダにフォルダ内のファイルが続くことが保証される
// "<" 演算子として使う
bool ComparePath(LPCTSTR path1, LPCTSTR path2, bool isFolder1, bool isFolder2)
{
	struct PathAndAttribute {
		LPCTSTR path;
		bool isFolder;
	};

	PathAndAttribute smallOne;
	PathAndAttribute largeOne;

	int r = ComparePath(path1, path2);
	if (r == 0) {
		// path1 だけがフォルダなら true
		return (isFolder1 && !isFolder2);
	} else if (r < 0) {
		if (isFolder1) return (r < 0);
		smallOne.path = path1;
		smallOne.isFolder = isFolder1;
		largeOne.path = path2;
		largeOne.isFolder = isFolder2;
	} else {
		if (isFolder2) return (r < 0);
		smallOne.path = path2;
		smallOne.isFolder = isFolder2;
		largeOne.path = path1;
		largeOne.isFolder = isFolder1;
	}

	// 比較の結果小さいのがファイルであればフォルダ優先にする都合上逆転があり得る
	// 比較のため親フォルダには末尾に '\\' を残しておく
	String s1 = smallOne.path;
	String p1(s1);
	int n1 = p1.ReverseFind(_TCHAR('\\'));
	int n2 = p1.ReverseFind(_TCHAR('/'));
	p1.MakeLeft(max(n1, n2) + 1);

	String s2 = largeOne.path;
	String p2(s2);
	p2.MakeLeft(p1.GetLength());
	// ↑文字列によっては２バイト文字の１バイト目で切ることになるのが気持ち悪いが…

	if (lstrcmpi(p1.c_str(), p2.c_str()) == 0) {
		if ((largeOne.isFolder)
			|| s2.ReverseFind(_TCHAR('\\')) > p2.GetLength() 
			|| s2.ReverseFind(_TCHAR('/')) > p2.GetLength()) 
		{
			return !(r < 0);
		}
	}

	return (r < 0);
}


// ファイル名を既存でないファイル名に変える
// 拡張子を保ってファイル名の終わりに「(1)」等を付ける
bool EvacuateFileName(String& rFilename) {
	if (!FileExists(rFilename.c_str())) return true;

	String path;
	String extention;

	GetExtention(rFilename.c_str(), extention);
	path = rFilename;
	path.MakeLeft(path.GetLength() - extention.GetLength());

	for (int i=2; ; ++i) {
		rFilename.Format(_T("%s(%d)%s"), path.c_str(), i, extention.c_str());
		if (!FileExists(rFilename.c_str())) {
			break;
		}
	}
	return true;
}

// フォルダ名を既存でないフォルダ名に変える
bool EvacuateFolderName(String& folderName) {
	if (!FileExists(folderName.c_str())) return true;
	String path;
	int i=1;
	do {
		path.Format(_T("%s(%d)"), folderName.c_str(), i);
		i++;
	} while(FileExists(path.c_str()));
	folderName = path;
	return true;
}

// パスの深さを得る
// 空文字列だと深さは 0
// 上のフォルダがなければ深さは 1
// '\\' が二つ並ぶようなパスには対応していない
int PathGetDepth(LPCTSTR path) 
{
	if (path == NULL || *path == '\0') {
		return 0;
	}

	// セパレータを数える
	// ただし末尾のセパレータは無視
	String str(path);
	int depth = 1;
	for (;;) {
		int pos = str.FindOneOf(_T("\\/"));
		if (pos == -1 || pos == str.GetLength()-1) break;
		depth++;
		str.MakeMid(pos+1);
	}
	return depth;
}

// 指定の段数パスを削る
// 削った段数を返す
int StripPath(String& path, int n)
{
	if (n <= 0) return 0;
	int d = PathGetDepth(path.c_str());

	// 全部削るのか
	if (n >= d) {
		path.Empty();
		return d;
	}

	// いくつ目のセパレータまで残せばいいのか
	int dd = d-n;
	int len = 0;

	String str(path);
	for (int i=0; i<dd; ++i) {
		int pos = str.FindOneOf(_T("\\/"));
		len += (pos+1);
		str.MakeMid(pos+1);
	}

	if (IsFullPath(path.c_str()) && dd==1) {
		path.MakeLeft(len);
	} else {
		path.MakeLeft(len-1);
	}

	return n;
}

int GetCommonPrefix(LPCTSTR path1, LPCTSTR path2, String& commonPrefix) 
{
	int n = ::PathCommonPrefix(path1, path2, NULL);
	if (n == 0) {
		commonPrefix.Empty();
		return 0;
	}
	LPTSTR p = commonPrefix.GetBuffer(n);
	n = ::PathCommonPrefix(path1, path2, p);
	commonPrefix.ReleaseBuffer();
	return n;
}



// パス文字列の "\\..\\" を展開する
// パスの区切り文字は '\\'
// 絶対パスであっても相対パスであっても
// 指定したよりも上の階層には駆け上がれない
// '.' のみで構成されたエレメントの後に複数の '\\' が連続していても
// 一つのみ取り除かれる
bool expandPath(LPCTSTR srcPath, String& dstPath) {
	String src = srcPath;

	if (src.Find(_T(".\\")) == -1) {
		dstPath = srcPath;
		return true;
	}

	vector<boost::shared_ptr<String> > elements;

	while (!src.IsEmpty()) {
		boost::shared_ptr<String> pDst(new String());
		int delimiterPos = src.Find(_TCHAR('\\'), 0);
		if (delimiterPos != -1) {
			pDst->NCopy(src.c_str(), delimiterPos);
			src.MakeMid(delimiterPos + 1);
		} else {
			pDst->Copy(src.c_str());
			src.Empty();
		}
		int dot = 0;
		for (int i = 0; i < pDst->GetLength(); ++i) {
			TCHAR c = pDst->GetAt(i);
			if (c == _TCHAR('.')) {
				dot++;
			} else {
				dot = 0;
				break;
			}
		}
		if (dot == 0) {
			elements.push_back(pDst);
		} else {
			while (dot >= 2) {
				if (elements.size() != 0) {
					if (!elements.back()->IsEmpty()) {
						dot--;
					}
					elements.pop_back();
				} else {
					// pop する要素がなければエラーを出す
					return false;
				}
			}
		}
	}

	// elements を全て結合して完成
	dstPath.Empty();
	vector<boost::shared_ptr<String> >::iterator it;
	for (it = elements.begin(); it != elements.end(); ++it) {
		dstPath += *(*it);
		if (it + 1 != elements.end()) dstPath += _TCHAR('\\');
	}

	return true;
}


bool FileEnumerator::enumerate(
	LPCTSTR path, 
	LPCTSTR pattern, 
	bool searchesSubfolders,
	vector<String>& v) {

	v.clear();
	return enumerateSub(path, pattern, searchesSubfolders, v);
}

bool FileEnumerator::enumerateSub(
	LPCTSTR path, 
	LPCTSTR pattern, 
	bool searchesSubfolders, 
	vector<String>& v) {

	String p;
	WIN32_FIND_DATA fd;
	HANDLE hSearch;

	// 探索パスを作成
	p = path;
	CatPath(p, pattern);

	hSearch = FindFirstFile(p.c_str(), &fd);

	if (hSearch == INVALID_HANDLE_VALUE) {
		return false;
	}

	do{
		// ディレクトリか、普通のファイルか
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			// ディレクトリであった

			// ルート及びカレントでないフォルダかどうかチェック
			if (lstrcmp(fd.cFileName,_T(".")) == 0 || lstrcmp(fd.cFileName, _T("..")) == 0) {
				 continue;
			}

			p = path;
			CatPath(p, fd.cFileName);
			v.push_back(p);

			if (searchesSubfolders) {
				if (!enumerateSub(p.c_str(), pattern, true, v)) {
					FindClose(hSearch);
					return false;
				}
			}
		} else {
			// 普通のファイルであった
			p = path;
			CatPath(p, fd.cFileName);
			v.push_back(p);
		}
	} while (FindNextFile(hSearch,&fd) != 0);

	FindClose(hSearch);

	return (GetLastError() == ERROR_NO_MORE_FILES);
}


} // end of namespace KSDK

