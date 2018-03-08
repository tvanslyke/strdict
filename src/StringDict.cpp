#include <Python.h>
#include <vector>
#include <optional>
#include <variant>
#include <string_view>
#include <string>
#include "StringDictEntry.h"
#include "MakeKeyInfo.h"
#include "PythonUtils.h"
#include <memory>
#include <climits>
#include <limits>
#include <type_traits>
#include <cstring>
#include <functional>
#include <utility>
#include <iostream>

struct DebugFunc
{
	DebugFunc(std::string nm): name(nm)
	{
		std::cerr << name << " ENTER" << std::endl;
	}
	~DebugFunc()
	{
		std::cerr << name << " EXIT" << std::endl;
	}
	const std::string name;
};


#define DBG_FUNC DebugFunc _d_b_g_(__func__);
	

struct Entry;


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
	Entry(const Entry&) = delete;
	Entry& operator=(const Entry&) = delete;
	Entry(Entry&&) = default;
	Entry& operator=(Entry&&) = default;

	Entry(const KeyInfo& ki, PyObject* value):
		entry_(Entry_FromKeyInfo(&ki, value)), hash_(ki.hash)
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
	
	PyObject* get_key() const
	{
		assert(self());
		return Entry_Key(self());
	}

	PyObject* get_key_newref() const
	{
		assert(self());
		PyObject* k = get_key();
		Py_XINCREF(k);
		return k;
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
		assert(not self());
	}
	
	bool matches(const KeyInfo& ki)
	{
		assert(self());
		return (hash_ == ki.hash) and Entry_Matches(self(), &ki);
	}

	void assign_from(const KeyInfo& ki, PyObject* value)
	{
		assert(is_empty());
		Entry tmp(ki, value);
		this->swap(tmp);
	}

	Py_hash_t hash() const
	{
		assert(self());
		return hash_;
	}

	PyObject* as_tuple() const
	{
		assert(self());
		return Entry_AsTuple(self());
	}

	std::optional<Entry> make_copy() const
	{
		if(is_empty())
			return Entry(nullptr, -1);
		StringDictEntry* ent_cpy = Entry_Copy(self());
		if(not ent_cpy)
			return std::nullopt;
		return Entry(std::unique_ptr<StringDictEntry, EntryDeleter>(ent_cpy), hash());
	}

	void swap(Entry& other) noexcept
	{
		entry_.swap(other.entry_);
		std::swap(hash_, other.hash_);
	}
	
	KeyInfo as_key_info() const noexcept
	{
		assert(self());
		KeyInfo ki;
		ki.hash = -1;
		Entry_AsKeyInfo(self(), &ki);
		assert(ki.hash == -1);
		ki.hash = hash();
		return ki;
	}
	
	int write_repr(_PyUnicodeWriter* writer) const
	{
		assert(self());
		return Entry_WriteRepr(self(), writer);
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
const Entry::is_closed_t Entry::is_closed{std::not_fn(Entry::is_open)};

void swap(Entry& l, Entry& r)
{
	l.swap(r);
}

struct StringDictBase: 
	public PyObject
{
	using uhash_t = std::make_unsigned_t<Py_hash_t>;
	static constexpr const uhash_t perturb_shift = 5;
	static constexpr const double max_load_factor = 0.667;
	static constexpr const Py_ssize_t min_buckets = 8;
	
	StringDictBase() = default;
	StringDictBase(const StringDictBase& other) = delete;
	StringDictBase(StringDictBase&& other) = default;

	
	int reserve_space(Py_ssize_t len)
	{
		// assert(size() == 0);
		assert(len >= 0);
		if(len == 0)
			return 0;
		std::size_t ofs_count_needed = len / max_load_factor;
		// overflow check
		if(ofs_count_needed < static_cast<std::size_t>(len))
		{
			PyErr_SetString(PyExc_OverflowError, "Overflow occurred while trying to satisfy "
				"strdict maximum load factor.");
			return -1;
		}

		// make sure the number of offsets is a power of two >= min_buckets
		// TODO: something a little faster
		std::size_t count = min_buckets;
		while(count < ofs_count_needed)
			count <<= 1;
		ofs_count_needed = count;
		if(ofs_count_needed <= bucket_count())
			return 0;

		// allocate
		try
		{
			entries.reserve(len);
			if(size() > 0)
			{
				// Reserve the space, resize to half the space, and finally
				// trigger a rehash by calling grow()
				offsets.reserve(ofs_count_needed);
				offsets.resize(ofs_count_needed >> 1, -1);
				grow();
			}
		}
		catch(const std::bad_alloc&)
		{
			PyErr_SetString(PyExc_MemoryError, "Allocation failed while trying to reserve memory for strdict instance.");
			return -1;
		}
		catch(const std::exception& e)
		{
			PyErr_SetString(PyExc_MemoryError, e.what());
			return -1;
		}
		return 0;
	}

	std::size_t bucket_count() const
	{ return offsets.size(); }

	std::size_t size() const
	{ return occupied; }

	std::size_t entry_slot_count() const
	{ return entries.size(); }

	const Py_ssize_t& offset_at(Py_ssize_t index) const
	{
		assert(index < static_cast<Py_ssize_t>(offsets.size()));
		assert(index >= 0);
		return offsets[index];
	}
	
	Py_ssize_t& offset_at(Py_ssize_t index) 
	{
		const Py_ssize_t& ofs = static_cast<const StringDictBase*>(this)->offset_at(index);
		return const_cast<Py_ssize_t&>(ofs);
	}
	
	const Entry* pointer_to_entry_at(Py_ssize_t index) const
	{
		assert(index < static_cast<Py_ssize_t>(entries.size()));
		if(not (index >= 0))
			std::cerr << index << std::endl;;
		return entries.data() + index;
	}
	
	Entry* pointer_to_entry_at(Py_ssize_t index) 
	{
		const Entry* ent = static_cast<const StringDictBase*>(this)->pointer_to_entry_at(index);
		return const_cast<Entry*>(ent);
	}
	
	const Entry& entry_at(Py_ssize_t index) const
	{ return *pointer_to_entry_at(index); }
	
	Entry& entry_at(Py_ssize_t index) 
	{ return *pointer_to_entry_at(index); }
	
	template <class Pred>
	void visit_with_hash(Py_hash_t hash_value, Pred pred)
	{
		assert(mask + 1 == offsets.size());
		uhash_t perturb;
		// copy the hash into 'perturb' bit-for-bit...
		// Py_hash_t is signed, but we want unsigned because 
		// '.advance_index()' does a logical shift right.
		std::memcpy(&perturb, &(hash_value), sizeof(perturb));
		
		for(uhash_t idx = hash_value & mask; not pred(idx); advance_index(idx, perturb))
		{
			/* LOOP */
		}
	}
	
	template <class Visitor>
	bool visit_nonempty_entries(Visitor visit) const
	{
		if(auto count = size(); not count)
			return false;

		auto pos = entries.begin();
		for(Py_ssize_t i = 0, count = size(); i < count; ++i)
		{
			pos = std::find_if(pos, entries.end(), Entry::is_closed);
			assert(pos < entries.end());
			if(visit(*pos++))
				return true;
		}
		return false;
	}

	template <class Visitor>
	void visit_all_nonempty_entries(Visitor visit) const
	{
		if(auto count = size(); not count)
			return;

		auto pos = entries.begin();
		for(Py_ssize_t i = 0, count = size(); i < count; ++i)
		{
			pos = std::find_if(pos, entries.end(), Entry::is_closed);
			assert(pos < entries.end());
			visit(*pos++);
		}
	}

	std::pair<Py_ssize_t, Entry*> find_existing(const KeyInfo& ki)
	{
		Entry* found = nullptr;
		Py_ssize_t offsets_index = -1;
		auto visit_pred = [&](std::size_t ofs_index) -> bool
		{
			auto ofs = offset_at(ofs_index);
			if(ofs < 0)
			{
				offsets_index = ofs_index;
				return true;
			}
			Entry* ent = pointer_to_entry_at(ofs);
			if((not ent->is_empty()) and ent->matches(ki))
			{
				found = ent;
				offsets_index = ofs_index;
				return true;
			}
			return false;
		};
		visit_with_hash(ki.hash, visit_pred);
		assert((not found) or (found == pointer_to_entry_at(offset_at(offsets_index))));
		return std::make_pair(offsets_index, found);
	}

	std::pair<Py_ssize_t, Entry*> find_insertion(const KeyInfo& ki) 
	{
		Entry* found = nullptr;
		Py_ssize_t offsets_index = -1;
		auto visit_pred = [&](std::size_t ofs_index) -> bool
		{
			auto ofs = offset_at(ofs_index);
			if(ofs < 0)
			{
				// Found an open bucket: break out of the traversal.
				// If we found an empty Entry instance earlier in the 
				// traversal, then that entry is the insertion position.
				// Otherwise, the insertion position is the current offset,
				// and it will have to be appended it to the 'entries' vector.
				//
				// In the former case, 'found' points to the empty 'Entry' instance
				// that we already found, in the latter 'Entry' is null.
				if(not found)
					offsets_index = static_cast<Py_ssize_t>(ofs_index);
				return true;
			}
			
			// check to see if this entry contains the key we're looking for
			Entry* ent = pointer_to_entry_at(ofs);
			assert(ent);

			if(ent->is_empty()) 
			{
				// Found an empty slot.  If this is the first empty slot that
				// we've encountered, then set 'found' and 'offsets_index' to 
				// the current position.
				// If we don't end up finding the key we're looking for, this 
				// will be the insertion position.
				if(not found)
				{
					found = ent;
					offsets_index = static_cast<Py_ssize_t>(ofs_index);
				}
				// Continue traversal.
				return false;
			}
			else if(ent->matches(ki))
			{
				// Found the key.  Set 'found' and 'offsets_index' then break out
				// of the traversal.
				found = ent;
				offsets_index = static_cast<Py_ssize_t>(ofs_index);
				return true;
			}
			// Found a non-empty, non-matching entry.  Continue traversal.
			return false;
		};
		visit_with_hash(ki.hash, visit_pred);
		
		// postconditions
		assert((not found) or (pointer_to_entry_at(offset_at(offsets_index)) == found));
		return std::make_pair(offsets_index, found);
	}

	void clear() noexcept
	{
		// get out early if we're already empty
		if(size() == 0)
		{
			return;
		}
		assert(offsets.size() >= min_buckets);
		// Before destructing 'ents', move the vector out of the way. 
		// This is to ensure we don't start calling destructors recursively.
		//
		// TODO: If we switch to a non-POCMA allocator, this might leak an exception.
		auto ents(std::move(entries));
			
		// don't forget to fix 'occupied'!
		occupied = 0;

		assert(entries.size() == 0 or std::all_of(entries.begin(), entries.end(), Entry::is_open));
		

		// try to reduce the capacity of offsets vector
		try
		{
			offsets.resize(min_buckets);
			offsets.shrink_to_fit();
		}
		catch(const std::bad_alloc&)
		{
			// only catch bad_alloc.  I don't want to silence 
			// unrelated exceptions and hide a bug
		}
		// whether or not it failed, fill with '-1' sentinal indices
		std::fill(offsets.begin(), offsets.end(), -1);
		assert(((offsets.size() & (offsets.size() - 1)) == 0) and "offsets.size() not a power of 2");
		// don't forget to fix 'mask'!
		mask = offsets.size() - 1;
		// finally, destroy the key-value-pairs
		// for(auto& ent: ents)
		// 	ent.clear();
		ents.clear();
	}
private:

	void grow_relocate_entry(const Entry& ent)
	{
		Py_hash_t ent_hash = ent.hash();
		Py_ssize_t idx = -1;
		auto visit_pred = [&](std::size_t ofs_idx)
		{
			if(offset_at(ofs_idx) < 0)
			{
				idx = ofs_idx;
				return true;
			}
			return false;
		};
		visit_with_hash(ent_hash, visit_pred);
		assert(offset_at(idx) == -1);
		offset_at(idx) = &ent - entries.data();
	}

	void advance_index(uhash_t& idx, uhash_t& perturb)
	{
		perturb >>= perturb_shift;
		idx = mask & (idx * 5 + perturb_shift);
	}

	void grow()
	{
		// double the size and set all offsets to -1
		offsets.assign(offsets.size() * 2, -1);
		// adjust the mask accordingly
		mask <<= 1;
		mask |= 1;
		// remove all empty Entry instances 
		grow_remove_empty_entries();
		// finaly, repoint all of the offsets
		for(auto& ent: entries)
			grow_relocate_entry(ent);
	}

	void grow_remove_empty_entries() noexcept
	{
		// don't do an O(n) traversal if there are no empty entries
		if(occupied == static_cast<Py_ssize_t>(size()))
		{
			assert(std::all_of(entries.begin(), entries.end(), Entry::is_closed));
			return;
		}

		// TODO: optimize for the fact that we know how many empty entries there are.
		//       also std::partition() uses swaps, but we only need moves.

		// put all of the empty entries at the end of the array
		auto empties = std::partition(entries.begin(), entries.end(), Entry::is_closed);

		// postconditions
		assert(std::all_of(empties, entries.end(), Entry::is_open));
		assert(std::all_of(entries.begin(), empties, Entry::is_closed));
		assert(empties - entries.begin() == occupied);

		// erase all of the empty elements at the end of the entries vector
		entries.erase(empties, entries.end());
	}

protected:
	Entry* add_entry(const KeyInfo& ki, Py_ssize_t offsets_index, PyObject* value) 
	{
		assert(ki.kind <= PY_UCS4);
		assert(ki.kind >= PY_BYTES);
		++occupied;
		int did_reserve = reserve_load_factor(); 
		if(did_reserve < 0) // attempted to reserve but failed
		{
			// roll back
			--occupied;
			return nullptr;
		}
		try
		{
			assert(ki.kind <= PY_UCS4);
			assert(ki.kind >= PY_BYTES);
			entries.emplace_back(ki, value);
		} 
		catch(const std::bad_alloc&)
		{
			PyErr_SetString(PyExc_MemoryError, "Attempt to allocate space for new strdict entry failed.");
			// roll back
			--occupied;
			return nullptr;
		}
		catch(const std::exception& e)
		{
			PyErr_SetString(PyExc_RuntimeError, e.what());
			// roll back
			--occupied;
			return nullptr;
		}
		assert(offset_at(offsets_index) == -1);
		offset_at(offsets_index) = entries.size() - 1;
		if(did_reserve)
			grow(); // shouldn't throw because we reserved the memory already
		return &(entries.back());
	}

	int reserve_load_factor()
	{
		if((double(occupied) / offsets.size()) >= max_load_factor)
		{
			try
			{
				offsets.reserve(2 * offsets.size());
				return 1;
			} 
			catch(const std::bad_alloc&)
			{
				PyErr_SetString(PyExc_MemoryError, "Attempt to grow strdict size due to high load factor failed.");
				return -1;
			}
			catch(const std::exception& e)
			{
				PyErr_SetString(PyExc_RuntimeError, e.what());
				return -1;
			}
		}
		else
		{
			return 0;
		}
	}

	int ensure_load_factor()
	{
		if((double(occupied) / offsets.size()) >= max_load_factor)
		{
			try
			{
				grow();
				return 1;
			} 
			catch(const std::bad_alloc&)
			{
				PyErr_SetString(PyExc_MemoryError, "Attempt to grow strdict size due to high load factor failed.");
				return -1;
			}
			catch(const std::exception& e)
			{
				PyErr_SetString(PyExc_RuntimeError, e.what());
				return -1;
			}
		}
		else
		{
			return 0;
		}
		
	}

	int assign_entry(const KeyInfo& ki, Entry* ent, PyObject* value) 
	{
		assert(ent);
		if(ent->is_empty())
		{
			++occupied;
			int did_reserve = reserve_load_factor();
			if(did_reserve < 0)
			{
				// roll back
				--occupied;
				return -1;
			}
			if(did_reserve)
				grow();
			ent->assign_from(ki, value);
		}
		else
		{
			assert(ent->matches(ki));
			ent->set_value(value);
		}
		return 0;
	}

	bool try_assign_entry(const KeyInfo& ki, Entry* ent, PyObject* value) 
	{
		assert(ent);
		if(ent->is_empty())
		{
			ent->assign_from(ki, value);
			return true;
		}
		else
		{
			assert(ent->matches(ki));
			return false;
		}
	}

	void remove_entry(Entry* ent)
	{
		assert(size() > 0);
		assert(not ent->is_empty());
		ent->set_empty();
		--occupied;
	}

	// Constructor that doesn't allocate.  This exists so that we can safely 
	// call the destructor in the strdict_dealloc() function when default
	// construction fails.
	StringDictBase(std::nullptr_t) noexcept:
		entries(), offsets(0), mask(0), occupied(0)
	{
		
	}
	
	std::vector<Entry> entries;
	std::vector<Py_ssize_t> offsets = std::vector<Py_ssize_t>(min_buckets, -1);
	std::make_unsigned_t<Py_ssize_t> mask = min_buckets - 1;
	Py_ssize_t occupied = 0;
};



extern "C" bool StringDict_Check(PyObject* self);
extern "C" bool StringDict_CheckExact(PyObject* self);

struct StringDict: 
	public StringDictBase
{
	using StringDictBase::StringDictBase;

	StringDict(const StringDict& other) = delete;
	StringDict(StringDict&& other) = default;

	int update_from_kwargs(PyObject* kwarg_dict)
	{
		assert(PyDict_Check(kwarg_dict) > 0);
		Py_ssize_t len = PyDict_Size(kwarg_dict);
		
		if((len < 0) or (0 != reserve_space(size() + len)))
			return -1;
		if(len == 0)
			return 0;
		
		// iterate through kwargs
		PyObject *key, *value;
		Py_ssize_t idx = 0;
		while(PyDict_Next(kwarg_dict, &idx, &key, &value)) 
		{
			Py_INCREF(key);
			PythonObject k(key);
			Py_INCREF(value);
			PythonObject v(value);
			PythonObject obj(this->set(key, value, false));
			if(not obj)
				return -1;
		}
		return 0;
	}

	int update_from_iterable(PyObject* iterable)
	{
		PythonObject iter(PyObject_GetIter(iterable));
		if(not iter)
			return -1;
		PythonObject kvp;

		while(kvp = std::move(PythonObject(PyIter_Next(iter))))
		{
			Py_ssize_t len = PyObject_Size(kvp);
			if(len < 0)
				return -1;
			if(len != 2)
			{
				PyErr_SetString(PyExc_ValueError, "Attempt to initialize dictionary "
					"item with tuple whose size is not 2.");
				return -1;
			}
			PythonObject k(PySequence_GetItem(kvp, 0));
			if(not k)
				return -1;
			PythonObject v(PySequence_GetItem(kvp, 1));
			if(not v)
				return -1;
			if(not this->set(k, v, false))
				return -1;
		}
		Py_DECREF(iter.release());
		return bool(PyErr_Occurred()) ? -1 : 0;
	}

	int update_from_mapping(PyObject* map)
	{
		PythonObject items(PyMapping_Items(map));
		if(not items)
			return -1;
		return update_from_iterable(items);
	}
	
	int update_from_string_dict(const StringDict& other)
	{
		if(other.size() == 0)
			return 0;
		int err = 0;
		auto visit_other = [&](const auto& other_ent) {
			assert(not other_ent.is_empty());
			auto ki = other_ent.as_key_info();
			auto [idx, ent] = find_insertion(ki);
			if(not ent)
			{
				if(not add_entry(ki, idx, other_ent.get_value()))
				{
					err = -1;
					return true;
				}
			}
			else if(ent->is_empty())
			{
				assign_entry(ki, ent, other_ent.get_value());
			}
			else 
			{
				ent->set_value(other_ent.get_value());
			}
			return false;
		};
		other.visit_nonempty_entries(visit_other);
		return err;
	}
	
	int update_from_object(PyObject* o)
	{
		if(StringDict_Check(o))
			if(o != static_cast<PyObject*>(this))
				return 0;
			else
				return update_from_string_dict(*static_cast<StringDict*>(o));
		else if(PyDict_Check(o))
			return update_from_kwargs(o);
		else if(PyMapping_Check(o) and PyObject_HasAttrString(o, "items")) 
			return update_from_mapping(o);
		else if(PyObject* it = PyObject_GetIter(o); (not it))
			return -1;
		else
			return update_from_iterable(it);
	}

	static bool try_default_construct(StringDict* mem)
	{
		assert(mem);
		PyObject* self = static_cast<PyObject*>(mem);
		// save PyObject_HEAD stuff
		const PyObject save_state(*self);
		auto guard = make_scope_guard([&](){
			// return PyObject_HEAD stuff
			*self = save_state;
		});
		try
		{
			new(mem) StringDict();
			assert(mem->size() == 0);
			assert(mem->bucket_count() == min_buckets);
			assert(mem->entry_slot_count() == 0);
			assert(mem->mask == (min_buckets - 1));
			return true;
		}
		catch(const std::exception& e)
		{
			new(mem) StringDict(nullptr);
			PyErr_SetString(PyExc_RuntimeError, e.what());
			return false;
		}
		// restore refcount and type
	}	




	operator const PyObject*() const
	{
		return static_cast<const PyObject*>(this);
	}

	operator PyObject*() 
	{
		return static_cast<PyObject*>(this);
	}

	PyObject* repr() 
	{
		if(size() == 0)
			return PyUnicode_FromString("strdict({})");
		if(auto count = Py_ReprEnter(this); count)
			return (count > 0) ? PyUnicode_FromString("strdict({...})") : nullptr;

		// scope guard to ensure we call Py_ReprLeave before exiting
		auto repr_guard = make_scope_guard([&](){ Py_ReprLeave(this); });

 		_PyUnicodeWriter writer;
		_PyUnicodeWriter_Init(&writer);
		auto writer_guardfunc = [&]() {
			_PyUnicodeWriter_Dealloc(&writer);
		};
		auto writer_guard = make_scope_guard_cancelable(writer_guardfunc);
		writer.overallocate = 1;
		if(0 != _PyUnicodeWriter_WriteASCIIString(&writer, "strdict({", 9))
			return nullptr;
		
		auto write_entry = [&, comma = '\0'](const auto& ent) mutable {
			assert(not ent.is_empty());
			char comma_sep[3] = {comma, ' ', '\0'};
			if(not comma) // don't write a comma the first time
				comma = ',';
			else if(0 != _PyUnicodeWriter_WriteASCIIString(&writer, comma_sep, 2))
				return true;
			if(0 != ent.write_repr(&writer))
				return true;
			return false;
		};
		
		if(visit_nonempty_entries(write_entry))
			return nullptr;
		else if(0 != _PyUnicodeWriter_WriteASCIIString(&writer, "})", 2))
			return nullptr;
		writer_guard.cancel();
		return _PyUnicodeWriter_Finish(&writer);
	}

	PyObject* pop(PyObject* key, PyObject* default_value) 
	{
		if(size() == 0)
		{
			if(default_value)
			{
				Py_INCREF(default_value);
				return default_value;
			}
			PyErr_SetString(PyExc_KeyError, "Attempt to call .pop() with no default on an empty strdict.");
			return nullptr;
		}
		[[maybe_unused]]
		const auto [ki, meta_] = make_key_info(key);
		if(not meta_)
			return nullptr;
		[[maybe_unused]]
		auto [idx, ent] = find_existing(ki);
		(void)idx;
		if(ent)
		{
			assert(not ent->is_empty());
			PyObject* value = ent->get_value_newref();
			remove_entry(ent);
			return value;
		}
		else if(default_value)
		{
			Py_INCREF(default_value);
			return default_value;
		}
		else
		{
			PyErr_SetObject(PyExc_KeyError, key);
			return nullptr;
		}
	}
	
	PyObject* popitem()
	{
		if(size() == 0)
		{
			PyErr_SetString(PyExc_KeyError, "Attempt to call .popitem() on an empty strdict.");
			return nullptr;
		}
		auto pos = std::find_if_not(entries.begin(), entries.end(), std::mem_fn(&Entry::is_empty));
		assert(pos < entries.end());
		assert(not pos->is_empty());
		PyObject* kvp = pos->as_tuple();
		if(not kvp)
			return nullptr;
		remove_entry(&(*pos));
		return kvp;
	}

	PyObject* getdefault(PyObject* key, PyObject* default_value) 
	{
		assert(default_value);
		const auto [ki, meta_] = make_key_info(key);
		if(not meta_)
			return nullptr;
		auto [idx, ent] = find_existing(ki);
		(void)idx;
		PyObject* value;
		if(ent)
		{
			assert(not ent->is_empty());
			value = ent->get_value_newref();
		}
		else
		{
			value = default_value;
			Py_INCREF(value);
		}
		return value;
	}

	PyObject* set(PyObject* key, PyObject* value, bool setdefault = false) 
	{
		assert(value);
		const auto [ki, meta_] = make_key_info(key);
		if(not meta_)
			return nullptr;
		auto [idx, ent] = find_insertion(ki);
		if(not ent)
		{
			ent = add_entry(ki, idx, value);
			if(not ent)
				return nullptr;
			return ent->get_value_newref();
		}
		else if(setdefault)
		{
			if(try_assign_entry(ki, ent, value))
			{
				Py_INCREF(value);
				return value;
			}
			else
			{
				return ent->get_value_newref();
			}
		}
		else
		{
			assign_entry(ki, ent, value);
			return value;
		}
	}

	int remove(PyObject* key)
	{
		const auto [ki, meta_] = make_key_info(key);
		if(not meta_)
			return -1;
		auto [idx, ent] = find_existing(ki);
		(void)idx;
		if(not ent)
		{
			PyErr_SetObject(PyExc_KeyError, key);
			return -1;
		}
		assert(not ent->is_empty());
		remove_entry(ent);
		return 0;
	}

	int contains(PyObject* key)
	{
		const auto [ki, meta_] = make_key_info(key);
		if(not meta_)
			return -1;
		auto [idx, ent] = find_existing(ki);
		(void)idx;
		if(ent)
			assert(not ent->is_empty());
		return bool(ent);
	}

	
	PyObject* subscript(PyObject* key)
	{
		const auto [ki, meta_] = make_key_info(key);
		if(not meta_)
			return nullptr;
		auto [idx, ent] = find_existing(ki);
		(void)idx;
		if(not ent)
		{
			if(not StringDict_CheckExact(this))
			{
				// check if subtype provides '.__missing__()'
				_Py_IDENTIFIER(__missing__);
				PythonObject missing_method(_PyObject_LookupSpecial(this, &PyId___missing__));
				if(missing_method)
					return PyObject_CallFunctionObjArgs(missing_method.get(), key, nullptr);
				else if(PyErr_Occurred())
					return nullptr;
			}
			PyErr_SetObject(PyExc_KeyError, key);
			return nullptr;
		}
		assert(not ent->is_empty());
		return ent->get_value_newref();
	}
	
	bool contains_entry_key(Entry& ent)
	{
		assert(not ent.is_empty());
		auto ki = ent.as_key_info();
		auto [idx, my_ent] = find_existing(ki);
		(void)idx;
		return bool(my_ent);
	}

	int contains_entry(Entry& other_ent)
	{
		assert(not other_ent.is_empty());
		auto ki = other_ent.as_key_info();
		auto [idx, ent] = find_existing(ki);
		(void)idx;
		if(not ent)
			return false;
		PyObject* value = ent->get_value();
		PyObject* other_value = other_ent.get_value();
		assert(value);
		assert(other_value);
		return PyObject_RichCompareBool(value, other_value, Py_EQ);
	}
	
	int operator==(StringDict& other)
	{
		if(size() != other.size())
			return false;
		
		// we're going to iterate through the StringDict that has fewer entry slots.  This means
		// less time spent skipping over empty entries and m
		StringDict& iter_dict =  (entry_slot_count() <= other.entry_slot_count()) ? *this : other;
		StringDict& other_dict = (entry_slot_count() <= other.entry_slot_count()) ? other : *this;
		// once this hits zero, we'll break out of the loop 
		Py_ssize_t nonempty_count = iter_dict.size();
		for(Entry& ent: iter_dict.entries)
		{
			if(not ent.is_empty()) 
			{
				int has_ent = other_dict.contains_entry(ent);
				if(has_ent < 0) // error
					return has_ent;
				else if(not has_ent) // iter_dict has key that other_dict doesn't
					return false;
				
				assert(nonempty_count > 0);
				--nonempty_count;
				if(nonempty_count == 0)
				{
					// assert that the rest of the entries are empty
					assert(std::all_of(
						iter_dict.entries.begin() + (&ent - iter_dict.entries.data()) + 1,
						iter_dict.entries.end(),
						std::mem_fn(&Entry::is_empty)
					));
					break;
				}
			}
		}
		return true;
	}
	
	int operator!=(StringDict& other)
	{
		int eq = (*this) == other;
		if(eq < 0)
			return eq;
		else 
			return not eq;
	}

	int equals_dict(PyObject* dict)
	{
		assert(PyDict_Check(dict));
		if(PyDict_Size(dict) != static_cast<Py_ssize_t>(size()))
			return false;
		
		// iterate over the dict()
		Py_ssize_t pos = 0;
		PyObject* key = nullptr;
		PyObject* value = nullptr;
		
		while(PyDict_Next(dict, &pos, &key, &value))
		{
			const auto [ki, meta_] = make_key_info(key);
			if(not meta_)
				return -1;
			auto [idx, ent] = find_existing(ki);
			(void)idx;
			if(not ent)
				return false;
			assert(not ent->is_empty());
			PyObject* strdict_value = ent->get_value();
			if(int cmp = PyObject_RichCompareBool(value, strdict_value, Py_EQ); cmp == -1)
				return -1;
			else if(not cmp)
				return false;
		}
		return true;
	}

	int make_copy(StringDict& other)
	{
		assert(other.size() == 0);
		assert(other.offsets.size() == min_buckets);
		assert(other.entries.size() == 0);
		try
		{
			other.entries.reserve(this->entries.size());
			other.offsets = this->offsets;
			for(Entry& ent: this->entries)
			{
				auto opt_ent = ent.make_copy();
				if(not opt_ent.has_value())
					throw std::runtime_error("Attempt to make copy strdict entry failed while copying strdict instance.");
				other.entries.push_back(std::move(*opt_ent));
			}
			other.mask = this->mask;
			other.occupied = this->occupied;
			return 0;
		}
		catch(const std::bad_alloc& e)
		{
			PyErr_SetString(PyExc_MemoryError, "Allocation failed while copying strdict instance.");
			return -1;
		}
		catch(const std::exception& e)
		{
			PyErr_SetString(PyExc_RuntimeError, e.what());
			return -1;
		}
	}

	template <class GetItem>
	PyObject* make_itemlist(GetItem get_item)
	{
		PythonObject itemlist(PyList_New(size()));
		if(not itemlist)
			return nullptr;
		
		auto list_size = static_cast<Py_ssize_t>(size());
		PyObject** list_items = PySequence_Fast_ITEMS(itemlist);
		[[maybe_unused]]
		PyObject** list_items_stop = list_items + list_size;
		auto visit_ent = [&](const Entry& ent) {
			assert(not ent.is_empty());
			assert(list_items < list_items_stop);
			*list_items = get_item(ent);
			if(not *list_items++)
			{
				itemlist.reset();
				return true;
			}
			else 
				return false;
		};
		if(visit_nonempty_entries(visit_ent))
			assert(not itemlist);
		else
			assert(itemlist);
		return itemlist.release();
	}
	
	PyObject* get_values()
	{
		return make_itemlist(std::mem_fn(&Entry::get_value_newref));
	}

	PyObject* get_keys()
	{
		return make_itemlist(std::mem_fn(&Entry::get_key_newref));
	}
	
	PyObject* get_items()
	{
		return make_itemlist(std::mem_fn(&Entry::as_tuple));
	}
	
	int gc_traverse(visitproc visit, void* arg)
	{
		int result = 0;

		// Wrap the Py_VISIT() macro, which conditionally returns an int.
		// Specifically, make it return a std::optional<int>.
		auto py_visit_wrap = [&](PyObject* obj) -> std::optional<int>{
			Py_VISIT(obj);
			return std::nullopt;
		};

		// Visit all entries in the strdict.  Note that this lambda may mutate 'result'
		auto visit_entry = [&](const auto& ent) {
			assert(not ent.is_empty());
			auto visit_result = py_visit_wrap(ent.get_value());
			if(visit_result.has_value())
			{
				result = *visit_result;
				return true;
			}
			return false;
		};
		visit_nonempty_entries(visit_entry);
		return result;
	}
	
	friend class StringDictIter;
};

extern "C" {

static PyObject* StringDict_GetType();

bool StringDict_CheckExact(PyObject* self)
{ return (PyObject*)(Py_TYPE(self)) == StringDict_GetType(); }

bool StringDict_Check(PyObject* self)
{ return PyObject_IsInstance(self, StringDict_GetType()); }


static bool StringDict_CheckErr(PyObject* self)
{
	if(not StringDict_Check(self))
	{
		PyErr_SetObject(PyExc_TypeError, self);
		return false;
	}
	else
	{
		return true;
	}
	
}

static StringDict* to_string_dict(PyObject* self)
{
	if(not StringDict_CheckErr(self))
		return nullptr;
	return static_cast<StringDict*>(self);
}

static std::tuple<StringDict*, PyObject*, PyObject*> dictmethod_2args(PyObject* self, PyObject* args, bool none_is_default = true)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return {nullptr, nullptr, nullptr};
	PyObject* key_;
	PyObject* default_value_ = nullptr;
	if(not PyArg_ParseTuple(args, "O|O", &key_, &default_value_))
		return {nullptr, nullptr, nullptr};
	if((not default_value_) and (none_is_default))
		default_value_ = Py_None;
	return std::make_tuple(dict, key_, default_value_);
}



static int strdict_contains(PyObject* self, PyObject* key)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return -1;
	return dict->contains(key);
}

