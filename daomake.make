
MODE ?= release

PLATS = linux macosx freebsd mingw minix

$(PLATS) :
	cd tools/daomake && make -f Makefile.bootstrap $@
	./tools/daomake/daomake --mode $(MODE) --suffix .daomake --platform $@
	make -f Makefile.daomake
