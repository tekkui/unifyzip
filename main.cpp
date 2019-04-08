#include <tchar.h>

#include <iostream>
#include <list>
#include <fstream>
#include <codecvt>
//#include <algorithm>

#include <boost/regex.hpp>
#include <boost/shared_ptr.hpp>

#pragma comment(lib, "winmm.lib")

#include <Shlwapi.h>
#pragma comment(lib,"shlwapi.lib")
#include <fcntl.h>
#include <io.h>

#include "ArchiveDll.h"
#include "TemporaryFolder.h"

#include "StrFunc.h"
#include "PathFunc.h"
#include "Error.h"
#include "Profile.h"
#include "CommandLine.h"
#include "String.h"
#include "Zip.h"
#include "NkfDll.h"

#include "resource.h"

using namespace std;
using namespace boost;
using namespace KSDK;

#ifdef _UNICODE
#define cout wcout
#define ofs wofs
#endif

// アーカイブに格納されるファイル・フォルダの情報
class IndividualInfo {
public:
	IndividualInfo(INDIVIDUALINFO& rIndividualInfo, DWORD attribute, bool isEncrypted) {
		// INDIVIDUALINFO 内の属性はフォルダ属性が正確でなかったりするので
		// 別に取得したもので指定する

#ifdef _UNICODE
		fullPath_.Utf8Copy(rIndividualInfo.szFileName);
#else
		fullPath_ = rIndividualInfo.szFileName;
#endif

		// UNLHA ではパスのセパレータが '/' なので '\\' に変換
		fullPath_.Replace(_TCHAR('/'), _TCHAR('\\'));

		// 末尾の'\\'を取り除く
		fullPath_.TrimRight(_TCHAR('\\'));

		GetFileName(fullPath_.c_str(), filename_);

		DosDateTimeToFileTime(rIndividualInfo.wDate, rIndividualInfo.wTime, &lastWriteTime_);

		attribute_ = attribute;

		isEncrypted_ = isEncrypted;

		// wRatio と szMode から圧縮レベルを判断する
#ifdef _UNICODE
		String szMode;
		szMode.Utf8Copy(rIndividualInfo.szMode);
		if (lstrcmp(szMode.c_str(), _T("Store")) == 0) {
#else
		if (lstrcmp(rIndividualInfo.szMode, _T("Store")) == 0) {
#endif
			compressLevel_ = 0;
		} else {
			compressLevel_ = -1;
		}
	}

	IndividualInfo(LPCTSTR fullPath, DWORD attribute, bool isEncrypted) {
		fullPath_ = fullPath;

		// UNLHA ではパスのセパレータが '/' なので '\\' に変換
		fullPath_.Replace(_TCHAR('/'), _TCHAR('\\'));

		// 末尾の'\\'を取り除く
		fullPath_.TrimRight(_TCHAR('\\'));

		GetFileName(fullPath_.c_str(), filename_);

		// 更新時刻は現在時刻を入れておく
		SYSTEMTIME st;
		GetSystemTime(&st);
		FILETIME ft;
		SystemTimeToFileTime(&st, &ft);
		LocalFileTimeToFileTime(&ft, &lastWriteTime_);

		attribute_ = attribute;

		isEncrypted_ = isEncrypted;

		compressLevel_ = -1;
	}

	// fileName は 書庫内のフルパス（パスのセパレータは '\\'）
	IndividualInfo(LPCTSTR fileName, FILETIME fileTime, DWORD attribute, int compressLevel, bool isEncrypted) {
		fullPath_ = fileName;
		GetFileName(fullPath_.c_str(), filename_);
		lastWriteTime_ = fileTime;
		attribute_ = attribute;
		compressLevel_ = compressLevel;
		isEncrypted_ = isEncrypted;
	}

	LPCTSTR getFullPath() const {
		return fullPath_.c_str();
	}
	LPCTSTR getFilename() const {
		return filename_.c_str();
	}
	DWORD getAttribute() {
		return attribute_;
	}
	const FILETIME* getLastWriteTime() const {
		return &lastWriteTime_;
	}
	int getCompressLevel() {
		return compressLevel_;
	}

	bool isEncrypted() {
		return isEncrypted_;
	}

private:
	String filename_;
	String fullPath_;
	FILETIME lastWriteTime_;

	// ファイル属性や、ファイルかフォルダか
	DWORD attribute_;

	// 圧縮レベル
	// -1 だと不明
	int compressLevel_;

	// 暗号化されているか
	bool isEncrypted_;
};


// temp フォルダを得る
bool getTemporaryPath(String& dest);

// 初期化
bool initialize();

// 書庫を処理する
bool process(LPCTSTR filename);

void deleteTemporaryFolder(TemporaryFolder &temp_folder);
void setFileFolderDateTime(TemporaryFolder &temp_folder, int redundantPathLen);
void setFileFolderAttribute(TemporaryFolder &temp_folder, int redundantPathLen);
bool checkArchiveDllRunning(ArchiveDll * pArchiveDll);

void convertFileNameInArchive(TemporaryFolder& temp_folder, int redundantPathLen);
void compressWithTemporaryFolder(String& srcFilename, String& destFilename, TemporaryFolder& temp_folder);
bool checkArchive(ArchiveDll*& pArchiveDll, String& destFilename);
void convertFileName(String& finalDestFilename);

void analyzePath(vector<String>& src, vector<String>& dest);
bool analyzePathSub(LPCTSTR src, vector<String>& dest);

// 指定のフォルダ内（サブフォルダ含む）のフォルダの更新日時を
// フォルダ内の最新の更新日時を持つファイル・フォルダと一致させる
void setFolderTime(LPCTSTR path);
bool setFolderTimeSub(LPCTSTR path, FILETIME& rNewestFileTime);

// 冗長フォルダのパスを得る
void GetRedundantPath(String& rRedundantPath);

bool AddLackFolder();

bool checkAttribute();
bool checkUpdateDateTime();

bool GetIndividualInfo(LPCTSTR path);
bool GetIndividualInfoSub(LPCTSTR searchPath, LPCTSTR basePath);

// フォルダ内を圧縮
bool compress(LPCTSTR srcPath, LPCTSTR dstPath, int compressLevel, bool showsProgress);
bool compressSub(LPCTSTR pszDir, ZipWriter& zw, int basePathLength);

int CheckRemarkableFile();
int CheckRemarkableFileSub();

list<boost::shared_ptr<IndividualInfo> > g_compressFileList;

ArchiveDllManager g_ArchiveDllManager;

// 各種オプション
int g_PriorityClass;

bool g_DeletesEmptyFolder;
bool UsesRecycleBin;
int g_RemovesRedundantFolder;
bool g_ShowsProgress;
String g_AppendFile;
String g_ExcludeFile;
String g_RemarkableFile;
int g_CompressLevel;	// 圧縮レベル = [ -1 | 1 | 0 | 9 ]

int g_FileTimeMode;
int g_FolderTimeMode;
int g_FileAttribute;
int g_FolderAttribute;

bool g_BatchDecompress;
bool g_SearchSubfolders;

bool g_RecyclesIndividualFiles;

String g_FileTime;
String g_FolderTime;

int g_WarningLevel;

bool g_ExitsImmediately;

bool g_PreCheck;

bool g_IgnoresEncryptedFiles;

bool g_Compress7Zip;
bool g_OutputTextFile;
bool g_ConvertFileName;
String g_NkfOption;
NkfDll *nkf;
bool g_CompressWithTemporaryFolder;
bool g_CheckArchive;

std::vector<UINT8> g_ArchiveComment;

namespace FileAttribute {
enum FileAttribute {
	READ_ONLY = 1,
	HIDDEN = 2,
	SYSTEM = 4,
	ARCHIVE = 32,
};
}

std::wofstream wofs;

// temp フォルダを得る
bool getTemporaryPath(String& dest) {
	const DWORD LEN = 512;
	TCHAR path[LEN+1];
	dest.Empty();
	DWORD ret = ::GetTempPath(LEN, path);

	// 短縮形なので
	TCHAR longPath[LEN+1];
	GetLongPathName(path, longPath, LEN);

	dest.Copy(longPath);
	return (ret == 0);
}


bool initialize() 
{
	// ロケールの設定
	// ライブラリのバグがあるので 
	// Visual C++ 2005 Express Edition ではとりあえず使えない
	//_tsetlocale( LC_ALL, _T("japanese") );
	SetConsoleOutputCP(CP_UTF8);

	SetCurrentDirectoryEx(_T(""));

	// iniファイルより設定を読み込む
	Profile profile;
	String Path;
	GetModuleFileName(NULL, Path);
	RemoveExtension(Path);
	Path.Cat(_T(".ini"));
	profile.Open(Path.c_str());

	g_PriorityClass = profile.GetInt(_T("Setting"), _T("PriorityClass"), 0);

	g_SearchSubfolders = (profile.GetInt(_T("Setting"), _T("SearchSubfolders"), 0) != 0);

	g_DeletesEmptyFolder = (profile.GetInt(_T("Setting"), _T("DeletesEmptyFolder"), 0) != 0);
	UsesRecycleBin = (profile.GetInt(_T("Setting"), _T("UsesRecycleBin"), 1) != 0);
	g_RemovesRedundantFolder = profile.GetInt(_T("Setting"), _T("RemovesRedundantFolder"), 0);
	g_ShowsProgress = (profile.GetInt(_T("Setting"), _T("ShowsProgress"), 0) != 0);
	g_CompressLevel = profile.GetInt(_T("Setting"), _T("CompressLevel"), 0);
	g_BatchDecompress = (profile.GetInt(_T("Setting"), _T("BatchDecompress"), 0) != 0);

	g_FileTimeMode = profile.GetInt(_T("Setting"), _T("FileTimeMode"), 0);
	g_FolderTimeMode = profile.GetInt(_T("Setting"), _T("FolderTimeMode"), 0);
	g_FileAttribute = profile.GetInt(_T("Setting"), _T("FileAttribute"), -1);
	g_FolderAttribute = profile.GetInt(_T("Setting"), _T("FolderAttribute"), -1);

	g_WarningLevel = profile.GetInt(_T("Setting"), _T("WarningLevel"), 0);

	g_RecyclesIndividualFiles = (profile.GetInt(_T("Setting"), _T("RecyclesIndividualFiles"), 0) != 0);

	profile.GetString(_T("Setting"), _T("AppendFile"), _T(""), g_AppendFile);
	profile.GetString(_T("Setting"), _T("ExcludeFile"), _T(""), g_ExcludeFile);
	profile.GetString(_T("Setting"), _T("RemarkableFile"), _T(""), g_RemarkableFile);

	profile.GetString(_T("Setting"), _T("FileTime"), _T(""), g_FileTime);
	profile.GetString(_T("Setting"), _T("FolderTime"), _T(""), g_FolderTime);

	g_ExitsImmediately = (profile.GetInt(_T("Setting"), _T("ExitsImmediately"), 0) != 0);

	g_PreCheck = (profile.GetInt(_T("Setting"), _T("PreCheck"), 1) != 0);

	g_IgnoresEncryptedFiles = (profile.GetInt(_T("Setting"), _T("IgnoresEncryptedFiles"), 1) != 0);

	g_Compress7Zip = (profile.GetInt(_T("Setting"), _T("Compress7Zip"), 0) != 0);
	g_OutputTextFile = (profile.GetInt(_T("Setting"), _T("OutputTextFile"), 0) != 0);
	if (g_OutputTextFile)
	{
		g_ExitsImmediately = 1;
	}

	if(g_OutputTextFile)
	{
#ifdef _UNICODE
#else
		ofstream ofs;
#endif
		ofs.open(_T("Log.txt"));
#ifdef _UNICODE
		ofs.imbue({ {}, new std::codecvt_utf8<wchar_t, 0x10FFFF, std::generate_header> });
#endif
		cout.rdbuf(ofs.rdbuf());
	}
	else
	{
#ifdef _UNICODE
		_setmode(_fileno(stdout), _O_U8TEXT);
#endif
	}

	g_ConvertFileName = (profile.GetInt(_T("Setting"), _T("ConvertFileName"), 0) != 0);
	if (g_ConvertFileName) {
		profile.GetString(_T("Setting"), _T("NkfOption"), _T(""), g_NkfOption);
		nkf = new NkfDll;
		if (!nkf->init(g_NkfOption)) {
			cout << _T("nkf32.dllが見つかりません") << endl;
			return false;
		}
	}

	g_CompressWithTemporaryFolder = (profile.GetInt(_T("Setting"), _T("CompressWithTemporaryFolder"), 0) != 0);
	g_CheckArchive = (profile.GetInt(_T("Setting"), _T("CheckArchive"), 0) != 0);

	if (g_CompressLevel != -1 && 
		g_CompressLevel != 0 && 
		g_CompressLevel != 1 && 
		g_CompressLevel != 9) 
	{
		cout << _T("圧縮レベルの指定が不正") << endl;
		return false;
	}

	if (g_WarningLevel != 0 && 
		g_WarningLevel != 1 && 
		g_WarningLevel != 2) 
	{
		cout << _T("警告レベルの指定が不正") << endl;
		return false;
	}

	// プロセスの優先順位クラスを設定
	HANDLE hProcess = GetCurrentProcess();
	DWORD priorityClass = NORMAL_PRIORITY_CLASS;
	switch (g_PriorityClass) {
	case 0:
		priorityClass = NORMAL_PRIORITY_CLASS;
		break;
	case 1:
		priorityClass = IDLE_PRIORITY_CLASS;
		break;
	case 2:
		priorityClass = HIGH_PRIORITY_CLASS;
		break;
	case 3:
		priorityClass = REALTIME_PRIORITY_CLASS;
		break;
	case 4:
		priorityClass = BELOW_NORMAL_PRIORITY_CLASS;
		break;
	case 5:
		priorityClass = ABOVE_NORMAL_PRIORITY_CLASS;
		break;
	default:
		cout << _T("プロセスの優先度の指定が不正") << endl;
		break;
	}

	if (priorityClass != NORMAL_PRIORITY_CLASS) {
		if (SetPriorityClass(hProcess, priorityClass) == 0) {
			cout << _T("プロセスの優先順位クラスの設定に失敗") << endl;
		}
	}

	return true;
}



void analyzePath(vector<String>& src, vector<String>& dest) {
	vector<String>::iterator it;
	for (it=src.begin(); it!=src.end(); ++it) {
		switch(GetPathAttribute(it->c_str())) {
		case PATH_INVALID:
			// 無効なパス
			break;
		case PATH_FILE:
			dest.push_back(*it);
			break;
		case PATH_FOLDER:
			// フォルダ内のファイルを追加
			analyzePathSub(it->c_str(), dest);
			break;
		}
	}
}

bool analyzePathSub(LPCTSTR src, vector<String>& dest) {
	WIN32_FIND_DATA fd;
	HANDLE hSearch;

	// 探索パスを作成
	String path(src);
	FormatTooLongPath(path);
	CatPath(path, _T("*.*"));

	hSearch = FindFirstFile(path.c_str(), &fd);

	if (hSearch==INVALID_HANDLE_VALUE) return false;

	bool succeeds = true;
	do{
		if ( (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=0 ){
			if (g_SearchSubfolders) {
				// ルート及びカレントでないフォルダかどうかチェック
				if (lstrcmp(fd.cFileName,_T("."))!=0 && lstrcmp(fd.cFileName,_T("..")) != 0) {
					String searchPath(src);
					CatPath(searchPath, fd.cFileName);
					if (!analyzePathSub(searchPath.c_str(), dest)) {
						succeeds = false;
						break;
					}
				}
			}
		}else{
			String str(src);
			CatPath(str, fd.cFileName);
			dest.push_back(str);
		}
	} while (FindNextFile(hSearch,&fd)!=0);

	FindClose(hSearch);

	return succeeds;
}



struct FileSortCriterion : public binary_function <String, String, bool> {
	bool operator()(const String& _Left, const String& _Right) const 
	{
		return ComparePath(_Left.c_str(), _Right.c_str(), false, false);
	}
};

#include <fstream>
int main(int, char*, char*) 
{
	if (!initialize()) {
		return 0;
	}

#ifdef _DEBUG
	cout << _T("プロセスにアタッチして下さい") << endl;
	getchar();
#endif

	vector<String> arguments;
	vector<String> files;
	String str;

	// コマンドラインにファイル・フォルダの指定はないか
	void* pv;
	pv = AllocCommandLine();
	int u = GetStringFromCommandLine(pv, 0xFFFFFFFF, str);
	if(u >= 2){
		for (int i=1; i<u; ++i) {
			GetStringFromCommandLine(pv, i, str);
			arguments.push_back(str);
		}
	}
	FreeCommandLine(pv);

	analyzePath(arguments, files);

	#ifdef _DEBUG
	{
		vector<String>::iterator it;
		int i;
		for (i = 0, it = files.begin(); it != files.end(); ++i, ++it) {
			cout << i << _T(" ") << it->c_str() << endl;
		}
	}
	#endif

	// ここで files にはファイルのみでフォルダは含まれていないはず
	// エクスプローラと同じにソート
	sort(files.begin(), files.end(), FileSortCriterion());

	vector<String>::iterator it;
	for (it=files.begin(); it!=files.end(); ++it) {
		cout << it->c_str() << endl;
		process(it->c_str());
		cout << endl;
	}

	if (g_ExitsImmediately) {
		cout << _T("完了") << endl;
	}
	else
	{
		cout << _T("エンターキーを押してください 終了します") << endl;
		getchar();
	}

	return 0;
}




// エクスプローラと同じ
// フォルダにフォルダ内のファイルが続くことが保証される
struct IndividualInfoSortCriterion : public binary_function <boost::shared_ptr<IndividualInfo>, boost::shared_ptr<IndividualInfo>, bool> {
	bool operator()(const boost::shared_ptr<IndividualInfo>& _Left, const boost::shared_ptr<IndividualInfo>& _Right) const
	{
		return ComparePath(_Left->getFullPath(), _Right->getFullPath(), 
			(_Left->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0, 
			(_Right->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0);
	}
};

// 書庫内のファイルの情報を得る
// ファイルオープン済みのArchiveDllを渡す
bool GetIndividualInfo(ArchiveDll& rArchiveDll) 
{
	g_compressFileList.clear();

	// アーカイブ内の全ファイル・フォルダを取得
	INDIVIDUALINFO ii;
	int ret = rArchiveDll.findFirst(_T("*"), &ii);

	for (;;) {
		if (ret == 0) {
			// INDIVIDUALINFO 内の属性はフォルダ属性が正確でなかったりするのでこっちで取得
			int attribute = rArchiveDll.getAttribute();
			DWORD dwAttribute = 0;
			if ((attribute & FA_RDONLY) != 0) dwAttribute |= FILE_ATTRIBUTE_READONLY;
			if ((attribute & FA_HIDDEN) != 0) dwAttribute |= FILE_ATTRIBUTE_HIDDEN;
			if ((attribute & FA_SYSTEM) != 0) dwAttribute |= FILE_ATTRIBUTE_SYSTEM;
			if ((attribute & FA_DIREC) != 0) dwAttribute |= FILE_ATTRIBUTE_DIRECTORY;
			if ((attribute & FA_ARCH) != 0) dwAttribute |= FILE_ATTRIBUTE_ARCHIVE;

			// パスワードつきか
			bool isEncrypted = ((attribute & FA_ENCRYPTED) != 0);

			// ボリュームラベルとのことだが不明
			if ((attribute & FA_LABEL) != 0) dwAttribute |= 0;

			if (dwAttribute == 0) FILE_ATTRIBUTE_NORMAL;

			g_compressFileList.push_back(boost::shared_ptr<IndividualInfo>(new IndividualInfo(ii, dwAttribute, isEncrypted)));
		} else if(ret == -1) {
			// 検索終了
			break;
		} else {
			// エラー
			cout << _T("書庫内検索中にエラーが起きました") << endl;
			return false;
		}
		ret = rArchiveDll.findNext(&ii);
	}

	// ソート
	g_compressFileList.sort(IndividualInfoSortCriterion());

	#ifdef _DEBUG
	{
		cout << _T("フォルダ追加前") << endl;
		list<boost::shared_ptr<IndividualInfo> >::iterator it;	
		for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
			cout << (*it)->getFullPath() << endl;
		}
	}
	#endif

	AddLackFolder();

	#ifdef _DEBUG
	{
		cout << _T("フォルダ追加後") << endl;
		list<boost::shared_ptr<IndividualInfo> >::iterator it;	
		for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
			cout << (*it)->getFullPath() << endl;
		}
	}
	#endif

	return true;
}


// 指定のフォルダ内のファイル・フォルダの情報取得
bool GetIndividualInfo(LPCTSTR path) 
{
	g_compressFileList.clear();

	if (!GetIndividualInfoSub(path, path)) {
		cout << _T("ファイル情報取得中にエラーが起きました") << endl;
		return false;
	}

	// ソート
	g_compressFileList.sort(IndividualInfoSortCriterion());

	#ifdef _DEBUG
	{
		list<boost::shared_ptr<IndividualInfo> >::iterator it;	
		for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
			cout << (*it)->getFullPath() << endl;
		}
	}
	#endif

	return true;
}


bool GetIndividualInfoSub(LPCTSTR searchPath, LPCTSTR basePath)
{
	WIN32_FIND_DATA fd;
	String path;
	HANDLE hSearch;

	// 探索パスを作成
	path = searchPath;
	CatPath(path, _T("*.*"));

	hSearch = FindFirstFile(path.c_str(), &fd);

	if (hSearch==INVALID_HANDLE_VALUE) {
		return false;
	}

	// ドライブのみの場合は\で終わりそれ以外は\で終わらないのでその調整
	int basePathLen = lstrlen(basePath);
	if (basePathLen <= lstrlen(_T("C:\\"))) {
	} else {
		basePathLen++;
	}

	do{
		// ディレクトリか、普通のファイルか
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=0){
			// ディレクトリであった

			// ルート及びカレントでないフォルダかどうかチェック
			if (lstrcmp(fd.cFileName,_T(".")) == 0 || lstrcmp(fd.cFileName, _T("..")) == 0) {
				 continue;
			}

			path = searchPath;
			CatPath(path, fd.cFileName);

			String destPath = path.c_str();
			destPath.MakeMid(basePathLen);

			g_compressFileList.push_back(boost::shared_ptr<IndividualInfo>(new IndividualInfo(destPath.c_str(), fd.ftLastWriteTime, fd.dwFileAttributes, 0, false)));

			if (!GetIndividualInfoSub(path.c_str(), basePath)) {
				FindClose(hSearch);
				return false;
			}
		} else {
			// 普通のファイルであった
			path = searchPath;
			CatPath(path, fd.cFileName);

			String destPath = path;
			destPath.MakeMid(basePathLen);

			g_compressFileList.push_back(boost::shared_ptr<IndividualInfo>(new IndividualInfo(destPath.c_str(), fd.ftLastWriteTime, fd.dwFileAttributes, 0, false)));
		}
	} while (FindNextFile(hSearch,&fd)!=0);