static PyObject* strdict___contains__(PyObject* self, PyObject* key)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return nullptr;
	int cont = dict->contains(key);
	switch(cont)
	{
	case 0:
		Py_RETURN_FALSE;
		break;
	case 1:
		Py_RETURN_TRUE;
		break;
	default:
		assert(cont < 0);
		return nullptr;
	}
	assert(false);
}


static Py_ssize_t strdict_length(PyObject* self)
{
	const auto* dict = to_string_dict(self);
	if(not dict)
		return -1;
	return dict->size();
}

static PyObject* strdict_repr(PyObject* self)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return nullptr;
	return dict->repr();
}

static PyObject* strdict_keys(PyObject* self)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return nullptr;
	return dict->get_keys();
}

static PyObject* strdict_values(PyObject* self)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return nullptr;
	return dict->get_values();
}

static PyObject* strdict_items(PyObject* self)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return nullptr;
	return dict->get_items();
}

static PyObject* strdict_subscript(PyObject* self, PyObject* key)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return nullptr;
	return dict->subscript(key);
}

static int strdict_assign_subscript(PyObject* self, PyObject* key, PyObject* value)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return -1;
	else if(not value)
		return dict->remove(key);
	else if(dict->set(key, value, false))
		return 0;
	else
		return -1;
}

