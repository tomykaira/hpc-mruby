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

compile_by_hpcmrb:
	./bin/hpcmrb ${FILE}.rb -o${FILE}.hpcmrb.c
	gcc -Wall -c ${FILE}.hpcmrb.c -I ${HPCMRB}/lib -I ./include -o ${FILE}.o
	gcc -o ${FILE} ${FILE}.o ${HPCMRB}/lib/lib_builtin_driver.a build/host/lib/libmruby.a -lm

lib_builtin_driver:
	gcc -c ${HPCMRB}/lib/builtin.c -I ${HPCMRB} -I ./include -o ${HPCMRB}/lib/builtin.o
	gcc -c ${HPCMRB}/lib/driver.c -I ${HPCMRB} -I ./include -o ${HPCMRB}/lib/driver.o
	ar r ${HPCMRB}/lib/lib_builtin_driver.a ${HPCMRB}/lib/*.o