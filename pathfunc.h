/*------------------------------------------------------------------------------
	File:	PathFunc.h
												2003.06.11 (c)YOSHIDA Kurima
===============================================================================
 Content:
	パス文字列に関する関数群

 Notes:
	※２バイト文字のアルファベットの大文字・小文字の違いがあっても
	同じパスとして扱われるよう（テストして確認する必要あり）なので
	それに対応しなければならない

	※ネットワークパスに対応する必要がある

	※このファイルの関数群はパスの区切り文字を「\」のみと仮定して組んである

	※基本方針として特に断っていない限り
	引数として正しい形式のパスが渡されると仮定し例外処理は行っていない
	空文字列は正しいパスのひとつとする

	フルパスの表し方
	※ルートドライブのときのみ末尾に\がつく

	c:\
	c:\data
	c:\data\sub
	c:\data\sub\a.txt
	c:\a.txt


	相対パスの表し方
	※先頭に\はつけない

	data
	data\a.txt
	data\sub\a.txt


	ネットワークフォルダ
	\\Prius\新しいフォル

	たとえ、Cドライブを共有しても以下のようになる
	\\Prius\c

 Sample:

------------------------------------------------------------------------------*/

#ifndef __PATHFUNC_H__
#define __PATHFUNC_H__

#include "types.h"
#include <vector>

