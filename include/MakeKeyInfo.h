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
		if((not error()) and (not buff_is_dummy()))
			PyBuffer_Release(&buff_);
	}

	bool error() const
	{ return (buff_.buf == nullptr); }

	operator bool() const
	{ return not error(); }

	static KeyMetaInfo as_error()
	{
		auto buff_cpy = as_error_.buff_;
		return KeyMetaInfo(buff_cpy);
	}
	static void ensure_no_error(Py_buffer* buff)
	{
		if(not buff->buf)
			buff->buf = (void*)dummy_buff;
	}
private:
	bool buff_is_dummy() const
	{
		return (const char*)buff_.buf == (const char*)dummy_buff;
	}
	static constexpr const char dummy_buff[1] = {'\0'};
	static const KeyMetaInfo as_error_;
	Py_buffer buff_{as_error_.buff_};
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
	buff.buf = nullptr;
	if(0 != KeyInfo_Init(key, &ki, &buff))
	{
		assert(PyErr_Occurred());
		assert(KeyMetaInfo::as_error().error());
		return std::make_pair(ki, KeyMetaInfo::as_error());
	}
	else
	{
		assert(ki.kind >= PY_BYTES);
		assert(ki.kind <= PY_UCS4);
		KeyMetaInfo::ensure_no_error(&buff);
		assert(not KeyMetaInfo(buff).error());
		return std::make_pair(ki, KeyMetaInfo(buff));
	}
}

KeyInfo make_key_info(const StringDictEntry* ent)
{
	KeyInfo ki;
	Entry_AsKeyInfo(ent, &ki);
	return ki;
}



#endif /* MAKE_KEY_INFO_H */
