.PHONY: all c java python ruby groovy haskell ocaml ocamlopt \
clean

RUBY_TARGETS=1.9.3-p392 2.0.0-p0 jruby-1.7.3 rbx-2.0.0-rc1 topaz-dev
PYTHON_TARGETS=2.7.4 3.3.1 jython-2.5.3 pypy-1.9

BENCH=ao-render
MACHINE=`whoami`_`uname -n`
TIME=../time.sh $(BENCH) $(MACHINE)

all: c java python ruby

c: ao-render.c
	gcc -Wall -Wextra -Ofast -s -fomit-frame-pointer -msse3 -march=core2 -ffast-math -fwhole-program -ftrapv -mfpmath=sse -std=c99 ao-render.c -lm -o c.out
	$(TIME) c ./c.out
	rm c.out ao-render-c.ppm

java: AO.java
	javac -g:none AO.java
	$(TIME) java java AO
	rm *.class ao_java.ppm

groovy: ao.groovy
	$(TIME) $@ groovy ao.groovy

python: ao_py.py
	for version in $(PYTHON_TARGETS); do \
		bin=`PYENV_VERSION=$$version pyenv which python`; \
		if [ $$? -eq 0 ]; then \
			$(TIME) python-$$version $$bin ao_py.py; \
		fi; \
	done
	rm ao_py.ppm

ruby: ao-render.rb
	for version in $(RUBY_TARGETS); do \
	  bin=`RBENV_VERSION=$$version rbenv which ruby`; \
	  if [ $$? -eq 0 ]; then \
	    $(TIME) ruby-$$version $$bin ao-render.rb > ao_rb.ppm; \
	  fi; \
	done
	rm ao_rb.ppm

ocaml: ao_ml.ml
	ocamlc ao_ml.ml -o ml.out
	${TIME} $@ ./ml.out
	rm ao.ppm

ocamlopt: ao_ml.ml
	ocamlopt ao_ml.ml -o ml.out
	${TIME} $@ ./ml.out
	rm ao.ppm

haskell: ao_hs.hs
	ghc --make -O3 ao_hs.hs -o hs.out
	${TIME} $@ ./hs.out > ao_hs.ppm
	rm ao_hs.ppm

clean:
	rm -rf *.out *.cmi *.cmo *.cmx *.class .rbx *.hi *.o
