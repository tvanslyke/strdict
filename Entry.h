#ifndef ENTRY_H
#define ENTRY_H

#include "StringDictEntry.h"
#include <Python.h>
#include <memory>

struct KeyInfo;

// Unicode objects cache their own hashes, 
// so we can take advantage of that here.
struct PyUnicodeEntry
{
	Py_hash_t hash() const
	{
		assert(PyUnicode_Check(key_));
		return ascii_key()->hash;
	}


	PyObject* value() const
	{
		return value_.get();
	}
private:
	static Py_hash_t unicode_hash(PyObject* unicode_object)
	{
		assert(PyUnicode_Check(unicode_object));
		// This is shady, I know, but I'm mad that there's no
		// PyUnicode_Hash() function in the CPython API so fuck it.
		//
		// Really.  I don't want to incur an extra indirection by 
		// going through PyObject_Hash.  Also dictobject.c does this 
		// so...
		return ((PyASCIIObject*)unicode_object)->hash;
	}
	void assert_invariants()
	{
		assert(bool(key_) == bool(value));
	}

	PyUnicodeObject* unicode_key() const
	{
		return (PyUnicodeObject*)key_;
	}

	PyASCIIObject* ascii() const
	{
		return (PyASCIIObject*)key_;
	}

	int _matches(PyObject* other_key)
	{
		assert(PyUnicode_Check(other_key));
		assert(PyUnicode_READY(key_));
		if(PyUnicode_READY(key) != 0)
			return -1;
		
		if(hash() != unicode_hash(other_key))
			return false;
		return _PyUnicode_EQ(key_, other_key);
	}

	PythonObject key_; // MUST be a READY'd PyUnicodeObject 
};

struct PyBytesEntry
{
	PythonObject value_; 
};

struct StringEntry
;

struct Entry {

	struct EntryDeleter{
		void operator()(StringDictEntry* ptr) const
		{
			if(ptr)
				Entry_Delete(ptr);
		}
	};

	struct EntryClearDeleter{
		void operator()(StringDictEntry* ptr) const
		{
			Entry_Clear(ptr);
		}
	};

	using hash_t = Py_hash_t;
	Entry() = delete;
	Entry(Entry&&) = default;
	Entry& operator=(Entry&&) = default;
	Entry(const char* begin, const char* end, PyObject* value, hash_t hash_value):
		entry_(Entry_New(begin, end, value)), hash_(hash_value)
	{ /* CTOR */ }

	Entry(std::string_view str, PyObject* value, hash_t hash_value):
		Entry(str.data(), str.data() + str.size(), value, hash_value)
	{ /* CTOR */ }

	Entry(const KeyInfo& ki, PyObject* value):
		Entry(ki.key, value, ki.hash)
	{ /* CTOR */ }

	PyObject* get_value() const
	{
		assert(self());
		return Entry_Value(self());
	}

	PyObject* get_value_newref() const
	{
		assert(self());
		auto v = Entry_Value(self());
		assert(v);
		assert(Py_REFCNT(v));
		Py_INCREF(v);
		return v;
	}

	void set_value(PyObject* value)
	{
		assert(self());
		Entry_SetValue(self(), value);
	}

	PyObject* exchange_value(PyObject* value)
	{
		assert(self());
		return Entry_ExchangeValue(self(), value);
	}
	
	std::string_view key() const
	{
		assert(self());
		return std::string_view(Entry_StringBegin(self()), Entry_StringLength(self()));
	}

	bool is_empty() const
	{ return not self(); }

	void set_empty() 
	{
		assert(self());
		entry_.reset(nullptr);
	}
	
	void clear()
	{
		std::unique_ptr<StringDictEntry, EntryClearDeleter> tmp(entry_.release());
		assert(not entry_);
	}
	
	bool matches(const KeyInfo& ki)
	{ return (hash_ == ki.hash) and (key() == ki.key); }

	void assign_from(const KeyInfo& ki, PyObject* value)
	{
		assert(is_empty());
		Entry tmp(ki, value);
		this->swap(tmp);
	}

	Py_hash_t hash() const
	{ return hash_; }

	PyObject* as_tuple()
	{
		PythonObject tup(PyTuple_New(2));
		if(not tup)
			return nullptr;
		auto k = key();
		PythonObject key_bytes(PyBytes_FromStringAndSize(k.data(), k.size()));
		if(not tup)
			return nullptr;
		
		PyTuple_SET_ITEM(tup, 0, key_bytes.release());
		PyTuple_SET_ITEM(tup, 1, get_value_newref());
		return tup.release();
	}

	std::optional<Entry> make_copy() const
	{
		if(is_empty())
			return Entry(nullptr, hash());
		StringDictEntry* ent_cpy = Entry_Copy(entry_.get());
		if(not ent_cpy)
			return std::nullopt;
		return Entry(std::unique_ptr<StringDictEntry, EntryDeleter>(ent_cpy), hash());
	}

	void swap(Entry& other) noexcept
	{
		entry_.swap(other.entry_);
		std::swap(hash_, other.hash_);
	}
	

	using is_open_t = decltype(std::mem_fn(&Entry::is_empty));
	using is_closed_t = decltype(std::not_fn(std::declval<is_open_t>()));
	static const is_open_t is_open;
	static const is_closed_t is_closed;

private:
	Entry(std::unique_ptr<StringDictEntry, EntryDeleter>&& ent, hash_t hash_val):
		entry_(std::move(ent)), hash_(hash_val)
	{ /* CTOR */ }

	StringDictEntry* self()
	{ return entry_.get(); }
 
	const StringDictEntry* self() const
	{ return entry_.get(); }
 
	std::unique_ptr<StringDictEntry, EntryDeleter> entry_;
	hash_t hash_;
	static constexpr const std::size_t hash_width = sizeof(Py_hash_t) * CHAR_BIT;
};

const Entry::is_open_t Entry::is_open{std::mem_fn(&Entry::is_empty)};



#endif /* ENTRY_H */
