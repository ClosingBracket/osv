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

#define COPY_VALUE(source,target) if (target) { \
       *target = source; \
} 

int getopt(int argc, char * const argv[], const char *optstring)
{
	int i;
	wchar_t c, d;
	int k, l;
	char *optchar;

	char **optarg2 = nullptr;
        int* optind2 = nullptr, *optopt2 = nullptr, *opterr2 = nullptr;
	auto __runtime = sched::thread::current()->app_runtime();
	if (__runtime) {
		auto obj = __runtime->app.lib();
        	assert(obj);
                optarg2 = reinterpret_cast<char**>(obj->lookup("optarg"));
                optind2 = reinterpret_cast<int*>(obj->lookup("optind"));
                optopt2 = reinterpret_cast<int*>(obj->lookup("optopt"));
                opterr2 = reinterpret_cast<int*>(obj->lookup("opterr"));
        }

	if (opterr2)
	   opterr = *opterr2;
	if (optind2)
	   optind = *optind2;

	if (!optind || __optreset) {
		__optreset = 0;
		__optpos = 0;
		optind = 1;
                COPY_VALUE(optind,optind2)
	}

	if (optind >= argc || !argv[optind] || argv[optind][0] != '-' || !argv[optind][1])
		return -1;
	if (argv[optind][1] == '-' && !argv[optind][2]) {
                optind++;
                COPY_VALUE(optind,optind2)
		return -1;
        }

	if (!optpos) optpos++;
	if ((k = mbtowc(&c, argv[optind]+optpos, MB_LEN_MAX)) < 0) {
		k = 1;
		c = 0xfffd; /* replacement char */
	}
	optchar = argv[optind]+optpos;
	optopt = c;
        COPY_VALUE(optopt,optopt2)
	optpos += k;

	if (!argv[optind][optpos]) {
		optind++;
                COPY_VALUE(optind,optind2)
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
		return '?';
	}

	if (optstring[i+1] == ':') {
		if (optind >= argc) {
			if (optstring[0] == ':') return ':';
			if (opterr) {
				write(2, argv[0], strlen(argv[0]));
				write(2, ": option requires an argument: ", 31);
				write(2, optchar, k);
				write(2, "\n", 1);
			}
			return '?';
		}
		optarg = argv[optind++] + optpos;
                COPY_VALUE(optarg,optarg2)
                COPY_VALUE(optind,optind2)
		optpos = 0;
	}
	return c;
}

weak_alias(getopt, __posix_getopt);
}
