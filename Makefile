# mruby is using Rake (http://rake.rubyforge.org) as a build tool.
# We provide a minimalistic version called minirake inside of our
# codebase.

RAKE = ruby ./minirake

.PHONY : all
all :
	$(RAKE)

.PHONY : test
test : all
	$(RAKE) test

.PHONY : clean
clean :
	$(RAKE) clean


HPCMRB = ./mrbgems/mruby-bin-hpcmrb/tools/hpcmrb

compile_by_hpcmrb: all lib_builtin_driver
	./bin/hpcmrb ${FILE}.rb -o${FILE}.hpcmrb.c
	gcc -g -Wall -c ${FILE}.hpcmrb.c -I ${HPCMRB}/lib -I ./include -o ${FILE}.o
	gcc -g -Wall -o ${FILE} ${FILE}.o ${HPCMRB}/lib/lib_builtin_driver.a build/host/lib/libmruby.a -lm

lib_builtin_driver:
	gcc -g -Wall -c ${HPCMRB}/lib/builtin.c -I ${HPCMRB} -I ./include -o ${HPCMRB}/lib/builtin.o
	gcc -g -Wall -c ${HPCMRB}/lib/driver.c -I ${HPCMRB} -I ./include -o ${HPCMRB}/lib/driver.o
	ar r ${HPCMRB}/lib/lib_builtin_driver.a ${HPCMRB}/lib/*.o

compare:
	make compile_by_hpcmrb FILE=${FILE}
	@echo "mruby:"
	bin/mruby ${FILE}.rb
	@echo "HPC:"
	${FILE}
