#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <stdlib.h>
#include <memory.h>
//#include "../KSDK/types.h"

class Buffer {
public:
	Buffer() : pBuffer_(NULL), bufferLength_(0), dataLength_(0) {}
	~Buffer() {
		free(pBuffer_);
		pBuffer_ = NULL;
	}

	bool append(const void* p, int length) {
		if (bufferLength_ < dataLength_+length) {
			if (!reserve(dataLength_+length)) return false;
		}
		memcpy(((UINT8*)pBuffer_) + dataLength_, p, length);
		dataLength_+=length;
		return true;
	}

	bool append(UINT8 data) {
		return append (&data, sizeof(UINT8));
	}

	bool append(UINT16 data) {
		return append (&data, sizeof(UINT16));
	}

	bool append(UINT32 data) {
		return append (&data, sizeof(UINT32));
	}

	int getDataLength() {
		return dataLength_;
	}

	bool setDataLength(int dataLength) {
		if (bufferLength_ < dataLength) {
			if (!reserve(bufferLength_)) return false;
		}
		dataLength_ = dataLength;
		return true;
	}

	int getBufferLength() {
		return bufferLength_;
	}

	bool reserve(int bufferLength) {		
		if (bufferLength_ == 0) {
			bufferLength_ = INITIAL_BUFFER_LENGTH;
		}
		while (bufferLength_ < bufferLength) {
			bufferLength_ *= 2;
		}

		if (pBuffer_ == NULL) {
			pBuffer_ = malloc(bufferLength_);
			if (pBuffer_ == NULL) return false;
		} else {
			pBuffer_ = realloc(pBuffer_, bufferLength_);
		}
		return (pBuffer_ != NULL);
	}
	void clear() {
		dataLength_ = 0;
	}
	void* getPointer() {
		return pBuffer_;
	}

	// CString の GetBuffer() ReleaseBuffer() のように
	// GetBuffer() で const でないポインタを返し
	// (getPointer() は const のポインタを返すようにする)
	// ReleaseBuffer() でデータの長さを明示的に指定しなおすようにしたほうが
	// ミスを防げるだろうか…

private:
	static const int INITIAL_BUFFER_LENGTH = 16;
	
	void* pBuffer_;
	int bufferLength_;
	int dataLength_;
};

#endif //__BUFFER_H__

