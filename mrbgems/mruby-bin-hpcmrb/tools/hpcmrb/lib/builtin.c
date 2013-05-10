#include "hpcmrb.h"
#include "mruby.h"
#include "mruby/value.h"
#include "mruby/string.h"
#include "mruby/array.h" /* mrb_ary_ref */
#include <limits.h> /* CHAR_BIT macro */
#include <string.h> /* memcpy */
#include <math.h>
#include <stdlib.h>

#define TYPES2(a,b) ((((uint16_t)(a))<<8)|(((uint16_t)(b))&0xff))

mrb_value hpc_str_plus(mrb_value, mrb_value);

extern mrb_state *mrb; /* using mrb_state in the driver-main */

mrb_value
num_add_1(mrb_value a, mrb_value b)
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
  case TYPES2(MRB_TT_STRING,MRB_TT_STRING):
    return hpc_str_plus(a, b);
  default:
    NOT_REACHABLE();
  }
}

mrb_value
num_addi_1(mrb_value a, mrb_int b)
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
num_sub_1(mrb_value a, mrb_value b)
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
num_subi_1(mrb_value a, mrb_int b)
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
num_mul_1(mrb_value a, mrb_value b)
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
num_div_1(mrb_value a, mrb_value b)
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
  if (a.value.v1 op b.value.v2) {\
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
    OP_CMP_BODY(op,i,i);\
  case TYPES2(MRB_TT_FIXNUM,MRB_TT_FLOAT):\
    OP_CMP_BODY(op,i,f);\
  case TYPES2(MRB_TT_FLOAT,MRB_TT_FIXNUM):\
    OP_CMP_BODY(op,f,i);\
  case TYPES2(MRB_TT_FLOAT,MRB_TT_FLOAT):\
    OP_CMP_BODY(op,f,f);\
  default:\
    NOT_REACHABLE();\
  }\
} while (0)

mrb_value
num_eq_1(mrb_value a, mrb_value b)
{
  if (mrb_type(a) != MRB_TT_FIXNUM
      && mrb_type(a) != MRB_TT_FLOAT) {
    return mrb_bool_value(mrb_obj_eq(mrb, a, b));
  }
  else {
    OP_CMP(==);
  }
}

mrb_value
num_lt_1(mrb_value a, mrb_value b)
{
  OP_CMP(<);
}

mrb_value
num_le_1(mrb_value a, mrb_value b)
{
  OP_CMP(<=);
}

mrb_value
num_gt_1(mrb_value a, mrb_value b)
{
  OP_CMP(>);
}

mrb_value
num_ge_1(mrb_value a, mrb_value b)
{
  OP_CMP(>=);
}


/* almost copied from string.c(mrb_str_plus) */
mrb_value
hpc_str_plus(mrb_value a, mrb_value b)
{
  struct RString *s = mrb_str_ptr(a);
  struct RString *s2 = mrb_str_ptr(b);


  mrb_value v = mrb_str_new(mrb, 0, s->len + s2->len);
  struct RString *t = mrb_str_ptr(v);

  memcpy(t->ptr, s->ptr, s->len);
  memcpy(t->ptr + s->len, s2->ptr, s2->len);

  return v;
}

/* almost copied from numeric.c(mrb_fixnum_to_str) */
mrb_value
hpc_fixnum_to_str(mrb_value x, int base)
{
  char buf[sizeof(mrb_int)*CHAR_BIT+1];
  char *b = buf + sizeof buf;
  mrb_int val = mrb_fixnum(x);

  if (base < 2 || 36 < base) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid radix %S", mrb_fixnum_value(base));
  }

  if (val == 0) {
    *--b = '0';
  } else if (val < 0) {
    do {
      *--b = mrb_digitmap[-(val % base)];
    } while (val /= base);
    *--b = '-';
  } else {
    do {
      *--b = mrb_digitmap[(int)(val % base)];
    } while (val /= base);
  }

  return mrb_str_new(mrb, b, buf + sizeof(buf) - b);
}

/* value n's type is expected to be <string> or <fixnum> */
mrb_value
puts_1(mrb_value __self__, mrb_value n)
{
  mrb_value str = mrb_funcall(mrb, n, "to_s", 0);
  const char *cstr = mrb_str_to_cstr(mrb, str);
  puts(cstr);
  return mrb_nil_value();
}

