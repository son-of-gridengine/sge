# Copyright Dave Love,  2010
# New BSD licence (see http://www.sunsource.net/TUPPCP.html)

CORELIB_DIR  = $(SDIR)/libs/libcore

corelib: libcore.o

corelib_clean: 
	$(RM) libcore.so libcore.o

libcore.so: libcore.o
	if [ -n "CORELIB" ]; then gcc -shared -o libcore.so libcore.o -lpthread; \
        else touch libcore.o; fi

libcore.o: $(CORELIB_DIR)/libcore.c
	if [ -n "CORELIB" ]; then gcc -O -Wall -Wstrict-prototypes -D_GNU_SOURCE -fPIC -c $(CORELIB_DIR)/libcore.c; \
        else touch libcore.o; fi
