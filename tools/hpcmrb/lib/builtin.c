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

mrb_value
num_addi(mrb_value a, mrb_int b)
{
  switch (mrb_type(a)) {
  case MRB_TT_FIXNUM:
    {
      mrb_int x, y, z;
      x = mrb_fixnum(a);
      y = b;
      z = x + y;
      if ((x < 0) != (z < 0) && ((x < 0) ^ (y < 0)) == 0) {
        /* integer overflow */
        return mrb_float_value((mrb_float)x + (mrb_float)y);
      }
      return mrb_fixnum_value(z);
    }
  case MRB_TT_FLOAT:
    return mrb_float_value(mrb_float(a) + (mrb_float)b);
  default:
    NOT_REACHABLE();
  }
}

mrb_value
num_sub(mrb_value a, mrb_value b)
{
  switch (TYPES2(mrb_type(a), mrb_type(b))) {
  case TYPES2(MRB_TT_FIXNUM,MRB_TT_FIXNUM):
    {
      mrb_int x, y, z;
      x = mrb_fixnum(a);
      y = mrb_fixnum(b);
      z = x - y;
      if (((x < 0) ^ (y < 0)) != 0 && (x < 0) != (z < 0)) {
        /* integer overflow */
        return mrb_float_value((mrb_float)x - (mrb_float)y);
      }
      return mrb_fixnum_value(z);
    }
  case TYPES2(MRB_TT_FIXNUM,MRB_TT_FLOAT):
    return mrb_float_value((mrb_float)mrb_fixnum(a) - mrb_float(b));
  case TYPES2(MRB_TT_FLOAT,MRB_TT_FIXNUM):
    return mrb_float_value(mrb_float(a) - (mrb_float)mrb_fixnum(b));
  case TYPES2(MRB_TT_FLOAT,MRB_TT_FLOAT):
    return mrb_float_value(mrb_float(a) - mrb_float(b));
  default:
    NOT_REACHABLE();
  }
}

mrb_value
num_subi(mrb_value a, mrb_int b)
{
  switch (mrb_type(a)) {
  case MRB_TT_FIXNUM:
    {
      mrb_int x, y, z;
      x = mrb_fixnum(a);
      y = b;
      z = x - y;
      if (((x < 0) ^ (y < 0)) != 0 && (x < 0) != (z < 0)) {      
        /* integer overflow */
        return mrb_float_value((mrb_float)x - (mrb_float)y);
      }
      return mrb_fixnum_value(z);
    }
  case MRB_TT_FLOAT:
    return mrb_float_value(mrb_float(a) - (mrb_float)b);
  default:
    NOT_REACHABLE();
  }
}

mrb_value
num_mul(mrb_value a, mrb_value b)
{
  switch (TYPES2(mrb_type(a), mrb_type(b))) {
  case TYPES2(MRB_TT_FIXNUM,MRB_TT_FIXNUM):
    {
      mrb_int x, y, z;
      x = mrb_fixnum(a);
      y = mrb_fixnum(b);
      z = x * y;
      if (x != 0 && z/x != y) {
        /* integer overflow */
        return mrb_float_value((mrb_float)x * (mrb_float)y);
      }
      return mrb_fixnum_value(z);
    }
  case TYPES2(MRB_TT_FIXNUM,MRB_TT_FLOAT):
    return mrb_float_value((mrb_float)mrb_fixnum(a) * mrb_float(b));
  case TYPES2(MRB_TT_FLOAT,MRB_TT_FIXNUM):
    return mrb_float_value(mrb_float(a) * (mrb_float)mrb_fixnum(b));
  case TYPES2(MRB_TT_FLOAT,MRB_TT_FLOAT):
    return mrb_float_value(mrb_float(a) * mrb_float(b));
  default:
    NOT_REACHABLE();
  }
}

mrb_value
num_div(mrb_value a, mrb_value b)
{
  switch (TYPES2(mrb_type(a), mrb_type(b))) {
  case TYPES2(MRB_TT_FIXNUM,MRB_TT_FIXNUM):
    {
      mrb_int x, y;
      x = mrb_fixnum(a);
      y = mrb_fixnum(b);
      return mrb_float_value((mrb_float)x / (mrb_float)y);
    }
  case TYPES2(MRB_TT_FIXNUM,MRB_TT_FLOAT):
    return mrb_float_value((mrb_float)mrb_fixnum(a) / mrb_float(b));
  case TYPES2(MRB_TT_FLOAT,MRB_TT_FIXNUM):
    return mrb_float_value(mrb_float(a) / (mrb_float)mrb_fixnum(b));
  case TYPES2(MRB_TT_FLOAT,MRB_TT_FLOAT):
    return mrb_float_value(mrb_float(a) / mrb_float(b));
  default:
    NOT_REACHABLE();
  }
}

#define OP_CMP_BODY(op,v1,v2) do {\
  if (a.v1 op b.v2) {\
    return mrb_true_value();\
  }\
  else {\
    return mrb_false_value();\
  }\
} while(0)

#define OP_CMP(op) do {\
  /* need to check if - is overridden */\
  switch (TYPES2(mrb_type(a),mrb_type(b))) {\
  case TYPES2(MRB_TT_FIXNUM,MRB_TT_FIXNUM):\
    OP_CMP_BODY(op,attr_i,attr_i);\
  case TYPES2(MRB_TT_FIXNUM,MRB_TT_FLOAT):\
    OP_CMP_BODY(op,attr_i,attr_f);\
  case TYPES2(MRB_TT_FLOAT,MRB_TT_FIXNUM):\
    OP_CMP_BODY(op,attr_f,attr_i);\
  case TYPES2(MRB_TT_FLOAT,MRB_TT_FLOAT):\
    OP_CMP_BODY(op,attr_f,attr_f);\
  default:\
    NOT_REACHABLE();\
  }\
} while (0)

mrb_value
num_eq(mrb_value a, mrb_value b)
{
  if(mrb_obj_eq(NULL, a, b)) {
    return mrb_true_value();
  }
  else {
    OP_CMP(==);
  }
}

mrb_value
num_lt(mrb_value a, mrb_value b)
{
  OP_CMP(<);
}

mrb_value
num_le(mrb_value a, mrb_value b)
{
  OP_CMP(<=);
}

mrb_value
num_gt(mrb_value a, mrb_value b)
{
  OP_CMP(>);
}

mrb_value
num_ge(mrb_value a, mrb_value b)
{
  OP_CMP(>=);
}
