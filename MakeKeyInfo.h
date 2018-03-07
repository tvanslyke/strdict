#ifndef MAKE_KEY_INFO_H
#define MAKE_KEY_INFO_H

#include "KeyInfo.h"
#include "StringDictEntry.h"
#include <variant>

struct KeyMetaInfo
{
	
	KeyMetaInfo() = delete;
	KeyMetaInfo(Py_buffer buf):
		buff_(buf)
	{
		
	}

	KeyMetaInfo(const KeyMetaInfo&) = delete;

	KeyMetaInfo(KeyMetaInfo&& other):
		buff_(std::move(other.buff_))
	{
		other.buff_ = as_error_.buff_;
	}
	~KeyMetaInfo()
	{
		if(not buff_.buf)
			PyBuffer_Release(&buff_);
	}

	bool error() const
	{ return buff_.buf; }

	operator bool() const
	{ return not error(); }

	static KeyMetaInfo as_error()
	{
		auto buff_cpy = as_error_.buff_;
		return KeyMetaInfo(buff_cpy);
	}
private:
	static const KeyMetaInfo as_error_;
	Py_buffer buff_{nullptr};
};

const KeyMetaInfo KeyMetaInfo::as_error_([]() {
	// use an immediately-invoked lambda expression to initialize
	// can't trust the C API to keep the order of these stable aparrently
	Py_buffer buff; 
	buff.buf        = nullptr; 
	buff.obj        = nullptr; 
	buff.len        = -1; 
	buff.itemsize   = -1; 
	buff.readonly   = -1; 
	buff.ndim       = -1; 
	buff.format     = nullptr; 
	buff.shape      = nullptr; 
	buff.strides    = nullptr; 
	buff.suboffsets = nullptr; 
	buff.internal   = nullptr; 
	return buff;
}());
	

std::pair<KeyInfo, KeyMetaInfo> make_key_info(PyObject* key)
{
	KeyInfo ki;
	Py_buffer buff;
	if(0 != KeyInfo_Init(key, &ki, &buff))
		return std::make_pair(ki, KeyMetaInfo(buff));
	else
		return std::make_pair(ki, KeyMetaInfo::as_error());
}

KeyInfo make_key_info(const StringDictEntry* ent)
{
	KeyInfo ki;
	Entry_AsKeyInfo(ent, &ki);
	return ki;
}



#endif /* MAKE_KEY_INFO_H */
