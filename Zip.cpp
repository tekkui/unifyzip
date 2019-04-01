#include "Zip.h"

#include <boost/crc.hpp>
#include <tchar.h>
#include <AtlConv.h>

#define ZLIB_WINAPI

//#pragma comment(lib, "zlibwapi.lib")
//#pragma comment(lib, "zlib.lib")
#pragma comment(lib, "zlibstat.lib")

//#include "../zlib/contrib/minizip/zip.h"
#include "../zlib-1.2.11/contrib/minizip/zip.h"
#include "../zlib-1.2.11/contrib/minizip/iowin32.h"

#include "StrFunc.h"
#include "PathFunc.h"
#include "BinaryFile.h"
#include "Buffer.h"

using namespace std;
using namespace boost;

const uLong LANGUAGE_ENCODING_FLAG = 0x1 << 11;

namespace {

// ファイルの更新日時を設定する
bool SetFileLastWriteTime(LPCTSTR filename, const FILETIME* pLastWriteTime) {
	HANDLE hFile = CreateFile(filename, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return false;
	}
	bool b = (SetFileTime(hFile, NULL, NULL, pLastWriteTime) != 0);
	CloseHandle(hFile);
	return b;
}

} // end of namespace

namespace KSDK {

inline bool isASCII(UINT8 c) {
	return ((c & 0x80) == 0);
}

inline bool isASCII(UINT32 c) {
	return ((c & 0x80808080) == 0);
}

inline bool isASCII(const void* p, UINT32 length) {
	const UINT32* p32 = (const UINT32*)p;
	int n = length >> 2;
	for (int i=0; i<n; ++i, ++p32) {
		if (!isASCII(*p32)) return false;
	}
	int residue = length & 0x3;
	if (residue == 0) return true;
	const UINT8* p8 = (const UINT8*)p32;
	for (int i=0; i<residue; ++i, ++p8) {
		if (!isASCII(*p8)) return false;
	}
	return true;
}

// 解凍
int DecompressSub(const void* pInput, UINT32 inputSize, Buffer& outputBuffer, UINT32 uncompressedSize)
{
	outputBuffer.clear();
	outputBuffer.reserve(uncompressedSize);

//	const int OUTBUFSIZ = 0x4000;
//	const int INBUFSIZ = 16384;         /* 入力バッファサイズ（任意） */

	const char* pin = (const char*)pInput;

	// 出力バッファ
//	char outbuf[OUTBUFSIZ];

//	int count;
	int status;

	/* ライブラリとやりとりするための構造体 */
	z_stream z;					 


/*
	// すべてのメモリ管理をライブラリに任せる
	z.zalloc = Z_NULL;
	z.zfree = Z_NULL;
	z.opaque = Z_NULL;
*/



	z.total_out = 0;
	z.avail_out = outputBuffer.getBufferLength();
	z.next_out = (Bytef*)outputBuffer.getPointer();
	z.total_in = 0;
	z.avail_in = inputSize;
	z.next_in = (Bytef*)pin;

	z.zalloc = 0;
	z.zfree = 0;
	z.opaque = 0;



/*
	// 入力ポインタをセット
	z.next_in = (Bytef*)pin;
	if (inputSize >= INBUFSIZ) {
		z.avail_in = INBUFSIZ;
		inputSize -= INBUFSIZ;
	} else {
		z.avail_in = inputSize;
		inputSize = 0;
	}
	pin+=z.avail_in;

	// 初期化
	z.next_in = Z_NULL;
	z.avail_in = 0;
*/

	if (inflateInit2(&z, -MAX_WBITS) != Z_OK) {
		return -1;
	}

/*
	z.next_out = (Bytef*)outbuf;		// 出力ポインタ
	z.avail_out = OUTBUFSIZ;	// 出力バッファ残量
*/

	status = Z_OK;

	while (status != Z_STREAM_END) {
/*
		if (z.avail_in == 0) {  // 入力残量がゼロになれば
			// 入力ポインタをセット
			z.next_in = (Bytef*)pin;
			if (inputSize >= INBUFSIZ) {
				z.avail_in = INBUFSIZ;
				inputSize -= INBUFSIZ;
			} else {
				z.avail_in = inputSize;
				inputSize = 0;
			}
			pin+=z.avail_in;
		}
*/

		// 入力残量が0になる事は無いはず
		if (z.avail_in == 0) {
			return -1;
		}

		//status = inflate(&z, Z_SYNC_FLUSH); /* 展開 */

		// 一度で展開する場合は Z_FINISH
		status = inflate(&z, Z_FINISH); /* 展開 */
		

		if (status == Z_STREAM_END) break; /* 完了 */
		if (status != Z_OK) {   /* エラー */
			return -2;
		}

/*
		if (z.avail_out == 0) { // 出力バッファが尽きれば
			// まとめて書き出す
			if (!outputBuffer.append(outbuf, OUTBUFSIZ)) {
				return -3;
			}
			z.next_out = (Bytef*)outbuf; // 出力ポインタを元に戻す
			z.avail_out = OUTBUFSIZ; // 出力バッファ残量を元に戻す
		}
*/

		// 出力バッファが尽きることも無いはず
		if (z.avail_out == 0) {
			return -1;
		}
	}

	// 残りを吐き出す
/*
	if ((count = OUTBUFSIZ - z.avail_out) != 0) {
		if (!outputBuffer.append(outbuf, count)) {
			return -4;
		}
	}
*/
	outputBuffer.setDataLength(z.total_out);


	/* 後始末 */
	if (inflateEnd(&z) != Z_OK) {
		return -5;
	}

	return 0;
}

// 解凍
// 指定の書庫内の全ファイルを指定の書庫内にフォルダ付きで解凍する
// destPath（フルパスのみ）
// zipFileName
int Decompress(LPCTSTR zipFileName, LPCTSTR destPath) {
	ZipReader zip;
	if (!zip.open(zipFileName)) return -1;

	int localFileNum = zip.getLocalFileNumber();

	Buffer outputBuffer;

	for (int i=0; i<localFileNum; ++i) {
		boost::shared_ptr<LocalFile> pLocalFile = zip.getLocalFile(i);

		UINT32 attributes = pLocalFile->getExternalFileAttributes();

		String path = destPath;
		CatPath(path, pLocalFile->getFileName());
		if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (!CreateDirectoryEx(path.c_str())) {
				return -2;
			}
		} else {
			String fileName;
			GetFileName(path.c_str(), fileName);
			RemoveFileName(path);
			if (!CreateDirectoryEx(path.c_str())) {
				return -3;
			}
			CatPath(path, fileName.c_str());
			BinaryFileWriter file;
			if (!file.open(path.c_str())) {
				return -4;
			}
			try {
				if (pLocalFile->getCompressionMethod() == 0) {
					file.write(pLocalFile->getPointer(), pLocalFile->getFileSize());
				} else {
					int r = DecompressSub(pLocalFile->getPointer(), pLocalFile->getFileSize(),
						outputBuffer, pLocalFile->getUncompressedSize());
					if (r < 0) return -5;
					file.write(outputBuffer.getPointer(), outputBuffer.getDataLength());
				}
				file.close();
				// 更新日時の設定
				if (!SetFileLastWriteTime(path.c_str(), &(pLocalFile->getFileTime()))) {
					return -7;
				}
			}
			catch (...) {
				return -6;
			}
		}
	}
	return 0;
}


