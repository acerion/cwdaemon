#ifndef CWDAEMON_TESTS_LIB_SUPERVISOR_H
#define CWDAEMON_TESTS_LIB_SUPERVISOR_H




typedef enum supervisor_id_t {
	/*
	  cwdaemon is executed without supervisor, aapart from the test code that
	  is a parent process of cwdaemon server.
	*/
	supervisor_id_none = 0,

	/*
	  cwdaemon is executed like this:
	  valgrind <valgrind opts> ./src/cwdaemon <cwdaemon opts>
	*/
	supervisor_id_valgrind,

	/*
	  cwdaemon is executed like this:
	  gdb --args  ./src/cwdaemon <cwdaemon opts>

	  TODO (acerion) 2024.02.25 using gdb as supervisor doesn't work yet.
	*/
	supervisor_id_gdb,
} supervisor_id_t;




/**
   @param Append to @p argv the options necessary to start valgrind

   @p argv will be passed to execv(). The variable is preallocated by caller.
   Count of elements in the vector is limited, but for now it's large enough.

   Function increments @p argc for each inserted value.

   @param argv Vector of arguments to execv() into which
   @param argc Counter of arguments in @p argv

   @return 0
*/
int append_options_valgrind(const char ** argv, int * argc);




/**
   @param Append to @p argv the options necessary to start gdb

   @p argv will be passed to execv(). The variable is preallocated by caller.
   Count of elements in the vector is limited, but for now it's large enough.

   Function increments @p argc for each inserted value.

   @reviewed_on{2024.04.20}

   @param argv Vector of arguments to execv() into which
   @param argc Counter of arguments in @p argv

   @return 0
*/
int append_options_gdb(const char ** argv, int * argc);




#endif /* #ifndef CWDAEMON_TESTS_LIB_SUPERVISOR_H */

