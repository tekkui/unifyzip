#include "ArchiveDll.h"

#include "PathFunc.h"
#include "MemoryMappedFile.h"

#include <tchar.h>

namespace KSDK {

using namespace boost;
using namespace std;

namespace {

// 文字列にスペースが含まれる場合
// 文字列をダブルクォートで囲む
void DoubleQuoteString(String& str) {
	// スペースを含むか
	if(str.Find(_TCHAR(' ')) == -1) return;

	str.Insert(0, _TCHAR('\"'));
	str.Cat(_TCHAR('\"'));
}

}


ArchiveDll::ArchiveDll()
:	mDllHandle(NULL),
	mArchiveHandle(NULL)
{
}

ArchiveDll::~ArchiveDll() {
	if (mArchiveHandle != NULL) {
		mArchiveHandle = NULL;
		closeArchive();
	}
	if (mDllHandle != NULL) {
		::FreeLibrary(mDllHandle);
		mDllHandle = NULL;
	}
}

// 指定のDLLで初期化
bool ArchiveDll::init(ArchiveDllID::ArchiveDllID archiveDllID) {
	archiveDllID_ = archiveDllID;

	if (mArchiveHandle != NULL) {
		mArchiveHandle = NULL;
		closeArchive();
	}
	if (mDllHandle != NULL) {
		::FreeLibrary(mDllHandle);
		mDllHandle = NULL;
	}

	switch (archiveDllID) {
	case ArchiveDllID::SEVEN_ZIP:
		mDllFilename = _T("7-zip32.dll");
		mPrefix = _T("SevenZip");
		break;

	case ArchiveDllID::UNLHA:
		mDllFilename = _T("UNLHA32.dll");
		mPrefix = _T("Unlha");
		break;

	case ArchiveDllID::UNRAR:
		mDllFilename = _T("unrar32.dll");
		mPrefix = _T("Unrar");
		break;

	case ArchiveDllID::UNZIP:
		mDllFilename = _T("UNZIP32.dll");
		mPrefix = _T("UnZip");
		break;

	default:
		mDllFilename.Empty();
		mPrefix.Empty();
		return false;
	}
	mDllHandle = ::LoadLibrary(mDllFilename.c_str());

	return (mDllHandle != NULL);
}

/*
// DLLのファイル名とプレフィックスをセット
bool ArchiveDll::setDllFilename(LPCTSTR filename, LPCTSTR prefix) {
	mDllFilename = filename;
	mPrefix = prefix;

	if (mDllHandle != NULL) {
		::FreeLibrary(mDllHandle);
	}
	mDllHandle = ::LoadLibrary(mDllFilename.c_str());
	return (mDllHandle != NULL);
}
*/

FARPROC ArchiveDll::getFuncAddress(LPCTSTR funcName) {
	String apiName;
	apiName.Format(_T("%s%s"), mPrefix.c_str(), funcName);
#ifdef _UNICODE
	return ::GetProcAddress(mDllHandle, apiName.ansi_str());
#else
	return ::GetProcAddress(mDllHandle, apiName.c_str());
#endif
}

// 順序数  1
int ArchiveDll::command(const HWND hwnd, LPCTSTR cmdLine, String& rOutput) {
	rOutput.Empty();

#ifdef _UNICODE
	FARPROC f = ::GetProcAddress(mDllHandle, mPrefix.ansi_str());
#else
	FARPROC f = ::GetProcAddress(mDllHandle, mPrefix.c_str());
#endif	
	// 関数がサポートされているか
	if (f == NULL) { return 1; }

#ifdef _UNICODE
	LPSTR lpBuffer = (LPSTR)malloc(65536 + 1);
#else
	LPSTR lpBuffer = rOutput.GetBuffer(65536+1);
#endif

	typedef int  (WINAPI* SEVEN_ZIP)(const HWND,LPCSTR,LPSTR,const DWORD);
#ifdef _UNICODE
	int r = ((SEVEN_ZIP)f)( hwnd, (LPCSTR)cmdLine, lpBuffer, 65536);
	rOutput.Utf8Copy(lpBuffer);
	free(lpBuffer);
#else
	int r = ((SEVEN_ZIP)f)( hwnd, cmdLine, lpBuffer, 65536);
#endif

	rOutput.ReleaseBuffer();

	if (r < 0x8000) {
		return r;
	} else {
		// 何らかの定義されたエラー
		return r;
	}
}

WORD ArchiveDll::getVersion() {
	FARPROC f = getFuncAddress(_T("GetVersion"));
	// 関数がサポートされているか
	if (f == NULL) { return 0; }
	typedef WORD (WINAPI * GET_VERSION)();
	return ((GET_VERSION)f)();
}

bool ArchiveDll::openArchive(const HWND hwnd, const DWORD mode)  {
	if (mArchiveHandle != NULL) {
		if (!closeArchive()) {
			return false;
		}
	}
	FARPROC f = getFuncAddress(_T("OpenArchive"));
	if (f == NULL) { return false; }
	typedef HARC (WINAPI* OPEN_ARCHIVE)(const HWND, LPCSTR, const DWORD);
#ifdef _UNICODE
	if (archiveDllID_ == ArchiveDllID::UNZIP)
	{
		mArchiveHandle = ((OPEN_ARCHIVE)f)(hwnd, (LPCSTR)archiveFilename_.ansi_str(), mode);
	}
	else
	{
		setUnicodeMode();
		mArchiveHandle = ((OPEN_ARCHIVE)f)(hwnd, (LPCSTR)archiveFilename_.utf8_str(), mode);
	}
#else
	mArchiveHandle = ((OPEN_ARCHIVE)f)(hwnd, archiveFilename_.c_str(), mode);
#endif
	return (mArchiveHandle != NULL);
}

bool ArchiveDll::closeArchive() {
	FARPROC f = getFuncAddress(_T("CloseArchive"));
	if (f == NULL) { return false; }
	typedef int (WINAPI * CLOSE_ARCHIVE)(HARC);
	int r = ((CLOSE_ARCHIVE)f)(mArchiveHandle);
	if (r==0) {
		mArchiveHandle = NULL;
		return true;
	} else {
		return false;
	}
} 

LPCTSTR ArchiveDll::getArchiveFilename() {
	return archiveFilename_.c_str();
}


// 一括解凍
// overwritesFile : 同名のファイルを上書きするか否か
//  0 : 正常終了
// -1 : 対応しない形式
// -2 : パスワード付きだった
// -3 : ユーザーキャンセル
// -4 : その他のエラー
int ArchiveDll::extract(LPCTSTR destPath, bool showsProgress, bool overwritesFile) {
	String commandLine;

	// スペースを含む場合ダブルクォートで囲む
	String archive = archiveFilename_;
	DoubleQuoteString(archive);

	String dest = destPath;

	if (archiveDllID_ == ArchiveDllID::SEVEN_ZIP) {
		commandLine.Cat(_T("x "));

		commandLine.Cat(archive.c_str());
		commandLine.Cat(_T(" "));

		if (overwritesFile) {
			commandLine.Cat(_T("-aoa "));
		} else {
			// 別名で解凍（ファイル名に _1 等をつける）
			commandLine.Cat(_T("-aou "));
		}

		if (!dest.IsEmpty()) {
			String dir;
			dir.Format(_T("-o%s"), dest.c_str());
			DoubleQuoteString(dir);	// スイッチごとダブルクォートで囲む
			dir.Cat(_T(" "));
			commandLine.Cat(dir.c_str());
		}

		if (showsProgress == false) {
			commandLine.Cat(_T("-hide "));
		}

	} else if (archiveDllID_  == ArchiveDllID::UNLHA) {
		commandLine.Cat(_T("e -x1"));

		// 属性をそのままに隠し属性・システム属性等全てのファイルを解凍
		commandLine.Cat(_T("-a1 "));

		if (showsProgress == false) {
			commandLine.Cat(_T("-n1 "));
		}

		if (overwritesFile) {
			// 常に上書き展開
			commandLine.Cat(_T("-m1 -c1 "));
		} else {
			// 同名のファイルがある場合拡張子を 000 ～ 999 に変えて展開
			commandLine.Cat(_T("-m2 "));
		}

		commandLine.Cat(archive.c_str());
		commandLine.Cat(_T(" "));

		// 展開先の指定の場合フォルダは'\\'で終わらなければならない
		if (!dest.IsEmpty()) {
			String dir = dest;
			if (dir.GetAt(dir.GetLength() - 1) != _TCHAR('\\')) {
				dir.Cat(_TCHAR('\\'));
			}
			DoubleQuoteString(dir);
			dir.Cat(_T(" "));
			commandLine.Cat(dir.c_str());
		}


	} else if (archiveDllID_ == ArchiveDllID::UNRAR) {
		commandLine.Cat(_T("-x "));

		if (showsProgress == false) {
			commandLine.Cat(_T("-q "));
		}

		if (overwritesFile) {
			// 常に上書き展開
			commandLine.Cat(_T("-o "));
		} else {
			// 上書きするか否かを選ぶダイアログが出る
		}

		commandLine.Cat(archive.c_str());
		commandLine.Cat(_T(" "));

		// 展開先の指定の場合フォルダは'\\'で終わらなければならない
		if (!dest.IsEmpty()) {
			String dir = dest;
			if (dir.GetAt(dir.GetLength() - 1) != _TCHAR('\\')) {
				dir.Cat(_TCHAR('\\'));
			}
			DoubleQuoteString(dir);
			dir.Cat(_T(" "));
			commandLine.Cat(dir.c_str());
		}

	} else if (archiveDllID_ == ArchiveDllID::UNZIP) {
		if (showsProgress == false) {
			commandLine.Cat(_T("--i "));
		}

		if (overwritesFile) {
			// 常に上書き展開
			commandLine.Cat(_T("-o "));
		} else {
			// 上書きするか否かを選ぶダイアログが出る（リネームも可能）
		}

		commandLine.Cat(archive.c_str());
		commandLine.Cat(_T(" "));

		// 展開先の指定の場合フォルダは'\\'で終わらなければならない
		String dir;
		if (!dest.IsEmpty()) {
			dir = dest;
			if (dir.GetAt(dir.GetLength() - 1) != _TCHAR('\\')) {
				dir.Cat(_TCHAR('\\'));
			}
			DoubleQuoteString(dir);
			dir.Cat(_T(" "));
			commandLine.Cat(dir.c_str());
		}
	} else {
		return -1;
	}

	String output;
#ifdef _UNICODE
	int ret;
	if (archiveDllID_ == ArchiveDllID::UNZIP)
	{
		ret = command(NULL, (LPCTSTR)commandLine.ansi_str(), output);
	}
	else
	{
		ret = command(NULL, commandLine.utf8_str(), output);
	}
#else
	int ret = command(NULL, commandLine.c_str(), output);
#endif

	if (ret == 0) {
		return 0;
	} else if (ret == ERROR_PASSWORD_FILE) {
		return -2;
	} else if (ret == ERROR_USER_CANCEL) {
		return -3;
	} else {
		if (archiveDllID_ == ArchiveDllID::UNRAR && ret < 0x8000) {
			// unrar32.dll(おそらく0.13以降)で -x オプションで
			// フォルダを含む書庫を解凍するとフォルダについて上書きが問われ
			// 上書きするとしても(あるいは -o オプションをつけても)
			// 1が返る(おそらくそのフォルダをスキップした扱い)
			// ので0x8000未満は成功として扱う
			return 0;
		} else {
			return -4;
		}
	}
}






// 指定のファイルを指定のパスに解凍する
// password が NULL ならばパスワードを使用しない
// 戻り値
//  0 : 成功
// -1 : パスワードが間違っている
// -2 : その他のエラー
int ArchiveDll::extract(LPCTSTR srcPath, LPCTSTR destPath, bool showsProgress, LPCTSTR password) {
	// スペースを含む場合ダブルクォートで囲む
	String archiveFilename(archiveFilename_);
	DoubleQuoteString(archiveFilename);

	String dest;
	dest = destPath;
	if (dest.ReverseFind(_TCHAR('\\')) != dest.GetLength() - 1) {
		dest.Cat(_TCHAR('\\'));
	}
	DoubleQuoteString(dest);

	String extractFilename = srcPath;
	DoubleQuoteString(extractFilename);
	
	String commandLine;
	String switchString;
	if (archiveDllID_ == ArchiveDllID::SEVEN_ZIP) {
		// -aos オプションは同名のファイルがサブフォルダ以下にあった場合
		// それも解凍されてしまうので、それで上書きしないようにするため

		WORD version = getVersion();

		switchString = _T("-aos ");

		if (showsProgress == false) {
			switchString.Cat(_T("-hide "));
		}

		// パスワード
		if (password != NULL) {
			String p;
			p.Format(_T("-p%s"), password);
			DoubleQuoteString(p);
			p.Cat(_T(" "));
			switchString.Cat(p.c_str());
		}

		if (version < 431) {
			commandLine.Format(_T("e %s %s %s %s"), switchString.c_str(), archiveFilename.c_str(), dest.c_str(), extractFilename.c_str());
		} else {
			commandLine.Format(_T("e %s %s -o%s %s"), switchString.c_str(), archiveFilename.c_str(), dest.c_str(), extractFilename.c_str());
		}
	} else if (archiveDllID_ == ArchiveDllID::UNLHA) {
		// 対象ディレクトリに存在しないファイルのみ 属性を有効にして 展開
		if (showsProgress) {
			commandLine.Format(_T("e -jn -a1 %s %s %s"), archiveFilename.c_str(), dest.c_str(), extractFilename.c_str());
		} else {
			commandLine.Format(_T("e -jn -a1 -n1 %s %s %s"), archiveFilename.c_str(), dest.c_str(), extractFilename.c_str());
		}
	} else if (archiveDllID_ == ArchiveDllID::UNRAR) {
		switchString = _T("-s ");

		if (showsProgress == false) {
			switchString.Cat(_T("-q "));
		}

		// パスワード
		if (password != NULL) {
			String p;
			p.Format(_T("-p%s"), password);			
			DoubleQuoteString(p);
			p.Cat(_T(" "));
			switchString.Cat(p.c_str());
		}

		commandLine.Format(_T("e %s %s %s %s"), switchString.c_str(), archiveFilename.c_str(), dest.c_str(), extractFilename.c_str());

	} else if (archiveDllID_ == ArchiveDllID::UNZIP) {
		if (showsProgress == false) {
			switchString.Cat(_T("--i "));
		}

		// パスワード
		if (password != NULL) {
			String p;
			p.Format(_T("-P%s"), password);			
			DoubleQuoteString(p);
			p.Cat(_T(" "));
			switchString.Cat(p.c_str());
		}

		// ファイル名は正規表現で指定する為
		// 「\」を「\\」としなければならない
		// 面倒なので「/」に置換する
		extractFilename.Replace(_TCHAR('\\'), _TCHAR('/'));

		commandLine.Format(_T("-j -n %s %s %s %s"), switchString.c_str(), archiveFilename.c_str(), dest.c_str(), extractFilename.c_str());

	} else {
		// その他の形式
		return -2;
	}

	String output;
	int ret = command(NULL, commandLine.c_str(), output);

	if (archiveDllID_ == ArchiveDllID::SEVEN_ZIP) {
		if (ret == 0) {
			return 0;
		} else if (ret == ERROR_PASSWORD_FILE) {
			return -1;
		} else {
			return -2;
		}
	} else if (archiveDllID_ == ArchiveDllID::UNRAR) {
		if (ret == 0) {
			return 0;
		} else if (ret == ERROR_PASSWORD_FILE) {
			return -1;
		} else {
			return -2;
		}

	} else if (archiveDllID_ == ArchiveDllID::UNZIP) {
		if (ret == 0) {
			return 0;
		} else if (ret == ERROR_PASSWORD_FILE) {
			return -1;
		} else {
			// パスワードが空文字列で解凍を試みた場合は
			// ここに来るようだ
			return -2;
		}
	} else {
		return 0;
	}
}

// 指定フォルダ内を指定ファイル名で圧縮
// 現在 7-zip32.dll にのみ対応
bool ArchiveDll::compress(LPCTSTR srcPath, LPCTSTR destPath, int compressLevel, bool showsProgress) {
		String filename(destPath);
		DoubleQuoteString(filename);

		// コマンドラインでは圧縮先に絶対パスは指定できないので
		// カレントディレクトリを変更して指定
		String oldCurrentDirectory;
		GetCurrentDirectory(oldCurrentDirectory);
		SetCurrentDirectoryEx(srcPath);

		String commandLine;
		if (showsProgress) {
#ifdef _UNICODE
			commandLine.Format(_T("a -tzip %s * -r -mx=%d -mcu=on -sccUTF-8"), filename.c_str(), compressLevel);
#else
			commandLine.Format(_T("a -tzip %s * -r -mx=%d"), filename.c_str(), compressLevel);
#endif
		} else {
#ifdef _UNICODE
			commandLine.Format(_T("a -tzip -hide %s * -r -mx=%d -mcu=on -sccUTF-8"), filename.c_str(), compressLevel);
#else
			commandLine.Format(_T("a -tzip -hide %s * -r -mx=%d"), filename.c_str(), compressLevel);
#endif
		}

		String output;
#ifdef _UNICODE
		int ret = command(NULL, commandLine.utf8_str(), output);
#else
		int ret = command(NULL, commandLine.c_str(), output);
#endif

		// カレントディレクトリを戻す
		SetCurrentDirectoryEx(oldCurrentDirectory.c_str());

		return (ret == 0);
}


// 戻り値
// 0			: 正常終了
// －1			: 検索終了
// 0, －1 以外	: 異常終了
int ArchiveDll::findFirst(LPCTSTR wildName, INDIVIDUALINFO* p) {
	FARPROC f = getFuncAddress(_T("FindFirst"));
	if (f == NULL) { return 1; }
	typedef int (WINAPI* FIND_FIRST)(HARC ,LPCSTR ,INDIVIDUALINFO*);
#ifdef _UNICODE
	String wName(wildName);
	return ((FIND_FIRST)f)(mArchiveHandle, wName.ansi_str(), p);
#else	
	return ((FIND_FIRST)f)(mArchiveHandle, wildName, p);
#endif
}

// 戻り値 
// 0			: 正常終了
// －1			: 検索終了
// 0, －1 以外	: 異常終了
int ArchiveDll::findNext(INDIVIDUALINFO* p) {
	FARPROC f = getFuncAddress(_T("FindNext"));
	if (f == NULL) { return 1; }
	typedef int (WINAPI* FIND_NEXT)(HARC ,INDIVIDUALINFO*);
	return ((FIND_NEXT)f)(mArchiveHandle, p);
}

// 順序数  10
bool ArchiveDll::getRunning() {
	FARPROC f = getFuncAddress(_T("GetRunning"));
	if (f == NULL) { return false; }
	typedef BOOL (WINAPI * GET_RUNNING)();
	BOOL b = ((GET_RUNNING)f)();
	return (b == TRUE);
}

// 順序数  12
bool ArchiveDll::checkArchive(int mode) {
	FARPROC f = getFuncAddress(_T("CheckArchive"));
	if (f == NULL) { return false; }
	typedef BOOL (WINAPI * CHECK_ARCHIVE)(LPCSTR, const int);
#ifdef _UNICODE
	BOOL b;
	if (archiveDllID_ == ArchiveDllID::UNZIP)
	{
		b = ((CHECK_ARCHIVE)f)((LPCSTR)archiveFilename_.ansi_str(), mode);
	}
	else
	{
		setUnicodeMode();
		b = ((CHECK_ARCHIVE)f)((LPCSTR)archiveFilename_.utf8_str(), mode);
	}
#else
	BOOL b = ((CHECK_ARCHIVE)f)(archiveFilename_.c_str(), mode);
#endif

	// UNZIP32.DLL はアーカイブでないファイルに対して
	// checkArchive() が通ってしまう事があるので
	// 自前でシグネチャチェック
	if (archiveDllID_ == ArchiveDllID::UNZIP) {
		ReadOnlyMemoryMappedFileEx mmf;
		if (!mmf.open(archiveFilename_.c_str())) return false;
		// シグネチャチェック
		boost::shared_ptr<ViewOfFile> pView = mmf.map(0, sizeof(UINT32));
		if (!pView) return false;
		if ( *((UINT32*)pView->getPointer()) != 0x04034b50) return false;
	}

	return (b == TRUE);
}

int ArchiveDll::setUnicodeMode() {
	FARPROC f = getFuncAddress(_T("SetUnicodeMode"));
	if (f == NULL) { return -1; }

	typedef int (WINAPI * SET_UNICODEMODE)(bool);
	return ((SET_UNICODEMODE)f)(true);
}

int ArchiveDll::getAttribute() {
	FARPROC f = getFuncAddress(_T("GetAttribute"));
	if (f == NULL) { return -1; }

	typedef int (WINAPI * GET_ATTRIBUTE)(HARC);
	return ((GET_ATTRIBUTE)f)(mArchiveHandle);
}

int ArchiveDll::getFileName(String& rFilename) {
	FARPROC f = getFuncAddress(_T("GetFileName"));
	if (f == NULL) { return -1; }
#ifdef _UNICODE
	LPSTR lpBuffer = (LPSTR)malloc(MAX_PATH);
#else
	LPSTR lpBuffer = rFilename.GetBuffer(MAX_PATH);
#endif
	typedef int (WINAPI * GET_FILE_NAME)(HARC, LPSTR, const int);
	int ret = ((GET_FILE_NAME)f)(mArchiveHandle, lpBuffer, 256);
#ifdef _UNICODE
	rFilename.Utf8Copy(lpBuffer);
	free(lpBuffer);
#else
	rFilename.ReleaseBuffer();
#endif
	return ret;
}

bool ArchiveDll::getWriteTimeEx(FILETIME& rLastWriteTime) {
	FARPROC f = getFuncAddress(_T("GetWriteTimeEx"));
	if (f == NULL) { return false; }
	typedef int (WINAPI * GET_WRITE_TIME_EX)(HARC, FILETIME*);
	BOOL b = ((GET_WRITE_TIME_EX)f)(mArchiveHandle, &rLastWriteTime);
	return (b == TRUE);
}

ArchiveDll* ArchiveDllManager::addArchiveDll(ArchiveDllID::ArchiveDllID archiveDllID) {
	boost::shared_ptr<ArchiveDll> ptr(new ArchiveDll);
	if (ptr->init(archiveDllID)) {
		archiveDlls_.push_back(ptr);
		return ptr.get();
	}
	return NULL;
}

ArchiveDll* ArchiveDllManager::getArchiveDll(ArchiveDllID::ArchiveDllID archiveDllID) {
	vector<boost::shared_ptr<ArchiveDll> >::iterator it;
	for (it = archiveDlls_.begin(); it != archiveDlls_.end(); ++it) {
		if ((*it)->getID() == archiveDllID) {
			return (*it).get();
		}
	}
	return NULL;
}

void ArchiveDllManager::releaseArchiveDll(ArchiveDllID::ArchiveDllID archiveDllID) {
	vector<boost::shared_ptr<ArchiveDll> >::iterator it;
	for (it = archiveDlls_.begin(); it != archiveDlls_.end(); ++it) {
		if ((*it)->getID() == archiveDllID) {
			archiveDlls_.erase(it);
			break;
		}
	}
}

ArchiveDll* ArchiveDllManager::getSuitableArchiveDll(LPCTSTR filename) {
	int id = -1;

	// 拡張子から適当なDLLを探す
	String ext;
	GetExtention(filename, ext);
	ArchiveDll* p = NULL;
	if (lstrcmpi(ext.c_str(), _T(".zip")) == 0 ||
		lstrcmpi(ext.c_str(), _T(".jar")) == 0 ||
		lstrcmpi(ext.c_str(), _T(".7z")) == 0) {
		id = ArchiveDllID::SEVEN_ZIP;
//		id = ArchiveDllID::UNZIP;
	} else if (lstrcmpi(ext.c_str(), _T(".lzh")) == 0 ||
		lstrcmpi(ext.c_str(), _T(".lha")) == 0 ||
		lstrcmpi(ext.c_str(), _T(".lzs")) == 0) {
		id = ArchiveDllID::UNLHA;
	} else if (lstrcmpi(ext.c_str(), _T(".rar")) == 0) {
		id = ArchiveDllID::UNRAR;
	}

	if (id != -1) {
		p = getArchiveDll((ArchiveDllID::ArchiveDllID)id);
		if (p == NULL) {
			p = addArchiveDll((ArchiveDllID::ArchiveDllID)id);
		}
		if (p != NULL) {
			// DLLが使用中でないかチェックする
			if (p->getRunning()) {
				// DLL使用中
			} else {
				p->setArchiveFilename(filename);
				if (p->checkArchive(0)) {
					return p;
				}
			}
		}
	}

	// 全てのアーカイブを試す
	for (int i=0; i<ArchiveDllID::MAX_ARCHIVE_DLL; ++i) {
		p = getArchiveDll((ArchiveDllID::ArchiveDllID)i);
		if (p == NULL) {
			p = addArchiveDll((ArchiveDllID::ArchiveDllID)i);
		}
		if (p != NULL) {
			// DLLが使用中でないかチェックする
			if (p->getRunning()) {
				// DLL使用中
			} else {
				p->setArchiveFilename(filename);
				if (p->checkArchive(0)) {
					return p;
				}
			}
		}
	}

	return NULL;
}

} // end of namespace KSDK
