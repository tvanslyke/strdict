

.PHONY: clean
clean:
	python3 setup.py clean --all

.PHONY: install
install:
	python3 setup.py install --user

.PHONY: install-dbg
install-dbg:
	python3.6-dbg setup.py install --user

.PHONY: test
test: install-dbg
	python3.6-dbg dicttest/test.py -v

.PHONY: test-valgrind
test-valgrind: install-dbg
	valgrind python3.6-dbg dicttest/test.py -v
