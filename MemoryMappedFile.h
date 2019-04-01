#ifndef __MEMORYMAPPEDFILE_H__
#define __MEMORYMAPPEDFILE_H__

#include <windows.h>
#include <boost/shared_ptr.hpp>

namespace KSDK {

class MemoryMappedFile {
public:
	MemoryMappedFile() : fh_(NULL), hMap_(NULL), p_(NULL), fileSize_(0) {}

	bool open(LPCTSTR filename) {
		close();

		// ファイルオープン
		fh_ = CreateFile(filename, GENERIC_READ|GENERIC_WRITE, 0, NULL,OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL, NULL);

		if (fh_ == NULL) return false;

		fileSize_ = GetFileSize(fh_, NULL);

		if (fileSize_ == -1) return false;

		// ファイルマッピングオブジェクト作成
		hMap_ = CreateFileMapping(fh_, NULL, PAGE_READWRITE, 0, 0, NULL);

		if (hMap_ == NULL) return false;

		// ファイルをマップし、先頭アドレスをlpBufに取得
		p_ = (LPBYTE)MapViewOfFile(hMap_, FILE_MAP_WRITE, 0, 0, 0);

		if (p_ == NULL) return false;

		return true;
	}

	void close() {
		flush();

		if (p_ != NULL) {
			UnmapViewOfFile(p_);
			p_ = NULL;
		}
		if (hMap_ != NULL) {
			CloseHandle(hMap_);
			hMap_ = NULL;
		}
		if (fh_ != NULL) {
			CloseHandle(fh_);
			fh_ = NULL;
		}
	}

	~MemoryMappedFile() {
		close();
	}

	bool flush() {
		if (p_ == NULL) return false;
		return (FlushViewOfFile(p_, 0) != 0);
	}

	DWORD getFileSize() {
		return fileSize_;
	}

	void* getPointer() {
		return p_;
	}

private:
	HANDLE fh_, hMap_;
	void* p_;
	DWORD fileSize_;
};


// ファイルサイズの大きなファイル（1GBを超えるファイルなど）の場合マッピングに失敗する
class ReadOnlyMemoryMappedFile {
public:
	ReadOnlyMemoryMappedFile() : fh_(NULL), hMap_(NULL), p_(NULL), fileSize_(0) {}

	bool open(LPCTSTR filename) {
		close();

		// ファイルオープン
		fh_ = CreateFile(filename, GENERIC_READ, 0, NULL,OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (fh_ == NULL) return false;

		fileSize_ = GetFileSize(fh_, NULL);

		if (fileSize_ == -1) return false;

		// ファイルサイズが0の時は open() は成功するが
		// 読むものは無い
		if (fileSize_ == 0) {
			p_ = NULL;
			hMap_ = NULL;
			return true;
		}

		// ファイルマッピングオブジェクト作成
		hMap_ = CreateFileMapping(fh_, NULL, PAGE_READONLY, 0, 0, NULL);

		if (hMap_ == NULL) return false;

		// ファイルをマップし、先頭アドレスをlpBufに取得
		p_ = (LPBYTE)MapViewOfFile(hMap_, FILE_MAP_READ, 0, 0, 0);

		if (p_ == NULL) return false;

		return true;
	}
	
	void close() {
		if (p_ != NULL) {
			UnmapViewOfFile(p_);
			p_ = NULL;
		}
		if (hMap_ != NULL) {
			CloseHandle(hMap_);
			hMap_ = NULL;
		}
		if (fh_ != NULL) {
			CloseHandle(fh_);
			fh_ = NULL;
		}
	}

	~ReadOnlyMemoryMappedFile() {
		close();
	}

	DWORD getFileSize() {
		return fileSize_;
	}

	void* getPointer() {
		return p_;
	}

private:
	HANDLE fh_, hMap_;
	void* p_;
	DWORD fileSize_;
};


// View を別クラスとして複数のViewに対応し巨大なファイルに対応したバージョン
// ViewOfFile の作成にあたっては dwAllocationGranularity を気にしないでいいようになっている
// ViewOfFile を作成した ReadOnlyMemoryMappedFileEx は ViewOfFile より後に削除すること
class ViewOfFile {
public:
	ViewOfFile(HANDLE hFileMappingObject, UINT32 fileOffset, UINT32 numberOfBytesToMap) {
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		UINT32 g = si.dwAllocationGranularity;
		DWORD offset = (fileOffset / g) * g;
		UINT32 d = fileOffset - offset;
		pView_ = MapViewOfFile(hFileMappingObject, FILE_MAP_READ, 0, offset, numberOfBytesToMap + d);
		if (pView_ == NULL) {
			fileOffset_ = 0;
			mapSize_ = 0;
			p_ = NULL;
		} else {
			fileOffset_ = fileOffset;
			mapSize_ = numberOfBytesToMap;
			p_ = (UINT8*)pView_ + d;
		}
	}

	~ViewOfFile() {
		if (pView_ != NULL) {
			UnmapViewOfFile(pView_);
			pView_ = NULL;
			p_ = NULL;
		}
	}

	void* getPointer() {
		return p_;
	}

	UINT32 getFileOffset() {
		return fileOffset_;
	}

	UINT32 getMapSize() {
		return mapSize_;
	}

private:
	ViewOfFile();
	ViewOfFile(const ViewOfFile&);
	const ViewOfFile& operator=(const ViewOfFile&);
	
	void* pView_;
	void* p_;
	UINT32 fileOffset_;
	UINT32 mapSize_;
};


class ReadOnlyMemoryMappedFileEx {
public:
	ReadOnlyMemoryMappedFileEx() : fh_(NULL), hMap_(NULL), fileSize_(0) {}

	bool open(LPCTSTR filename) {
		close();

		// ファイルオープン
		fh_ = CreateFile(filename, GENERIC_READ, 0, NULL,OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (fh_ == INVALID_HANDLE_VALUE) return false;

		fileSize_ = GetFileSize(fh_, NULL);
		if (fileSize_ == -1) return false;

		// ファイルマッピングオブジェクト作成
		hMap_ = CreateFileMapping(fh_, NULL, PAGE_READONLY, 0, 0, NULL);
		if (hMap_ == NULL) return false;

		return true;
	}

	// fileOffset と numberOfBytesToMap を 0 と指定するとファイルサイズでマップされる
	boost::shared_ptr<ViewOfFile> map(UINT32 fileOffset, UINT32 numberOfBytesToMap) {
		boost::shared_ptr<ViewOfFile> p;
		if (hMap_ == NULL) return p;
		if (numberOfBytesToMap == 0 && fileOffset == 0) {
			numberOfBytesToMap = fileSize_;
		}
		p.reset(new ViewOfFile(hMap_, fileOffset, numberOfBytesToMap));
		if (p->getPointer() == NULL) p.reset();
		return p;
	}

	void close() {
		if (hMap_ != NULL) {
			CloseHandle(hMap_);
			hMap_ = NULL;
		}
		if (fh_ != NULL) {
			CloseHandle(fh_);
			fh_ = NULL;
		}
	}

	~ReadOnlyMemoryMappedFileEx() {
		close();
	}

	UINT32 getFileSize() {
		return fileSize_;
	}

private:
	HANDLE fh_, hMap_;
	UINT32 fileSize_;
};



}// end of namespace KSDK

#endif //__MEMORYMAPPEDFILE_H__
