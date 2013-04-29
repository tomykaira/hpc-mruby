#include "hpcmrb.h"
#include "mruby.h"
#include "mruby/value.h"

#define TYPES2(a,b) ((((uint16_t)(a))<<8)|(((uint16_t)(b))&0xff))

mrb_value
num_add(mrb_value a, mrb_value b)
{
  switch (TYPES2(mrb_type(a), mrb_type(b))) {
  case TYPES2(MRB_TT_FIXNUM,MRB_TT_FIXNUM):
    {
      mrb_int x, y, z;
      x = mrb_fixnum(a);
      y = mrb_fixnum(b);
      z = x + y;
      if ((x < 0) != (z < 0) && ((x < 0) ^ (y < 0)) == 0) {
        /* integer overflow */
        return mrb_float_value((mrb_float)x + (mrb_float)y);
      }
      return mrb_fixnum_value(z);
    }
  case TYPES2(MRB_TT_FIXNUM,MRB_TT_FLOAT):
    return mrb_float_value((mrb_float)mrb_fixnum(a) + mrb_float(b));
  case TYPES2(MRB_TT_FLOAT,MRB_TT_FIXNUM):
    return mrb_float_value(mrb_float(a) + (mrb_float)mrb_fixnum(b));
  case TYPES2(MRB_TT_FLOAT,MRB_TT_FLOAT):
    return mrb_float_value(mrb_float(a) + mrb_float(b));
  default:
    NOT_REACHABLE();
  }
}