static PyObject* strdict_update(PyObject* self, PyObject* args, PyObject* kwargs)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return nullptr;
	assert(PyTuple_Check(args));
	if(Py_ssize_t argc = PyTuple_GET_SIZE(args); argc > 1)
	{
		PyErr_SetString(PyExc_TypeError, "strdict.update() takes at most 1 positional argument.");
		return nullptr;
	}
	else if(argc == 1)
	{
		if(0 != dict->update_from_object(PyTuple_GET_ITEM(args, 0)))
			return nullptr;
	}
	
	if(kwargs)
	{
		assert(PyDict_Check(kwargs));
		if(dict->update_from_kwargs(kwargs) != 0)
			return nullptr;
	}
	Py_RETURN_NONE;
}

static PyObject *
strdict_richcompare(PyObject *left, PyObject *right, int op)
{
	if((op != Py_EQ) and (op != Py_NE))
		Py_RETURN_NOTIMPLEMENTED;
	
	bool left_is_strdict = StringDict_Check(left);
	bool right_is_strdict = StringDict_Check(right);
	if(left_is_strdict and right_is_strdict)
	{
		// both strdicts
		StringDict& left_dict = *static_cast<StringDict*>(left);
		StringDict& right_dict = *static_cast<StringDict*>(right);
		int cmp = 0;

		assert(op == Py_EQ or op == Py_NE);
		// compare and check if an error occurred 
		cmp = (left_dict == right_dict);
		if(cmp < 0)
			return nullptr;
		// invert the result if != was requested
		if(op == Py_NE)
			cmp = not cmp;
		if(cmp)
			Py_RETURN_TRUE;
		else 
			Py_RETURN_FALSE;
	}
	else
	{
		// only other allowable case is that one is a strdict() and the other is a dict()
		StringDict* strdict = nullptr;
		PyObject* dict = nullptr;
		if(left_is_strdict)
		{
			if(not PyDict_Check(right))
				Py_RETURN_NOTIMPLEMENTED;
			strdict = static_cast<StringDict*>(left);
			dict = right;
		}
		else if(right_is_strdict)
		{
			if(not PyDict_Check(left))
				Py_RETURN_NOTIMPLEMENTED;
			strdict = static_cast<StringDict*>(right);
			dict = left;
		}
		else
		{
			Py_RETURN_NOTIMPLEMENTED;
		}
		int cmp = strdict->equals_dict(dict);
		if(cmp < 0)
			return nullptr;
		// invert the result if != was requested
		if(op == Py_NE)
			cmp = not cmp;
		if(cmp)
			Py_RETURN_TRUE;
		else 
			Py_RETURN_FALSE;
	}
}