// 指定フォルダ内を検索してファイルを追加
// basePathLength は ZipWriter に追加するファイルのフルパスから何文字
// 削ったら Zip 内でのそのファイルのパスになるか（'\\' が末尾についてるパスなのかを注意）
bool CompressSub(LPCTSTR pszDir, ZipWriter& zw, int basePathLength) {
	WIN32_FIND_DATA fd;

	String searchPath;
	HANDLE hSearch;

	// 探索パスを作成
	searchPath += pszDir;
	FormatTooLongPath(searchPath);
	CatPath(searchPath, _T("*.*"));

	hSearch = FindFirstFile(searchPath.c_str(), &fd);

	if (hSearch==INVALID_HANDLE_VALUE) return false;

	do {
		if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)!=0) {
			// ルート及びカレントでないフォルダかどうかチェック
			if (lstrcmp(fd.cFileName,_T("."))!=0 && lstrcmp(fd.cFileName,_T(".."))!=0) {
				String path = pszDir;
				CatPath(path, fd.cFileName);

				// フォルダのパスを追加
				zw.add(path.c_str()+basePathLength, path.c_str());

				// サブフォルダの検索
				if(!CompressSub(path.c_str(), zw, basePathLength)) {
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

bool Compress(LPCTSTR srcPath, LPCTSTR dstPath, int compressLevel)
{
	// フォルダ以下を圧縮
	bool b;
	ZipWriter zw;
	b = zw.open(dstPath);
	if (!b) return false;

	// basePathLength は 末尾に '\\' がついているかどうかで変わる
	int basePathLength = lstrlen(srcPath);
	if (strrchrex(srcPath, _TCHAR('\\')) != srcPath+(basePathLength-1)) {
		basePathLength++;
	}

	b = CompressSub(srcPath, zw, basePathLength);
	if (!b) return false;
	zw.setCompressLevel(compressLevel);
	int n = zw.close();
	if (n != 0) return false;
	return true;
}


int LocalFile::read(void* pBuffer, UINT32 len) {
	if (getPointer() == NULL) return 0;
	UINT32 maxLen = getFileSize();
	UINT32 offset = (UINT32)(p_ - pFileData_);
	maxLen -= offset;
	if (len > maxLen) len = maxLen;
	if (len > 0) {
		memcpy(pBuffer, p_, len);
		p_+=len;
	} else {
		len = 0;
	}
	return len;
}

bool LocalFile::seek(int offset, int origin) {
	if (getPointer() == NULL) return false;

	if (origin == SEEK_CUR) {
		p_ += offset;
	} else if (origin == SEEK_END) {
		p_ = pFileData_ + getFileSize() + offset - 1;
	} else if (origin == SEEK_SET) {
		p_ = pFileData_ + offset;
	} else {
		return false;
	}

	if (p_ < pFileData_) {
		p_ = pFileData_;
	} else if (p_ > pFileData_ + getFileSize() - 1) {
		p_ = pFileData_ + getFileSize() - 1;
	}

	return true;
}

//#ifdef USE_ZIP_DETAIL_INFO
// 書庫内のファイルを走査して InternalFileAttributes を得る
UINT16 LocalFile::obtainInternalFileAttributes() 
{
	UINT32 fileAttributes = getExternalFileAttributes();
	UINT32 fileSize = getFileSize();

	// フォルダは 0
	if (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) return 0;

	// 空ファイルは 1
	if (fileSize == 0) return 1;

	UINT32 offset = getFileOffset();

	SYSTEM_INFO si;
	GetSystemInfo(&si);
	const UINT32 MIN_MAPPING_SIZE = min(256, si.dwAllocationGranularity);
	UINT32 mappingSize = MIN_MAPPING_SIZE;
	UINT32 prevMappingSize = 0;
	if (mappingSize > fileSize) {
		mappingSize = fileSize;
	}
	UINT32 checkedSize = 0;
	bool maximizes = false;
	for (;;) {
		boost::shared_ptr<ViewOfFile> pView(pMemoryMappedFile_->map(offset + checkedSize, mappingSize));
		if (pView->getPointer() == NULL) {
			// MIN_MAPPING_SIZE で
			// マッピングが失敗するならお手上げ
			if (mappingSize <= MIN_MAPPING_SIZE) return 0;
			// これ以上マッピングサイズを増やせないのだろう
			maximizes = true;
			mappingSize = prevMappingSize;
			pView.reset();
			pView = pMemoryMappedFile_->map(offset+checkedSize, prevMappingSize);
			if (pView->getPointer() == NULL) return 0;
		}
		if (!isASCII(pView->getPointer(), mappingSize)) {
			return 0;
		}
		checkedSize += mappingSize;
		if (checkedSize == fileSize) break;
		prevMappingSize = mappingSize;
		if (!maximizes) {
			mappingSize <<= 1;
		}
		if (checkedSize + mappingSize > fileSize) {
			mappingSize = fileSize - checkedSize;
		}
	}
	return 1;
}
//#endif // USE_ZIP_DETAIL_INFO



inline UINT32 GetUINT32(UINT8* p, UINT32 offset) {
	return *((UINT32*)(p+offset));
}

inline UINT16 GetUINT16(UINT8* p, UINT32 offset) {
	return *((UINT16*)(p+offset));
}



bool ZipReader::open(LPCTSTR fileName) {
	close();

	// ファイルサイズがUINT32の範囲を超えていたらエラーとする
	WIN32_FIND_DATA fd;
	HANDLE hFindFile = FindFirstFile(fileName, &fd);
	if (hFindFile != INVALID_HANDLE_VALUE) {
		DWORD d = fd.nFileSizeHigh;
		FindClose(hFindFile);
		if (d != 0) return false;
	} else {
		return false;
	}

	if (!mmf_.open(fileName)) return false;
	
	{
		// シグネチャチェック
		boost::shared_ptr<ViewOfFile> pView = mmf_.map(0, sizeof(UINT32));
		if (!pView) return false;
		if ( *((UINT32*)pView->getPointer()) != 0x04034b50) return false;
	}

	// zipに格納されているファイルの数(分割zipは非対応)
	INT16 filenum;

	boost::shared_ptr<ViewOfFile> pCentralDirView;

	// ファイル終端から End of central directory record の最大長の範囲で
	// end of central dir signature を探す
	{
		const UINT32 MAX_EOCDR = (0xffff + 22);

		boost::shared_ptr<ViewOfFile> pEOCDRView;
		{
			UINT32 fileSize = mmf_.getFileSize();
			UINT32 size = MAX_EOCDR;
			if (fileSize < size) {
				pEOCDRView = mmf_.map(0, 0);
			} else {
				pEOCDRView = mmf_.map(fileSize - MAX_EOCDR, size);
			}
			if (!pEOCDRView) return false;
		}
		for (UINT32 i=pEOCDRView->getMapSize() - 22; ; --i) {
			if (GetUINT32((UINT8*)(pEOCDRView->getPointer()), i) == 0x06054b50) { // "PK\0x05\0x06"
				UINT8* pEOCDR = (UINT8*)pEOCDRView->getPointer() + i;
				UINT16 endcommentlength = GetUINT16(pEOCDR, 20);
				if (pEOCDRView->getFileOffset() + i + 22 + endcommentlength != mmf_.getFileSize()) {
					continue;
				}
				filenum = GetUINT16(pEOCDR, 10);
				UINT32 centralDirectorySize = GetUINT32(pEOCDR, 12);
				UINT32 centralDirectoryPos = GetUINT32(pEOCDR, 16);

				// central directory をマップ
				pCentralDirView = mmf_.map(centralDirectoryPos, centralDirectorySize);
				if (!pCentralDirView) return false;

				goto endrecFound;
			}
			// UINT32 なので 0 より小さいかの判定が出来ないので
			// ここで判定
			if (i == 0) break;
		}
	}
	// End of central directory record を見つけられなかった
	return false;

endrecFound:;

	// central directory を先頭から走査

	UINT8* pCentralDir = (UINT8*)pCentralDirView->getPointer();

	while (filenum-- > 0) {
		if ( *((UINT32*)pCentralDir) != 0x02014b50 ) {	// シグネチャー確認
			return false; // ファイルがおかしい
		}

//		#ifdef USE_ZIP_DETAIL_INFO
			UINT16 versionMadeBy = GetUINT16(pCentralDir, 4);
			UINT16 versionNeededToExtract = GetUINT16(pCentralDir, 6);
			UINT16 generalPurposeBitFlag = GetUINT16(pCentralDir, 8);
//		#endif

		// compression method == 8 についてはzlibで解凍可能なようだが無視
		UINT16 compressionMethod = GetUINT16(pCentralDir, 10);

		UINT16 last_mod_file_time = GetUINT16(pCentralDir, 12);
		UINT16 last_mod_file_date = GetUINT16(pCentralDir, 14);
		FILETIME ft, fileTime;
		DosDateTimeToFileTime(last_mod_file_date, last_mod_file_time, &ft);
		LocalFileTimeToFileTime(&ft, &fileTime);

		UINT32 compressed_size = GetUINT32(pCentralDir, 20);
		UINT32 uncompressed_size = GetUINT32(pCentralDir, 24);
		UINT16 filename_length = GetUINT16(pCentralDir, 28);
		UINT16 extra_field_length = GetUINT16(pCentralDir, 30);
		UINT16 file_comment_length = GetUINT16(pCentralDir, 32);

//		#ifdef USE_ZIP_DETAIL_INFO
			UINT16 internal_file_attributes = GetUINT16(pCentralDir, 36);
//		#endif

		UINT32 external_file_attributes = GetUINT32(pCentralDir, 38);

//		bool isEncrypted = (generalPurposeBitFlag & 1);

		// 念のためファイルサイズも確認
		bool isUncompressed = (compressionMethod == 0);
		if (compressed_size != uncompressed_size) {
			isUncompressed = false;
		}

		// ファイル名の取得
		String fileName;
		#ifdef _UNICODE
			{
				// TODO 終端のNULL文字の位置チェック
				char* src = (char*)(pCentralDir+46);
				int srclen = filename_length;
				TCHAR dst[MAX_PATH];
				int dstlen = MultiByteToWideChar( CP_UTF8, 0, src, srclen, dst, MAX_PATH);
				if (dstlen == 0 || dstlen >= MAX_PATH) {
					return false;
				}
				dst[dstlen] = 0;
				fileName = srclen;
			}
		#else
			fileName.NCopy((char*)(pCentralDir+46), filename_length);
		#endif


		// '/' 区切りを '\\' 区切りに直す
		fileName.Replace(_TCHAR('/'), _TCHAR('\\'));

		// 末尾の '\\' を取り除く
		FixPath(fileName);


		// local file header と central directory の情報は同じであると仮定して
		// 全て central directory から取得する
		// ただし local file を読む際にシグネチャの確認ぐらいはした方がいいかもしれない
		// さらに言えば fileDataPos も local file header からとったデータで計算した方がいいのかも

		UINT32 localHeaderPos = GetUINT32(pCentralDir, 42);
		UINT32 fileDataPos = localHeaderPos + 30 + filename_length + extra_field_length;

		/*
		// Local file header に移動して情報を得る
		shared_ptr<ViewOfFile> pLocalFileHeaderView;
		{
			pLocalFileHeaderView = mmf_.map(localHeaderPos, 30);
			if (!pLocalFileHeaderView) return false;
		}
		UINT8* pLocalFileHeader = pLocalFileHeaderView.getPointer();

		if (GetUINT32(pLocalFileHeader, lh_pos) != 0x04034b50) {
			// Local file header 先頭のシグネチャがおかしい
			return false;
		}

		UINT16 lh_filename_length = GetUINT16(pLocalFileHeader, localHeaderPos+26);
		UINT16 lh_extra_field_length = GetUINT16(pLocalFileHeader, localHeaderPos+28);
		UINT32 fileDataPos = localHeaderPos + 30 + lh_filename_length + lh_extra_field_length;
		*/


		// 無圧縮で暗号化されていなければ格納
		//if (!isEncrypted && isUncompressed) {
			LocalFileInfo info;
			info.fileName = fileName;
			info.fileDataPos = fileDataPos;
			info.fileSize = compressed_size;

			info.externalFileAttributes = external_file_attributes;

//			#ifdef USE_ZIP_DETAIL_INFO
				info.uncompressedSize = uncompressed_size;
				info.fileTime = fileTime;
				info.versionMadeBy = versionMadeBy;
				info.versionNeededToExtract = versionNeededToExtract;
				info.generalPurposeBitFlag = generalPurposeBitFlag;
				info.compressionMethod = compressionMethod;
				info.internalFileAttributes = internal_file_attributes;

				{
					UINT8* pComment = (pCentralDir + 46 + filename_length + extra_field_length);
					info.comment.clear();
					info.comment.reserve(file_comment_length);
					for (int i=0; i<file_comment_length; ++i) {
						info.comment.push_back(pComment[i]);
					}
				}
//			#endif

			fileInfo_.push_back(info);
		//} else {
			// エラーとしてしまう
		//	return false;
		//}

		// 次の格納ファイルへ
		pCentralDir += 46 + filename_length + extra_field_length + file_comment_length;

		// ポインタが正しいか確認すべき？
	}

	name_ = fileName;

	return true;
}

void ZipReader::close() {
	name_.Empty();
	fileInfo_.clear();
	mmf_.close();
}

boost::shared_ptr<LocalFile> ZipReader::getLocalFile(LPCTSTR fileName) {
	boost::shared_ptr<LocalFile> p;

	vector<LocalFileInfo>::iterator it;
	for (it=fileInfo_.begin(); it != fileInfo_.end(); ++it) {
		if (ComparePath(it->fileName.c_str(), fileName) == 0) {
			p.reset(new LocalFile(&mmf_, &(*it)));
			return p;
		}
	}
	return p;
}

boost::shared_ptr<LocalFile> ZipReader::getLocalFile(int index) {
	boost::shared_ptr<LocalFile> p;

	if (index < 0 || index >= (int)fileInfo_.size() ) return p;

	LocalFileInfo& r = fileInfo_[index];
	p.reset(new LocalFile(&mmf_, &r));
	return p;
}


ZipReaderManager& ZipReaderManager::instance() {
    static ZipReaderManager instance;
    return instance;
}

ZipReader* ZipReaderManager::getZipReader(LPCTSTR zipFileName) {
	vector< boost::shared_ptr<ZipReader> >::iterator it;
	for (it=archives_.begin(); it!=archives_.end(); ++it) {
		if (ComparePath((*it)->getName(), zipFileName) == 0) {
			return it->get();
		}
	}

	boost::shared_ptr<ZipReader> p(new ZipReader);
	if (p->open(zipFileName)) {
		archives_.push_back(p);
		return p.get();
	} else {
		return NULL;
	}
}

ZipWriter::ZipWriter() : isOpen_(false), compressLevel_(Z_DEFAULT_COMPRESSION) {}

/*
uLong filetime(LPCTSTR f, tm_zip* tmzip, uLong* dt)
{
	int ret = 0;
	{
		FILETIME ftLocal;
		HANDLE hFind;
		WIN32_FIND_DATA  ff32;

		hFind = FindFirstFile(f,&ff32);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			FileTimeToLocalFileTime(&(ff32.ftLastWriteTime),&ftLocal);
			FileTimeToDosDateTime(&ftLocal,((LPWORD)dt)+1,((LPWORD)dt)+0);
			FindClose(hFind);
			ret = 1;
		}
	}
	return ret;
}
*/

bool ZipWriter::addLackFolder() 
{	
	String path;
	list<LocalFileData>::iterator it;	
	for (it=localFileData_.begin(); it!=localFileData_.end(); ++it) {
		// 階層の違い
		int difference;

		if ((it->fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			// フォルダを見つけたらパスを記憶
			// ただし、２階層一気に追加される可能性もあるので
			// １階層ずつチェック

			String common;
			GetCommonPrefix(it->dstFileName.c_str(), path.c_str(), common);

			int commonDepth = PathGetDepth(common.c_str());
			int dstDepth = PathGetDepth(it->dstFileName.c_str());

			difference = dstDepth - commonDepth;

			path = it->dstFileName;
		} else {
			// ファイルを見つけたら
			// そのファイルを持つディレクトリがあるかチェックする
			String parentFolderPath = it->dstFileName;
			StripPath(parentFolderPath);
			int r = lstrcmpi(parentFolderPath.c_str(), path.c_str());
			if (r == 0) {
				// フォルダは登録済み
				continue;
			} else if (r < 0) {
				// 上の階層のフォルダにあった
				path = parentFolderPath;
				continue;
			}

			String common;
			GetCommonPrefix(it->dstFileName.c_str(), path.c_str(), common);

			int commonDepth = PathGetDepth(common.c_str());
			int dstDepth = PathGetDepth(it->dstFileName.c_str());

			difference = dstDepth - commonDepth;

			path = parentFolderPath;
		}

		// srcFileName はパスを削って大丈夫なのか
		int srcDepth = PathGetDepth(it->srcFileName.c_str());
		if (srcDepth <= difference-1) return false;

		// 欠けているフォルダを挿入
		for (int i=difference-1; i>0; --i) {
			LocalFileData lfd;
			lfd.srcFileName = it->srcFileName.c_str();
			lfd.dstFileName = it->dstFileName.c_str();
			StripPath(lfd.srcFileName, i);
			StripPath(lfd.dstFileName, i);
			if (!correctLocalFileData(lfd)) return false;
			it = localFileData_.insert(it, lfd);
			it++;
		}
	}
	return true;
}

// ソート用
// エクスプローラと同じ並び順にする
// フォルダにフォルダ内のファイルが続くことが保証される
struct SortCriterion : public binary_function <ZipWriter::LocalFileData, ZipWriter::LocalFileData, bool> 
{
	bool operator()(const ZipWriter::LocalFileData& _Left, const ZipWriter::LocalFileData& _Right) const 
	{
		return ComparePath(_Left.dstFileName.c_str(), _Right.dstFileName.c_str(), 
			(_Left.fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0, 
			(_Right.fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0);
	}
};

#ifdef _UNICODE
typedef struct {
	HANDLE hf;
	int error;
} WIN32FILE_IOWIN;

void* ZipOpenFunc(void *opaque, const char* filename, int mode) {
	DWORD desired_access = 0, creation_disposition = 0;
	DWORD share_mode = 0, flags_and_attributes = 0;
	HANDLE file = 0;
	void* ret = NULL;
	if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER) == ZLIB_FILEFUNC_MODE_READ) {
		desired_access = GENERIC_READ;
		creation_disposition = OPEN_EXISTING;
		share_mode = FILE_SHARE_READ;
	}
	else if (mode & ZLIB_FILEFUNC_MODE_EXISTING) {
		desired_access = GENERIC_WRITE | GENERIC_READ;
		creation_disposition = OPEN_EXISTING;
	}
	else if (mode & ZLIB_FILEFUNC_MODE_CREATE) {
		desired_access = GENERIC_WRITE | GENERIC_READ;
		creation_disposition = CREATE_ALWAYS;
	}

	file = CreateFile(LPCWSTR(filename), desired_access, share_mode,
			NULL, creation_disposition, flags_and_attributes, NULL);

	if (file == INVALID_HANDLE_VALUE)
		file = NULL;
	if (file != NULL) {
		WIN32FILE_IOWIN file_ret;
		file_ret.hf = file;
		file_ret.error = 0;
		ret = malloc(sizeof(WIN32FILE_IOWIN));
		if (ret == NULL)
			CloseHandle(file);
		else
			*(static_cast<WIN32FILE_IOWIN*>(ret)) = file_ret;
	}
	return ret;
}
#endif

int ZipWriter::close()
{
	// 無圧縮の場合は自前で処理
	if (compressLevel_ == 0) {
		int r = closeSub();
		return r;
	}

	if (!isOpen_) return 0;

	list<LocalFileData>::iterator it;
	for (it=localFileData_.begin(); it!=localFileData_.end(); ++it) {
		if (!correctLocalFileData(*it)) return -1;
	}

	// ソート
	localFileData_.sort(SortCriterion());

	// 足りないフォルダの追加
	addLackFolder();

	int err=ZIP_OK;

	Buffer buf;
	if (!buf.reserve(WRITEBUFFERSIZE)) {
		return -1;
	}

	zipFile zf;

#ifdef _UNICODE
	zlib_filefunc_def* zip_func_ptrs = NULL;
	zlib_filefunc_def zip_funcs;
	fill_win32_filefunc(&zip_funcs);
	zip_funcs.zopen_file = ZipOpenFunc;
	zip_func_ptrs = &zip_funcs;

	zf = zipOpen2((const char *)zipFileName_.c_str(), APPEND_STATUS_CREATE, 0, zip_func_ptrs);
#else
	zf = zipOpen(zipFileName_.c_str(), 0);
#endif

	if (zf == NULL) {
		err= ZIP_ERRNO;
		return -1;
	}

	for (it=localFileData_.begin(); it!=localFileData_.end(); ++it) {
		FILE* fin=NULL;
		int size_read;
		zip_fileinfo zi;

		zi.tmz_date.tm_sec = zi.tmz_date.tm_min = zi.tmz_date.tm_hour =
		zi.tmz_date.tm_mday = zi.tmz_date.tm_mon = zi.tmz_date.tm_year = 0;
		zi.dosDate = 0;
		zi.internal_fa = getInternalFileAttributes(it->srcFileName.c_str(), it->fileAttributes, it->fileSize);
		zi.external_fa = it->fileAttributes;

		FILETIME ftLocal;
		uLong* dt = &zi.dosDate;
		FileTimeToLocalFileTime(&(it->lastWriteTime),&ftLocal);
		FileTimeToDosDateTime(&ftLocal,((LPWORD)dt)+1,((LPWORD)dt)+0);

		// '\\' を '/' に変える
		// フォルダの場合は末尾に '/' を加える
		String dstFileName = it->dstFileName;
		dstFileName.Replace(_TCHAR('\\'), _TCHAR('/'));
		if ((it->fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			if (dstFileName.GetAt(dstFileName.GetLength()-1) != _TCHAR('/')) {
				dstFileName.Cat(_TCHAR('/'));
			}
		}

#ifdef _UNICODE
		err = zipOpenNewFileInZip4(zf, (const char*)dstFileName.utf8_str(), &zi,
			NULL,0,NULL,0,NULL /* comment*/,
			(compressLevel_ != 0) ? Z_DEFLATED : 0,
			compressLevel_, 0,
			-MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
			NULL, 0,
			0, LANGUAGE_ENCODING_FLAG);
#else
		err = zipOpenNewFileInZip3(zf, dstFileName.c_str(), &zi,
			NULL,0,NULL,0,NULL /* comment*/,
			(compressLevel_ != 0) ? Z_DEFLATED : 0,
			compressLevel_, 0,
			-MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
			NULL, 0);
#endif

		if (err != ZIP_OK) {
			//_tprintf("error in opening %s in zipfile\n", filenameinzip);
			break;
		}

		if ((it->fileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
			fin = _tfopen(it->srcFileName.c_str(), _T("rb"));
			if (fin==NULL) {
				err=ZIP_ERRNO;
				_tprintf(_T("error in opening %s for reading\n"), it->srcFileName.c_str());
				break;
			}

			do {
				err = ZIP_OK;
				size_read = (int)fread(buf.getPointer(), 1, buf.getBufferLength(), fin);
				if (size_read < buf.getBufferLength() && feof(fin)==0) {
					//_tprintf(_T("error in reading %s\n"),it->srcFileName.c_str());
					err = ZIP_ERRNO;
				}

				if (size_read>0) {
					err = zipWriteInFileInZip (zf, buf.getPointer(), size_read);
					if (err<0) {
						_tprintf(_T("error in writing %s in the zipfile\n"),
							it->srcFileName.c_str());
					}
				}
			} while ((err == ZIP_OK) && (size_read>0));
			if (fin) {
				fclose(fin);
			}
		} else {
		}

		if (err<0) {
			err = zipCloseFileInZip(zf);
			if (err!=ZIP_OK) {
				//_tprintf("error in closing %s in the zipfile\n", 
				//	filenameinzip);
				break;
			}
			err = ZIP_ERRNO;
			break;
		} else {
			err = zipCloseFileInZip(zf);
			if (err!=ZIP_OK) {
				//_tprintf("error in closing %s in the zipfile\n", 
				//	filenameinzip);
				break;
			}
		}
	}

	int errclose = zipClose(zf, NULL);
	if (errclose != ZIP_OK) {
		//_tprintf("error in closing %s\n",filename_try);
		return -1;
	}

	if (err==ZIP_OK) {
		isOpen_ = false;
	} else {
		// 失敗しているのでファイル削除
		DeleteFileOrFolder(zipFileName_.c_str(), false);
		return -1;
	}

	return 0;
}

int ZipWriter::closeSub() 
{
	if (!isOpen_) return 0;

	list<LocalFileData>::iterator it;
	for (it=localFileData_.begin(); it!=localFileData_.end(); ++it) {
		if (!correctLocalFileData(*it)) return -1;
	}

	// ソート
	localFileData_.sort(SortCriterion());

	// 足りないフォルダの追加
	addLackFolder();

	BinaryFileWriter zipFile;
	if (!zipFile.open(zipFileName_.c_str())) {
		return -1;
	}

	try {
		Buffer centralDirectory;

		list<LocalFileData>::iterator it;
		for (it = localFileData_.begin(); it != localFileData_.end(); ++it) {
			UINT32 crc;

			FILETIME ft;
			FileTimeToLocalFileTime(&(it->lastWriteTime), &ft);

			WORD fatDate, fatTime;
			if (!FileTimeToDosDateTime(&ft, &fatDate, &fatTime)) {
				throw RuntimeException();
			}

			if ((it->fileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && it->fileSize != 0) {
				crc = getCRC32(it->srcFileName.c_str());
				if (crc == 0) {
					throw RuntimeException();
				}
			} else {
				crc = 0;
			}

			UINT32 relativeOffset = zipFile.tell();

			// '\\' を '/' に変える
			// フォルダの場合は末尾に '/' を加える
			String dstFileName = it->dstFileName;
			dstFileName.Replace(_TCHAR('\\'), _TCHAR('/'));
			if ((it->fileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
				if (dstFileName.GetAt(dstFileName.GetLength()-1) != _TCHAR('/')) {
					dstFileName.Cat(_TCHAR('/'));
				}
			}

			// Local file header

			// local file header signature     4 bytes  (0x04034b50)
			UINT32 sig = 0x04034b50;
			zipFile.write(&sig, sizeof(UINT32));

			// version needed to extract       2 bytes
			UINT16 vnte;
			if ((it->fileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				vnte = 0x000a;
			} else {
				vnte = 0x0014;
			}
			zipFile.write(&vnte, sizeof(UINT16));

			// general purpose bit flag        2 bytes
			// 暗号化無しで無圧縮ならば0でいい
			UINT16 flag = 0x0000;
#ifdef _UNICODE
			flag = flag | LANGUAGE_ENCODING_FLAG;
#endif
			zipFile.write(&flag, sizeof(UINT16));

			// compression method              2 bytes
			// 無圧縮ならば0でいい
			UINT16 method = 0x0000;
			zipFile.write(&method, sizeof(UINT16));

			// last mod file time              2 bytes
			// last mod file date              2 bytes
			zipFile.write(&fatTime, sizeof(WORD));
			zipFile.write(&fatDate, sizeof(WORD));

			// crc-32                          4 bytes
			// (フォルダの場合は0)
			zipFile.write(&crc, sizeof(UINT32));

			// compressed size                 4 bytes
			// uncompressed size               4 bytes
			zipFile.write(&(it->fileSize), sizeof(UINT32));
			zipFile.write(&(it->fileSize), sizeof(UINT32));

			// file name length                2 bytes
			// フォルダの場合 末尾に / が付くがその長さも含める
#ifdef _UNICODE
			UINT16 fileNameLen = (UINT16)dstFileName.GetUtf8Length();
#else
			UINT16 fileNameLen = (UINT16)dstFileName.GetLength();
#endif
			zipFile.write(&fileNameLen, sizeof(UINT16));

			// extra field length              2 bytes
			UINT16 extraFieldLen = 0x0000;
			zipFile.write(&extraFieldLen, sizeof(UINT16));

			// file name (variable size)
#ifdef _UNICODE
			zipFile.write((void*)dstFileName.utf8_str(), fileNameLen);
#else
			zipFile.write((void*)dstFileName.c_str(), fileNameLen);
#endif
			// extra field (variable size)

			// file data
			if (it->fileSize != 0) {
				ReadOnlyMemoryMappedFile mmf;
				if (!mmf.open(it->srcFileName.c_str())) {
					throw RuntimeException();
				}
				zipFile.write(mmf.getPointer(), mmf.getFileSize());
			}

			// central directory

			// central file header signature   4 bytes  (0x02014b50)
			centralDirectory.append((UINT32)0x02014b50);

			// version made by                 2 bytes
			centralDirectory.append((UINT16)0x0014);
			
			// version needed to extract       2 bytes
			centralDirectory.append(vnte);

			// general purpose bit flag        2 bytes
			centralDirectory.append((UINT16)flag);

			// compression method              2 bytes
			centralDirectory.append((UINT16)0x0000);

			// last mod file time              2 bytes
			// last mod file date              2 bytes
			centralDirectory.append((UINT16)fatTime);
			centralDirectory.append((UINT16)fatDate);

			// crc-32                          4 bytes
			centralDirectory.append(crc);

			// compressed size                 4 bytes
			centralDirectory.append(it->fileSize);

			// uncompressed size               4 bytes
			centralDirectory.append(it->fileSize);
		
			// file name length                2 bytes
			centralDirectory.append(fileNameLen);

			// extra field length              2 bytes
			centralDirectory.append((UINT16)0x0000);
		
			// file comment length             2 bytes
			centralDirectory.append((UINT16)0x0000);

			// disk number start               2 bytes
			centralDirectory.append((UINT16)0x0000);

			// internal file attributes        2 bytes
			centralDirectory.append(getInternalFileAttributes(it->srcFileName.c_str(), it->fileAttributes, it->fileSize));

			// external file attributes        4 bytes
			centralDirectory.append((UINT32)(it->fileAttributes));

			// relative offset of local header 4 bytes
			centralDirectory.append(relativeOffset);

			// file name
#ifdef _UNICODE
			centralDirectory.append(dstFileName.utf8_str(), fileNameLen);
#else
			centralDirectory.append(dstFileName.c_str(), fileNameLen);
#endif
		}

		// end of central directory record
		Buffer eocdr;

		// end of central dir signature    4 bytes  (0x06054b50)
		eocdr.append((UINT32)0x06054b50);

		// number of this disk             2 bytes
		eocdr.append((UINT16)0x0000);

		// number of the disk with the
		// start of the central directory  2 bytes
		eocdr.append((UINT16)0x0000);

		// total number of entries in the
		// central directory on this disk  2 bytes
		eocdr.append((UINT16)localFileData_.size());

		// total number of entries in
		// the central directory           2 bytes
		eocdr.append((UINT16)localFileData_.size());

		// size of the central directory   4 bytes
		eocdr.append((UINT32)centralDirectory.getDataLength());

		// offset of start of central
		// directory with respect to
		// the starting disk number        4 bytes
		eocdr.append((UINT32)zipFile.tell());

		// .ZIP file comment length        2 bytes
		eocdr.append((UINT16)0x0000);

		// .ZIP file comment       (variable size)

		zipFile.write(centralDirectory.getPointer(), centralDirectory.getDataLength());
		zipFile.write(eocdr.getPointer(), eocdr.getDataLength());

		zipFile.close();

		isOpen_ = false;

		return 0;
	}

	catch ( ... ) {
		// 失敗したのでファイル削除
		zipFile.close();
		DeleteFileOrFolder(zipFileName_.c_str(), false);
		return -1;
	}
}

// 0 が CRC32 の値としてあり得るのか調べる必要がある
// TODO 巨大なファイルの場合は複数回に分けてマッピングする必要がある
// ファイルサイズが 0 のファイルはオープンに失敗するので渡さないこと
UINT32 ZipWriter::getCRC32(LPCTSTR fileName) 
{
	ReadOnlyMemoryMappedFileEx mmf;
	if (!mmf.open(fileName)) return 0;
	UINT32 fileSize = mmf.getFileSize();
	boost::shared_ptr<ViewOfFile> pView(mmf.map(0, fileSize));

//	const int buffer_size = 1024;

	try {
		crc_32_type  result;
		result.process_bytes(pView->getPointer(), fileSize);
		return result.checksum();
	}

	/*
	catch ( exception &e ) {
		//cerr << "Found an exception with '" << e.what() << "'." << endl;
		return 0;
	}
	*/

	catch ( ... ) {
		//cerr << "Found an unknown exception." << endl;
		return 0;
	}
}


UINT16 ZipWriter::getInternalFileAttributes(LPCTSTR fileName, UINT32 fileAttributes, UINT32 fileSize) 
{
	// フォルダは 0
	if (fileAttributes & FILE_ATTRIBUTE_DIRECTORY) return 0;

	// 空ファイルは 1
	if (fileSize == 0) return 1;

	ReadOnlyMemoryMappedFileEx mmf;
	if (!mmf.open(fileName)) return 0;
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	const UINT32 MIN_MAPPING_SIZE = min(256, si.dwAllocationGranularity);
	UINT32 mappingSize = MIN_MAPPING_SIZE;
	UINT32 prevMappingSize = 0;
	if (mappingSize > fileSize) {
		mappingSize = fileSize;
	}
	UINT32 checkedSize = 0;
	bool maximizes = false;
	for (;;) {
		boost::shared_ptr<ViewOfFile> pView(mmf.map(checkedSize, mappingSize));
		if (pView->getPointer() == NULL) {
			// MIN_MAPPING_SIZE で
			// マッピングが失敗するならお手上げ
			if (mappingSize <= MIN_MAPPING_SIZE) return 0;
			// これ以上マッピングサイズを増やせないのだろう
			maximizes = true;
			mappingSize = prevMappingSize;
			pView.reset();
			pView = mmf.map(checkedSize, prevMappingSize);
			if (pView->getPointer() == NULL) return 0;
		}
		if (!isASCII(pView->getPointer(), mappingSize)) {
			return 0;
		}
		checkedSize += mappingSize;
		if (checkedSize == fileSize) break;
		prevMappingSize = mappingSize;
		if (!maximizes) {
			mappingSize <<= 1;
		}
		if (checkedSize + mappingSize > fileSize) {
			mappingSize = fileSize - checkedSize;
		}
	}
	return 1;	
}




void ZipWriter::add(LPCTSTR dstFileName, LPCTSTR srcFileName) 
{
	LocalFileData lfd;
	lfd.dstFileName = dstFileName;
	lfd.srcFileName = srcFileName;
	localFileData_.push_back(lfd);
}

ZipWriter::~ZipWriter() {
	if (isOpen_) {
		close();
	}
}

// LocalFileData の 更新日時 や ファイル属性 など足りない情報を埋める
bool ZipWriter::correctLocalFileData(LocalFileData& localFileData) 
{
	WIN32_FIND_DATA fd;
	HANDLE hSearch;
	hSearch = FindFirstFile(localFileData.srcFileName.c_str(), &fd);
	if (hSearch == INVALID_HANDLE_VALUE) {
		return false;
	}
	FindClose(hSearch);
	localFileData.fileAttributes = fd.dwFileAttributes;
	localFileData.lastWriteTime = fd.ftLastWriteTime;

	if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
		localFileData.fileSize = fd.nFileSizeLow;
	} else {
		localFileData.fileSize = 0;
	}

	// ファイルサイズが大きすぎると失敗
	if (fd.nFileSizeHigh != 0) return false;

	return true;
}

bool ZipWriter::setCompressLevel(int compressLevel) {
	switch (compressLevel) {
	case -1:
		compressLevel_ = Z_DEFAULT_COMPRESSION;
		break;
	case 0:
		compressLevel_ = Z_NO_COMPRESSION;
		break;
	case 1:
		compressLevel_ = Z_BEST_SPEED;
		break;
	case 9:
		compressLevel_ = Z_BEST_COMPRESSION;
		break;
	default:
		if (compressLevel >= 0 || compressLevel <= 9) {
			compressLevel_ = compressLevel;
		} else {
			return false;
		}
		break;
	}
	return true;
}

} // end of namespace KSDK
