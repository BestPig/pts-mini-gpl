#define DUMMY \
  set -ex; \
  gcc -W -Wall -s -O2 -o "${0%.*}" "$0"; exit
/*
 * test_find_duplicate_value.c
 * by pts@fazekas.hu at Sun Jul 12 20:09:30 CEST 2009
 *
 * This program solves the problem:
 *   You're given an array with N+1 cells (the cells are numbered 0 to N). The
 *   array is filled with integers from 0 to N-1 (a total of N integers).
 *   Suggest an algorithm that finds a number that appears twice in the array
 *   (if there are several of them, one of them will do) when you all you can
 *   use is the array and two integer variables. The solution must run in O(N)
 *   (in simple words - you may access each element of the array a constant
 *   number of times).
 *   [it is OK to change the array]
 *
 * Problem and solution pseudocode from
 * http://neworder.box.sk/forum/topic41133-who-appears-twice.html
 */
#include <stdio.h>
#include <stdlib.h>

int find_duplicate_value(int *a, const int n) {
  int i = 0, j, k;
  while (i <= n) {
    j = a[i];
    if (j == i) {
      ++i;
    } else {
      k = a[j];
      if (k < i || k == j) {
        return k;
      }
      a[j] = j;
      a[i] = k;
    }
  }
  abort();  /* should not happen */
}

#define ASIZE 10000
int a[ASIZE];

int main(int argc, char **argv) {
  int n, i;
  (void)argc; (void)argv;
  while (0 < scanf("%d", &n)) {
    if (n < 1 || n >= ASIZE)
      abort();
    for (i = 0; i <= n; ++i) {
      if (0 >= scanf("%d", a + i))
        abort();
      if (a[i] < 0 || a[i] >= n)
        abort();
    }
    printf("%d\n", find_duplicate_value(a, n));
  }
  return 0;
}
