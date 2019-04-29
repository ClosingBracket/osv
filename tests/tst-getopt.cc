// http://www.informit.com/articles/article.aspx?p=175771&seqNum=3
// https://www.gnu.org/software/libc/manual/html_node/Getopt-Long-Option-Example.html

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cassert>
#include <cstring>

void
test_getopt(int argc, char * const argv[], int expected_aflag, int expected_bflag, const char *expected_cvalue, const char *expected_non_option_arg)
{
  int aflag = 0;
  int bflag = 0;
  char *cvalue = NULL;
  int index;
  int c;

  opterr = 0;
  optind = 0;

  while ((c = getopt (argc, argv, "abc:")) != -1)
    switch (c)
      {
      case 'a':
        aflag = 1;
        break;
      case 'b':
        bflag = 1;
        break;
      case 'c':
        cvalue = optarg;
        break;
      case '?':
        if (optopt == 'c')
          fprintf (stderr, "Option -%c requires an argument.\n", optopt);
        else if (isprint (optopt))
          fprintf (stderr, "Unknown option `-%c'.\n", optopt);
        else
          fprintf (stderr,
                   "Unknown option character `\\x%x'.\n",
                   optopt);
        assert(0);
      default:
        assert(0);
      }

  assert(expected_aflag == aflag);
  assert(expected_bflag == bflag);
  if (expected_cvalue && cvalue) {
     assert(strcmp(expected_cvalue,cvalue)==0); 
  }
  else {
     assert(!cvalue && !expected_cvalue);
  }

  if (expected_non_option_arg) {
     assert(optind + 1 == argc);
     assert(strcmp(expected_non_option_arg,argv[optind])==0); 
  }
  else {
    assert(optind == argc);
  }
}

int
main (int argc, char *argv[]) {
  char * const test1[] = {(char*)"tst-getopt",nullptr};
  test_getopt(1,test1,0,0,nullptr,nullptr);

  char * const test2[] = {(char*)"tst-getopt",(char*)"-a",(char*)"-b",nullptr};
  test_getopt(3,test2,1,1,nullptr,nullptr);

  char * const test3[] = {(char*)"tst-getopt",(char*)"-ab",nullptr};
  test_getopt(2,test3,1,1,nullptr,nullptr);

  char * const test4[] = {(char*)"tst-getopt",(char*)"-c",(char*)"foo",nullptr};
  test_getopt(3,test4,0,0,"foo",nullptr);

  char * const test5[] = {(char*)"tst-getopt",(char*)"-cfoo",nullptr};
  test_getopt(2,test5,0,0,"foo",nullptr);

  char * const test6[] = {(char*)"tst-getopt",(char*)"arg1",nullptr};
  test_getopt(2,test6,0,0,nullptr,"arg1");

  char * const test7[] = {(char*)"tst-getopt",(char*)"-a",(char*)"arg1",nullptr};
  test_getopt(3,test7,1,0,nullptr,"arg1");

  char * const test8[] = {(char*)"tst-getopt",(char*)"-c",(char*)"foo",(char*)"arg1",nullptr};
  test_getopt(4,test8,0,0,"foo","arg1");

  char * const test9[] = {(char*)"tst-getopt",(char*)"-a",(char*)"--",(char*)"-b",nullptr};
  test_getopt(4,test9,1,0,nullptr,"-b");

  char * const test10[] = {(char*)"tst-getopt",(char*)"-a",(char*)"-",nullptr};
  test_getopt(3,test10,1,0,nullptr,"-");

  return 0;
}