static PyObject* strdict_sizeof(PyObject* self)
{
	const auto* dict = to_string_dict(self);
	if(not dict)
		return nullptr;
	return PyLong_FromSsize_t(Py_ssize_t(sizeof(*dict)));
}






static PyObject* strdict_get(PyObject* self, PyObject* args)
{
	auto [dict, key, default_value] = dictmethod_2args(self, args);
	if(not dict)
		return nullptr;
	assert(key);
	assert(default_value);
	return dict->getdefault(key, default_value);
}

static PyObject* strdict_setdefault(PyObject* self, PyObject* args)
{
	auto [dict, key, default_value] = dictmethod_2args(self, args);
	if(not dict)
		return nullptr;
	assert(key);
	assert(default_value);
	return dict->set(key, default_value, true);
}

static PyObject* strdict_clear(PyObject* self)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return nullptr;
	dict->clear();
	Py_RETURN_NONE;
}

static PyObject* strdict_pop(PyObject* self, PyObject* args)
{
	auto [dict, key, default_value] = dictmethod_2args(self, args, false);
	if(not dict)
		return nullptr;
	return dict->pop(key, default_value);
}

static PyObject* strdict_popitem(PyObject* self)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return nullptr;
	return dict->popitem();
}

static int strdict_tp_clear(PyObject *op)
{
	PyObject* obj = strdict_clear(op);
	if(not obj)
		return -1;
	return 0;
}

