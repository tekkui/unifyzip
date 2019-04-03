#ifndef __ARCHIVEDLL_H__
#define __ARCHIVEDLL_H__

#include <vector>
#include <boost/shared_ptr.hpp>
#include "comm-arc.h"
#include "String.h"

namespace KSDK {

namespace ArchiveDllID {
enum ArchiveDllID {
	SEVEN_ZIP,
	UNLHA,
	UNRAR,
	UNZIP,
	MAX_ARCHIVE_DLL,
};
}

class ArchiveDll {
public:
	ArchiveDll();
	~ArchiveDll();

	bool init(ArchiveDllID::ArchiveDllID archiveDllID);

	//bool setDllFilename(LPCTSTR filename, LPCTSTR prefix);

	//LPCTSTR getDllFilename() {
	//	return mDllFilename.c_str();
	//}

	ArchiveDllID::ArchiveDllID getID() {
		return archiveDllID_;
	}

	WORD getVersion();

	bool openArchive(const HWND hwnd, const DWORD mode);
	bool closeArchive();

	LPCTSTR getArchiveFilename();

	void setArchiveFilename(LPCTSTR filename) {
		archiveFilename_ = filename;
	}

	// 一括解凍
	int extract(LPCTSTR destPath, bool showsProgress, bool overwritesFile);

	// 指定のファイル・フォルダを指定のフォルダに解凍
	int extract(LPCTSTR srcPath, LPCTSTR destPath, bool showsProgress, LPCTSTR password = NULL);

	// 指定フォルダ以下を指定ファイル名で圧縮
	bool compress(LPCTSTR srcPath, LPCTSTR destPath, int compressLevel, bool showsProgress);

	int findFirst(LPCTSTR wildName, INDIVIDUALINFO* p);
	int findNext(INDIVIDUALINFO* p);

	// 順序数  1
	int command(const HWND hwnd, LPCTSTR cmdLine, String& rOutput);

	// 順序数  10
	bool getRunning();

	// 順序数  12
	bool checkArchive();

	// 順序数  47
	int getAttribute();

	// 順序数  40
	int getFileName(String& rFilename);

	// 順序数  65
	bool getWriteTimeEx(FILETIME& rLastWriteTime);

	int setUnicodeMode();

private:
	ArchiveDll(const ArchiveDll& archiveDll);
	ArchiveDll& operator=(const ArchiveDll& archiveDll);

	FARPROC getFuncAddress(LPCTSTR funcName);

	String mDllFilename;
	String mPrefix;

	String archiveFilename_;

	HINSTANCE mDllHandle;
	HARC mArchiveHandle;

	ArchiveDllID::ArchiveDllID archiveDllID_;
};


class ArchiveDllManager {
public:
	ArchiveDll* addArchiveDll(ArchiveDllID::ArchiveDllID archiveDllID);
	ArchiveDll* getArchiveDll(ArchiveDllID::ArchiveDllID archiveDllID);

	void releaseArchiveDll(ArchiveDllID::ArchiveDllID archiveDllID);

	// 指定の書庫を扱うのに適当なアーカイブDLLを返す
	ArchiveDll* getSuitableArchiveDll(LPCTSTR filename);

private:
	std::vector<boost::shared_ptr<ArchiveDll> > archiveDlls_;
};

} // end of namespace KSDK

#endif //__ARCHIVEDLL_H__