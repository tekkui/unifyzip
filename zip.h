// Zipファイル処理
// 現在は無圧縮のみ対応
// USE_ZIP_DETAIL_INFO を定義すると local file の詳細情報が得られる
#ifndef __ZIP_H__
#define __ZIP_H__

#include <vector>
#include <list>
#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>

#include "MemoryMappedFile.h"
#include "String.h"

const unsigned long LANGUAGE_ENCODING_FLAG = 0x1 << 11;

namespace KSDK {

bool Compress(LPCTSTR srcPath, LPCTSTR dstPath, int compressLevel);
int Decompress(LPCTSTR zipFileName, LPCTSTR destPath);

struct LocalFileInfo {
	String fileName;	// 名前
	UINT32 fileDataPos;	// アーカイブ内でのファイルデータの位置
	UINT32 fileSize;	// サイズ

	UINT32 externalFileAttributes;

//	#ifdef USE_ZIP_DETAIL_INFO
		UINT32 uncompressedSize;
		FILETIME fileTime;
		UINT16 versionMadeBy;
		UINT16 versionNeededToExtract;
		UINT16 generalPurposeBitFlag;
		UINT16 compressionMethod;
		UINT16 internalFileAttributes;
		std::vector<UINT8> comment;
//	#endif
};


// Zip内の個々のファイルを読む為のクラス
class LocalFile : boost::noncopyable {
public:
	LocalFile(ReadOnlyMemoryMappedFileEx* pMemoryMappedFile, LocalFileInfo* pInfo) {
		pMemoryMappedFile_ = pMemoryMappedFile;
		pInfo_ = pInfo;
		pFileData_ = NULL;
		p_ = NULL;
	}

	int read(void* pBuffer, UINT32 len);
	bool seek(int offset, int origin);
	UINT32 tell() {
		return (UINT32)(p_ - pFileData_);
	}
	LPCTSTR getFileName() {
		return pInfo_->fileName.c_str();
	}
	UINT32 getFileSize() {
		return pInfo_->fileSize;
	}

	UINT32 getExternalFileAttributes() {
		return pInfo_->externalFileAttributes;
	}

	// Zip ファイルの何処にファイルデータがあるのか
	UINT32 getFileOffset() {
		return pInfo_->fileDataPos;
	}


//#ifdef USE_ZIP_DETAIL_INFO
	UINT32 getUncompressedSize() {
		return pInfo_->uncompressedSize;
	}
	FILETIME getFileTime() {
		return pInfo_->fileTime;
	}
	UINT16 getVersionMadeBy() {
		return pInfo_->versionMadeBy;
	}
	UINT16 getVersionNeededToExtract() {
		return pInfo_->versionNeededToExtract;
	}
	UINT16 getGeneralPurposeBitFlag() {
		return pInfo_->generalPurposeBitFlag;
	}
	UINT16 getCompressionMethod() {
		return pInfo_->compressionMethod;
	}
	UINT16 getInternalFileAttributes() {
		return pInfo_->internalFileAttributes;
	}

	UINT16 obtainInternalFileAttributes();

	UINT16 getCommentLength() {
		return (UINT16)(pInfo_->comment.size());
	}

	void getComment(void* pComment) {
		memcpy(pComment, &(*(pInfo_->comment.begin())), getCommentLength());
	}
//#endif

	// ポインタは取得できない方がいいっぽいか…
	// バッファ用意してそこにread()させて代用すべきかも
	// 無圧縮でないとポインター取得しても圧縮済みデータ参照することになるし
	// LocalFile １つのサイズ分マッピングする
	void* getPointer() {
		if (!pView_) {
			pView_ = pMemoryMappedFile_->map(pInfo_->fileDataPos, pInfo_->fileSize);
			if (!pView_) return NULL;
			pFileData_ = (UINT8*)(pView_->getPointer());
			p_ = pFileData_;
		}
		return p_;
	}

private:
	LocalFile();
	ReadOnlyMemoryMappedFileEx* pMemoryMappedFile_;
	LocalFileInfo* pInfo_;
	UINT8* pFileData_;
	UINT8* p_;
	boost::shared_ptr<ViewOfFile> pView_;

	// ポインタ取得のためメモリマップされた長さ
	UINT32 mappedSize_;
};


class ZipReader {
public:
	~ZipReader() {
		close();
	}

	// アーカイブファイルオープン
	// 内蔵ファイルの情報取得
	bool open(LPCTSTR fileName);
	void close();

	// 内蔵ファイルへのアクセッサを得る
	boost::shared_ptr<LocalFile> getLocalFile(LPCTSTR fileName);

	boost::shared_ptr<LocalFile> getLocalFile(int index);

	int getLocalFileNumber() {
		return (int)fileInfo_.size();
	}

	LPCTSTR getName() {
		return name_.c_str();
	}

private:
	String name_;
	std::vector<LocalFileInfo> fileInfo_;
	ReadOnlyMemoryMappedFileEx mmf_;
};

// singleton
class ZipReaderManager {
public:
	static ZipReaderManager& instance();
	ZipReader* getZipReader(LPCTSTR zipFileName);
private:
	ZipReaderManager() {}
	ZipReaderManager(const ZipReaderManager&);
	const ZipReaderManager& operator=(const ZipReaderManager&);

	// 書庫の数はそう多くないだろうから書庫の検索も線形探索で十分
	std::vector< boost::shared_ptr<ZipReader> > archives_;
};

class ZipWriter {
public:
	struct LocalFileData {
		String dstFileName;
		String srcFileName;
		DWORD fileAttributes;
		FILETIME lastWriteTime;

		// ファイルサイズはフォルダの場合 0 にすること
		UINT32 fileSize;
	};

	ZipWriter();
	~ZipWriter();

	bool open(LPCTSTR zipFileName) {
		zipFileName_ = zipFileName;
		isOpen_ = true;
		return true;
	}

	int close();

	void add(LPCTSTR dstFileName, LPCTSTR srcFileName);

	bool isOpen() {
		return isOpen_;
	}

	// -1: デフォルト
	//  0: 無圧縮
	//  1: 高速
	//  9: 高圧縮
	bool setCompressLevel(int compressLevel);

	bool addLackFolder();

	UINT16 getInternalFileAttributes(LPCTSTR fileName, UINT32 fileAttributes, UINT32 fileSize);

private:
	static const int WRITEBUFFERSIZE = 16384;

	int closeSub();
	UINT32 getCRC32(LPCTSTR fileName);

	bool correctLocalFileData(LocalFileData& localFileData);

	bool isOpen_;
	int compressLevel_;

	String zipFileName_;

	std::list<LocalFileData> localFileData_;
};

} // end of namespace KSDK

#endif //__ZIP_H__
