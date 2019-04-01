#include "ArchiveDll.h"

#include "PathFunc.h"
#include "MemoryMappedFile.h"

#include <tchar.h>

namespace KSDK {

using namespace boost;
using namespace std;

namespace {

// ������ɃX�y�[�X���܂܂��ꍇ
// ��������_�u���N�H�[�g�ň͂�
void DoubleQuoteString(String& str) {
	// �X�y�[�X���܂ނ�
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

// �w���DLL�ŏ�����
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
// DLL�̃t�@�C�����ƃv���t�B�b�N�X���Z�b�g
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

// ������  1
int ArchiveDll::command(const HWND hwnd, LPCTSTR cmdLine, String& rOutput) {
	rOutput.Empty();

#ifdef _UNICODE
	FARPROC f = ::GetProcAddress(mDllHandle, mPrefix.ansi_str());
#else
	FARPROC f = ::GetProcAddress(mDllHandle, mPrefix.c_str());
#endif	
	// �֐����T�|�[�g����Ă��邩
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
		// ���炩�̒�`���ꂽ�G���[
		return r;
	}
}

WORD ArchiveDll::getVersion() {
	FARPROC f = getFuncAddress(_T("GetVersion"));
	// �֐����T�|�[�g����Ă��邩
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
	mArchiveHandle = ((OPEN_ARCHIVE)f)(hwnd, (LPCSTR)archiveFilename_.utf8_str(), mode);
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






// �ꊇ��
// overwritesFile : �����̃t�@�C�����㏑�����邩�ۂ�
//  0 : ����I��
// -1 : �Ή����Ȃ��`��
// -2 : �p�X���[�h�t��������
// -3 : ���[�U�[�L�����Z��
// -4 : ���̑��̃G���[
int ArchiveDll::extract(LPCTSTR destPath, bool showsProgress, bool overwritesFile) {
	String commandLine;

	// �X�y�[�X���܂ޏꍇ�_�u���N�H�[�g�ň͂�
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
			// �ʖ��ŉ𓀁i�t�@�C������ _1 ��������j
			commandLine.Cat(_T("-aou "));
		}

		if (!dest.IsEmpty()) {
			String dir;
			dir.Format(_T("-o%s"), dest.c_str());
			DoubleQuoteString(dir);	// �X�C�b�`���ƃ_�u���N�H�[�g�ň͂�
			dir.Cat(_T(" "));
			commandLine.Cat(dir.c_str());
		}

		if (showsProgress == false) {
			commandLine.Cat(_T("-hide "));
		}

	} else if (archiveDllID_  == ArchiveDllID::UNLHA) {
		commandLine.Cat(_T("e -x1"));

		// ���������̂܂܂ɉB�������E�V�X�e���������S�Ẵt�@�C������
		commandLine.Cat(_T("-a1 "));

		if (showsProgress == false) {
			commandLine.Cat(_T("-n1 "));
		}

		if (overwritesFile) {
			// ��ɏ㏑���W�J
			commandLine.Cat(_T("-m1 -c1 "));
		} else {
			// �����̃t�@�C��������ꍇ�g���q�� 000 �` 999 �ɕς��ēW�J
			commandLine.Cat(_T("-m2 "));
		}

		commandLine.Cat(archive.c_str());
		commandLine.Cat(_T(" "));

		// �W�J��̎w��̏ꍇ�t�H���_��'\\'�ŏI���Ȃ���΂Ȃ�Ȃ�
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
			// ��ɏ㏑���W�J
			commandLine.Cat(_T("-o "));
		} else {
			// �㏑�����邩�ۂ���I�ԃ_�C�A���O���o��
		}

		commandLine.Cat(archive.c_str());
		commandLine.Cat(_T(" "));

		// �W�J��̎w��̏ꍇ�t�H���_��'\\'�ŏI���Ȃ���΂Ȃ�Ȃ�
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
			// ��ɏ㏑���W�J
			commandLine.Cat(_T("-o "));
		} else {
			// �㏑�����邩�ۂ���I�ԃ_�C�A���O���o��i���l�[�����\�j
		}

		commandLine.Cat(archive.c_str());
		commandLine.Cat(_T(" "));

		// �W�J��̎w��̏ꍇ�t�H���_��'\\'�ŏI���Ȃ���΂Ȃ�Ȃ�
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
	int ret = command(NULL, commandLine.utf8_str(), output);
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
			// unrar32.dll(�����炭0.13�ȍ~)�� -x �I�v�V������
			// �t�H���_���܂ޏ��ɂ��𓀂���ƃt�H���_�ɂ��ď㏑��������
			// �㏑������Ƃ��Ă�(���邢�� -o �I�v�V���������Ă�)
			// 1���Ԃ�(�����炭���̃t�H���_���X�L�b�v��������)
			// �̂�0x8000�����͐����Ƃ��Ĉ���
			return 0;
		} else {
			return -4;
		}
	}
}






