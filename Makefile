
UNAME = $(shell uname)

ifeq ($(UNAME), Linux)
  UNIX = 1
  LINUX = 1
endif

ifeq ($(UNAME), Darwin)
  UNIX = 1
  MACOSX = 1
endif

ifdef UNIX
  include Makefile.unix
else
  WIN32 = 1
  include Makefile.mingw32
endif
