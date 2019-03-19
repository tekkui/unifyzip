#include "NkfDll.h"

#include <tchar.h>

namespace KSDK {

typedef int (WINAPI * SET_NKF_OPTION)(LPCSTR);
typedef BOOL (WINAPI * NKF_CONVERT)(LPSTR, DWORD, LPDWORD, LPCSTR, DWORD);

NkfDll::NkfDll()
:	mDllHandle(NULL), mOption(NULL)
{
}

NkfDll::~NkfDll() {
	if (mDllHandle != NULL) {
		::FreeLibrary(mDllHandle);
		mDllHandle = NULL;
	}
}

bool NkfDll::init(String& option) {
	mOption = option;
	mDllHandle = ::LoadLibrary(_T("nkf32.dll"));
	if(mDllHandle != NULL)
	{
		OptFunc = GetProcAddress(mDllHandle, "SetNkfOption");
		CnvFunc = GetProcAddress(mDllHandle, "NkfConvertSafe");
		return true;
	}
	else
	{
		return false;
	}
	
}

void NkfDll::Option(String& option) {
	mOption = option;
}

void NkfDll::Convert(String& inFileName, String& outFileName) {
	((SET_NKF_OPTION)OptFunc)((LPCSTR)mOption.ansi_str());

	TCHAR OutBuff[MAX_PATH];
	memset(&OutBuff[0], 0x00, sizeof(OutBuff));
	DWORD bytesReturn = 0;
	((NKF_CONVERT)CnvFunc)((LPSTR)&OutBuff[0], sizeof(OutBuff), &bytesReturn, (LPCSTR)inFileName.utf8_str(), inFileName.GetUtf8Length());
	outFileName.Copy(OutBuff);
}

}
