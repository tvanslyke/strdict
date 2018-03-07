#ifndef PYTHON_UTILS_H
#define PYTHON_UTILS_H

#include <Python.h>
#include <memory>

struct ScopedPyBuffer{

	ScopedPyBuffer(Py_buffer buf):
		buf_(buf)
	{
		
	}

	~ScopedPyBuffer()
	{
		PyBuffer_Release(&buf_);
	}
private:
	Py_buffer buf_;
};

template <class GuardFunc>
struct PythonScopeGuard
{
	PythonScopeGuard(const GuardFunc& func):
		guardfunc(func)
	{
		
	}
	
	~PythonScopeGuard()
	{
		guardfunc();
	}

	GuardFunc guardfunc;
};

template <class GuardFunc>
struct PythonScopeGuard_Cancelable
{
	PythonScopeGuard_Cancelable(GuardFunc& func):
		guardfunc(&func)
	{
		
	}
	
	~PythonScopeGuard_Cancelable()
	{
		if(guardfunc)
			(*guardfunc)();
	}

	void cancel()
	{ guardfunc = nullptr; }

	GuardFunc* guardfunc;
};

template <class T>
PythonScopeGuard<T> make_scope_guard(const T& func)
{
	return PythonScopeGuard<T>(func);
}

template <class T>
PythonScopeGuard_Cancelable<T> make_scope_guard_cancelable(T& func)
{
	return PythonScopeGuard_Cancelable<T>(func);
}


struct PyObjectDeleter {
	void operator()(void* obj) const
	{
		Py_XDECREF(((PyObject*)obj));
	}
};

struct PythonObject:
	public std::unique_ptr<PyObject, PyObjectDeleter>
{
	using std::unique_ptr<PyObject, PyObjectDeleter>::unique_ptr;
	operator PyObject*()
	{
		return this->get();
	}

	operator const PyObject*() const
	{
		return this->get();
	}
	
	template <class MaybePythonObject>
	operator const MaybePythonObject* () const
	{
		error_if_not_pyobject<MaybePythonObject>();
		return (MaybePythonObject*)get();
	}
	
	template <class MaybePythonObject>
	operator MaybePythonObject* ()
	{
		error_if_not_pyobject<MaybePythonObject>();
		return (MaybePythonObject*)get();
	}
	
private:
	template <class MaybePythonObject>
	static void error_if_not_pyobject()
	{
		using python_base_type = std::decay_t<decltype(std::declval<MaybePythonObject>().ob_base)>; // error if not a PyObject
		// also error
		static_assert(std::is_same_v<python_base_type, PyObject> 
			or std::is_same_v<python_base_type, PyVarObject>,
			"Attempt to cast instance of PythonObject to pointer to class which does not derive from PyObject.");
	}
};






#endif /* PYTHON_UTILS_H */