void strdict_dealloc(PyObject* self)
{
	// TODO: Do *I* have to untrack this?  I think so, but I'm not sure.
	PyObject_GC_UnTrack(self);
	Py_TRASHCAN_SAFE_BEGIN(self)
	StringDict* dict = static_cast<StringDict*>(self);
	dict->clear();
	dict->~StringDict();
	Py_TYPE(self)->tp_free(self);
	Py_TRASHCAN_SAFE_END(self)
}

static PyObject* strdict_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	assert(args);
	assert(type);
	assert(type->tp_alloc);
	assert(PyType_Check(type));
	PyObject* self = type->tp_alloc(type, 0);
	if(not self)
		return nullptr;
	if(not StringDict::try_default_construct(static_cast<StringDict*>(self)))
	{
		assert(type->tp_dealloc != nullptr);
		Py_DECREF(self);
		return nullptr;
	}
	assert(Py_REFCNT(self));
	assert(static_cast<StringDict*>(self)->size() == 0);
	assert(static_cast<StringDict*>(self)->bucket_count() == 8);
	// PyObject_GC_Track(self);
	return self;
}

static int strdict_init(PyObject* self, PyObject* args, PyObject* kwargs)
{
	assert(PyTuple_Check(args));
	Py_ssize_t argc = PyTuple_GET_SIZE(args);
	if(argc == 0)
	{
		
		assert((not kwargs) or PyDict_Check(kwargs));
		if(kwargs and PyDict_Size(kwargs) != 0)
		{
			return static_cast<StringDict*>(self)->update_from_kwargs(kwargs);
		}
		else
			return 0;
	}
	else if(argc > 1)
	{
		PyErr_SetString(PyExc_TypeError, "strdict.__init__() only takes a single positional argument, but got more than one.");
		return -1;
	}
	else
	{
		return static_cast<StringDict*>(self)->update_from_object(PyTuple_GET_ITEM(args, 0));
	}
}

