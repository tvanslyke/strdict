#ifndef STRING_DICT_ENTRY_H
#define STRING_DICT_ENTRY_H
#include <Python.h>
#include "KeyInfo.h"
# ifdef __cplusplus
extern "C" {
# endif 

typedef struct string_dict_entry StringDictEntry;

PyObject* Entry_Value(const StringDictEntry* self);

int Entry_Matches(const StringDictEntry* self, const KeyInfo* ki);

PyObject* Entry_Value(const StringDictEntry* self);

PyObject* Entry_Key(const StringDictEntry* self);

void Entry_SetValue(StringDictEntry* self, PyObject* new_value);

PyObject* Entry_ExchangeValue(StringDictEntry* self, PyObject* new_value);

StringDictEntry* Entry_FromKeyInfo(const KeyInfo* ki, PyObject* value);

void Entry_AsKeyInfo(const StringDictEntry* self, KeyInfo* ki);

PyObject* Entry_AsTuple(const StringDictEntry* self);

StringDictEntry* Entry_Copy(const StringDictEntry* other);

int Entry_WriteRepr(const StringDictEntry* self, _PyUnicodeWriter* writer);

void Entry_Delete(StringDictEntry* self);

void Entry_Clear(StringDictEntry* self);
# ifdef __cplusplus
} /* extern "C" */
# endif 

#endif /* STRING_DICT_ENTRY_H */
