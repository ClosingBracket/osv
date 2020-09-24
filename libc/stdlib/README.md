qsort_r.c - never implemented on musl side, should stay as is

all 3 others:
------------
strtod.c
strtol.c
wcstol.c

"f.lock = -1;" replaced with "f.no_locking = true;" in respect to older musl 0.9.12

Cannot be upgraded to new musl as there have been some locale (?) related changes.
