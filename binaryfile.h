// バイナリファイル

#ifndef __BINARYFILE_H__
#define __BINARYFILE_H__

#include <Windows.h>
#include "types.h"
#include <fstream>

#include "Exception.h"

namespace KSDK {

class BinaryFileWriter {
public:
	BinaryFileWriter() : hFile_(NULL) {}

	bool open(LPCTSTR fileName) {
		hFile_ = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile_ != INVALID_HANDLE_VALUE) {
			return true;
		}
		hFile_ = NULL;

		// ファイルオープンに失敗したので属性変更を試みる
		DWORD dwAttribute;
		dwAttribute = GetFileAttributes(fileName);
		if (dwAttribute == 0xFFFFFFFF) {
			return false;
		}
		if ((dwAttribute & FILE_ATTRIBUTE_READONLY) == 0) {
			return false;
		}
		SetFileAttributes(fileName, dwAttribute & ~FILE_ATTRIBUTE_READONLY);

		hFile_ = CreateFile(fileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFile_ != INVALID_HANDLE_VALUE) {
			return true;
		}
		hFile_ = NULL;
		return false;
	}

	void close() {
		if (hFile_ != NULL) {
			CloseHandle(hFile_);
		}
	}

	// 書き込みに失敗したら例外を投げる
	// 主にディスク容量不足で起こる
	bool write(void* p, UINT32 size) {
		DWORD numberOfBytesWritten;
		if (hFile_ != NULL && WriteFile(hFile_, p, size, &numberOfBytesWritten, NULL) != 0) {
		} else {
			return false;
		}
		return true;
	}

	// ファイルを開いていない場合は 0 を返す
	// ファイルサイズが DWORD, int のいずれかの最大値を超える場合
	// 返る値は不定
	int tell() {
		if (hFile_ == NULL) {
			return 0;
		}
		return (int)GetFileSize(hFile_, NULL);
	}

private:
	BinaryFileWriter(const BinaryFileWriter&);
	BinaryFileWriter& operator=(const BinaryFileWriter&);

	// 2007/09/09 
	// std::ofstream だと Microsoft Visual C++ 2005 のライブラリで
	// マルチバイト文字セットを使用する場合
	// 日本語のパスを渡すとファイルがオープンできない
	// _tsetlocale( LC_ALL, _T("japanese") ); で解決するが
	// すると cout への日本語の出力ができなくなるので CreateFile を使用する
	HANDLE hFile_;
};

} // end of namespace KSDK

#endif //__BINARYFILE_H__