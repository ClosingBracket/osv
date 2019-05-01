#include <unistd.h>
#include <wchar.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>
#include <libc/libc.hh>
#include <osv/sched.hh>
#include <osv/app.hh>

extern "C" {

char *optarg;
int optind=1, opterr=1, optopt, __optpos, __optreset=0;

#define optpos __optpos
weak_alias(__optreset, optreset);

#define COPY_GLOBALS_AND_RETURN(retval) { \
	if (other_optarg) { \
		*other_optarg = optarg; \
	} \
	if (other_optind) { \
		*other_optind = optind; \
	}\
	if (other_optopt) { \
		*other_optopt = optopt; \
	}\
	return retval; \
}

// As explained in http://www.shrubbery.net/solaris9ab/SUNWdev/LLM/p22.html#CHAPTER4-84604
// newer versions of gcc produce position independent executables with copies of
// some global variables like those used by getopt() and getopt_long() for optimizations reason.
// In those circumstances the caller of these functions uses different copies of
// global variables (like optind) than the getopt() code that is part of OSv kernel.
// For that reason in the beginning of the function we need to copy values of the caller
// copies of those variables to the kernel placeholders. Likewise on every from the function
// we need to copy the values of kernel copies of global variables to the caller ones.
//
// See http://man7.org/linux/man-pages/man3/getopt.3.html
//
int getopt(int argc, char * const argv[], const char *optstring)
{
	int i;
	wchar_t c, d;
	int k, l;
	char *optchar;

	char **other_optarg = nullptr;
	int* other_optind = nullptr, *other_optopt = nullptr, *other_opterr = nullptr;

	auto __runtime = sched::thread::current()->app_runtime();
	if (__runtime) {
		auto obj = __runtime->app.lib();
		other_optarg = reinterpret_cast<char**>(obj->cached_lookup("optarg"));
		other_optind = reinterpret_cast<int*>(obj->cached_lookup("optind"));
		other_optopt = reinterpret_cast<int*>(obj->cached_lookup("optopt"));
		other_opterr = reinterpret_cast<int*>(obj->cached_lookup("opterr"));
	}

	// Copy values of caller copies
	if (other_opterr) {
		opterr = *other_opterr;
	}
	if (other_optind) {
		optind = *other_optind;
	}

	if (!optind || __optreset) {
		__optreset = 0;
		__optpos = 0;
		optind = 1;
	}

	if (optind >= argc || !argv[optind] || argv[optind][0] != '-' || !argv[optind][1])
		COPY_GLOBALS_AND_RETURN(-1)
	if (argv[optind][1] == '-' && !argv[optind][2]) {
		optind++;
		COPY_GLOBALS_AND_RETURN(-1)
	}

	if (!optpos) optpos++;
	if ((k = mbtowc(&c, argv[optind]+optpos, MB_LEN_MAX)) < 0) {
		k = 1;
		c = 0xfffd; /* replacement char */
	}
	optchar = argv[optind]+optpos;
	optopt = c;
	optpos += k;

	if (!argv[optind][optpos]) {
		optind++;
		optpos = 0;
	}

	for (i=0; (l = mbtowc(&d, optstring+i, MB_LEN_MAX)) && d!=c; i+=l>0?l:1);

	if (d != c) {
		if (optstring[0] != ':' && opterr) {
			write(2, argv[0], strlen(argv[0]));
			write(2, ": illegal option: ", 18);
			write(2, optchar, k);
			write(2, "\n", 1);
		}
		COPY_GLOBALS_AND_RETURN('?')
	}
	if (optstring[i+1] == ':') {
		if (optind >= argc) {
			if (optstring[0] == ':') COPY_GLOBALS_AND_RETURN(':')
			if (opterr) {
				write(2, argv[0], strlen(argv[0]));
				write(2, ": option requires an argument: ", 31);
				write(2, optchar, k);
				write(2, "\n", 1);
			}
			COPY_GLOBALS_AND_RETURN('?')
		}
		optarg = argv[optind++] + optpos;
		optpos = 0;
	}
	COPY_GLOBALS_AND_RETURN(c)
}

weak_alias(getopt, __posix_getopt);
}
