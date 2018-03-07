#include "StringDictEntry.h"
#include <Python.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>
#include <stdint.h>
#include "LEB128.h"

static const uintptr_t pyptr_lowbits_mask = alignof(PyObject) - 1;

static const uintptr_t pyobject_pointer_lowbits = 
	alignof(PyObject) == 1 ? ((uintptr_t)0) :
	alignof(PyObject) == 2 ? ((uintptr_t)1) :
	alignof(PyObject) == 4 ? ((uintptr_t)2) :
	alignof(PyObject) == 8 ? ((uintptr_t)3) :
	alignof(PyObject) == 16 ? ((uintptr_t)4) : 0;

#define PYOBJECT_MASKED_(op) ((PyObject*)(((uintptr_t)(op)) & ~pyobject_pointer_lowbits))
#define PYOBJECT_TAG_(op) (((uintptr_t)(op)) & pyobject_pointer_lowbits)

typedef unsigned char uchar_t;
struct string_dict_entry
{
	// cached bytes() or str() (PyByteObject or PyUnicodeObject)
	void* cached_key;
	// value corresponding to the key in this entry
	void* value;
	const uchar_t data[];
};

static PyObject* Entry_GetValue(const StringDictEntry* self)
{
	return PYOBJECT_MASKED_(self->value);
}

static PyObject* Entry_GetKey(const StringDictEntry* self)
{
	if(self->cached_key)
		return PYOBJECT_MASKED_(self->cached_key);
	else
		return self->cached_key;
}

static uintptr_t Entry_ValueTag(const StringDictEntry* self)
{
	return PYOBJECT_TAG_(self->value);
}

static uintptr_t Entry_KeyTag(const StringDictEntry* self)
{
	return PYOBJECT_TAG_(self->cached_key);
}


static DataKind Entry_Kind(const StringDictEntry* self)
{
	uchar_t kind = 0;
	// this should be constant-folded by the compiler
	switch(pyobject_pointer_lowbits)
	{
	case 0:
		kind = self->data[0];
		break;
	case 1:
		assert(Entry_KeyTag(self) < 2);
		assert(Entry_ValueTag(self) < 2);
		kind |= Entry_KeyTag(self);
		kind |= Entry_ValueTag(self) << 1;
		break;
	case 2:
	case 3:
		assert(Entry_KeyTag(self) < 4);
		assert(Entry_ValueTag(self) == 0);
		kind = Entry_KeyTag(self);
		break;
	default:
		assert(0);
	};
	return (DataKind)kind;
}

static int Entry_SetKind(StringDictEntry* self, DataKind kind)
{
	assert(kind >= PY_BYTES);
	assert(kind <= PY_UCS4);
	// this should be constant-folded by the compiler
	switch(pyobject_pointer_lowbits)
	{
	case 0:
		return 0;
		break;
	case 1:
		// set the least significant bit if it's set in kind
		if(kind & 0x01)
			self->cached_key = ((char*)self->cached_key) + 1;
		// set the least significant bit if the 2nd least significant bit is set in kind
		if(kind & 0x02)
			self->value = ((char*)self->value) + 1;
		break;
	case 2:
	case 3:
		// offset the cached_key pointer by 'kind' (sets the low bits)
		self->cached_key = ((char*)self->cached_key) + kind;
		break;
	default:
		assert(0);
	};
	return 1;
}




static int Entry_KeyIsUnicode(const StringDictEntry* self)
{
	assert(self);
	assert(Entry_Kind(self) >= PY_BYTES);
	assert(Entry_Kind(self) <= PY_UCS4);
	assert((!Entry_GetKey(self)) || (PyUnicode_Check(Entry_GetKey(self)) == (Entry_Kind(self) != PY_BYTES)));
	return Entry_Kind(self) != PY_BYTES;
}


static int Entry_KeyIsBytes(const StringDictEntry* self)
{
	return !Entry_KeyIsUnicode(self);
}