	FindClose(hSearch);

	return (GetLastError() == ERROR_NO_MORE_FILES);
}




// リストから空フォルダを取り除く
bool deleteEmptyFolder(LPCTSTR basePath) {
	if (g_compressFileList.empty()) {
		return true;
	}

	list<boost::shared_ptr<IndividualInfo> >::iterator cur;
	list<boost::shared_ptr<IndividualInfo> >::iterator next;

	next = g_compressFileList.end();
	cur = next;
	cur--;
	for (;;) {
		// 次の要素がディレクトリ内のものをさしていなければ空フォルダ
		if (((*cur)->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			if (next == g_compressFileList.end()
			 || (_tcsncmp((*cur)->getFullPath(), (*next)->getFullPath(), lstrlen((*cur)->getFullPath())) != 0)
			 || (*((*next)->getFullPath() + lstrlen((*cur)->getFullPath())) != _TCHAR('\\')) ) {
				if (g_BatchDecompress) {
					String path = basePath;
					CatPath(path, (*cur)->getFullPath());
					if(!RemoveDirectory(path.c_str())) {
						cout << _T("空フォルダ ") << (*cur)->getFullPath() << _T(" の削除に失敗") << endl;
						return false;
					}
				}

				cout << _T("空フォルダ ") << (*cur)->getFullPath() << _T(" を削除") << endl;
				g_compressFileList.erase(cur);
				if (g_compressFileList.empty()) break;
				cur = next;
			} else {
				next = cur;
			}
		} else {
			next = cur;
		}
		if (cur == g_compressFileList.begin()) break;
		cur--;
	}
	return true;
}


// リストに空フォルダがあればfalseを返す
bool ExistsEmptyFolder() {
	if (g_compressFileList.empty()) {
		return false;
	}

	list<boost::shared_ptr<IndividualInfo> >::iterator cur;
	list<boost::shared_ptr<IndividualInfo> >::iterator next;

	next = g_compressFileList.end();
	cur = next;
	cur--;
	for (;;) {
		// 次の要素がディレクトリ内のものをさしていなければ空フォルダ
		if (((*cur)->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			if (next == g_compressFileList.end()) {
				return true;
			} else if ((_tcsncmp((*cur)->getFullPath(), (*next)->getFullPath(), lstrlen((*cur)->getFullPath())) != 0) ||
				(*((*next)->getFullPath() + lstrlen((*cur)->getFullPath())) != _TCHAR('\\')) ) {
				return true;
			} else {
				next = cur;
			}
		} else {
			next = cur;
		}
		if (cur == g_compressFileList.begin()) break;
		cur--;
	}
	return false;
}

bool ExistsRedundantFolde() {
	String path;
	GetRedundantPath(path);
	return (path.IsEmpty() == false);
}

// 冗長フォルダのパスを得る
void GetRedundantPath(String& rRedundantPath) {
	rRedundantPath.Empty();

	bool isEmpty = true;
	list<boost::shared_ptr<IndividualInfo> >::iterator it;
	for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
		if (((*it)->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) == 0) {
			// ファイル

			// ファイルを見つけたらそれ以上の冗長パスの伸びは無い
			TCHAR path[MAX_PATH + 1];
			PathCommonPrefix((*it)->getFullPath(), rRedundantPath.c_str(), path);

			if (lstrlen(path) < rRedundantPath.GetLength()) {
				rRedundantPath = path;
				if (*path == _TCHAR('\0')) break;
			}
			// そのまま
			isEmpty = false;

		} else {
			// フォルダ
			TCHAR path[MAX_PATH + 1];
			PathCommonPrefix((*it)->getFullPath(), rRedundantPath.c_str(), path);

			if (lstrlen(path) < rRedundantPath.GetLength()) {
				rRedundantPath = path;
				if (*path == _TCHAR('\0')) break;
				isEmpty = false;
			} else {
				if (isEmpty) {
					rRedundantPath = (*it)->getFullPath();
				} else {
					// そのまま
				}
			}
		}
	}
	FixPath(rRedundantPath);
}

// wstring にMBCS文字列をUNICODEに変換してセットする
void setMultiByteString(wstring& rWString, const char* string) {
	rWString.erase();

	// Unicodeに必要な文字数の取得
	int len = ::MultiByteToWideChar(CP_THREAD_ACP, 0, string, -1, NULL, 0);
	WCHAR* pUnicode = new WCHAR[len];

	//変換
	::MultiByteToWideChar(CP_THREAD_ACP, 0, string, (int)strlen(string)+1, pUnicode, len);

	rWString = pUnicode;

	delete	pUnicode;
}

// -1 : エラー
//  0 : なにもない
//  1 : 中断すべき
int CheckRemarkableFile() 
{
	int r = CheckRemarkableFileSub();
	if (r == 0) {
		return 0;
	} else if (r == 1) {
		switch (g_WarningLevel) {
		case 1:
		{
			// メッセージと構成出力
			cout << _T("注目ファイルのみで構成されています") << endl;
			list<boost::shared_ptr<IndividualInfo> >::iterator it;
			for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
				cout << (*it)->getFullPath() << endl;
			}
			break;
		}
		case 2:
		{
			// メッセージと構成出力
			cout << _T("注目ファイルが含まれています") << endl;
			list<boost::shared_ptr<IndividualInfo> >::iterator it;
			for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
				cout << (*it)->getFullPath() << endl;
			}
			break;
		}
		default:
			break;
		}
		return 1;
	} else {
		return -1;
	}
}

