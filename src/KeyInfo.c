#include "KeyInfo.h"
#include <stdbool.h>

void DataKind_Info(DataKind kind, Py_ssize_t* size, Py_ssize_t* alignment)
{
	assert(kind >= PY_BYTES);
	assert(kind <= PY_UCS4);
	switch(kind)
	{
	case PY_BYTES:
		*size = 1;
		*alignment = 1;
		break;
	case PY_UCS1:
		*size = sizeof(Py_UCS1);
		*alignment = alignof(Py_UCS1);
		break;
	case PY_UCS2:
		*size = sizeof(Py_UCS2);
		*alignment = alignof(Py_UCS2);
		break;
	case PY_UCS4:
		*size = sizeof(Py_UCS4);
		*alignment = alignof(Py_UCS4);
		break;
	default:
		assert(false);
		*size = -1;
		*alignment = -1;
	}
}

Py_ssize_t DataKind_ItemSize(DataKind kind)
{
	assert(kind >= PY_BYTES);
	assert(kind <= PY_UCS4);
	return kind + ((kind == PY_BYTES) || (kind == PY_UCS4));
}

Py_ssize_t DataKind_Alignment(DataKind kind)
{
	assert(kind >= PY_BYTES);
	assert(kind <= PY_UCS4);
	Py_ssize_t alignment, _size;
	DataKind_Info(kind, &_size, &alignment);
	return alignment;
}

int KeyInfo_Init(PyObject* key, KeyInfo* ki, Py_buffer* buff)
{
	assert(key);
	assert(ki);

	// Bypass user-defined hashes for str() and bytes() subtypes.  
	// Sounds bad, but this hash table is for strings and other string-like
	// data, so we treat strings as strings here.  
	if(PyUnicode_Check(key))
	{
		ki->key = key;
		ki->hash = PyUnicode_Type.tp_hash(key);
		if((ki->hash == -1) && PyErr_Occurred())
			return -1;

		int _kind = PyUnicode_KIND(key);
		switch(_kind)
		{
		case PyUnicode_1BYTE_KIND:
			ki->kind = PY_UCS1;
			break;
		case PyUnicode_2BYTE_KIND:
			ki->kind = PY_UCS2;
			break;
		case PyUnicode_4BYTE_KIND:
			ki->kind = PY_UCS4;
			break;
		default:
			assert(false);
			ki->kind = -1;
		};
		ki->data = PyUnicode_DATA(key);
		ki->data_size = PyUnicode_GET_LENGTH(key);
		assert(ki->kind >= PY_BYTES);
		assert(ki->kind <= PY_UCS4);
		return 0;
	}
	else if(PyBytes_Check(key))
	{
		ki->key = key;
		ki->kind = PY_BYTES;
		ki->hash = PyBytes_Type.tp_hash(key);
		if((ki->hash == -1) && PyErr_Occurred())
			return -1;
		char* data;
		Py_ssize_t len;
		if(0 != PyBytes_AsStringAndSize(key, &data, &len))
			return -1;
		ki->data = (unsigned char*)data;
		ki->data_size = len;
		assert(ki->kind >= PY_BYTES);
		assert(ki->kind <= PY_UCS4);
		return 0;
	}
	else
	{
		// null indicates that we can't cache the given key.
		// also indicates that the Py_buffer shouldn't be thrown out.
		ki->key = NULL; 
		if(0 != PyObject_GetBuffer(key, buff, PyBUF_SIMPLE))
			return -1;
		ki->kind = PY_BYTES;
		ki->data = buff->buf;
		ki->data_size = buff->len;
		ki->hash = _Py_HashBytes(ki->data, ki->data_size);
		assert(ki->kind >= PY_BYTES);
		assert(ki->kind <= PY_UCS4);
		return 0;
	}

}

