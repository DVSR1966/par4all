/* Check for synbolic involutive loops. Beyond NSAD2010 and forwards
 * with codes generated by Vivien Maisonneuve.
 *
 * Si on utilise les deux boucles imbriquees avec les tests sur l et
 * k, la postcondition de boucle, j==i+1 est bien obtenue. Je ne sais
 * pas pourquoi.
 *
 * Si on utilise uniquement la boucle sur y, la postcondition de
 * boucle est bien obtenue. Je ne sais pas pourquoi.
 *
 * Enfin, si on utilise les deux boucles sur x et y, la postcondition
 * n'est plus obtenue. Ouf, ca laisse quelque chose de nouveau a
 * trouver:-).
 */

#include <stdio.h>

main()
{
  int i, j = i+1, k, l;
  float x, y;

  while(l>0) {
    while(k>0) {
      j = i+1;
    }
  }

  while(y>0.) {
    j = i+1;
  }

  while(x>0.) {
    while(y>0.) {
      j = i+1;
    }
  }
  printf("%d\n", i);
}