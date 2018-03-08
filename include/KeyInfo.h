#ifndef KEY_INFO_H
#define KEY_INFO_H

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#else
# include <stdalign.h>
#endif


typedef enum data_kind_ {
	PY_BYTES = 0,
	PY_UCS1 = 1,
	PY_UCS2 = 2,
	PY_UCS4 = 3
} DataKind;


void DataKind_Info(DataKind kind, Py_ssize_t* size, Py_ssize_t* alignment);

Py_ssize_t DataKind_ItemSize(DataKind kind);

Py_ssize_t DataKind_Alignment(DataKind kind);

typedef struct key_info_ {
	PyObject* key;
	Py_hash_t hash;
	const unsigned char* data;
	Py_ssize_t data_size;
	DataKind kind;
} KeyInfo;

int KeyInfo_Init(PyObject* key, KeyInfo* ki, Py_buffer* buff);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* KEY_INFO_H */
