#ifndef __COMMANDLINE_H__
#define __COMMANDLINE_H__

#include <windows.h>
#include "String.h"

namespace KSDK {

// コマンドライン関係の関数群
void* AllocCommandLine(void);
void FreeCommandLine(void* pvCommandLine);
UINT GetStringFromCommandLine(void* pvCommandLine, UINT iParam, String& Param);

} // end of namespace KSDK

#endif //__COMMANDLINE_H__