// �w��̃t�@�C�����w��̃p�X�ɉ𓀂���
// password �� NULL �Ȃ�΃p�X���[�h���g�p���Ȃ�
// �߂�l
//  0 : ����
// -1 : �p�X���[�h���Ԉ���Ă���
// -2 : ���̑��̃G���[
int ArchiveDll::extract(LPCTSTR srcPath, LPCTSTR destPath, bool showsProgress, LPCTSTR password) {
	// �X�y�[�X���܂ޏꍇ�_�u���N�H�[�g�ň͂�
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
		// -aos �I�v�V�����͓����̃t�@�C�����T�u�t�H���_�ȉ��ɂ������ꍇ
		// ������𓀂���Ă��܂��̂ŁA����ŏ㏑�����Ȃ��悤�ɂ��邽��

		WORD version = getVersion();

		switchString = _T("-aos ");

		if (showsProgress == false) {
			switchString.Cat(_T("-hide "));
		}

		// �p�X���[�h
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
		// �Ώۃf�B���N�g���ɑ��݂��Ȃ��t�@�C���̂� ������L���ɂ��� �W�J
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

		// �p�X���[�h
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

		// �p�X���[�h
		if (password != NULL) {
			String p;
			p.Format(_T("-P%s"), password);			
			DoubleQuoteString(p);
			p.Cat(_T(" "));
			switchString.Cat(p.c_str());
		}

		// �t�@�C�����͐��K�\���Ŏw�肷���
		// �u\�v���u\\�v�Ƃ��Ȃ���΂Ȃ�Ȃ�
		// �ʓ|�Ȃ̂Łu/�v�ɒu������
		extractFilename.Replace(_TCHAR('\\'), _TCHAR('/'));

		commandLine.Format(_T("-j -n %s %s %s %s"), switchString.c_str(), archiveFilename.c_str(), dest.c_str(), extractFilename.c_str());

	} else {
		// ���̑��̌`��
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
			// �p�X���[�h���󕶎���ŉ𓀂����݂��ꍇ��
			// �����ɗ���悤��
			return -2;
		}
	} else {
		return 0;
	}
}

// �w��t�H���_�����w��t�@�C�����ň��k
// ���� 7-zip32.dll �ɂ̂ݑΉ�
bool ArchiveDll::compress(LPCTSTR srcPath, LPCTSTR destPath, int compressLevel, bool showsProgress) {
		String filename(destPath);
		DoubleQuoteString(filename);

		// �R�}���h���C���ł͈��k��ɐ�΃p�X�͎w��ł��Ȃ��̂�
		// �J�����g�f�B���N�g����ύX���Ďw��
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

		// �J�����g�f�B���N�g����߂�
		SetCurrentDirectoryEx(oldCurrentDirectory.c_str());

		return (ret == 0);
}


// �߂�l
// 0			: ����I��
// �|1			: �����I��
// 0, �|1 �ȊO	: �ُ�I��
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

// �߂�l 
// 0			: ����I��
// �|1			: �����I��
// 0, �|1 �ȊO	: �ُ�I��
int ArchiveDll::findNext(INDIVIDUALINFO* p) {
	FARPROC f = getFuncAddress(_T("FindNext"));
	if (f == NULL) { return 1; }
	typedef int (WINAPI* FIND_NEXT)(HARC ,INDIVIDUALINFO*);
	return ((FIND_NEXT)f)(mArchiveHandle, p);
}

// ������  10
bool ArchiveDll::getRunning() {
	FARPROC f = getFuncAddress(_T("GetRunning"));
	if (f == NULL) { return false; }
	typedef BOOL (WINAPI * GET_RUNNING)();
	BOOL b = ((GET_RUNNING)f)();
	return (b == TRUE);
}

// ������  12
bool ArchiveDll::checkArchive() {
	FARPROC f = getFuncAddress(_T("CheckArchive"));
	if (f == NULL) { return false; }
	typedef BOOL (WINAPI * CHECK_ARCHIVE)(LPCSTR, const int);
#ifdef _UNICODE
	setUnicodeMode();
	BOOL b = ((CHECK_ARCHIVE)f)((LPCSTR)archiveFilename_.utf8_str(), 0);
#else
	BOOL b = ((CHECK_ARCHIVE)f)(archiveFilename_.c_str(), 0);
#endif

	// UNZIP32.DLL �̓A�[�J�C�u�łȂ��t�@�C���ɑ΂���
	// checkArchive() ���ʂ��Ă��܂���������̂�
	// ���O�ŃV�O�l�`���`�F�b�N
	if (archiveDllID_ == ArchiveDllID::UNZIP) {
		ReadOnlyMemoryMappedFileEx mmf;
		if (!mmf.open(archiveFilename_.c_str())) return false;
		// �V�O�l�`���`�F�b�N
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

	// �g���q����K����DLL��T��
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
			// DLL���g�p���łȂ����`�F�b�N����
			if (p->getRunning()) {
				// DLL�g�p��
			} else {
				p->setArchiveFilename(filename);
				if (p->checkArchive()) {
					return p;
				}
			}
		}
	}

	// �S�ẴA�[�J�C�u������
	for (int i=0; i<ArchiveDllID::MAX_ARCHIVE_DLL; ++i) {
		p = getArchiveDll((ArchiveDllID::ArchiveDllID)i);
		if (p == NULL) {
			p = addArchiveDll((ArchiveDllID::ArchiveDllID)i);
		}
		if (p != NULL) {
			// DLL���g�p���łȂ����`�F�b�N����
			if (p->getRunning()) {
				// DLL�g�p��
			} else {
				p->setArchiveFilename(filename);
				if (p->checkArchive()) {
					return p;
				}
			}
		}
	}

	return NULL;
}

} // end of namespace KSDK
