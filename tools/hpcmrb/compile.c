#include "hpcmrb.h"
#include "mruby/array.h"
#include "mruby/data.h"

/* Lattice for abstract intepreration */
enum lat_type {
  LAT_UNKNOWN,
  LAT_DYNAMIC,
  LAT_SET,
};

struct lattice {
  MRB_OBJECT_HEADER;
  enum lat_type type;
  union {
    mrb_value value;
    mrb_value elems;
  };
};

#define LAT(obj)  ((struct lattice*)DATA_PTR(obj))

static struct RClass *lat_class;
#define LAT_P(mrb, obj) (mrb_obj_class(mrb, obj) == lat_class)
#define LAT_CHECK_TYPE(mrb, obj, ty) (LAT_P(mrb, obj) && LAT(obj)->type == ty)

static int lat_equal(mrb_state*, mrb_value, mrb_value);

static void
lat_free(mrb_state *mrb, void *p)
{
  mrb_free(mrb, p);
}

static const struct mrb_data_type lat_data_type = {
  "hpcmrb_lattice", lat_free 
};

static mrb_value
lat_new(mrb_state *mrb, enum lat_type type)
{
  struct lattice *lat;
  struct RData *data;
  Data_Make_Struct(mrb, lat_class, struct lattice, &lat_data_type, lat, data);
  lat->type = type;
  lat->value = mrb_nil_value();
  if (type == LAT_SET)
    lat->elems = mrb_ary_new(mrb);
  return mrb_obj_value(data);
}

static int
ary_include(mrb_state *mrb, mrb_value ary[], int len, mrb_value val)
{
  int i;
  for (i = 0; i < len; i++) {
    if (LAT_CHECK_TYPE(mrb, ary[i], LAT_DYNAMIC) || lat_equal(mrb, ary[i], val))
      return TRUE;
  }
  return FALSE;
}

static int
lat_include(mrb_state *mrb, mrb_value lat, mrb_value val)
{
  if (LAT_P(mrb, lat))
    return mrb_equal(mrb, lat, val);
  if (LAT(lat)->type == LAT_DYNAMIC)
    return TRUE;
  if (LAT(lat)->type != LAT_SET)
    return FALSE;
  return ary_include(mrb,
      RARRAY_PTR(LAT(lat)->elems), 
      RARRAY_LEN(LAT(lat)->elems),
      val);
}

static int
lat_equal(mrb_state *mrb, mrb_value lat1, mrb_value lat2)
{
  if (mrb_obj_eq(mrb, lat1, lat2))
    return TRUE;

  if (LAT_P(mrb, lat1) != LAT_P(mrb, lat2))
      return FALSE;
  if (!LAT_P(mrb, lat1))
      return mrb_equal(mrb, lat1, lat2);
  if (LAT(lat1)->type != LAT(lat2)->type)
    return FALSE;

  /* TODO: check instance variables */
  switch (LAT(lat1)->type) {
    case LAT_UNKNOWN:
    case LAT_DYNAMIC:
      return FALSE;
    case LAT_SET:
      {
        mrb_value elems1 = LAT(lat1)->elems;
        mrb_value elems2 = LAT(lat2)->elems;
        if (RARRAY_LEN(elems1) != RARRAY_LEN(elems2))
          return FALSE;
        mrb_value *ary1 = RARRAY_PTR(elems1);
        mrb_value *ary2 = RARRAY_PTR(elems2);
        int i, len = RARRAY_LEN(elems1);
        for (i = 0; i < len; i++) {
          if (!ary_include(mrb, ary2, len, ary1[i]))
            return FALSE;
        }
        return TRUE;
      }
    default:
      NOT_REACHABLE();
  }
}

HIR*
hpc_compile_file(mrb_state *mrb, FILE *rfp, mrbc_context *c)
{
  puts("hpc_compile_file: NOT IMPLEMENTED YET");
  return (HIR*) 1; /* stub */
}

void
hpcmrb_init(mrb_state *mrb)
{
  lat_class = mrb_define_class(mrb, "Lattice", mrb->object_class);
}
