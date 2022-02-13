# program(s) to be built in current dir
check_PROGRAMS = test

# source code files used to build cwdaemon program
test_SOURCES = main.c ../library/key_source.c ../library/key_source_serial.c ../library/cw_rec_utils.c ../library/misc.c ../library/socket.c ../library/process.c
# target-specific preprocessor flags (#defs and include dirs)
test_CPPFLAGS = ${AM_CFLAGS} ${LIBCW_CFLAGS} -I../library/
# target-specific linker flags (objects to link)
test_LDADD = ${LIBCW_LIBS} -lpthread

test_CFLAGS = -std=c99

test_LDFLAGS = -Wl,-rpath=${prefix}/lib

