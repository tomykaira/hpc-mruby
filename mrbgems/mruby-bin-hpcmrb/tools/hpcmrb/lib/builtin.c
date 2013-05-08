#include "hpcmrb.h"
#include "mruby.h"
#include "mruby/value.h"
#include "mruby/string.h"
#include "mruby/array.h" /* mrb_ary_ref */
#include <limits.h> /* CHAR_BIT macro */
#include <string.h> /* memcpy */

#define TYPES2(a,b) ((((uint16_t)(a))<<8)|(((uint16_t)(b))&0xff))

mrb_value hpc_str_plus(mrb_value, mrb_value);
extern struct RString* str_new(mrb_state *, const char *, int);/* defined in string.c, but not decleared in string.h (for hpc_str_plus)*/

extern mrb_state *mrb; /* using mrb_state in the driver-main */


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
  case TYPES2(MRB_TT_STRING,MRB_TT_STRING):
    return hpc_str_plus(a, b);
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


/* almost copied from string.c(mrb_str_plus) */
mrb_value
hpc_str_plus(mrb_value a, mrb_value b)
{
  struct RString *s = mrb_str_ptr(a);
  struct RString *s2 = mrb_str_ptr(b);
  struct RString *t;

  t = str_new(mrb, 0, s->len + s2->len);
  memcpy(t->ptr, s->ptr, s->len);
  memcpy(t->ptr + s->len, s2->ptr, s2->len);

  return mrb_obj_value(t);
}

/* almost copied from numeric.c(hpcmrb_fixnum_to_str) */
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
void
hpc_puts(mrb_value __self__, mrb_value n)
{
  mrb_value str;

  switch (mrb_type(n)) {
  case MRB_TT_FIXNUM:
    str = hpc_fixnum_to_str(n, 10);
    break;
  case MRB_TT_STRING:
    str = n;
    break;
  default:
    str = mrb_any_to_s(mrb, n);
  }

  const char *cstr = mrb_str_to_cstr(mrb, str);
  /* calling c puts */
  printf("%s\n", cstr);
}

void
hpc_print(mrb_value __self__, mrb_value n)
{
  mrb_value str;

  switch (mrb_type(n)) {
  case MRB_TT_FIXNUM:
    str = hpc_fixnum_to_str(n, 10);
    break;
  case MRB_TT_STRING:
    str = n;
    break;
  default:
    str = mrb_any_to_s(mrb, n);
  }

  const char *cstr = mrb_str_to_cstr(mrb, str);
  /* calling c puts */
  printf("%s", cstr);
}


mrb_value
hpc_ary_aget(mrb_value __self__, mrb_value index)
{
  if(mrb_type(index) != MRB_TT_FIXNUM){
    return mrb_ary_ref(mrb, __self__, mrb_fixnum(index));
  }
  puts("TYPE_ERROR: expected Fixnum for 1st argument (hpc_ary_aget)");
  return mrb_nil_value(); /* dummy to avoid warning : not reach here */
}

mrb_value
hpc_ary_aset(mrb_value __self__, mrb_value index, mrb_value value)
{

  if(mrb_type(index) != MRB_TT_FIXNUM){
    mrb_ary_set(mrb, __self__, mrb_fixnum(index), value);
    return value;
  }
  puts("TYPE_ERROR: expected Fixnum for 1st argument (hpc_ary_aset)");
  return mrb_nil_value(); /* dummy to avoid warning : not reach here */
}
