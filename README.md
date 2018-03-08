# StringDict
[![Language](https://img.shields.io/badge/language-Python-3.6-blue.svg)](https://isocpp.org/) [![Language](https://img.shields.io/badge/language-C++17-blue.svg)](https://isocpp.org/) [![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://opensource.org/licenses/MIT)

A Python3.6 extension that adds a string-keys-only, dict-like type with an emphasis on fast lookup.  Presently this project is still in "beta", but passes the test suite.  The test suite is a modified version of the CPython dict test suite.

## Installation
Install using the provided setup.py script:

```sh
$ python3 setup.py install --user
```
Note that at least Python3.3 is required and that this is only known to work on Python3.6.

A version of either GCC or Clang that supports both C++17 and C11 is required.

## Usage
You can use it just like a normal dict!
### example.py
```python
from StringDict import strdict

# you can use strdict() in a similar fashion to how dict() is used.
sd = strdict(apples = 100, oranges = "not apples")
print(sd)

for fruit, price in zip(('cherries', 'plums', 'peaches'), (1.25, 2.50, 0.75)):
        sd[fruit] = price

sd["self"] = sd

# for fun
sd.update(locals())

del sd['cherries']
print(sd)
```

#### Output
```
strdict({'apples': 100, 'oranges': 'not apples'})
strdict({'apples': 100, 'oranges': 'not apples', 'plums': 2.5, 'peaches': 0.75, 'self': strdict({...}), '__name__': '__main__', '__doc__': None, '__package__': None, '__loader__': <_frozen_importlib_external.SourceFileLoader object at 0x7f927c088160>, '__spec__': None, '__annotations__': {}, '__builtins__': <module 'builtins' (built-in)>, '__file__': 'example.py', '__cached__': None, 'strdict': <class 'strdict'>, 'sd': strdict({...}), 'fruit': 'peaches', 'price': 0.75})
```
### Authors
* **Timothy VanSlyke** - vanslyke.t@husky.neu.edu

### License
This project is licensed under the MIT License

### Contributing
Contributions and bug reports are welcome!  Please submit any issues or pull requests through github.
