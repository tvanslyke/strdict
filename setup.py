from distutils.core import setup, Extension

module1 = Extension('StringDict',
                    sources = ['StringDict.cpp', 'StringDictEntry.c', 'KeyInfo.c'],
		    extra_compile_args = ["-g", "-std=c++17", "-O0"])

setup (name = 'PackageName',
       version = '1.0',
       description = 'This is a demo package',
       ext_modules = [module1])