//  0 : 正常終了
//  1 : 条件にかかった
// -1 : エラー
int CheckRemarkableFileSub() 
{
	if (g_WarningLevel == 0) return 0;
	if (g_RemarkableFile.IsEmpty()) return 0;

	#ifdef _UNICODE
		wstring append(g_RemarkableFile.c_str());
	#else
		wstring append;
		setMultiByteString(append, g_RemarkableFile.c_str());
	#endif

	try {
		wregex regex(append, regex_constants::normal | regex_constants::icase);

		// 正規表現とファイル名がマッチしなかったら取り除く
		list<boost::shared_ptr<IndividualInfo> >::iterator it;
		for (it=g_compressFileList.begin(); it!=g_compressFileList.end(); ++it) {
			// フォルダは無視
			if (((*it)->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				continue;
			}

			#ifdef _UNICODE
				wstring filename((*it)->getFullPath());
			#else
				wstring filename;
				setMultiByteString(filename, (*it)->getFullPath());
			#endif

			wsmatch results;
			if (regex_match(filename, results, regex)) {
				if (g_WarningLevel == 2) return 1;
			} else {
				if (g_WarningLevel == 1) return 0;
			}
		}
		if (g_WarningLevel == 1) return 1;
	}

	catch (boost::bad_expression) {
		cout << _T("正規表現のコンパイルに失敗") << endl;
		return -1;
	}

	return 0;
}

// 追加ファイルをチェック
bool CheckAppendFile(LPCTSTR basePath) {
	if (g_AppendFile.IsEmpty()) return true;

	#ifdef _UNICODE
		wstring append(g_AppendFile.c_str());
	#else
		wstring append;
		setMultiByteString(append, g_AppendFile.c_str());
	#endif

	try {
		wregex regex(append, regex_constants::normal | regex_constants::icase);

		// 正規表現とファイル名がマッチしなかったら取り除く
		list<boost::shared_ptr<IndividualInfo> >::iterator it = g_compressFileList.begin();
		while (it != g_compressFileList.end()) {
			if (((*it)->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				++it;
				continue;
			}

			#ifdef _UNICODE
				wstring filename((*it)->getFullPath());
			#else
				wstring filename;
				setMultiByteString(filename, (*it)->getFullPath());
			#endif

			wsmatch results;
			if (regex_match(filename, results, regex)) {
				++it;
			} else {
				// 一括解凍の場合は解凍済みの実際のファイルも削除
				if (g_BatchDecompress) {
					String path = basePath;
					CatPath(path, (*it)->getFullPath());

					// 読み取り専用ファイルも消せるように
					// DeleteFile() ではなく DeleteFileOrFolder() を使用
					bool usesRecycleBin = g_RecyclesIndividualFiles;
					if(!DeleteFileOrFolder(path.c_str(), usesRecycleBin)) {
						cout << _T("ファイル ") << (*it)->getFullPath() << _T(" の削除に失敗") << endl;
						return false;
					}
				}
				cout << _T("ファイル ") << (*it)->getFullPath() << _T(" を削除") << endl;
				it = g_compressFileList.erase(it);
			}
		}
	}

	catch (boost::bad_expression) {
		cout << _T("正規表現のコンパイルに失敗") << endl;
		return false;
	}

	return true;
}

// 追加ファイルをチェック
bool ExistsNotAppendFile() {
	#ifdef _UNICODE
		wstring append(g_AppendFile.c_str());
	#else
		wstring append;
		setMultiByteString(append, g_AppendFile.c_str());
	#endif

	try {
		wregex regex(append, regex_constants::normal | regex_constants::icase);

		list<boost::shared_ptr<IndividualInfo> >::iterator it = g_compressFileList.begin();
		while (it != g_compressFileList.end()) {
			if (((*it)->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				++it;
				continue;
			}

			#ifdef _UNICODE
				wstring filename((*it)->getFullPath());
			#else
				wstring filename;
				setMultiByteString(filename, (*it)->getFullPath());
			#endif

			wsmatch results;
			if (regex_match(filename, results, regex)) {
				++it;
			} else {
				return true;
			}
		}
	}

	// エラーはここでは出さず
	// チェックを中断させる形で返す
	catch (boost::bad_expression) {
		return true;
	}

	return false;
}

// 除外ファイルをチェック
bool CheckExcludeFile(LPCTSTR basePath) {
	if (g_ExcludeFile.IsEmpty()) return true;

	#ifdef _UNICODE
		wstring exclude(g_ExcludeFile.c_str());
	#else
		wstring exclude;
		setMultiByteString(exclude, g_ExcludeFile.c_str());
	#endif

	try {
		wregex regex(exclude, regex_constants::normal | regex_constants::icase);

		// 正規表現とファイル名がマッチしたら取り除く
		list<boost::shared_ptr<IndividualInfo> >::iterator it = g_compressFileList.begin();
		while (it != g_compressFileList.end()) {
			if (((*it)->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				++it;
				continue;
			}

			#ifdef _UNICODE
				wstring filename((*it)->getFullPath());
			#else
				wstring filename;
				setMultiByteString(filename, (*it)->getFullPath());
			#endif

			wsmatch results;
			if ( regex_match(filename, results, regex) ) {
				// 一括解凍の場合は解凍済みの実際のファイルも削除
				if (g_BatchDecompress) {
					String path = basePath;
					CatPath(path, (*it)->getFullPath());

					// 読み取り専用ファイルも消せるように
					// DeleteFile() ではなく DeleteFileOrFolder() を使用
					bool usesRecycleBin = g_RecyclesIndividualFiles;
					if(!DeleteFileOrFolder(path.c_str(), usesRecycleBin)) {
						cout << _T("ファイル ") << (*it)->getFullPath() << _T(" の削除に失敗") << endl;
						return false;
					}
				}
				cout << _T("ファイル ") << (*it)->getFullPath() << _T(" を削除") << endl;
				it = g_compressFileList.erase(it);
			} else {
				++it;
			}
		}
	}

	catch (boost::bad_expression) {
		cout << _T("正規表現のコンパイルに失敗") << endl;
		return false;
	}

	return true;
}


// 除外ファイルをチェック
bool ExistsExcludeFile() {
	#ifdef _UNICODE
		wstring exclude(g_ExcludeFile.c_str());
	#else
		wstring exclude;
		setMultiByteString(exclude, g_ExcludeFile.c_str());
	#endif

	try {
		wregex regex(exclude, regex_constants::normal | regex_constants::icase);

		// 正規表現とファイル名がマッチしたら取り除く
		list<boost::shared_ptr<IndividualInfo> >::iterator it = g_compressFileList.begin();
		while (it != g_compressFileList.end()) {
			if (((*it)->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				++it;
				continue;
			}

			#ifdef _UNICODE
				wstring filename((*it)->getFullPath());
			#else
				wstring filename;
				setMultiByteString(filename, (*it)->getFullPath());
			#endif

			wsmatch results;
			if ( regex_match(filename, results, regex) ) {
				return true;
			} else {
				++it;
			}
		}
	}

	// エラーはここでは出さず
	// チェックを中断させる形で返す
	catch (boost::bad_expression) {
		return true;
	}

	return false;
}


void GetToken(String& rDest, String& rSrc, LPCTSTR token) {
	rDest.Empty();
	int pos = rSrc.FindOneOf(token);
	if (pos == -1) return;
	rDest.NCopy(rSrc.c_str(), pos);
	rSrc.MakeMid(pos+1);
}

void StringToFileTime(LPCTSTR fileTime, FILETIME& rFileTime) {
	SYSTEMTIME st;

	String src(fileTime);
	String token;
	GetToken(token, src, _T("/"));
	st.wYear = (WORD)_ttoi(token.c_str());
	GetToken(token, src, _T("/"));
	st.wMonth = (WORD)_ttoi(token.c_str());
	GetToken(token, src, _T(" "));
	st.wDay = (WORD)_ttoi(token.c_str());
	GetToken(token, src, _T(":"));
	st.wHour = (WORD)_ttoi(token.c_str());
	GetToken(token, src, _T(":"));
	st.wMinute = (WORD)_ttoi(token.c_str());
	token = src;
	st.wSecond = (WORD)_ttoi(token.c_str());
	st.wDayOfWeek = 0;
	st.wMilliseconds = 0;

	FILETIME ft;
	SystemTimeToFileTime(&st, &ft);

	LocalFileTimeToFileTime(&ft, &rFileTime); //変換
}

// 文字列でファイルの更新日時を設定する
bool SetFileLastWriteTime(LPCTSTR filename, LPCTSTR lastWriteTime) {
	FILETIME ft;

	StringToFileTime(lastWriteTime, ft);

	HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		cout << _T("ファイルの更新日時変更に失敗しました") << endl;
		return false;
	}

	bool b = (SetFileTime(hFile, NULL, NULL, &ft) != 0);

	if (b == false) {
		cout << _T("ファイルの更新日時変更に失敗しました") << endl;
	}

	CloseHandle(hFile);

	return b;
}


DWORD MyFileAttributeToFileAttribute(int attribute, bool isFolder) {
	DWORD dw = 0;
	if (isFolder) dw = FILE_ATTRIBUTE_DIRECTORY;
	if ((attribute & FileAttribute::READ_ONLY) != 0)	dw |= FILE_ATTRIBUTE_READONLY;
	if ((attribute & FileAttribute::HIDDEN) != 0)		dw |= FILE_ATTRIBUTE_HIDDEN;
	if ((attribute & FileAttribute::SYSTEM) != 0)		dw |= FILE_ATTRIBUTE_SYSTEM;
	if ((attribute & FileAttribute::ARCHIVE) != 0)		dw |= FILE_ATTRIBUTE_ARCHIVE;
	if (dw == 0) dw = FILE_ATTRIBUTE_NORMAL;
	return dw;
}



bool AddLackFolder() {
	bool adds = false;
	
	String path;
	list<boost::shared_ptr<IndividualInfo> >::iterator it;
	for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
		if (((*it)->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			// フォルダを見つけたらパスを記憶
			// ただし、２階層一気に追加される可能性もあるので
			// １階層ずつチェック

			TCHAR common[MAX_PATH + 1];
			PathCommonPrefix((*it)->getFullPath(), path.c_str(), common);
			LPCTSTR p;
			if (*common == _TCHAR('\0')) {
				p = (*it)->getFullPath();
			} else {
				p = (*it)->getFullPath() + lstrlen(common) + 1;
			}
			if (strchrex(p, _TCHAR('\\')) == NULL) {
				// １階層しか変化していなければ
				// この前にフォルダを挿入する必要はない
				path = (*it)->getFullPath();
			} else {
				// フォルダが登録されていない
				String filePath = (*it)->getFullPath();
				RemoveFileName(filePath);

				adds = true;

				path = common;
				for (;;) {
					LPCTSTR p1 = filePath.c_str() + path.GetLength() + 1;
					LPCTSTR p2 = strchrex(p1, _TCHAR('\\'));
					if (p2 == NULL) {
						path = filePath;
					} else {
						path.NCopy(filePath.c_str(), path.GetLength() + (int)(p2 - p1) + 1);
					}
					it = g_compressFileList.insert(it, boost::shared_ptr<IndividualInfo>(new IndividualInfo(path.c_str(), FILE_ATTRIBUTE_DIRECTORY, false)));
					it++;
					if (p2 == NULL) break;
				}
			}

		} else {
			// ファイルを見つけたら
			// そのファイルを持つディレクトリがあるかチェックする
			String filePath = (*it)->getFullPath();
			RemoveFileName(filePath);
			int r = lstrcmpi(filePath.c_str(), path.c_str());
			if (r == 0) {
				// フォルダは登録済み
			} else if (r < 0) {
				// 上の階層のフォルダにあった
				path = filePath;
			} else {
				// フォルダが登録されていない
				adds = true;

				TCHAR common[MAX_PATH + 1];
				PathCommonPrefix((*it)->getFullPath(), path.c_str(), common);
				path = common;

				for (;;) {
					LPCTSTR p1 = filePath.c_str() + path.GetLength() + 1;
					LPCTSTR p2 = strchrex(p1, _TCHAR('\\'));
					if (p2 == NULL) {
						path = filePath;
					} else {
						path.NCopy(filePath.c_str(), path.GetLength() + (int)(p2 - p1) + 1);
					}
					it = g_compressFileList.insert(it, boost::shared_ptr<IndividualInfo>(new IndividualInfo(path.c_str(), FILE_ATTRIBUTE_DIRECTORY, false)));
					it++;
					if (p2 == NULL) break;
				}
			}
		}
	}
	
	return adds;
}




//  0:	書庫の情報を取得し 処理済み
//  1:	書庫の情報を取得し 未処理
// -1:	書庫の情報取得できず
int isProcessedArchive(LPCTSTR filename) 
{
	g_compressFileList.clear();

	ZipReader zip;

	if (!zip.open(filename)) {
		return -1;
	}

	bool isProcessed = true;

	if (g_CompressLevel != 0) {
		isProcessed = false;
	}

	boost::shared_ptr<IndividualInfo> pLastIndividualInfo;

	int localFileNumber = zip.getLocalFileNumber();
	for (int i=0; i<localFileNumber; ++i) {
		boost::shared_ptr<LocalFile> p = zip.getLocalFile(i);
		
		int compressLevel;
		if (p->getCompressionMethod() == 0) {
			compressLevel = 0;
		} else {
			compressLevel = -1;
		}
		bool isEncrypted = p->getGeneralPurposeBitFlag() & 1;

		boost::shared_ptr<IndividualInfo> pIndividualInfo(new IndividualInfo(p->getFileName(), p->getFileTime(), p->getExternalFileAttributes(), compressLevel, isEncrypted));
		g_compressFileList.push_back(pIndividualInfo);

		if (p->getVersionMadeBy() != 0x0014
		 || p->getCompressionMethod() != 0
		 || p->getGeneralPurposeBitFlag() != 0) 
		{
	 		isProcessed = false;
		}

		// internal file attributes は無圧縮でない場合検証できない
		if ((p->getExternalFileAttributes() & FILE_ATTRIBUTE_DIRECTORY) == 0) {
			if (p->getVersionNeededToExtract() != 0x000a) {
				isProcessed = false;
			}
			if (p->getInternalFileAttributes() != p->obtainInternalFileAttributes()) {
				isProcessed = false;
			}
		} else {
			if (p->getVersionNeededToExtract() != 0x0014) isProcessed = false;
			if (p->getInternalFileAttributes() != 0) isProcessed = false;
		}

		// ソートが不要かどうかチェック
		if (pLastIndividualInfo && 
			IndividualInfoSortCriterion()(pIndividualInfo, pLastIndividualInfo)) 
		{
			isProcessed = false;
		}

		/*
		if (pLastIndividualInfo && CompareLocalFileName(pIndividualInfo->getFullPath(), pLastIndividualInfo->getFullPath()) < 0) {
			isProcessed = false;
		}
		*/

		pLastIndividualInfo = pIndividualInfo;
	}

	// 書庫の情報を取得し終わったので
	// ここまでで処理済みでないと判断された場合抜ける
	if (isProcessed == false) {
		return 1;
	}

	// ソート
	// Zipとして正しくソートされていても処理用として正しくないかもしれないので再ソート
	//g_compressFileList.sort(IndividualInfoSortCriterion());

	#ifdef _DEBUG
	{
		list<boost::shared_ptr<IndividualInfo> >::iterator it;	
		for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
			cout << (*it)->getFullPath() << endl;
		}
	}
	#endif

	// 欠けているフォルダのチェック
	// フォルダが欠けている場合は追加と同時に処理済みではないと判断される
	if (AddLackFolder()) {
		return 1;
	}

	// 追加ファイルのチェック
	if (g_AppendFile.IsEmpty() == false) {
		if (ExistsNotAppendFile()) return 1;
	}

	// 除外ファイルのチェック
	if (g_ExcludeFile.IsEmpty() == false) {
		if (ExistsExcludeFile()) return 1;
	}

	// 空フォルダのチェック
	if (g_DeletesEmptyFolder) {
		if (ExistsEmptyFolder()) return 1;
	}

	// 冗長フォルダのチェック
	if (g_RemovesRedundantFolder != 0) {
		if (ExistsRedundantFolde()) return 1;
	}

	// 属性のチェック
	if (checkAttribute())
	{
		return 1;
	}

	// 更新日時のチェック
	if (checkUpdateDateTime())
	{
		return 1;
	}

	return 0;
}

bool checkAttribute()
{
	list<boost::shared_ptr<IndividualInfo> >::iterator it;
	for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
		if (((*it)->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			if (g_FolderAttribute == -1) continue;
			DWORD attribute = MyFileAttributeToFileAttribute(g_FolderAttribute, true);
			if (attribute != (*it)->getAttribute()) return true;
		}
		else {
			if (g_FileAttribute == -1) continue;
			DWORD attribute = MyFileAttributeToFileAttribute(g_FileAttribute, false);
			if (attribute != (*it)->getAttribute()) return true;
		}
	}

	return false;
}

bool checkUpdateDateTime()
{
	list<boost::shared_ptr<IndividualInfo> >::iterator it;
	for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
		if (((*it)->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			if (g_FolderTimeMode == 0) continue;

			if (g_FolderTimeMode == 1) {
				FILETIME ft;
				StringToFileTime(g_FolderTime.c_str(), ft);
				if (CompareFileTime((*it)->getLastWriteTime(), &ft) != 0) {
					return true;
				}
			}
			else if (g_FolderTimeMode == 2) {
				// フォルダの更新日時よりも新しいファイル・フォルダが
				// フォルダ以下にあったら中止
				// フォルダ以下の最新のファイル・フォルダとフォルダの更新日時が
				// 一致しない場合も中止
				FILETIME ft;
				ft.dwHighDateTime = 0;
				ft.dwLowDateTime = 0;

				list<boost::shared_ptr<IndividualInfo> >::iterator i = it;
				++i;
				for (; i != g_compressFileList.end(); ++i) {
					if (_tcsncmp((*it)->getFullPath(), (*i)->getFullPath(), lstrlen((*it)->getFullPath())) != 0 ||
						*((*i)->getFullPath() + lstrlen((*it)->getFullPath())) != _TCHAR('\\')) {
						break;
					}
					if (CompareFileTime((*it)->getLastWriteTime(), (*i)->getLastWriteTime()) == -1) {
						return true;
					}
					if (CompareFileTime(&ft, (*i)->getLastWriteTime()) == -1) {
						ft = *((*i)->getLastWriteTime());
					}
				}
				if (CompareFileTime(&ft, (*it)->getLastWriteTime()) != 0) {
					return true;
				}
			}
		}
		else {
			if (g_FileTimeMode == 0) continue;

			FILETIME ft;
			StringToFileTime(g_FileTime.c_str(), ft);
			if (CompareFileTime((*it)->getLastWriteTime(), &ft) != 0) {
				return true;
			}
		}
	}

	return false;
}



// パスワードダイアログ
String g_password;
String g_archive;
String g_file;
bool g_masksPassword = false;

// サイズ制限なしで結果を String に返す GetWindowText()
void GetWindowText(HWND hWnd, String& String)
{
	DWORD dwSize;
	DWORD dwRet;
	LPTSTR pszString;
	dwSize = 256;
	for(;;){
		pszString = new TCHAR [dwSize];

		// 関数が成功するとコピーされた文字列の文字数が返る(終端の NULL 文字は含まない)
		dwRet = GetWindowText(hWnd, pszString, dwSize);

		if(dwRet == 0){
			// エラーや空の文字列
			delete [] pszString;
			String.Empty();
			return;
		}else if(dwRet == dwSize - 1){
			// バッファが小さかった
			delete [] pszString;
			dwSize*=2;
		}else{
			// 成功
			break;
		}
	}
	String = pszString;
	delete [] pszString;
}

INT_PTR CALLBACK PasswordDialogProc(HWND hwndDlg,
	UINT uMsg,
	WPARAM wParam,
	LPARAM /* lParam */) {
	switch (uMsg) {
	case WM_INITDIALOG:
		SetWindowText(GetDlgItem(hwndDlg, IDC_STATIC1), g_archive.c_str());
		SetWindowText(GetDlgItem(hwndDlg, IDC_STATIC2), g_file.c_str());
		if (g_masksPassword) {
			CheckDlgButton(hwndDlg, IDC_CHECK1, BST_CHECKED);
			SendMessage(GetDlgItem(hwndDlg, IDC_EDIT1), (UINT)EM_SETPASSWORDCHAR, (WPARAM)'*', (LPARAM) 0);  
		}
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			GetWindowText(GetDlgItem(hwndDlg, IDC_EDIT1), g_password);
			g_masksPassword = (IsDlgButtonChecked(hwndDlg, IDC_CHECK1) == BST_CHECKED);
			EndDialog(hwndDlg, IDOK);
			break;
		case IDCANCEL:
			g_password.Empty();
			g_masksPassword = (IsDlgButtonChecked(hwndDlg, IDC_CHECK1) == BST_CHECKED);
			EndDialog(hwndDlg, IDCANCEL);
			break;
		case IDC_CHECK1:
			if (IsDlgButtonChecked(hwndDlg, IDC_CHECK1) == BST_CHECKED) {
				SendMessage(GetDlgItem(hwndDlg, IDC_EDIT1), (UINT)EM_SETPASSWORDCHAR, (WPARAM)'*', (LPARAM)0);  
			}else{
				SendMessage(GetDlgItem(hwndDlg, IDC_EDIT1), (UINT)EM_SETPASSWORDCHAR, (WPARAM)0, (LPARAM)0);
			}
			InvalidateRect(GetDlgItem(hwndDlg, IDC_EDIT1), NULL, FALSE);
			return TRUE;

		default:
			return FALSE;
		}
    default:
        return FALSE;
    }
}

// IDOK : 入力された
// IDCANCEL : キャンセルされた
int getPassword(LPCTSTR archive, LPCTSTR file, String& password) {
	g_archive = archive;
	g_file = file;
	INT_PTR r = DialogBox(NULL, MAKEINTRESOURCE(IDD_DIALOG1), NULL, (DLGPROC)PasswordDialogProc);
	password = g_password;
	return (int)r;
}

// 書庫を処理する
bool process(LPCTSTR filename) 
{
	if (!FileExists(filename)) {
		cout << _T("書庫が見つかりません") << endl;
		return false;
	}

	// 書庫から構成ファイルのリストを得て
	// そのリストを参照、操作し、必要なもののみ解凍
	// （解凍の際、冗長フォルダも考慮して展開する）
	// 解凍されたものを指定の圧縮率で圧縮する

	// 相対パスでの指定もあり得るので
	// アーカイブファイル名をフルパスにする
	String srcFilename(filename);
	GetFullPath(srcFilename);

	if (srcFilename.GetLength() > MAX_PATH) {
		cout << _T("書庫のパスが長すぎます") << endl;
		return false;
	}

	// 処理済みかどうかチェック
	int result = isProcessedArchive(srcFilename.c_str());
	if (g_PreCheck && result == 0) {
		cout << _T("処理は不要です") << endl;

		// 注目ファイルのチェック
		CheckRemarkableFile();

		// ファイル名をチェック
		String ext;
		GetExtention(srcFilename.c_str(), ext);
		if (lstrcmpi(ext.c_str(), _T(".zip")) == 0) {
			return true;
		}

		// 元の書庫削除後目標の名前に変える
		String destFilename(srcFilename);
		RemoveExtension(destFilename);
		destFilename.Cat(_T(".zip"));
		EvacuateFileName(destFilename);
		if (MoveFile(srcFilename.c_str(), destFilename.c_str()) == 0) {
			cout << _T("書庫のリネームに失敗しました") << endl;
			cout << destFilename.c_str() << endl;
		}
		return true;
	}

	ArchiveDll* pArchiveDll;
	pArchiveDll = g_ArchiveDllManager.getSuitableArchiveDll(srcFilename.c_str());
	if (pArchiveDll == NULL) {
		cout << _T("書庫を扱うのに適当なDLLが見つかりません") << endl;
		return false;
	}

	pArchiveDll->setArchiveFilename(srcFilename.c_str());

	// 解凍先に使うテンポラリフォルダを得る
	String destPath;
	getTemporaryPath(destPath);

	// 一意な名前のフォルダを作る
	CatPath(destPath, _T("uz"));
	EvacuateFolderName(destPath);

	// ファイル名を元にフォルダ名を作成する場合
	//CatPath(destPath, srcFilename.c_str());
	//RemoveExtension(destPath);
	//evacuateFolderName(destPath);

	// destPath に一意なフォルダ名が入ったのでそのフォルダを作成
	TemporaryFolder temp_folder;
	if (!temp_folder.create(destPath.c_str())) {
		cout << _T("テンポラリフォルダ作成に失敗しました") << endl;
		return false;
	}

	if (g_BatchDecompress) {
		// 書庫内のファイル名によっては UNZIP32.DLL で解凍を試みると異常終了するので
		// 書庫内のファイル名を取得できている場合そのファイル名をチェック
		if (result >= 0) {
			list<boost::shared_ptr<IndividualInfo> >::iterator it;
			for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
				bool error = false;
				String filename = (*it)->getFilename();
				String path = destPath;
				CatPath(path, filename.c_str());

				// 書庫内のファイルのパスが長すぎないか
				if (path.GetLength() > 255) {
					error = true;
				}

				// NTFSのファイル名に使えない文字は「?  "  /  \  <  >  *  |  :」
				// パスの区切りに使われている '/' と '\\' のチェックは省略
				if (filename.FindOneOf(_T(":")) != -1) {
					error = true;
				}
				if (path.FindOneOf(_T("?/<>*|")) != -1) {
					error = true;
				}

				if (error) {
					cout << _T("解凍時にエラーが発生しました") << endl;
					return false;
				}
			}
		} else {
			// g_compressFileList が取得できていないときはチェックはあきらめる
			// 失敗するのは UNZIP32.DLL での時であるが
			// 基本的にZIPファイルであるならば取得出来ているはず
			// コストをかけてまでチェックしない
		}

		// 一括解凍
		int r = pArchiveDll->extract(temp_folder.getPath(), g_ShowsProgress, true);

		if (r != 0) {
			cout << _T("解凍時にエラーが発生しました") << endl;
			return false;
		}

		// ファイル情報取得
		if (!GetIndividualInfo(temp_folder.getPath())) {
			return false;
		}
	} else {
		// 解凍するファイルの情報を未取得ならば取得
		if (result == 1) {
			// 得た情報は不完全である可能性があるので
			// ソートして欠けているフォルダを補う
			g_compressFileList.sort(IndividualInfoSortCriterion());
			AddLackFolder();
		} else if (result == -1) {
			if (!pArchiveDll->openArchive(NULL, 0)) {
				cout << _T("書庫が開けません") << endl;
				return false;
			}
			if (!GetIndividualInfo(*pArchiveDll)) {
				return false;
			}
			pArchiveDll->closeArchive();
		}
	}

	// この時点で書庫内にファイルが見つからなかった場合
	// 空の書庫、もしくは、壊れて空に見える書庫という事になる
	if (g_compressFileList.empty()) {
		cout << _T("書庫内にファイルが見あたりません") << endl;
		return false;
	}

	// 注目ファイルのチェック
	int r = CheckRemarkableFile();
	if (r == 0) {
	} else if (r == 1) {
		return true;
	} else {
		return false;
	}

	// 追加ファイルのチェック
	if (CheckAppendFile(temp_folder.getPath()) == false) {
		return false;
	}

	// 除外ファイルのチェック
	if (CheckExcludeFile(temp_folder.getPath()) == false) {
		return false;
	}

	// 空フォルダの除外
	if (g_DeletesEmptyFolder) {
		if (deleteEmptyFolder(temp_folder.getPath()) == false) {
			return false;
		}
	}

	// パスワード付きファイルのチェック
	if (g_IgnoresEncryptedFiles && g_BatchDecompress == false) {
		list<boost::shared_ptr<IndividualInfo> >::iterator it = g_compressFileList.begin();
		while (it != g_compressFileList.end()) {
			if ((*it)->isEncrypted()) {
				cout << _T("暗号化されたファイル ") << (*it)->getFullPath() << _T(" を削除") << endl;
				it = g_compressFileList.erase(it);
			} else {
				++it;
				continue;
			}
		}
	}

	#ifdef _DEBUG
	{
		cout << _T("冗長フォルダ検出前") << endl;
		list<boost::shared_ptr<IndividualInfo> >::iterator it;	
		for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
			cout << (*it)->getFullPath() << endl;
		}
	}
	#endif

	// 冗長フォルダの検出
	String redundantPath;
	int redundantPathLen;
	if (g_RemovesRedundantFolder != 0) {
		GetRedundantPath(redundantPath);
		if (redundantPath.IsEmpty() == false) {
			cout << _T("冗長フォルダ ") << redundantPath.c_str() << _T(" を短縮") << endl;
		}
	}
	// 冗長パスの長さを得る
	// ただし冗長パス分パスを削るのに使うのでパスをつなぐ'\\'の分だけインクリメント
	redundantPathLen = redundantPath.GetLength();
	if (redundantPathLen != 0 && redundantPath.ReverseFind(TCHAR('\\')) != redundantPathLen - 1) {
		redundantPathLen += 1;
	}

	// 解凍

	// 指定のファイルを指定の場所に解凍
	if (g_BatchDecompress == false) {
		bool hasValidPassword = false;
		String password;

		list<boost::shared_ptr<IndividualInfo> >::iterator it;
		for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
			String extractFilename = (*it)->getFullPath();
			if (extractFilename.GetLength() <= redundantPathLen) continue;

			String destPath(temp_folder.getPath());
			CatPath(destPath, extractFilename.c_str() + redundantPathLen);

			// TODO ファイル名が不正な文字が使われていないかチェックすべきか

			if (((*it)->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				// フォルダはパスがかかっていても関係ない
				// UNLHA32.dll はフォルダを解凍できないようなので自前で作成
				if (CreateDirectory(destPath.c_str(), NULL) == 0) {
					cout << _T("解凍時にエラーが発生しました") << endl;
					return false;
				}
			} else {
				// 解凍先はファイル名を取り除いた場所になる
				RemoveFileName(destPath);

				bool isEncrypted = (*it)->isEncrypted();

				RETRY:

				// パスワード処理
				if (isEncrypted) {
					if (hasValidPassword == false) {
						if (getPassword(pArchiveDll->getArchiveFilename(), (*it)->getFullPath(), password) == IDCANCEL) {
							cout << _T("パスワードの入力がキャンセルされました") << endl;
							return false;
						}
					}

					int r = pArchiveDll->extract(extractFilename.c_str(), destPath.c_str(), g_ShowsProgress, password.c_str());
					if (r == 0) {
						hasValidPassword = true;
						continue;
					} else if (r == -1) {
						// パスワードが違う
						if (hasValidPassword) {
							hasValidPassword = false;
						} else {
							cout << _T("パスワードが違います") << endl;
						}
						goto RETRY;
					} else {
						cout << _T("解凍時にエラーが発生しました") << endl;
						return false;
					}
				} else {
					int r = pArchiveDll->extract(extractFilename.c_str(), destPath.c_str(), g_ShowsProgress);
					if (r==0) {
						// 成功
						continue;
					} else if (r == -1) {
						isEncrypted = true;
						goto RETRY;
					} else {
						cout << _T("解凍時にエラーが発生しました") << endl;
						return false;
					}
				}
			}
		}
	}

	if (!checkArchiveDllRunning(pArchiveDll))
	{
		return false;
	}

	setFileFolderDateTime(temp_folder, redundantPathLen);

	setFileFolderAttribute(temp_folder, redundantPathLen);

	convertFileNameInArchive(temp_folder, redundantPathLen);

	// 圧縮しようとするフォルダ内が
	// 指定の拡張子のファイル(例えばアーカイブ)のみだったら
	// 元のフォルダにそのファイルを移動して終わり

	// 圧縮しようとするフォルダ内が空であったら終了
	// （空のアーカイブとなることを表示）
	if (IsEmptyFolder(temp_folder.getPath())) {
		cout << _T("空の書庫となるため書庫を作成しません") << endl;

		deleteTemporaryFolder(temp_folder);

		// 元の書庫を削除
		if (!DeleteFileOrFolder(srcFilename.c_str(), UsesRecycleBin)) {
			// ファイル削除失敗
			cout << _T("元の書庫の削除に失敗しました") << endl;
			cout << srcFilename.c_str() << endl;
		}

		return true;
	}

	// 作成するアーカイブファイル名を指定
	// 基本的に元と同じファイル名にする
	// ただし、拡張子は zip
	// 元と異なる場合は既に存在するファイル名を避ける
	String destFilename(srcFilename);
	String ext;
	GetExtention(destFilename.c_str(), ext);
	if (lstrcmpi(ext.c_str(), _T(".zip")) != 0) {
		RemoveExtension(destFilename);
		destFilename.Cat(_T(".zip"));
		EvacuateFileName(destFilename);
	}

	// 元のアーカイブと同じファイル名の場合は
	// 一時的に別のファイル名で作成し
	// 元の書庫削除後ファイル名を戻す
	String finalDestFilename(destFilename);
	if (redundantPathLen != 0 && g_RemovesRedundantFolder == 2) {
		StripPath(finalDestFilename);
		String fileName(redundantPath);
		int n = fileName.Find(_TCHAR('\\'));
		if (n > 0) {
			fileName.MakeLeft(n);
		}
		fileName.Cat(_T(".zip"));
		CatPath(finalDestFilename, fileName.c_str());
	}

	compressWithTemporaryFolder(srcFilename, destFilename, temp_folder);

	// 指定の名前で必要なフォルダ以下を圧縮
	{
		bool b;

		if(!g_Compress7Zip)
		{
			if (g_BatchDecompress) {
				// 一括解凍の場合はここで冗長フォルダを考慮する
				String path = temp_folder.getPath();
				CatPath(path, redundantPath.c_str());
				b = compress(path.c_str(), destFilename.c_str(), g_CompressLevel, g_ShowsProgress);
			} else {
				b = compress(temp_folder.getPath(), destFilename.c_str(), g_CompressLevel, g_ShowsProgress);
			}
		}
		else
		{
			if (pArchiveDll->getID() != ArchiveDllID::SEVEN_ZIP) {
				pArchiveDll = g_ArchiveDllManager.getArchiveDll(ArchiveDllID::SEVEN_ZIP);
				if (pArchiveDll == NULL) {
					pArchiveDll = g_ArchiveDllManager.addArchiveDll(ArchiveDllID::SEVEN_ZIP);
				}
				if (pArchiveDll == NULL) {
					cout << _T("書庫を圧縮するのに適当なDLLが見つかりません") << endl;
					return false;
				}
			}
			if (g_BatchDecompress) {
				// 一括解凍の場合はここで冗長フォルダを考慮する
				String path = temp_folder.getPath();
				CatPath(path, redundantPath.c_str());
				b = pArchiveDll->compress(path.c_str(), destFilename.c_str(), g_CompressLevel, g_ShowsProgress);
			} else {
				b = pArchiveDll->compress(temp_folder.getPath(), destFilename.c_str(), g_CompressLevel, g_ShowsProgress);
			}
		}
		if (b == false) {
			cout << _T("圧縮時にエラーが発生しました") << endl;
			return false;
		}
	}

	if (!checkArchive(pArchiveDll, destFilename))
	{
		return false;
	}


	// 元の書庫を削除
	if (!DeleteFileOrFolder(srcFilename.c_str(), UsesRecycleBin)) {
		// ファイル削除失敗
		cout << _T("元の書庫の削除に失敗しました") << endl;
		cout << srcFilename.c_str() << endl;
	}
	
	convertFileName(finalDestFilename);
	
	// 元の書庫削除後目標の名前に変える
	if (finalDestFilename != destFilename) {
		// 念のためファイルがディスク上で実際に移動するまで待つ
		// MoveFileEx() は Windows 2000 以降でしか使えない
		int n = 2;
		for (int i = 0; i < n; ++i) {
			if (MoveFileEx(destFilename.c_str(), finalDestFilename.c_str(), MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
				break;
			} else if (i < n - 1) {
				Sleep(3000);
			} else {
				cout << _T("作成した書庫のリネームに失敗しました") << endl;
				cout << destFilename.c_str() << endl;
			}
		}
	}

	deleteTemporaryFolder(temp_folder);

	// 処理した結果注目ファイルのみで構成される場合
	// その旨を表示する
	CheckRemarkableFile();

	return true;
}

// テンポラリフォルダの削除(明示的に行う)
void deleteTemporaryFolder(TemporaryFolder &temp_folder)
{
	if (!temp_folder.destroy()) {
		// フォルダ削除失敗
		cout << _T("テンポラリフォルダの削除に失敗しました") << endl;
		cout << temp_folder.getPath() << endl;
	}
}

// ファイル・フォルダの日時設定
void setFileFolderDateTime(TemporaryFolder &temp_folder, int redundantPathLen)
{
	if (g_FolderTimeMode != 2 || g_FileTimeMode != 0) {
		list<boost::shared_ptr<IndividualInfo> >::iterator it;
		for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
			String path(temp_folder.getPath());
			if (lstrlen((*it)->getFullPath()) <= redundantPathLen)
				continue;

			if (g_BatchDecompress) {
				CatPath(path, (*it)->getFullPath());
			}
			else {
				CatPath(path, (*it)->getFullPath() + redundantPathLen);
			}

			if (((*it)->getAttribute() & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				// フォルダの場合
				if (g_FolderTimeMode != 0 && g_FolderTimeMode != 1)
					continue;

				DWORD attribute = (*it)->getAttribute();

				// フォルダは自前で作成されたものなので
				// 読み込み専用属性はついていない
				// ただし一括解凍の場合はその限りではない
				// 属性が読み込み専用だと更新日時変更に失敗するので一時的に変更する
				bool isReadOnly = false;
				if (g_BatchDecompress && (attribute & FILE_ATTRIBUTE_READONLY) != 0) {
					if (SetFileAttributes(path.c_str(), (attribute & (~FILE_ATTRIBUTE_READONLY)))) {
						isReadOnly = true;
					}
					else {
						cout << _T("フォルダの属性一時変更に失敗しました") << endl;
					}
				}

				HANDLE hFile = CreateFile(path.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
				if (hFile == INVALID_HANDLE_VALUE) {
					cout << _T("フォルダの更新日時変更に失敗しました") << endl;
				}
				else {
					FILETIME ft;
					if (g_FolderTimeMode == 0) {
						// フォルダは解凍時の日付になってしまうので直す				
						ft = *((*it)->getLastWriteTime());
					}
					else if (g_FolderTimeMode == 1) {
						StringToFileTime(g_FolderTime.c_str(), ft);
					}
					if (SetFileTime(hFile, NULL, NULL, &ft) == 0) {
						cout << _T("フォルダの更新日時変更に失敗しました") << endl;
					}
					CloseHandle(hFile);
				}

				// 属性を元に戻す
				if (isReadOnly && g_FolderAttribute == -1) {
					if (!SetFileAttributes(path.c_str(), attribute)) {
						cout << _T("フォルダの属性復帰に失敗しました") << endl;
					}
				}
			}
			else {
				if (g_FileTimeMode != 1) continue;

				DWORD attribute = (*it)->getAttribute();

				// 属性が読み込み専用だと更新日時変更に失敗するので一時的に変更する
				bool isReadOnly = false;
				if ((attribute & FILE_ATTRIBUTE_READONLY) != 0) {
					if (SetFileAttributes(path.c_str(), (attribute & (~FILE_ATTRIBUTE_READONLY)))) {
						isReadOnly = true;
					}
					else {
						cout << _T("ファイルの属性一時変更に失敗しました") << endl;
					}
				}

				if (SetFileLastWriteTime(path.c_str(), g_FileTime.c_str()) == false) {
					cout << _T("ファイルの更新日時変更に失敗しました") << endl;
				}

				// 属性を元に戻す
				if (isReadOnly && g_FileAttribute == -1) {
					if (!SetFileAttributes(path.c_str(), attribute)) {
						cout << _T("ファイルの属性復帰に失敗しました") << endl;
					}
				}
			}
		}
	}

	if (g_FolderTimeMode == 2) {
		setFolderTime(temp_folder.getPath());
	}
}

// ファイル・フォルダの属性設定
void setFileFolderAttribute(TemporaryFolder &temp_folder, int redundantPathLen)
{
	if (g_FolderAttribute != -1 || g_FileAttribute != -1) {
		list<boost::shared_ptr<IndividualInfo> >::iterator it;
		for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
			String path(temp_folder.getPath());
			if (lstrlen((*it)->getFullPath()) <= redundantPathLen)
				continue;

			if (g_BatchDecompress) {
				CatPath(path, (*it)->getFullPath());
			}
			else {
				CatPath(path, (*it)->getFullPath() + redundantPathLen);
			}

			DWORD attribute = (*it)->getAttribute();

			if ((attribute & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				// フォルダの場合
				DWORD newAttribute;
				if (g_FolderAttribute == -1) {
					newAttribute = attribute;
				}
				else {
					newAttribute = FILE_ATTRIBUTE_DIRECTORY;
					if ((g_FolderAttribute & FileAttribute::READ_ONLY) != 0)	newAttribute |= FILE_ATTRIBUTE_READONLY;
					if ((g_FolderAttribute & FileAttribute::HIDDEN) != 0)		newAttribute |= FILE_ATTRIBUTE_HIDDEN;
					if ((g_FolderAttribute & FileAttribute::SYSTEM) != 0)		newAttribute |= FILE_ATTRIBUTE_SYSTEM;
					if ((g_FolderAttribute & FileAttribute::ARCHIVE) != 0)		newAttribute |= FILE_ATTRIBUTE_ARCHIVE;
				}
				if (SetFileAttributes(path.c_str(), newAttribute) == 0) {
					cout << _T("フォルダの属性変更に失敗しました") << endl;
					continue;
				}
			}
			else {
				if (g_FileAttribute != -1) {
					DWORD newAttribute = 0;
					if ((g_FileAttribute & FileAttribute::READ_ONLY) != 0)	newAttribute |= FILE_ATTRIBUTE_READONLY;
					if ((g_FileAttribute & FileAttribute::HIDDEN) != 0)		newAttribute |= FILE_ATTRIBUTE_HIDDEN;
					if ((g_FileAttribute & FileAttribute::SYSTEM) != 0)		newAttribute |= FILE_ATTRIBUTE_SYSTEM;
					if ((g_FileAttribute & FileAttribute::ARCHIVE) != 0)	newAttribute |= FILE_ATTRIBUTE_ARCHIVE;
					if (newAttribute == 0) newAttribute = FILE_ATTRIBUTE_NORMAL;
					if (newAttribute == attribute) continue;
					if (SetFileAttributes(path.c_str(), newAttribute) == 0) {
						cout << _T("ファイルの属性変更に失敗しました") << endl;
						continue;
					}
				}
			}
		}
	}
}

// よく分からないが一応前のDLLの処理が終わるようにしてみる
bool checkArchiveDllRunning(ArchiveDll * pArchiveDll)
{
	{
		DWORD startTime = timeGetTime();
		while (pArchiveDll->getRunning()) {
			Sleep(10);
			if (timeGetTime() - startTime > 3000) {
				cout << _T("解凍時にエラーが発生しました") << endl;
				return false;
			}
		}
	}
	return true;
}

void convertFileNameInArchive(TemporaryFolder& temp_folder, int redundantPathLen)
{
	if (g_ConvertFileName) {
		// ファイル名の文字変換
		list<boost::shared_ptr<IndividualInfo> >::iterator it;
		for (it = g_compressFileList.begin(); it != g_compressFileList.end(); ++it) {
			String path(temp_folder.getPath());
			if (lstrlen((*it)->getFullPath()) <= redundantPathLen) continue;

			if (g_BatchDecompress) {
				CatPath(path, (*it)->getFullPath());
			}
			else {
				CatPath(path, (*it)->getFullPath() + redundantPathLen);
			}

			DWORD attribute = (*it)->getAttribute();

			String orgFilename;
			String cnvFilename;
			GetFileName(path.c_str(), orgFilename);
			nkf->Convert(orgFilename, cnvFilename);

			if ((orgFilename != cnvFilename) || (orgFilename.GetLength() != cnvFilename.GetLength())) {
				if ((attribute & FILE_ATTRIBUTE_DIRECTORY) != 0) {
					continue;
				}
				else {
					cout << _T("ファイル名 変換前：") << orgFilename.c_str() << endl;
					cout << _T("ファイル名 変換後：") << cnvFilename.c_str() << endl;
				}

				String cnvPath(path);
				RemoveFileName(cnvPath);
				CatPath(cnvPath, cnvFilename.c_str());
				int n = 2;
				for (int i = 0; i < n; ++i) {
					if (MoveFileEx(path.c_str(), cnvPath.c_str(), MOVEFILE_WRITE_THROUGH) != 0) {
						break;
					}
					else if (i < n - 1) {
						Sleep(3000);
					}
					else {
						cout << _T("リネームに失敗しました") << endl;
					}
				}
			}
		}

		// フォルダ名の文字変換
		list<boost::shared_ptr<IndividualInfo> >::reverse_iterator ir;
		for (ir = g_compressFileList.rbegin(); ir != g_compressFileList.rend(); ++ir) {
			String path(temp_folder.getPath());
			if (lstrlen((*ir)->getFullPath()) <= redundantPathLen)
				continue;

			if (g_BatchDecompress) {
				CatPath(path, (*ir)->getFullPath());
			}
			else {
				CatPath(path, (*ir)->getFullPath() + redundantPathLen);
			}

			DWORD attribute = (*ir)->getAttribute();

			String orgFilename;
			String cnvFilename;
			GetFileName(path.c_str(), orgFilename);
			nkf->Convert(orgFilename, cnvFilename);

			if ((orgFilename != cnvFilename) || (orgFilename.GetLength() != cnvFilename.GetLength())) {
				if ((attribute & FILE_ATTRIBUTE_DIRECTORY) != 0) {
					cout << _T("フォルダ名 変換前：") << orgFilename.c_str() << endl;
					cout << _T("フォルダ名 変換後：") << cnvFilename.c_str() << endl;
				}
				else {
					continue;
				}

				String cnvPath(path);
				RemoveFileName(cnvPath);
				CatPath(cnvPath, cnvFilename.c_str());
				int n = 2;
				for (int i = 0; i < n; ++i) {
					if (MoveFileEx(path.c_str(), cnvPath.c_str(), MOVEFILE_WRITE_THROUGH) != 0) {
						break;
					}
					else if (i < n - 1) {
						Sleep(3000);
					}
					else {
						cout << _T("リネームに失敗しました") << endl;
					}
				}
			}
		}
	}
}

// 圧縮をTempフォルダで行う
void compressWithTemporaryFolder(String& srcFilename, String& destFilename, TemporaryFolder& temp_folder)
{
	if (g_CompressWithTemporaryFolder)
	{
		String orgFilename;
		GetFileName(srcFilename.c_str(), orgFilename);
		destFilename = temp_folder.getPath();
		CatPath(destFilename, orgFilename.c_str());
		RemoveExtension(destFilename);
		destFilename.Cat(_T(".zip"));
		EvacuateFileName(destFilename);
	}

	if (destFilename == srcFilename) {
		EvacuateFileName(destFilename);
	}
}

// 作成した書庫のテストを行う
bool checkArchive(ArchiveDll*& pArchiveDll, String& destFilename)
{
	if (g_CheckArchive)
	{
		if (pArchiveDll->getID() != ArchiveDllID::SEVEN_ZIP) {
			pArchiveDll = g_ArchiveDllManager.getArchiveDll(ArchiveDllID::SEVEN_ZIP);
			if (pArchiveDll == NULL) {
				pArchiveDll = g_ArchiveDllManager.addArchiveDll(ArchiveDllID::SEVEN_ZIP);
			}
			if (pArchiveDll == NULL) {
				return false;
			}
		}

		pArchiveDll->setArchiveFilename(destFilename.c_str());
		if (!pArchiveDll->checkArchive(2))
		{
			cout << _T("作成した書庫のテストに失敗しました") << endl;
			cout << destFilename.c_str() << endl;
			return false;
		}
	}
	return true;
}

// 書庫のファイル名をnkfで変換
void convertFileName(String& finalDestFilename)
{
	if (g_ConvertFileName) {
		String orgFilename;
		GetFileName(finalDestFilename.c_str(), orgFilename);
		String cnvFilename;
		nkf->Convert(orgFilename, cnvFilename);
		RemoveFileName(finalDestFilename);
		CatPath(finalDestFilename, cnvFilename.c_str());
		if (orgFilename != cnvFilename) {
			cout << _T("書庫名 変換前：") << orgFilename.c_str() << endl;
			cout << _T("書庫名 変換後：") << cnvFilename.c_str() << endl;
		}
	}
}

// 指定のフォルダ内（サブフォルダ含む）のフォルダの更新日時を
// フォルダ内の最新の更新日時を持つファイル・フォルダと一致させる
void setFolderTime(LPCTSTR path) {
	FILETIME ft;
	setFolderTimeSub(path, ft);
}

bool setFolderTimeSub(LPCTSTR path, FILETIME& rNewestFileTime) {
	rNewestFileTime.dwLowDateTime = 0;
	rNewestFileTime.dwHighDateTime = 0;

	bool isEmptyFolder = true;

	WIN32_FIND_DATA fd;
	HANDLE hSearch;

	// 探索パスを作成
	String searchPath(path);
	FormatTooLongPath(searchPath);
	CatPath(searchPath, _T("*.*"));

	hSearch = FindFirstFile(searchPath.c_str(), &fd);

	if (hSearch==INVALID_HANDLE_VALUE)
		return false;

	do {
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0){
			// ルート及びカレントでないフォルダかどうかチェック
			if (lstrcmp(fd.cFileName,_T("."))!=0 && lstrcmp(fd.cFileName,_T("..")) != 0) {
				isEmptyFolder = false;

				searchPath = path;
				CatPath(searchPath, fd.cFileName);
				FILETIME ft;

				if (setFolderTimeSub(searchPath.c_str(), ft) == false) continue;

				// 属性が読み込み専用だと更新日時変更に失敗するので一時的に変更する
				bool isReadOnly = false;
				if ((fd.dwFileAttributes & FILE_ATTRIBUTE_READONLY) != 0) {
					isReadOnly = true;
					SetFileAttributes(searchPath.c_str(), (fd.dwFileAttributes & (~FILE_ATTRIBUTE_READONLY)));
				}
				HANDLE hFile = CreateFile(searchPath.c_str(), GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
				if (hFile == INVALID_HANDLE_VALUE) {
					cout << _T("フォルダの更新日時変更に失敗しました") << endl;
				} else {
					if (SetFileTime(hFile, NULL, NULL, &ft) == 0) {
						cout << _T("フォルダの更新日時変更に失敗しました") << endl;
						GetLastErrorMssage();
					}
					CloseHandle(hFile);
				}
				// 属性を元に戻す
				if (isReadOnly && g_FolderAttribute == -1) {
					SetFileAttributes(searchPath.c_str(), fd.dwFileAttributes);
				}

				if (CompareFileTime(&rNewestFileTime,  &ft) == -1) {
					rNewestFileTime = ft;
				}
			}
		} else {
			isEmptyFolder = false;

			if (CompareFileTime(&rNewestFileTime, &fd.ftLastWriteTime) == -1) {
				rNewestFileTime = fd.ftLastWriteTime;
			}
		}
	} while (FindNextFile(hSearch,&fd)!=0);

	FindClose(hSearch);

	return (!isEmptyFolder);
}



bool compress(LPCTSTR srcPath, LPCTSTR dstPath, int compressLevel, bool /* showsProgress */)
{
	// フォルダ以下を圧縮
	bool b;
	ZipWriter zw;
	b = zw.open(dstPath);
	if (!b) return false;

	// basePathLength は 末尾に '\\' がついているかどうかで変わる
	int basePathLength = lstrlen(srcPath);
	if (strrchrex(srcPath, '\\') != srcPath+(basePathLength-1)) {
		basePathLength++;
	}

	b = compressSub(srcPath, zw, basePathLength);
	if (!b) return false;
	zw.setCompressLevel(compressLevel);
	int n = zw.close();
	if (n != 0) return false;
	return true;
}

// 指定フォルダ内を検索してファイルを追加
// basePathLength は ZipWriter に追加するファイルのフルパスから何文字
// 削ったら Zip 内でのそのファイルのパスになるか（'\\' が末尾についてるパスなのかを注意）
bool compressSub(LPCTSTR pszDir, ZipWriter& zw, int basePathLength)
{
	WIN32_FIND_DATA fd;

	String searchPath;
	HANDLE hSearch;

	// 探索パスを作成
	searchPath += pszDir;
	FormatTooLongPath(searchPath);
	CatPath(searchPath, _T("*.*"));

	hSearch = FindFirstFile(searchPath.c_str(), &fd);

	if (hSearch==INVALID_HANDLE_VALUE)
		return false;

	do {
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=0) {
			// ルート及びカレントでないフォルダかどうかチェック
			if (lstrcmp(fd.cFileName,_T("."))!=0 && lstrcmp(fd.cFileName,_T(".."))!=0) {
				String path = pszDir;
				CatPath(path, fd.cFileName);

				// フォルダのパスを追加
				zw.add(path.c_str()+basePathLength, path.c_str());

				// サブフォルダの検索
				if(!compressSub(path.c_str(), zw, basePathLength)) {
					return false;
				}
			}
		}else{
			String path = pszDir;
			CatPath(path, fd.cFileName);

			// ファイルのパスをvectorに追加
			zw.add(path.c_str()+basePathLength, path.c_str());
		}
		// メッセージ処理
		//DoEvents();
	} while (FindNextFile(hSearch, &fd)!=0);

	FindClose(hSearch);

	return true;
}



