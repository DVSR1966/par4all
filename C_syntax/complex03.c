#include <stdio.h>
#include <complex.h>

void complex03()
{
  _Complex x1 = 1 + 2I;
  float _Complex x2;
  double _Complex x3;
  long double _Complex x4;

  fprintf(stderr,"sizeof(x1)=%d\n", sizeof(x1));
  fprintf(stderr,"sizeof(x2)=%d\n", sizeof(x2));
  fprintf(stderr,"sizeof(x3)=%d\n", sizeof(x3));
  fprintf(stderr,"sizeof(x4)=%d\n", sizeof(x4));
}

main()
{
  complex03();
}
