#include "hpcmrb.h"
#include "mruby/array.h"
#include "mruby/data.h"

/* Lattice for abstract intepreration */
enum lat_type {
  LAT_UNKNOWN,
  LAT_DYNAMIC,
  LAT_CONSTANT,
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

static struct RClass *lat_class;

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
