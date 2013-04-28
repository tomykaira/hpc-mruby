#include "hpcmrb.h"
#include "mruby/array.h"
#include "mruby/data.h"
#include "mruby/string.h"

/* Lattice for abstract intepreration */

enum lat_type { /* Do no change this ordering */
  LAT_UNKNOWN,
  LAT_DYNAMIC,
  LAT_CONST,    /* any non-LAT object is classified as LAT_CONST
                   used for lat_join */
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
#define LAT_HAS_TYPE(mrb, obj, ty) (LAT_P(mrb, obj) && LAT(obj)->type == ty)

#define LAT_TYPE(mrb, obj)  (LAT_P(mrb, obj) ? LAT(obj)->type : LAT_CONST)

static mrb_value lat_unknown;
static mrb_value lat_dynamic;

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

static mrb_value
lat_inspect(mrb_state *mrb, mrb_value lat)
{
  switch (LAT_TYPE(mrb, lat)) {
    case LAT_UNKNOWN:
      return mrb_str_new(mrb, "<unknown>", 9);
    case LAT_DYNAMIC:
      return mrb_str_new(mrb, "<dynamic>", 9);
    case LAT_SET:
      {
        mrb_value buf = mrb_str_buf_new(mrb, 10);
        mrb_value *ary = RARRAY_PTR(LAT(lat)->elems);
        int i, n = RARRAY_LEN(LAT(lat)->elems);
        mrb_str_buf_cat(mrb, buf, "{", 1);
        for (i = 0; i < n; i++) {
          mrb_str_buf_append(mrb, buf, mrb_inspect(mrb, ary[i]));
          if (i != n-1)
            mrb_str_buf_cat(mrb, buf, ", ", 2);
        }
        return mrb_str_buf_cat(mrb, buf, "}", 1);
      }
    /* LAT_CONST is not of Lattice class */
    default:
      break;
  }
  NOT_REACHABLE();
  return mrb_nil_value(); /* to avoid warning */
}

static int
ary_include_lat(mrb_state *mrb, mrb_value ary, mrb_value lat)
{
  int i;
  int len = RARRAY_LEN(ary);
  mrb_value *ptr = RARRAY_PTR(ary);

  for (i = 0; i < len; i++) {
    if (LAT_HAS_TYPE(mrb, ptr[i], LAT_DYNAMIC) || lat_equal(mrb, ptr[i], lat))
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
  return ary_include_lat(mrb, LAT(lat)->elems, val);
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
          if (!ary_include_lat(mrb, elems2, ary1[i]))
            return FALSE;
        }
        return TRUE;
      }
    default:
      NOT_REACHABLE();
  }
}

/* Returns true if lat1 <= lat2 in the lattice of types */
static int
lat_le(mrb_state *mrb, mrb_value lat1, mrb_value lat2)
{
  if (LAT(lat1)->type == LAT_UNKNOWN)
    return TRUE;
  if (LAT(lat2)->type == LAT_DYNAMIC)
    return TRUE;
  if (!LAT_P(mrb, lat2)) {
    if (LAT_P(mrb, lat1))
      return FALSE;
    return mrb_equal(mrb, lat1, lat2);
  }
  if (!LAT_P(mrb, lat1))
    return lat_include(mrb, lat2, lat1);

  /* here, lat1 and lat2 are lattices */
  if (LAT(lat1)->type != LAT(lat2)->type)
    return FALSE;

  /* here, lat1 and lat2 are not unknown and dynamic */
  /* TODO: check instance variables */
  switch (LAT(lat1)->type) {
    case LAT_SET:
      {
        mrb_value elems1 = LAT(lat1)->elems;
        mrb_value elems2 = LAT(lat2)->elems;
        if (RARRAY_LEN(elems1) > RARRAY_LEN(elems2))
          return FALSE;
        mrb_value *ary1 = RARRAY_PTR(elems1);
        mrb_value *ary2 = RARRAY_PTR(elems2);
        int i, len1 = RARRAY_LEN(elems1), len2 = RARRAY_LEN(elems2);
        /* NB: We assume that len is small enough */
        for (i = 0; i < len1; i++) {
          if (!ary_include_lat(mrb, elems2, ary1[i]))
            return FALSE;
        }
        return TRUE;
      }
    default:
      NOT_REACHABLE();
  }
}

/* utilities */

static int
lat_set_add(mrb_state *mrb, mrb_value lat, mrb_value val)
{
  if (!LAT_P(mrb, lat) || LAT(lat)->type != LAT_SET)
    NOT_REACHABLE();
  if (LAT_P(mrb, val) && LAT(val)->type == LAT_SET)
    NOT_REACHABLE();
  if (!LAT_P(mrb, val))
    val = mrb_obj_value(mrb_obj_class(mrb, val));
  if (lat_include(mrb, lat, val))
    return FALSE;

  mrb_ary_push(mrb, LAT(lat)->elems, val);
  return TRUE;
}

