#ifndef __NKFDLL_H__
#define __NKFDLL_H__

#include "String.h"

namespace KSDK {

class NkfDll {
public:
	NkfDll();
	~NkfDll();

	bool init(String& option);
	void Option(String& option);
	void Convert(String& inFileName, String& outFileName);

private:
	HINSTANCE mDllHandle;
	String mOption;
	FARPROC OptFunc;
	FARPROC CnvFunc;
};

}

#endif //__NKFDLL_H__