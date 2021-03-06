/*
 * (c) HPC Project - 2010-2011 - All rights reserved
 *
 */

#include "scilab_rt.h"


int __lv0;
int __lv1;
int __lv2;
int __lv3;

/*----------------------------------------------------*/


/*----------------------------------------------------*/

int main(int argc, char* argv[])
{
  scilab_rt_init(argc, argv, COLD_MODE_STANDALONE);

  /*  t82.sce - testing elseif */
  int _u_a = 3;
  int _u_b = 5;
  int _u_c = 4;
  if ((_u_a>_u_b)) {
    if (_u_b) {
      scilab_rt_disp_s0_("a=b");
    }
  } else { 
    if ((_u_a<_u_b)) {
      if ((_u_a<_u_c)) {
        scilab_rt_disp_s0_("a<c<b");
      }
    }
  }

  scilab_rt_terminate();
}

