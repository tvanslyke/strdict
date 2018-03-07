#ifndef STRINGDICT_DOCS_
#define STRINGDICT_DOCS_

PyDoc_STRVAR(strdict_fromkeys__doc__,
"fromkeys($type, iterable, value=None, /)\n"
"--\n"
"\n"
"Create a new dictionary with keys from iterable and values set to value.");

PyDoc_STRVAR(strdict___contains____doc__,
"__contains__($self, key, /)\n"
"--\n"
"\n"
"True if the dictionary has the specified key, else False.");

PyDoc_STRVAR(strdict_get__doc__,
"get($self, key, default=None, /)\n"
"--\n"
"\n"
"Return the value for key if key is in the dictionary, else default.");

PyDoc_STRVAR(strdict_setdefault__doc__,
"setdefault($self, key, default=None, /)\n"
"--\n"
"\n"
"Insert key with a value of default if key is not in the dictionary.\n"
"\n"
"Return the value for key if key is in the dictionary, else default.");

PyDoc_STRVAR(strdict_doc,
"StringDict() -> new empty dictionary\n"
"StringDict(mapping) -> new string dictionary initialized from a mapping object's\n"
"    (key, value) pairs\n"
"StringDict(iterable) -> new string dictionary initialized as if via:\n"
"    d = StringDict()\n"
"    for k, v in iterable:\n"
"        d[k] = v\n"
"StringDict(**kwargs) -> new string dictionary initialized with the name=value pairs\n"
"    in the keyword argument list.  For example:  StringDict(one=1, two=2)");

PyDoc_STRVAR(getitem__doc__, "x.__getitem__(y) <==> x[y]");

PyDoc_STRVAR(sizeof__doc__,
"D.__sizeof__() -> size of D in memory, in bytes");

PyDoc_STRVAR(pop__doc__,
"D.pop(k[,d]) -> v, remove specified key and return the corresponding value.\n\
If key is not found, d is returned if given, otherwise KeyError is raised");

PyDoc_STRVAR(popitem__doc__,
"D.popitem() -> (k, v), remove and return some (key, value) pair as a\n\
2-tuple; but raise KeyError if D is empty.");

PyDoc_STRVAR(update__doc__,
"D.update([E, ]**F) -> None.  Update D from dict/iterable E and F.\n\
If E is present and has a .keys() method, then does:  for k in E: D[k] = E[k]\n\
If E is present and lacks a .keys() method, then does:  for k, v in E: D[k] = v\n\
In either case, this is followed by: for k in F:  D[k] = F[k]");

PyDoc_STRVAR(clear__doc__,
"D.clear() -> None.  Remove all items from D.");

PyDoc_STRVAR(copy__doc__,
"D.copy() -> a shallow copy of D");

PyDoc_STRVAR(keys__doc__,
"D.keys() -> a set-like object providing a view on D's keys");

PyDoc_STRVAR(items__doc__,
"D.items() -> a set-like object providing a view on D's items");

PyDoc_STRVAR(values__doc__,
"D.values() -> an object providing a view on D's values");


#endif /* STRINGDICT_DOCS_ */