namespace KSDK {

class String;

// パスの種類
enum PathAttribute {
	PATH_INVALID,	// 存在しないパス
	PATH_FILE,		// ファイルのパス
	PATH_FOLDER,	// フォルダのパス
};

// 指定のファイル名をフルパスにして返す
// ファイルの指定は相対パス、フルパス、拡張子あり、拡張子無しのいずれも可
// pszDefPathは空の文字列やNULL指定をするとpszFileのみで指定することになる
//char* GetFullPath(const char* pszFile, const char* pszDefPath, const char* pszDefExt, char* pszPath);

// 相対パスにカレントディレクトリをつけてフルパスにする
bool GetFullPath(String& Path);

// UNC (Universal Naming Convention) パス か
// 「\\」で始まっているか
bool IsUNCPath(LPCTSTR pszPath);

// 指定のパスがフルパスかどうか
// 「:」が2文字目にあるか
bool IsFullPath(LPCTSTR pszPath);

// 指定のパスがフルパスかどうか（UNCもフルパスと扱う）
bool IsFullPathEx(LPCTSTR pszPath);

// 指定のパスがファイルのパスか（拡張子があるか）
//bool IsFilePath(LPCTSTR pszPath);

// 指定のファイルが存在するか
bool FileExists(LPCTSTR pszPath);

// 先頭のスペース 末尾のスペースとピリオド を削除
void FixFileName(String& FileName);

// 正しい形式にする
// （最初に「\」があったら取り除く）
// （ただし2つ以上「\」が続く場合はネットワークパスと考え2つ残す）
// （最後に「\」があったら取り除く ただし 「c:\」の様にルートの場合はそうならない）
void FixPath(String& Path);

// サイズ制限なしで結果を String に返す GetCurrentDirectory()
void GetCurrentDirectory(String& string);

// MAX_PATHを超える長さのパスを
// ワイド文字 (W) バージョンの API 関数に渡すときの
// フォーマットに変換する
void FormatTooLongPath(String& String);

// 指定のパスがフォルダかファイルかを返す
PathAttribute GetPathAttribute(LPCTSTR pszPath);

// ディレクトリの作成（複数階層のディレクトリも作成可能）
// フルパスのみ対応
// MakeSureDirectoryPathExists() を使えばいいっぽい？
bool CreateDirectoryEx(LPCTSTR path);

// パスからファイル名を取り除く
// 有効なパスであるかは確かめない
bool RemoveFileName(String& path);

// パスからファイル名を取り除く
// 有効な（存在する）パスについてのみ有効
bool RemoveFileNameEx(String& path);

// パスから拡張子を取り除く
// 有効なパスであるかは確かめない
bool RemoveExtension(String& path);

// ファイル名から拡張子を得る
void GetExtention(LPCTSTR filename, String& ext);

// カレントディレクトリ設定
// 絶対パスでも、相対パス（空文字列でも可）でも指定可能
// "\\data" とか "C:\\data" といったように指定する
// 絶対パスでない場合exeファイルを含むフォルダからの相対パスとみなす
bool SetCurrentDirectoryEx(LPCTSTR pszDir);

// パスの追加
//void CatPath(char* pszPath1, const char* pszPath2);
void CatPath(String& String, LPCTSTR pszPath);

// フォルダが空かどうか調べる
bool IsEmptyFolder(LPCTSTR pszPath);

// 2つのパスの先頭から共通するパスの長さを返す
int PathCommonPrefixLen(const char* psz1, const char* psz2);

// 指定したパスを長いパスに変換する
//bool GetLongPathName(char* pszPath);

// 指定された文字列がファイル名として正しいか
// （ファイル名に使えない文字が使われていないか）チェック
// ファイルに使えない文字は
// WinXPだと「TAB」「\」「/」「:」「*」「?」「"」「<」「>」「|」
// Win98SEだとさらに「,」「;」が加わる
// （どうやらロングファイルネームにおいては「,」も「;」も使える模様）
// 空の文字列や先頭にピリオドがある場合不正とする
bool IsFileName(LPCTSTR pszFileName);

// パスを表す文字列からスペースを検索し
// スペース文字が見つかった場合
// 文字列全体をダブルクォーテーションマークで囲む
void QuotePath(String& Path);

// サイズ制限なしで結果を String に返す GetModuleFileName()
void GetModuleFileName(HMODULE hModule, String& filename);

// フルパスからファイル名を得る
void GetFileName(LPCTSTR path, String& filename);

LPCTSTR GetFileName(LPCTSTR Path);

// ファイルやフォルダの削除
// ごみ箱に送ることもできる
// フォルダはそのフォルダに含まれるファイルごと削除する
bool DeleteFileOrFolder(LPCTSTR filename, bool usesRecycleBin = false);

// 不完全な実装
// 省略されたパスや駆け上がりパスにも対応する必要がある
int ComparePath(LPCTSTR path1, LPCTSTR path2);

// ファイル名を既存でないファイル名に変える
// 拡張子を保ってファイル名の終わりに「(1)」等を付ける
bool EvacuateFileName(String& rFilename);

// フォルダ名を既存でないフォルダ名に変える
bool EvacuateFolderName(String& folderName);

int PathGetDepth(LPCTSTR path);

int StripPath(String& path, int n=1);

int GetCommonPrefix(LPCTSTR path1, LPCTSTR path2, String& commonPrefix);

bool ComparePath(LPCTSTR path1, LPCTSTR path2, bool isFolder1, bool isFolder2);

bool expandPath(LPCTSTR srcPath, String& dstPath);

class FileEnumerator {
public:
	// path はフルパスのみ対応
	// pattern は FindFirstFile() への引数同様
	// 「*」または「?」のワイルドカード文字を含めることができる
	// TODO ルートディレクトリの検索に対応していない 以下を参考にすること
	// http://msdn.microsoft.com/library/ja/default.asp?url=/library/ja/jpfileio/html/_win32_findfirstfile.asp
	// TODO 「*」のみでないパターンを指定した場合サブフォルダ以下の検索ができない
	static bool enumerate(
		LPCTSTR path, 
		LPCTSTR pattern, 
		bool searchesSubfolders,
		std::vector<String>& v);
private:
	static bool enumerateSub(
		LPCTSTR path, LPCTSTR pattern, 
		bool searchesSubfolders, 
		std::vector<String>& v);
};

} // end of namespace KSDK

#endif //__PATHFUNC_H__

