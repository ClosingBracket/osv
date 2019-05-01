#include <stddef.h>
#include <getopt.h>
#include <stdio.h>
#include <osv/sched.hh>
#include <osv/app.hh>

#define COPY_GLOBALS_AND_RETURN(retval) { \
	if (other_optarg) { \
		*other_optarg = optarg; \
	} \
	if (other_optind) { \
		*other_optind = optind; \
	} \
	return retval; \
}

extern "C" {

extern int __optpos, __optreset;

static int __getopt_long(int argc, char *const *argv, const char *optstring, const struct option *longopts, int *idx, int longonly)
{
	char **other_optarg = nullptr;
	int* other_optind = nullptr;

	auto __runtime = sched::thread::current()->app_runtime();
	if (__runtime) {
		auto obj = __runtime->app.lib();
		other_optarg = reinterpret_cast<char**>(obj->cached_lookup("optarg"));
		other_optind = reinterpret_cast<int*>(obj->cached_lookup("optind"));
	}

	// Copy value of caller copy
	if (other_optind) {
		optind = *other_optind;
	}

	if (!optind || __optreset) {
		__optreset = 0;
		__optpos = 0;
		optind = 1;
	}
	if (optind >= argc || !argv[optind] || argv[optind][0] != '-') COPY_GLOBALS_AND_RETURN(-1)
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
			} else {
				if (longopts[i].has_arg == required_argument) {
					if (!(optarg = argv[++optind]))
						COPY_GLOBALS_AND_RETURN(':')
				} else optarg = NULL;
			}
			optind++;
			if (idx) *idx = i;
			if (longopts[i].flag) {
				*longopts[i].flag = longopts[i].val;
				COPY_GLOBALS_AND_RETURN(0)
			}
			COPY_GLOBALS_AND_RETURN(longopts[i].val)
		}
		if (argv[optind][1] == '-') {
			optind++;
			COPY_GLOBALS_AND_RETURN('?')
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
