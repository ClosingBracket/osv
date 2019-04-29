http://www.informit.com/articles/article.aspx?p=175771&seqNum=3
https://www.gnu.org/software/libc/manual/html_node/Getopt-Long-Option-Example.html

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cassert>
#include <string>

void
test_getopt(int argc, char * argv[], int expected_aflag, int expected_bflag, char *expected_cvalue)
{
  int aflag = 0;
  int bflag = 0;
  char *cvalue = NULL;
  int index;
  int c;

  opterr = 0;

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
  assert(std::string(expected_cvalue) == std::string(cvalue));

  for (index = optind; index < argc; index++)
    printf ("Non-option argument %s\n", argv[index]);
}

int
main (int argc, char *argv[]) {
  char const * const args1[] = {"tst-getopt"};
  test_getopt(1,args1,0,0,nullptr);
  return 0;
}