/* check lat == {val} */
static int
lat_set_equal1(mrb_state *mrb, mrb_value lat, mrb_value val)
{
  if (!LAT_P(mrb, lat))
    return mrb_equal(mrb, lat, val);
  if (LAT(lat)->type != LAT_SET)
    return FALSE;

  mrb_value elems = LAT(lat)->elems;
  if (RARRAY_LEN(elems) != 1)
    return FALSE;
  return lat_equal(mrb, RARRAY_PTR(elems)[0], val);
}

static int
lat_set_equal2(mrb_state *mrb, mrb_value lat, mrb_value val1, mrb_value val2)
{
  if (!LAT_P(mrb, lat))
    return FALSE;
  if (LAT(lat)->type != LAT_SET)
    return FALSE;

  mrb_value elems = LAT(lat)->elems;
  if (RARRAY_LEN(elems) != 2)
    return FALSE;
  return ary_include_lat(mrb, elems, val1) &&
         ary_include_lat(mrb, elems, val2);
}

static int
lat_set_equal3(mrb_state *mrb, mrb_value lat, mrb_value val1, mrb_value val2,
               mrb_value val3)
{
  if (!LAT_P(mrb, lat))
    return FALSE;
  if (LAT(lat)->type != LAT_SET)
    return FALSE;

  mrb_value elems = LAT(lat)->elems;
  if (RARRAY_LEN(elems) != 3)
    return FALSE;
  return ary_include_lat(mrb, elems, val1) &&
         ary_include_lat(mrb, elems, val2) &&
         ary_include_lat(mrb, elems, val3);
}

static int
lat_set_equal4(mrb_state *mrb, mrb_value lat, mrb_value val1, mrb_value val2,
               mrb_value val3, mrb_value val4)
{
  if (!LAT_P(mrb, lat))
    return FALSE;
  if (LAT(lat)->type != LAT_SET)
    return FALSE;

  mrb_value elems = LAT(lat)->elems;
  if (RARRAY_LEN(elems) != 3)
    return FALSE;
  return ary_include_lat(mrb, elems, val1) &&
         ary_include_lat(mrb, elems, val2) &&
         ary_include_lat(mrb, elems, val3) &&
         ary_include_lat(mrb, elems, val4);
}

static mrb_value
lat_set_new1(mrb_state *mrb, mrb_value val)
{
  mrb_value lat = lat_new(mrb, LAT_SET);
  lat_set_add(mrb, lat, val);
  return lat;
}

static mrb_value
lat_set_new2(mrb_state *mrb, mrb_value val1, mrb_value val2)
{
  mrb_value lat = lat_new(mrb, LAT_SET);
  lat_set_add(mrb, lat, val1);
  lat_set_add(mrb, lat, val2);
  return lat;
}

static mrb_value
lat_set_new3(mrb_state *mrb, mrb_value val1, mrb_value val2, mrb_value val3)
{
  mrb_value lat = lat_new(mrb, LAT_SET);
  lat_set_add(mrb, lat, val1);
  lat_set_add(mrb, lat, val2);
  lat_set_add(mrb, lat, val3);
  return lat;
}

static mrb_value
lat_set_new4(mrb_state *mrb, mrb_value val1, mrb_value val2, mrb_value val3,
             mrb_value val4)
{
  mrb_value lat = lat_new(mrb, LAT_SET);
  lat_set_add(mrb, lat, val1);
  lat_set_add(mrb, lat, val2);
  lat_set_add(mrb, lat, val3);
  lat_set_add(mrb, lat, val4);
  return lat;
}

static mrb_value
lat_clone(mrb_state *mrb, mrb_value lat)
{
  switch (LAT_TYPE(mrb, lat)) {
    case LAT_UNKNOWN:
    case LAT_DYNAMIC:
    case LAT_CONST:
      return lat;
    case LAT_SET:
      {
        mrb_value new_lat = lat_new(mrb, LAT_SET);
        LAT(new_lat)->elems = mrb_obj_clone(mrb, LAT(lat)->elems);
        return new_lat;
      }
    default:
      NOT_REACHABLE();
  }
}

