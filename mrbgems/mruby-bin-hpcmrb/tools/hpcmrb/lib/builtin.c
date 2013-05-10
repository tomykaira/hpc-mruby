#include "hpcmrb.h"
#include "lib/builtin.h"
#include "mruby.h"
#include "mruby/value.h"
#include "mruby/string.h"
#include "mruby/array.h" /* mrb_ary_ref, mrb_ary_set */
#include "mruby/numeric.h" /* FIXABLE macro, mrb_flo_to_str */
#include <limits.h> /* CHAR_BIT macro */
#include <string.h> /* memcpy */
#include <math.h>   /* sqrt */
#include <stdlib.h> /* rand */

#define TYPES2(a,b) ((((uint16_t)(a))<<8)|(((uint16_t)(b))&0xff))

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
  if(mrb_obj_eq(NULL, a, b)) {
    return mrb_true_value();
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


/* string.c (mrb_str_plus) */
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


/* dont care __self__ */
mrb_value
puts_1(mrb_value __self__, mrb_value n)
{
  mrb_value str;
  str = to_s_0(n);
  const char *cstr = mrb_str_to_cstr(mrb, str);
  printf("%s\n", cstr);
  return mrb_nil_value();
}

/* dont care __self__ */
mrb_value
print_1(mrb_value __self__, mrb_value n)
{
  mrb_value str;
  str = to_s_0(n);
  const char *cstr = mrb_str_to_cstr(mrb, str);
  printf("%s", cstr);
  return mrb_nil_value();
}

mrb_value
hpc_ary_aget_1(mrb_value __self__, mrb_value index)
{
  if(mrb_type(index) != MRB_TT_FIXNUM){
    return mrb_ary_ref(mrb, __self__, mrb_fixnum(index));
  }
  puts("ERROR: builtin.c - hpc_ary_aget_1");
  return mrb_nil_value();
}

mrb_value
hpc_ary_aset_1(mrb_value __self__, mrb_value index, mrb_value value)
{

  if(mrb_type(index) != MRB_TT_FIXNUM){
    mrb_ary_set(mrb, __self__, mrb_fixnum(index), value);
    return value;
  }
  puts("ERROR: builtin.c - hpc_ary_aset");
  return mrb_nil_value();
}

/* dont care __self__ */
/* n is expected float or fixnum */
mrb_value
sqrt_1(mrb_value __self__, mrb_value n)
{
  mrb_float x;
  switch (mrb_type(n)) {
  case MRB_TT_FIXNUM:
    x = (mrb_float)mrb_fixnum(n);
    x = sqrt(x);
    return mrb_float_value(x);
  case MRB_TT_FLOAT:
    x = mrb_float(n);
    x = sqrt(x);
    return mrb_float_value(x);
  default:
    puts("ERROR: builtin.c - sqrt_1");
    return mrb_nil_value();
  }
}


/* fix_uminus(numeric.c) */
/* n is expected float or fixnum */
mrb_value
num_uminus_0(mrb_value n)
{
  switch (mrb_type(n)) {
  case MRB_TT_FIXNUM:
    return mrb_fixnum_value(0 - mrb_fixnum(n));
  case MRB_TT_FLOAT:
    return mrb_float_value(0 - mrb_float(n));
  default:
    puts("ERROR: builtin.c - num_uminux_0");
    return mrb_nil_value();    
  }
}


/* flo_truncate(numeric.c) */
/* n is expected float fixnum */
mrb_value
to_i_0(mrb_value n)
{
  mrb_float f;
  switch (mrb_type(n)) {
  case MRB_TT_FIXNUM:
    return n;

  case MRB_TT_FLOAT:
    f = mrb_float(n);
    if (f > 0.0) f = floor(f);
    if (f < 0.0) f = ceil(f);
    if (!FIXABLE(f)) { /* check if f is in the range of INT */
      return mrb_float_value(f);
    }
    return mrb_fixnum_value((mrb_int)f);
  default:
    puts("ERROR: builtin.c - to_i_0");
    return mrb_nil_value();    
  }
}

/* fix_to_f (numeric.c) */
/* n is expected float fixnum */
mrb_value
to_f_0(mrb_value n)
{
  mrb_float val;
  switch (mrb_type(n)) {
  case MRB_TT_FIXNUM:
    val = (mrb_float)mrb_fixnum(n);
    return mrb_float_value(val);

  case MRB_TT_FLOAT:
    return n;
  default:
    puts("ERROR: builtin.c - num_uminux_0");
    return mrb_nil_value();    
  }
}


/* numeric_ext.c (mrb_int_chr) */
/* x is expected fixnum */
mrb_value
chr_0(mrb_value x)
{
  if(!mrb_fixnum_p(x)){
    puts("ERROR: builtin.c - chr_0");
    return mrb_nil_value();    
  }
  mrb_int chr;
  char c;

  chr = mrb_fixnum(x);
  if (chr >= (1 << CHAR_BIT)) {
    puts("ERROR: builtin.c - chr_0 - out of char range");
  }
  c = (char)chr;

  return mrb_str_new(mrb, &c, 1);
}

/* dont care __self__ */
mrb_value
rand_0(mrb_value __self__)
{
  mrb_float x = rand() / (mrb_float)RAND_MAX;
  return mrb_float_value(x);
}

/* dont care __self__, n is expected flaot */
mrb_value
cos_1(mrb_value __self__, mrb_value n)
{
  if(!mrb_float_p(n)){
    puts("ERROR: builtin.c - cos_0");
    return mrb_nil_value();    
  }
  mrb_float x = mrb_float(n);
  x = cos(x);
  return mrb_float_value(x);
}

/* dont care __self__, n is expected flaot */
mrb_value
sin_1(mrb_value __self__, mrb_value n)
{
  if(!mrb_float_p(n)){
    puts("ERROR: builtin.c - sin_0");
    return mrb_nil_value();    
  }
  mrb_float x = mrb_float(n);
  x = sin(x);
  return mrb_float_value(x);
}


mrb_value
to_s_0(mrb_value n)
{
  mrb_value str;

  switch (mrb_type(n)) {
  case MRB_TT_FIXNUM:
    str = hpc_fix_to_s(n);
    break;
  case MRB_TT_FLOAT:
    str = hpc_flo_to_s(n);
    break;
  case MRB_TT_STRING:
    str = n;
    break;
  default:
    str = mrb_any_to_s(mrb, n);
  }
  return str;
}

mrb_value
hpc_flo_to_s(mrb_value f)
{
#ifdef MRB_USE_FLOAT
  return mrb_flo_to_str(mrb, f, 7);
#else
  return mrb_flo_to_str(mrb, f, 14);
#endif
}

/* numeric.c(fix_to_s) */
mrb_value
hpc_fix_to_s(mrb_value x)
{
  return mrb_fixnum_to_str(mrb, x, 10);
}