static PyObject* strdict_copy(PyObject* self)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return nullptr;

	PythonObject args(PyTuple_New(0));
	if(not args)
		return nullptr;

	PythonObject kwargs(PyDict_New());
	if(not kwargs)
		return nullptr;
	
	PythonObject new_dict(strdict_new((PyTypeObject*)(StringDict_GetType()), args, kwargs));
	if(not new_dict)
		return nullptr;

	auto err = dict->make_copy(*static_cast<StringDict*>(new_dict.get()));
	if(err)
		return nullptr;
	return new_dict.release();
}

static int strdict_traverse(PyObject* self, visitproc visit, void *arg)
{
	auto* dict = to_string_dict(self);
	if(not dict)
		return -1;
	return dict->gc_traverse(visit, arg);
}




static PySequenceMethods strdict_as_sequence = {
    0,                          /* sq_length */
    0,                          /* sq_concat */
    0,                          /* sq_repeat */
    0,                          /* sq_item */
    0,                          /* sq_slice */
    0,                          /* sq_ass_item */
    0,                          /* sq_ass_slice */
    strdict_contains,           /* sq_contains */
    0,                          /* sq_inplace_concat */
    0,                          /* sq_inplace_repeat */
};


static PyMappingMethods strdict_as_mapping = {
    (lenfunc)strdict_length, /*mp_length*/
    (binaryfunc)strdict_subscript, /*mp_subscript*/
    (objobjargproc)strdict_assign_subscript, /*mp_ass_subscript*/
};