static mrb_value
lat_join(mrb_state *mrb, mrb_value val1, mrb_value val2)
{
  /* TODO: treat instance variables */
  if (mrb_eql(mrb, val1, val2))
    return val1;
  if (LAT_TYPE(mrb, val1) > LAT_TYPE(mrb, val2))
    return lat_join(mrb, val2, val1);
  switch (LAT_TYPE(mrb, val1)) {
    case LAT_UNKNOWN:
      return val2;
    case LAT_DYNAMIC:
      return lat_dynamic;
    case LAT_CONST:
      switch (LAT_TYPE(mrb, val2)) {
        case LAT_CONST:
          return lat_set_new2(mrb, val1, val2);
        case LAT_SET:
          {
            val2 = lat_clone(mrb, val2);
            lat_set_add(mrb, val2, val1);
            return val2;
          }
        default:
          NOT_REACHABLE();
      }
    case LAT_SET:
      switch (LAT_TYPE(mrb, val2)) {
        case LAT_SET:
          {
            val1 = lat_clone(mrb, val1);
            mrb_value *ary = RARRAY_PTR(LAT(val2)->elems);
            int i, n = RARRAY_LEN(LAT(val2)->elems);
            for (i = 0; i < n; i++)
              lat_set_add(mrb, val1, ary[i]);
            return val1;
          }
        default:
          NOT_REACHABLE();
      }
    default:
      NOT_REACHABLE();
  }
}

/* Type analysis */
typedef mrb_ast_node node;
typedef struct mrb_parser_state parser_state;

enum looptype {
  LOOP_NORMAL,
  LOOP_BLOCK,
  LOOP_FOR,
  LOOP_BEGIN,
  LOOP_RESCUE,
} type;

struct loopinfo {
  enum looptype type;
  int pc1, pc2, pc3, acc;
  int ensure_level;
  struct loopinfo *prev;
};

typedef struct scope {
  mrb_state *mrb;
  mrb_pool *mpool;
  jmp_buf jmp;

  struct scope *prev;

  node *lv;
  int sp;

  int lastlabel;
  int ainfo:15;
  mrb_bool mscope:1;

  struct loopinfo *loop;
  int ensure_level;
  char *filename;
  short lineno;

  HIR *hir;

  int nlocals;

  int ai;
} hpc_scope;

static int
node_len(node *tree)
{
  int n = 0;

  while (tree) {
    n++;
    tree = tree->cdr;
  }
  return n;
}

static void
hpc_error(hpc_scope *s, const char *message)
{
  if (!s) return;
  while (s->prev) {
    mrb_pool_close(s->mpool);
    s = s->prev;
  }
  mrb_pool_close(s->mpool);
#ifdef ENABLE_STDIO
  if (s->filename && s->lineno) {
    fprintf(stderr, "hpcmrb error:%s:%d: %s\n", s->filename, s->lineno, message);
  }
  else {
    fprintf(stderr, "hpcmrb error: %s\n", message);
  }
#endif
  longjmp(s->jmp, 1);
}

static hpc_scope*
scope_new(mrb_state *mrb, hpc_scope *prev, node *lv)
{
  static const hpc_scope hpc_scope_zero = { 0 };
  mrb_pool *pool = mrb_pool_open(mrb);
  hpc_scope *p = (hpc_scope *)mrb_pool_alloc(pool, sizeof(hpc_scope));

  if (!p) hpc_error(p, "mrb_pool_alloc failed");
  *p = hpc_scope_zero;
  p->mrb = mrb;
  p->mpool = pool;
  if (!prev) return p;
  p->prev = prev;
  p->ainfo = -1;
  p->mscope = 0;
  p->lv = lv;
  p->sp += node_len(lv) + 1;  /* +1 for self */
  p->nlocals = p->sp;
  p->ai = mrb_gc_arena_save(mrb);

  p->filename = prev->filename;
  p->lineno = prev->lineno;
  return p;
}

static HIR*
compile(mrb_state *mrb, parser_state *p)
{
  puts("compile: NOT IMPLEMENTED YET");
  return 0;
}

HIR*
hpc_compile_file(mrb_state *mrb, FILE *rfp, mrbc_context *c)
{
  parser_state *p = mrb_parse_file(mrb, rfp, c);

  if (!p) return 0;
  if (!p->tree || p->nerr) {
    if (p->capture_errors) {
      char buf[256];

      int n = snprintf(buf, sizeof(buf), "line %d: %s\n",
      p->error_buffer[0].lineno, p->error_buffer[0].message);
      mrb->exc = mrb_obj_ptr(mrb_exc_new(mrb, E_SYNTAX_ERROR, buf, n));
      mrb_parser_free(p);
      return 0;
    }
    else {
      static const char msg[] = "syntax error";
      mrb->exc = mrb_obj_ptr(mrb_exc_new(mrb, E_SYNTAX_ERROR, msg, sizeof(msg) - 1));
      mrb_parser_free(p);
      return 0;
    }
  }
  return compile(mrb, p);
}

void
init_hpc_compiler(mrb_state *mrb)
{
  lat_class = mrb_define_class(mrb, "Lattice", mrb->object_class);
  lat_unknown = lat_new(mrb, LAT_UNKNOWN);
  lat_dynamic = lat_new(mrb, LAT_DYNAMIC);
  mrb_define_method(mrb, lat_class, "inspect", lat_inspect, ARGS_NONE());
}
