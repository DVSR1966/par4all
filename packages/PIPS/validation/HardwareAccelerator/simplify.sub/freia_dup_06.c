#include "freia.h"

freia_status
freia_dup_06(freia_data2d * o, freia_data2d * i0, freia_data2d * i1)
{
  freia_data2d * t = freia_common_create_data(16, 128, 128);
  // same operation performed twice, with commutativity
  // first is dead
  // t = i0 + i1
  freia_aipo_add(t, i0, i1);
  // o = i1 + i0
  freia_aipo_add(o, i1, i0);
  freia_common_destruct_data(t);
  return FREIA_OK;
}
