from distutils.core import setup, Extension

StringDict_module = Extension('StringDict',
                    sources = ['src/StringDict.cpp', 'src/StringDictEntry.c', 'src/KeyInfo.c'],
                    depends = ['LEB128.h', 'MakeKeyInfo.h', 'PythonUtils.h', 'StringDict_Docs.h', 'StringDictEntry.h', 'setup.py'],
                    include_dirs = ['include'],
		    extra_compile_args = ["-std=c++17", "-O3", '-fno-delete-null-pointer-checks'])

setup (name = 'StringDict',
       version = '0.1',
       description = 'Provides a dict-like type that only allows bytes() (and bytes-like types) or str() keys.',
       ext_modules = [StringDict_module]
)
