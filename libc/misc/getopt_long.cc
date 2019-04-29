#include <stddef.h>
#include <getopt.h>
#include <stdio.h>
#include <osv/sched.hh>
#include <osv/app.hh>

#define COPY_VALUE(source,target) if (target) { \
       *target = source; \
} 

extern "C" {

extern int __optpos, __optreset;

static int __getopt_long(int argc, char *const *argv, const char *optstring, const struct option *longopts, int *idx, int longonly)
{
	char **optarg2 = nullptr;
        int* optind2 = nullptr;
	auto __runtime = sched::thread::current()->app_runtime();
	if (__runtime) {
		auto obj = __runtime->app.lib();
        	assert(obj);
                optarg2 = reinterpret_cast<char**>(obj->lookup("optarg"));
                optind2 = reinterpret_cast<int*>(obj->lookup("optind"));
        }

	if (!optind || __optreset) {
		__optreset = 0;
		__optpos = 0;
		optind = 1;
                COPY_VALUE(optind,optind2)
	}
	if (optind >= argc || !argv[optind] || argv[optind][0] != '-') return -1;
	if ((longonly && argv[optind][1]) ||
		(argv[optind][1] == '-' && argv[optind][2]))
	{
		int i;
		for (i=0; longopts[i].name; i++) {
			const char *name = longopts[i].name;
			char *opt = argv[optind]+1;
			if (*opt == '-') opt++;
			for (; *name && *name == *opt; name++, opt++);
			if (*name || (*opt && *opt != '=')) continue;
			if (*opt == '=') {
				if (!longopts[i].has_arg) continue;
				optarg = opt+1;
                                COPY_VALUE(optarg,optarg2)
			} else {
				if (longopts[i].has_arg == required_argument) {
					if (!(optarg = argv[++optind])) {
                                                COPY_VALUE(optarg,optarg2)
                                                COPY_VALUE(optind,optind2)
						return ':';
                                        }
				} else {
                                  optarg = NULL;
                                  COPY_VALUE(optarg,optarg2)
                                }
			}
			optind++;
                        COPY_VALUE(optind,optind2)
			if (idx) *idx = i;
			if (longopts[i].flag) {
				*longopts[i].flag = longopts[i].val;
				return 0;
			}
			return longopts[i].val;
		}
		if (argv[optind][1] == '-') {
			optind++;
                        COPY_VALUE(optind,optind2)
			return '?';
		}
	}
	return getopt(argc, argv, optstring);
}

int getopt_long(int argc, char *const *argv, const char *optstring, const struct option *longopts, int *idx)
{
	return __getopt_long(argc, argv, optstring, longopts, idx, 0);
}

int getopt_long_only(int argc, char *const *argv, const char *optstring, const struct option *longopts, int *idx)
{
	return __getopt_long(argc, argv, optstring, longopts, idx, 1);
}
}