static void* tag_pyobject(PyObject* obj, uintptr_t tag)
{
	assert(tag < alignof(PyObject));
	assert(PYOBJECT_TAG_(obj) == 0);
	assert((tag & (uintptr_t)obj) == 0);
	return (void*)(((uintptr_t)obj) | tag);
}

static const uchar_t* Entry_DataAndSize(const StringDictEntry* self, Py_ssize_t* size, DataKind kind)
{
	assert(self);
	assert(Entry_Kind(self) == kind);
	const uchar_t* data_header = self->data + (pyobject_pointer_lowbits == 0);
	size_t bytecount = 0;
	*size = leb128_decode(data_header, &bytecount);
	assert(bytecount);
	return data_header + bytecount;
}

PyObject* Entry_ExchangeValue(StringDictEntry* self, PyObject* new_value)
{
	assert(Entry_GetValue(self));
	assert(new_value);
	assert(Entry_GetValue(self));
	assert((PYOBJECT_MASKED_(new_value) == new_value) && "Bad alignment of PyObject encountered "
		"or this is a platform for which alignment > 1 does not imply 2^n address values.");
	
	Py_INCREF(new_value);
	PyObject* old = Entry_GetValue(self);
	uintptr_t tag = Entry_ValueTag(self);
	self->value = tag_pyobject(new_value, tag);
	return old;
}

void Entry_SetValue(StringDictEntry* self, PyObject* new_value)
{
	Py_DECREF(Entry_ExchangeValue(self, new_value));
}

static void* Entry_Alloc(Py_ssize_t header_size, Py_ssize_t data_size, Py_ssize_t item_size)
{
	Py_ssize_t data_bytes = data_size * item_size;
	assert(header_size > 0);
	assert(data_size >= 0);
	assert(item_size > 0);
	// total size is the sum of:
	// the members of StringDictEntry
	// the header of the data:
	// 	- [if alignof(PyObject) == 1] 1 byte that holds the Kind tag
	// 	- an unsigned leb128 encoding of the length of the data
	// the data, not aligned at all, starting at the first byte after the header
	// one null byte
	size_t alloc_size = sizeof(StringDictEntry) + header_size + data_bytes + 1;
	return malloc(alloc_size);
}

static void Entry_Free(void* ent_mem)
{
	free(ent_mem);
}

StringDictEntry* Entry_FromKeyInfo(const KeyInfo* ki, PyObject* value)
{
	assert(value);
	Leb128Encoding enc = leb128_encode(ki->data_size);
	Py_ssize_t header_size = enc.len;
	
	uchar_t* mem = Entry_Alloc(header_size, ki->data_size, DataKind_ItemSize(ki->kind));
	if(!mem)
	{
		PyErr_SetString(PyExc_MemoryError, "Failed to allocate new entry for StringDict.");
		return NULL;
	}


	Py_INCREF(value);
	Py_XINCREF(ki->key);

	StringDictEntry* ent = (StringDictEntry*)mem;
	ent->cached_key = ki->key;
	ent->value = value;
	uchar_t* data = mem + offsetof(StringDictEntry, data);
	if(pyobject_pointer_lowbits == 0)
	{
		data[0] = ki->kind;
		++data;
	}
	else
	{
		assert(Entry_SetKind(ent, ki->kind));
	}
	
	memcpy(data, enc.encoding, enc.len);

	// skip over the length encoding
	data += enc.len;

	// write the string 
	Py_ssize_t data_bytes = ki->data_size * DataKind_ItemSize(ki->kind);
	memcpy(data, ki->data, data_bytes);

	// and of course, the null terminator
	data[data_bytes] = '\0';
	
	return ent;
}

static Py_ssize_t Entry_Data(const StringDictEntry* self, const uchar_t** begin, const uchar_t** end, DataKind kind)
{
	assert(Entry_Kind(self) == kind);
	Py_ssize_t len;
	const uchar_t* data = Entry_DataAndSize(self, &len, kind);
	Py_ssize_t itemsize = DataKind_ItemSize(kind);
	*begin = data;
	*end = data + len * itemsize;
	return len;
}