mrb_value
print_1(mrb_value __self__, mrb_value n)
{
  if (!mrb_string_p(n))
    n = mrb_funcall(mrb, n, "to_s", 0);
  const char *cstr = RSTRING_PTR(n);
  size_t i, len = RSTRING_LEN(n);
  for (i = 0; i < len; ++i) {
    putchar(cstr[i]);
  }
  return mrb_nil_value();
}

mrb_value
bob_not_0(mrb_value __self__)
{
  return mrb_bool_value(!mrb_test(__self__));
}

mrb_value
hpc_ary_aget_1(mrb_value __self__, mrb_value index)
{
  if(mrb_type(index) == MRB_TT_FIXNUM){
    return mrb_ary_ref(mrb, __self__, mrb_fixnum(index));
  }
  puts("TYPE_ERROR: expected Fixnum for 1st argument (hpc_ary_aget)");
  return mrb_nil_value();
}

mrb_value
hpc_ary_aset_2(mrb_value __self__, mrb_value index, mrb_value value)
{

  if(mrb_type(index) == MRB_TT_FIXNUM){
    mrb_ary_set(mrb, __self__, mrb_fixnum(index), value);
    return value;
  }
  puts("TYPE_ERROR: expected Fixnum for 1st argument (hpc_ary_aset)");
  return mrb_nil_value();
}

#define EVAL_FLOAT(num, exp) do {               \
  mrb_float f;                                  \
  switch (mrb_type(num)) {                      \
  case MRB_TT_FIXNUM:                           \
    f = (mrb_float)mrb_fixnum(num);             \
    break;                                      \
  case MRB_TT_FLOAT:                            \
    f = mrb_float(num);                         \
    break;                                      \
  default:                                      \
    puts("TypeError: can't be coerced into Float"); \
    return mrb_float_value(nan(""));            \
  }                                             \
  return mrb_float_value(exp);                  \
} while (1)

mrb_value
sqrt_1(mrb_value __self__, mrb_value num)
{
  EVAL_FLOAT(num, sqrt(f));
}

mrb_value
cos_1(mrb_value __self__, mrb_value num)
{
  EVAL_FLOAT(num, cos(f));
}

mrb_value
sin_1(mrb_value __self__, mrb_value num)
{
  EVAL_FLOAT(num, sin(f));
}

mrb_value
num_uminus_0(mrb_value num)
{
  switch (mrb_type(num)) {
  case MRB_TT_FIXNUM:
    return mrb_fixnum_value(0 - mrb_fixnum(num));
  case MRB_TT_FLOAT:
    return mrb_float_value(0 - mrb_float(num));
  default:
    return mrb_funcall(mrb, num, "-@", 0);
  }
}

mrb_value
to_i_0(mrb_value num)
{
  switch (mrb_type(num)) {
  case MRB_TT_FIXNUM:
    return num;
  case MRB_TT_FLOAT:
    return mrb_fixnum_value((mrb_int)mrb_float(num));
  default:
    return mrb_funcall(mrb, num, "to_i", 0);
  }
}

mrb_value
to_f_0(mrb_value num)
{
  switch (mrb_type(num)) {
  case MRB_TT_FIXNUM:
    return mrb_float_value((mrb_float)mrb_fixnum(num));
  case MRB_TT_FLOAT:
    return num;
  default:
    return mrb_funcall(mrb, num, "to_i", 0);
  }
}

mrb_value
chr_0(mrb_value num)
{
  switch (mrb_type(num)) {
  case MRB_TT_FIXNUM:
    {
      char c;
      c = (char)mrb_fixnum(num);
      return mrb_str_new(mrb, &c, 1);
    }
  default:
    return mrb_funcall(mrb, num, "chr", 0);
  }
}

mrb_value
to_s_0(mrb_value __self__)
{
  return mrb_funcall(mrb, __self__, "to_s", 0);
}

mrb_value
rand_0(mrb_value __self__)
{
  mrb_float x = rand() / (mrb_float)RAND_MAX;
  return mrb_float_value(x);
}
