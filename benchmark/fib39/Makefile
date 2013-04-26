.PHONY: all c java python ruby ocaml ocamlopt clean

RUBY_TARGETS=1.9.3-p392 2.0.0-p0 jruby-1.7.3 rbx-2.0.0-rc1 topaz-dev
PYTHON_TARGETS=2.7.4 3.3.1 jython-2.5.3 pypy-1.9

BENCH=fib39
MACHINE=`whoami`_`uname -n`
TIME=../time.sh $(BENCH) $(MACHINE)

all: c java python ruby ocaml ocamlopt

c: fib39.c
	gcc -O3 fib39.c -o c.out
	$(TIME) c ./c.out

java: fib39.java
	javac -g:none fib39.java
	$(TIME) java java fib39

ocaml: fib39.ml
	ocamlc fib39.ml -o $@.out
	$(TIME) $@ ./$@.out

ocamlopt: fib39.ml
	ocamlopt fib39.ml -o $@.out
	$(TIME) $@ ./$@.out

python: fib39.py
	for version in $(PYTHON_TARGETS); do \
	  bin=`PYENV_VERSION=$$version pyenv which python`; \
	  if [ $$? -eq 0 ]; then \
	    $(TIME) python-$$version $$bin fib39.py; \
	  fi; \
	done

ruby: fib39.rb
	for version in $(RUBY_TARGETS); do \
	  bin=`RBENV_VERSION=$$version rbenv which ruby`; \
	  if [ $$? -eq 0 ]; then \
	    $(TIME) ruby-$$version $$bin fib39.rb; \
	  fi; \
	done

clean:
	rm -rf *.out *.cmi *.cmo *.cmx *.class .rbx