PyObject* Entry_AsTuple(const StringDictEntry* self)
{
	assert(self);
	return PyTuple_Pack(2, Entry_Key(self), Entry_Value(self));
}

int Entry_Matches(const StringDictEntry* self, const KeyInfo* ki)
{
	if(ki->key == Entry_GetKey(self))
		return 1;
	DataKind kind = Entry_Kind(self);
	if(ki->kind != kind)
		return 0;
	const uchar_t* begin;
	const uchar_t* end;
	Py_ssize_t len = Entry_Data(self, &begin, &end, kind);
	if(ki->data_size != len)
		return 0;

	return memcmp(ki->data, begin, end - begin) == 0;
}

void Entry_AsKeyInfo(const StringDictEntry* self, KeyInfo* ki)
{
	DataKind kind = Entry_Kind(self);
	ki->key = Entry_GetKey(self);
	const uchar_t* data_end;
	ki->data_size = Entry_Data(self, &(ki->data), &data_end, kind);
	ki->kind = kind;
}

int Entry_WriteRepr(const StringDictEntry* self, _PyUnicodeWriter* writer)
{
	assert(self);
	assert(writer);
	PyObject* key = Entry_GetKey(self);
	DataKind kind = Entry_Kind(self);
	if((!key) || kind == PY_BYTES)
	{
		assert(Entry_Kind(self) == PY_BYTES);
		assert((!key) || PyBytes_Check(key));
		const uchar_t* first;
		const uchar_t* last;
		Py_ssize_t len = Entry_Data(self, &first, &last, PY_BYTES);
		assert(last - first == len);
		if(0 != _PyUnicodeWriter_WriteASCIIString(writer, (char*)first, len))
			return -1;
	}
	else 
	{
		assert(key || PyUnicode_Check(key));
		if(0 != _PyUnicodeWriter_WriteStr(writer, key))
			return -1;
	}
	
	if(0 != _PyUnicodeWriter_WriteASCIIString(writer, ": ", 2))
		return -1;
	PyObject* value = Entry_GetValue(self);
	assert(value);
	PyObject* val_repr = PyObject_Repr(value);
	if(!val_repr)
		return -1;
	
	int errc = _PyUnicodeWriter_WriteStr(writer, val_repr);
	Py_DECREF(val_repr);
	return errc;
}

void Entry_Delete(StringDictEntry* self)
{
	assert(self);
	assert(Entry_Value(self));
	Py_SETREF(self->value, NULL);
	Py_XDECREF(self->cached_key);
	Entry_Free(self);
}

void Entry_Clear(StringDictEntry* self)
{
	assert(self);
	assert(Entry_Value(self));
	Py_CLEAR(self->value);
	Py_XDECREF(self->cached_key);
	Entry_Free(self);
}


PyObject* Entry_Value(const StringDictEntry* self)
{
	assert(self);
	return Entry_GetValue(self);
}


PyObject* Entry_Key(const StringDictEntry* self_)
{
	assert(self_);
	{
		PyObject* key = Entry_GetKey(self_);
		if(key)
			return key;
	}

	// if the key isn't cached yet, then create a Bytes object for 
	// it and save it in self->cached_key
	
	// it's okay I'm a professional...
	StringDictEntry* self = (StringDictEntry*)self_;
	// get the data in 'self'
	assert(Entry_Kind(self) == PY_BYTES);
	const uchar_t* data_begin;
	const uchar_t* data_end;
	Py_ssize_t sz = Entry_Data(self, &data_begin, &data_end, PY_BYTES);
	assert(sz == (data_end - data_begin));
	// make the bytes object
	PyObject* bytes_obj = PyBytes_FromStringAndSize((char*)data_begin, sz);
	if(!bytes_obj)
		return NULL;
	assert(PYOBJECT_TAG_(bytes_obj) == 0);

	uintptr_t tag = Entry_KeyTag(self);
	self->cached_key = (PyObject*)(((uintptr_t)bytes_obj) | tag);
	return Entry_GetKey(self);
}
