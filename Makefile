
PLATS = linux macosx freebsd mingw minix

MODE   ?= release
SUFFIX ?= .daomake

first :
	$(MAKE) -f Makefile$(SUFFIX)


$(PLATS) :
	cd tools/daomake && $(MAKE) -f Makefile.bootstrap $@
	./tools/daomake/daomake --mode $(MODE) --suffix $(SUFFIX) --platform $@
	$(MAKE) -f Makefile$(SUFFIX)


test :
	$(MAKE) -f Makefile$(SUFFIX) test
	$(MAKE) -f Makefile$(SUFFIX) testsum

install :
	$(MAKE) -f Makefile$(SUFFIX) install

uinstall :
	$(MAKE) -f Makefile$(SUFFIX) uninstall

clean :
	$(MAKE) -f Makefile$(SUFFIX) clean
