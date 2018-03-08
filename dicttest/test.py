import collections
import collections.abc
import gc
import random
import string
import sys
import unittest
import weakref
from StringDict import strdict

class DictTest(unittest.TestCase):

    def test_invalid_keyword_arguments(self):
        class Custom(strdict):
            pass
        Custom({'': 1})
        for invalid in {1 : 2}, dict({1 : 2}):
            with self.assertRaises(TypeError):
                strdict(**invalid)
            with self.assertRaises(TypeError):
                strdict({}).update(**invalid)

    def test_constructor(self):
        self.assertEqual(strdict(), strdict({}))
        self.assertIsNot(strdict(), strdict({}))

    def test_bool(self):
        self.assertIs(not strdict({}), True)
        self.assertTrue(strdict({"": None}))
        self.assertIs(bool(strdict({})), False)
        self.assertIs(bool(strdict({"": 2})), True)

    def test_keys(self):
        d = strdict({})
        self.assertEqual(set(d.keys()), set())
        d = strdict({'a': 1, 'b': 2})
        k = d.keys()
        self.assertEqual(set(k), {'a', 'b'})
        self.assertIn('a', k)
        self.assertIn('b', k)
        self.assertIn('a', d)
        self.assertIn('b', d)
        self.assertRaises(TypeError, d.keys, None)
        self.assertEqual(repr(strdict(a=1).keys()), "['a']")

    def test_values(self):
        d = strdict({})
        self.assertEqual(set(d.values()), set())
        d = {'1':2}
        self.assertEqual(set(d.values()), {2})
        self.assertRaises(TypeError, d.values, None)
        self.assertEqual(repr(strdict(a=1).values()), "[1]")

    def test_items(self):
        d = strdict({})
        self.assertEqual(set(d.items()), set())

        d = {'1':2}
        self.assertEqual(set(d.items()), {('1', 2)})
        self.assertRaises(TypeError, d.items, None)
        self.assertEqual(repr(strdict(a=1).items()), "[('a', 1)]")

    def test_contains(self):
        d = strdict({})
        self.assertNotIn('a', d)
        self.assertFalse('a' in d)
        self.assertTrue('a' not in d)
        d = {'a': 1, 'b': 2}
        self.assertIn('a', d)
        self.assertIn('b', d)
        self.assertNotIn('c', d)

        self.assertRaises(TypeError, d.__contains__)

    def test_len(self):
        d = strdict({})
        self.assertEqual(len(d), 0)
        d = {'a': 1, 'b': 2}
        self.assertEqual(len(d), 2)

    def test_getitem(self):
        d = {'a': 1, 'b': 2}
        self.assertEqual(d['a'], 1)
        self.assertEqual(d['b'], 2)
        d['c'] = 3
        d['a'] = 4
        self.assertEqual(d['c'], 3)
        self.assertEqual(d['a'], 4)
        del d['b']
        self.assertEqual(d, {'a': 4, 'c': 3})

        self.assertRaises(TypeError, d.__getitem__)

        class Exc(Exception): pass

        class BadHash(str):
            fail = False

            def __hash__(self):
                if self.fail:
                    raise Exc()
                else:
                    return 0

        x = BadHash()
        d[x] = 42
        x.fail = True
        self.assertRaises(Exc, d.__getitem__, x)

    def test_clear(self):
        d = strdict({'1':1, '2':2, '3':3})
        d.clear()
        self.assertEqual(d, strdict({}))

        self.assertRaises(TypeError, d.clear, None)

    def test_update(self):
        d = strdict({})
        d.update({'1':100})
        d.update(strdict({'2':20}))
        d.update({'1':1, '2':2, '3':3})
        self.assertEqual(d, {'1':1, '2':2, '3':3})

        d.update()
        self.assertEqual(d, {'1':1, '2':2, '3':3})

        self.assertRaises((TypeError, AttributeError), d.update, None)

        class SimpleUserDict:
            def __init__(self):
                self.d = {'1':1, '2':2, '3':3}
            def keys(self):
                return self.d.keys()
            def __getitem__(self, i):
                return self.d[i]
            def items(self):
                return self.d.items()
            
        d.clear()
        d.update(SimpleUserDict())
        self.assertEqual(d, {'1':1, '2':2, '3':3})

        class Exc(Exception): pass

        d.clear()
        class FailingUserDict:
            def items(self):
                raise Exc
            def __getitem__(self, key):
                return key

        self.assertRaises(Exc, d.update, FailingUserDict())

        class FailingUserDict:
            def items(self):
                class BogonIter:
                    def __init__(self):
                        self.i = 1
                    def __iter__(self):
                        return self
                    def __next__(self):
                        if self.i:
                            self.i = 0
                            return ('a', self.i)
                        raise Exc
                return BogonIter()

            def __getitem__(self, key):
                return key
        self.assertRaises(Exc, d.update, FailingUserDict())

        class FailingUserDict:
            def keys(self):
                class BogonIter:
                    def __init__(self):
                        self.i = ord('a')
                    def __iter__(self):
                        return self

                    def __next__(self):
                        if self.i <= ord('z'):
                            rtn = chr(self.i)
                            self.i += 1
                            return rtn
                        raise StopIteration

                return BogonIter()

            def __getitem__(self, key):
                raise Exc

        self.assertRaises(Exc, d.update, FailingUserDict())

        class badseq(object):
            def __iter__(self):
                return self
            def __next__(self):
                raise Exc()

        self.assertRaises(Exc, strdict({}).update, badseq())

        self.assertRaises(ValueError, strdict({}).update, [('1', '2', '3')])

    def test_copy(self):
        d = strdict({'1': 1, '2': 2, '3': 3})
        self.assertIsNot(d.copy(), d)
        self.assertEqual(d.copy(), d)
        self.assertEqual(d.copy(), {'1': 1, '2': 2, '3': 3})

        copy = d.copy()
        d['4'] = 4
        self.assertNotEqual(copy, d)

        self.assertEqual(strdict({}).copy(), strdict({}))
        self.assertRaises(TypeError, d.copy, None)

    def test_copy_fuzz(self):
        for dict_size in [10, 100, 1000, 10000, 100000]:
            dict_size = random.randrange(
                dict_size // 2, dict_size + dict_size // 2)
            with self.subTest(dict_size=dict_size):
                d = strdict({})
                for i in range(dict_size):
                    d[str(i)] = i

                d2 = d.copy()
                self.assertIsNot(d2, d)
                self.assertEqual(d, d2)
                d2['key'] = 'value'
                self.assertNotEqual(d, d2)
                self.assertEqual(len(d2), len(d) + 1)

    def test_copy_maintains_tracking(self):
        class A:
            pass

        key = A()

        for d in (strdict({}), strdict({'a': 1}), strdict({repr(key): 'val'})):
            d2 = d.copy()
            self.assertEqual(gc.is_tracked(d), gc.is_tracked(d2))

    def test_copy_noncompact(self):
        # Dicts don't compact themselves on del/pop operations.
        # Copy will use a slow merging strategy that produces
        # a compacted copy when roughly 33% of dict is a non-used
        # keys-space (to optimize memory footprint).
        # In this test we want to hit the slow/compacting
        # branch of dict.copy() and make sure it works OK.
        d = strdict({str(k): k for k in range(1000)})
        assert '1' in d
        assert '0' in d
        for k in range(950):
            del d[str(k)]
        d2 = d.copy()
        self.assertEqual(d2, d)

    def test_get(self):
        d = strdict({})
        self.assertIs(d.get('c'), None)
        self.assertEqual(d.get('c', 3), 3)
        d = {'a': 1, 'b': 2}
        self.assertIs(d.get('c'), None)
        self.assertEqual(d.get('c', 3), 3)
        self.assertEqual(d.get('a'), 1)
        self.assertEqual(d.get('a', 3), 1)
        self.assertRaises(TypeError, d.get)
        self.assertRaises(TypeError, d.get, None, None, None)

    def test_setdefault(self):
        # dict.setdefault()
        d = strdict({})
        self.assertIs(d.setdefault('key0'), None)
        d.setdefault('key0', [])
        self.assertIs(d.setdefault('key0'), None)
        d.setdefault('key', []).append(3)
        self.assertEqual(d['key'][0], 3)
        d.setdefault('key', []).append(4)
        self.assertEqual(len(d['key']), 2)
        self.assertRaises(TypeError, d.setdefault)

        class Exc(Exception): pass

        class BadHash(str):
            fail = False
            def __hash__(self):
                if self.fail:
                    raise Exc()
                else:
                    return 42

        x = BadHash()
        d[x] = 42
        x.fail = True
        try:
            d.setdefault(x, [])
        except Exc:
            self.fail("setdefault() called custom hash, but should have used str.__hash__()")

    def test_setdefault_atomic(self):
        # Issue #13521: setdefault() calls __hash__ and __eq__ only once.
        class Hashed(str):
            def __init__(self, *args, **kwargs):
                self.hash_count = 0
                self.eq_count = 0

            def __hash__(self):
                self.hash_count += 1
                return 42

            def __eq__(self, other):
                self.eq_count += 1
                return super(Hashed, self).__eq__(other)

        hashed1 = Hashed('s')
        y = strdict({hashed1: 5})
        hashed2 = Hashed('r')
        y.setdefault(hashed2, [])
        self.assertEqual(hashed1.hash_count, 1)
        self.assertEqual(hashed2.hash_count, 0)
        self.assertEqual(hashed1.eq_count + hashed2.eq_count, 0)

    def test_setitem_atomic_at_resize(self):
        class Hashed(str):

            def __init__(self, *args, **kwargs):
                self.hash_count = 0
                self.eq_count = 0

            def __hash__(self):
                self.hash_count += 1
                return 42

            def __eq__(self, other):
                self.eq_count += 1
                return super(Hashed, self).__eq__(other)

        hashed1 = Hashed('s')
        # 5 items
        y = strdict({hashed1: 5, '0': 0, '1': 1, '2': 2, '3': 3})
        hashed2 = Hashed('r')
        # 6th item forces a resize
        y[hashed2] = []
        self.assertEqual(hashed1.hash_count, 1)
        self.assertEqual(hashed2.hash_count, 0)
        self.assertEqual(hashed1.eq_count + hashed2.eq_count, 0)

    def test_popitem(self):
        # dict.popitem()
        for copymode in -1, +1:
            # -1: b has same structure as a
            # +1: b is a.copy()
            for log2size in range(12):
                size = 2**log2size
                a = strdict({})
                b = strdict({})
                for i in range(size):
                    a[repr(i)] = str(i)
                    if copymode < 0:
                        b[repr(i)] = repr(i)
                if copymode > 0:
                    b = a.copy()
                for i in range(size):
                    ka, va = ta = a.popitem()
                    self.assertEqual(va, ka)
                    kb, vb = tb = b.popitem()
                    self.assertEqual(vb, kb)
                    self.assertFalse(copymode < 0 and ta != tb)
                self.assertFalse(a)
                self.assertFalse(b)

        d = strdict({})
        self.assertRaises(KeyError, d.popitem)

    def test_pop(self):
        # Tests for pop with specified key
        d = strdict({})
        k, v = 'abc', 'def'
        d[k] = v
        self.assertRaises(KeyError, d.pop, 'ghi')

        self.assertEqual(d.pop(k), v)
        self.assertEqual(len(d), 0)

        self.assertRaises(KeyError, d.pop, k)

        self.assertEqual(d.pop(k, v), v)
        d[k] = v
        self.assertEqual(d.pop(k, 1), v)

        self.assertRaises(TypeError, d.pop)

        class Exc(Exception): pass

        class BadHash(str):
            fail = False

            def __hash__(self):
                if self.fail:
                    raise Exc()
                else:
                    return 42

        x = BadHash()
        d[x] = 42
        x.fail = True
        try:
            d.pop(x)
        except Exc:
            self.fail("pop() called custom hash, but should have used str.__hash__()")

    def test_mutating_lookup(self):
        # changing dict during a lookup (issue #14417)
        class NastyKey(bytearray):
            mutate_dict = None

            def __init__(self, *args, **kwargs):
                super(NastyKey, self).__init__(*args, **kwargs)

            def __hash__(self):
                if NastyKey.mutate_dict:
                    mydict, key = NastyKey.mutate_dict
                    NastyKey.mutate_dict = None
                    del mydict[key]
                return 0

        key1 = NastyKey([1])
        key2 = NastyKey([2])
        d = {key1: 1}
        NastyKey.mutate_dict = (d, key1)
        d[key2] = 2
        self.assertEqual(d, {key2: 2})

    def test_repr(self):
        d = strdict({})
        self.assertEqual(repr(d), "strdict({})")
        d['1'] = 2
        self.assertEqual(repr(d), "strdict({'1': 2})")
        d = strdict({})
        d['1'] = d
        self.assertEqual(repr(d), "strdict({'1': strdict({...})})")

        class Exc(Exception): pass

        class BadRepr(object):
            def __repr__(self):
                raise Exc()

        d = {1: BadRepr()}
        self.assertRaises(Exc, repr, d)

    def test_repr_deep(self):
        d = strdict({})
        for i in range(sys.getrecursionlimit() + 100):
            d = strdict({'1': d})
        self.assertRaises(RecursionError, repr, d)

    def test_eq(self):
        self.assertEqual(strdict({}), strdict({}))
        self.assertEqual(strdict({'1': 2}), {'1': 2})
        self.assertEqual({'1': 2}, strdict({'1': 2}))
        self.assertEqual(strdict({'1': 2}), strdict({'1': 2}))

        class Exc(Exception): pass

        class BadCmp(object):
            def __eq__(self, other):
                raise Exc()

        d1 = {'1': BadCmp()}
        d2 = {'1': 1}

        with self.assertRaises(Exc):
            d1 == d2

    def test_keys_contained(self):
        self.helper_keys_contained(lambda x: set(x.keys()))
        self.helper_keys_contained(lambda x: set(x.items()))

    def helper_keys_contained(self, fn):
        # Test rich comparisons against dict key views, which should behave the
        # same as sets.
        empty = fn(strdict())
        empty2 = fn(strdict())
        smaller = fn(strdict({'1':1, '2':2}))
        larger =  fn(strdict({'1':1, '2':2, '3':3}))
        larger2 = fn(strdict({'1':1, '2':2, '3':3}))
        larger3 = fn(strdict({'4':1, '2':2, '3':3}))

        self.assertTrue(smaller <  larger)
        self.assertTrue(smaller <= larger)
        self.assertTrue(larger >  smaller)
        self.assertTrue(larger >= smaller)

        self.assertFalse(smaller >= larger)
        self.assertFalse(smaller >  larger)
        self.assertFalse(larger  <= smaller)
        self.assertFalse(larger  <  smaller)

        self.assertFalse(smaller <  larger3)
        self.assertFalse(smaller <= larger3)
        self.assertFalse(larger3 >  smaller)
        self.assertFalse(larger3 >= smaller)

        # Inequality strictness
        self.assertTrue(larger2 >= larger)
        self.assertTrue(larger2 <= larger)
        self.assertFalse(larger2 > larger)
        self.assertFalse(larger2 < larger)

        self.assertTrue(larger == larger2)
        self.assertTrue(smaller != larger)

        # There is an optimization on the zero-element case.
        self.assertTrue(empty == empty2)
        self.assertFalse(empty != empty2)
        self.assertFalse(empty == smaller)
        self.assertTrue(empty != smaller)

        # With the same size, an elementwise compare happens
        self.assertTrue(larger != larger3)
        self.assertFalse(larger == larger3)

    def test_dictview_set_operations_on_keys(self):
        k1 = set({1:1, 2:2}.keys())
        k2 = set({1:1, 2:2, 3:3}.keys())
        k3 = set({4:4}.keys())

        self.assertEqual(k1 - k2, set())
        self.assertEqual(k1 - k3, {1,2})
        self.assertEqual(k2 - k1, {3})
        self.assertEqual(k3 - k1, {4})
        self.assertEqual(k1 & k2, {1,2})
        self.assertEqual(k1 & k3, set())
        self.assertEqual(k1 | k2, {1,2,3})
        self.assertEqual(k1 ^ k2, {3})
        self.assertEqual(k1 ^ k3, {1,2,4})

    def test_dictview_set_operations_on_items(self):
        k1 = set({1:1, 2:2}.items())
        k2 = set({1:1, 2:2, 3:3}.items())
        k3 = set({4:4}.items())

        self.assertEqual(k1 - k2, set())
        self.assertEqual(k1 - k3, {(1,1), (2,2)})
        self.assertEqual(k2 - k1, {(3,3)})
        self.assertEqual(k3 - k1, {(4,4)})
        self.assertEqual(k1 & k2, {(1,1), (2,2)})
        self.assertEqual(k1 & k3, set())
        self.assertEqual(k1 | k2, {(1,1), (2,2), (3,3)})
        self.assertEqual(k1 ^ k2, {(3,3)})
        self.assertEqual(k1 ^ k3, {(1,1), (2,2), (4,4)})

    def test_missing(self):
        from inspect import currentframe, getframeinfo
        # Make sure dict doesn't have a __missing__ method
        self.assertFalse(hasattr(strdict, "__missing__"))
        self.assertFalse(hasattr(strdict({}), "__missing__"))
        class D(strdict):
            def __init__(self, *args, **kwargs):
                super(D, self).__init__(*args, **kwargs)

            def __missing__(self, key):
                return 42
        d = D({'1': 2, '3': 4})
        self.assertEqual(d['1'], 2)
        self.assertEqual(d['3'], 4)
        self.assertNotIn('2', d)
        self.assertNotIn('2', d.keys())
        self.assertEqual(d['2'], 42)

        e = strdict()
        with self.assertRaises(KeyError) as c:
            e['42']
        self.assertEqual(c.exception.args, ('42',))

        class F(strdict):
            def __init__(self):
                # An instance variable __missing__ should have no effect
                self.__missing__ = lambda key: None
        f = F()
        with self.assertRaises(KeyError) as c:
            f['42']
        self.assertEqual(c.exception.args, ('42',))

        class G(strdict):
            pass
        g = G()
        with self.assertRaises(KeyError) as c:
            g['42']
        self.assertEqual(c.exception.args, ('42',))

    def test_tuple_keyerror(self):
        # SF #1576657
        d = strdict({})
        with self.assertRaises(KeyError) as c:
            d['1']

    def test_resize1(self):
        # Dict resizing bug, found by Jack Jansen in 2.2 CVS development.
        # This version got an assert failure in debug build, infinite loop in
        # release build.  Unfortunately, provoking this kind of stuff requires
        # a mix of inserts and deletes hitting exactly the right hash codes in
        # exactly the right order, and I can't think of a randomized approach
        # that would be *likely* to hit a failing case in reasonable time.

        d = strdict({})
        for i in range(5):
            d[chr(i)] = i
        for i in range(5):
            del d[chr(i)]
        for i in range(5, 9):  # i==8 was the problem
            d[chr(i)] = i

    def test_resize2(self):
        # Another dict resizing bug (SF bug #1456209).
        # This caused Segmentation faults or Illegal instructions.

        class X(str):
            def __hash__(self):
                return 5
            def __eq__(self, other):
                if resizing:
                    d.clear()
                return False
        d = strdict({})
        resizing = False
        d[X()] = 1
        d[X()] = 2
        d[X()] = 3
        d[X()] = 4
        d[X()] = 5
        # now trigger a resize
        resizing = True
        d['9'] = 6


    def test_resize_copy_and_deletion(self):
        # test adding a bunch of items and then deleting them
        keys = [str(i) for i in range(1000)]
        d = strdict()
        dcpy = d.copy()
        self.assertEqual(d, dcpy)
        for k in keys:
            d[k] = int(int(k) ** 2)
        self.assertEqual(len(d), len(keys))
        self.assertNotEqual(d, dcpy)
        for k in keys[30:]:
            del d[k]
        self.assertNotEqual(d, dcpy)
        self.assertNotEqual(len(d), len(keys))
        for k in (keys[:30])[::-1]:
            dcpy[k] = int(int(k) ** 2)
        self.assertEqual(d, dcpy)

    def _not_tracked(self, t):
        # Nested containers can take several collections to untrack
        gc.collect()
        gc.collect()
        self.assertFalse(gc.is_tracked(t), t)

    def _tracked(self, t):
        self.assertTrue(gc.is_tracked(t), t)
        gc.collect()
        gc.collect()
        self.assertTrue(gc.is_tracked(t), t)

    def check_reentrant_insertion(self, mutate):
        # This object will trigger mutation of the dict when replaced
        # by another value.  Note this relies on refcounting: the test
        # won't achieve its purpose on fully-GCed Python implementations.
        class Mutating:
            def __del__(self):
                mutate(d)

        d = strdict({k: Mutating() for k in 'abcdefghijklmnopqr'})
        for k in d.keys():
            d[k] = k

    def test_reentrant_insertion(self):
        # Reentrant insertion shouldn't crash (see issue #22653)
        def mutate(d):
            d['b'] = 5
        self.check_reentrant_insertion(mutate)

        def mutate(d):
            d.update(self.__dict__)
            d.clear()
        self.check_reentrant_insertion(mutate)

        def mutate(d):
            while d:
                d.popitem()
        self.check_reentrant_insertion(mutate)

    def test_equal_operator_modifying_operand(self):
        # test fix for seg fault reported in issue 27945 part 3.
        dict_a = strdict()
        dict_b = strdict()
        class X(str):
            def __del__(self):
                dict_b.clear()

            def __eq__(self, other):
                dict_a.clear()
                return True

            def __hash__(self):
                return 0

        dict_a[X('')] = 0
        dict_b[X('')] = X('')
        self.assertTrue(dict_a == dict_b)

    def test_dictitems_contains_use_after_free(self):
        class X:
            def __eq__(self, other):
                d.clear()
                return NotImplemented

        d = strdict({'0': set()})
        ('0', X()) in d.items()

    def test_equality_inequality(self):
        
        a = strdict()
        b = strdict()
        keys = [
                ("a", 1),
                ("b", 2),
                ("c", 3),
                ("d", 4),
                ("e", 5),
                ("f", 6),
                ("asdffdsaasdf", 7),
                ("000000000000000000000000", 8), 
                ("asdfdssebsbrssr", "asdfadsaf")
        ]
        a.update(keys)
        b.update(keys)
        a_cpy = a.copy()
        self.assertTrue(a == b)
        self.assertFalse(a != b)
        self.assertTrue(a == a_cpy)
        a.clear()
        self.assertTrue(a != b)
        self.assertFalse(a == b)
        for k, v in keys:
                a[k] = v
        self.assertTrue(a == b)
        self.assertFalse(a != b)
        b.clear()
        b.update(dict(keys))
        self.assertTrue(a == b)
        self.assertFalse(a != b)
        del b['a']
        self.assertTrue(a != b)
        self.assertFalse(a == b)
        b['a'] = a['a'] + 1
        self.assertTrue(a != b)
        self.assertFalse(a == b)
        b['a'] = a['a']
        self.assertTrue(a == b)
        self.assertFalse(a != b)


if __name__ == "__main__":
    unittest.main()

