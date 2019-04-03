#include "CommandLine.h"

#include <tchar.h>

namespace KSDK {

// メモリを確保しコマンドライン文字列をコピー
// 確保したメモリへのポインタを返す
void* AllocCommandLine(void)
{
	LPTSTR psz;
	size_t size;
	void* pv;

	psz = GetCommandLine();
	size = _tcslen(psz) + 1;
	if((pv = malloc(size * sizeof(TCHAR))) == NULL) return NULL;
	_tcscpy((LPTSTR)pv, psz);
	return pv;
}


void FreeCommandLine(void* pvCommandLine)
{
	free(pvCommandLine);
}


// 引数の区切りは半角スペースまたはTAB（いくつ続いても可）
// 「"」で囲まれた文字列に関しては半角スペースは区切りとしての意味を持たずTABは無視される
// DragQueryFile() の仕様を参考にした
// iParam
// 0xFFFFFFFF : 引数の数を返す
UINT GetStringFromCommandLine(void* pvCommandLine, UINT iParam, String& Param)
{
	LPTSTR pszSrc;
	TCHAR c;
	int nIndex;		// 現在処理中の引数のインデックス
	bool bTrim;		// 引数の前の不要な スペース TAB を取り払ったか
	bool bDQ;		// ダブルクォーテーションで囲まれているか

	if(iParam != 0xFFFFFFFF) Param.Empty();

	pszSrc = (LPTSTR)pvCommandLine;

	nIndex = -1;

	bTrim = false;
	bDQ = false;

	for(;;){
		c = *pszSrc++;

		// NULL文字で終了
		if(c == _TCHAR('\0')) break;
		
		if(!bTrim){
			// コマンドの前の不要な TAB スペース を取り払っていない
			if(c == _TCHAR(' ') || c == _TCHAR('\t')){
				if(iParam != 0xFFFFFFFF && nIndex == (int)iParam){
					break;		// 目的の引数の取得が終わった場合
				}else{
					continue;	// スペース TAB は無視
				}
			}else{
				bTrim = true;
				nIndex++;
			}
		}
					
		if(c == _TCHAR('"')){
			bDQ = !bDQ;
		}else{
			if(!bDQ){
				// ダブルクォーテーションでくくられて無い場合
				if(c == _TCHAR(' ') || c == _TCHAR('\t')){
					// 引数の区切りとしての スペース TAB
					bTrim = false;
				}else{
					if(iParam != 0xFFFFFFFF && nIndex == (int)iParam) Param += c;
				}
			}else{
				// ダブルクォーテーションでくくられている場合
				if(c == _TCHAR('\t')){
					// TAB は無視
				}else{
					if(iParam != 0xFFFFFFFF && nIndex == (int)iParam) Param += c;
				}
			}
		}
	}
	
	if(iParam == 0xFFFFFFFF){
		return nIndex + 1;			// 引数の数を返す
	}else{
		return Param.GetLength();	// コピーした文字数を返す
	}
}

} // end of namespace KSDK