#include "StringDict_Docs.h"

static PyMethodDef strdict_methods[] = {
    {"__contains__", (PyCFunction)strdict___contains__, METH_O|METH_COEXIST,          strdict___contains____doc__},
    {"__getitem__",  (PyCFunction)strdict_subscript,    METH_O | METH_COEXIST,        getitem__doc__},
    {"__sizeof__",   (PyCFunction)strdict_sizeof,       METH_NOARGS,                  sizeof__doc__},
    {"get",          (PyCFunction)strdict_get,          METH_VARARGS,                 strdict_get__doc__},
    {"setdefault",   (PyCFunction)strdict_setdefault,   METH_VARARGS,                 strdict_setdefault__doc__},
    {"pop",          (PyCFunction)strdict_pop,          METH_VARARGS,                 pop__doc__},
    {"popitem",      (PyCFunction)strdict_popitem,      METH_NOARGS,                  popitem__doc__},
    {"keys",         (PyCFunction)strdict_keys,         METH_NOARGS,                  keys__doc__},
    {"items",        (PyCFunction)strdict_items,        METH_NOARGS,                  items__doc__},
    {"values",       (PyCFunction)strdict_values,       METH_NOARGS,                  values__doc__},
    {"update",       (PyCFunction)strdict_update,       METH_VARARGS | METH_KEYWORDS, update__doc__},
    {"clear",        (PyCFunction)strdict_clear,        METH_NOARGS,                  clear__doc__},
    {"copy",         (PyCFunction)strdict_copy,         METH_NOARGS,                  copy__doc__},
    {NULL,           NULL}   /* sentinel */
};

