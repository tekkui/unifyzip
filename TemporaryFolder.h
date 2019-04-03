#ifndef __TEMPORARYFOLDER_H__
#define __TEMPORARYFOLDER_H__

#include "String.h"
#include "PathFunc.h"

namespace KSDK {
// テンポラリフォルダ
// create() でテンポラリフォルダを作成し
// デストラクタでフォルダごと削除する
class TemporaryFolder {
public:
	TemporaryFolder() :
		isCreated_(false),
		usesRecycleBin_(false) 
	{
	}

	TemporaryFolder(LPCTSTR path) :
		isCreated_(false),
		usesRecycleBin_(false) 
	{
		create(path);
	}

	~TemporaryFolder() {
		destroy();
	}

	void setUsesRecycleBin( bool uses ) {
		usesRecycleBin_ = uses;
	}

	bool create( LPCTSTR path ) {
		if (KSDK::FileExists(path)) return false;
		if (::CreateDirectory(path, NULL) != 0) {
			path_.Copy(path);
			isCreated_ = true;
			return true;
		} else {
			return false;
		}
	}

	bool destroy() {
		if (!isCreated_) return true;
		bool b = (KSDK::DeleteFileOrFolder(path_.c_str(), usesRecycleBin_));
		if (b) {
			path_.Empty();
			isCreated_ = false;
		}
		return b;
	}

	LPCTSTR getPath() {
		return path_.c_str();
	}

private:
	TemporaryFolder( const TemporaryFolder& temporaryFolder );
	TemporaryFolder& operator=( const TemporaryFolder& temporaryFolder );
	
	KSDK::String path_;
	bool isCreated_;
	bool usesRecycleBin_;
};

} // end of namespace KSDK

#endif //__TEMPORARYFOLDER_H__