PyObject* test_alloc(PyTypeObject *type, Py_ssize_t nitems)
{
	assert(nitems == 0);
	return PyObject_GC_New(StringDict, type);
}


PyTypeObject StringDict_Type{
	PyVarObject_HEAD_INIT(nullptr, 0)
	"strdict",
	sizeof(StringDict),
	0,
	(destructor)strdict_dealloc,                         /* tp_dealloc */
	0,                                                   /* tp_print */
	0,                                                   /* tp_getattr */
	0,                                                   /* tp_setattr */
	0,                                                   /* tp_as_async */
	(reprfunc)strdict_repr,                   	     /* tp_repr */
	0,                                                   /* tp_as_number */
	&strdict_as_sequence,                                /* tp_as_sequence */
	&strdict_as_mapping,                                 /* tp_as_mapping */
	PyObject_HashNotImplemented,                         /* tp_hash */
	0,                                                   /* tp_call */
	0,                                                   /* tp_str */
	PyObject_GenericGetAttr,                             /* tp_getattro */
	0,                                                   /* tp_setattro */
	0,                                                   /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | 
		Py_TPFLAGS_BASETYPE,                         /* tp_flags */
	strdict_doc,                                         /* tp_doc */
	strdict_traverse,                                    /* tp_traverse */
	strdict_tp_clear,                                    /* tp_clear */
	strdict_richcompare,                                 /* tp_richcompare */
	0,                                                   /* tp_weaklistoffset */
	0,                                                   /* tp_iter */
	0,                                                   /* tp_iternext */
	strdict_methods,                                     /* tp_methods */
	0,                                                   /* tp_members */
	0,                                                   /* tp_getset */
	0,                                                   /* tp_base */
	0,                                                   /* tp_dict */
	0,                                                   /* tp_descr_get */
	0,                                                   /* tp_descr_set */
	0,                                                   /* tp_dictoffset */
	strdict_init,                                        /* tp_init */
	0,//PyType_GenericAlloc,                                 /* tp_alloc */
	strdict_new,                                         /* tp_new */
	0,//PyObject_GC_Del,                                     /* tp_free */
	// 0,                                                /* tp_is_gc */ 
	// 0,                                                /* tp_bases */
	// 0,                                                /* tp_mro */
	// 0,                                                /* tp_cache */
	// 0,                                                /* tp_subclasses */
	// 0,                                                /* tp_weaklist */
	// 0,                                                /* tp_del */
	// 0,                                                /* tp_version_tag */
	// 0                                                 /* tp_finalize */
};

static PyObject* StringDict_GetType()
{
	return (PyObject*)(&StringDict_Type);
}











static PyModuleDef StringDictmodule = {
	PyModuleDef_HEAD_INIT,
	"StringDict",
	"Like dict(), but only string keys are allowed.",
	-1,
	nullptr, 
	nullptr, 
	nullptr,
	nullptr, 
	nullptr
};


PyMODINIT_FUNC
PyInit_StringDict(void)
{

	if (PyType_Ready(&StringDict_Type) < 0)
		return NULL;

	PyObject* m = PyModule_Create(&StringDictmodule);
	if (m == NULL)
		return NULL;

	Py_INCREF(&StringDict_Type);
	PyModule_AddObject(m, "strdict", (PyObject *)&StringDict_Type);
	return m;
}



} /* extern "C" */


