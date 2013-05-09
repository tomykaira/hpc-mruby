#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "hpcmrb.h"
#include "mruby/array.h"
#include "mruby/class.h"
#include "mruby/data.h"
#include "mruby/proc.h"
#include "mruby/string.h"
#include "mruby/variable.h"
#include "node.h"

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
    /* LAT_CONST is not of Lattice class, but for pretty printing */
    case LAT_CONST:
      return mrb_any_to_s(mrb, lat);
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
    if (lat_equal(mrb, ptr[i], lat))
      return TRUE;
  }
  return FALSE;
}

static int
lat_include(mrb_state *mrb, mrb_value lat, mrb_value val)
{
  switch (LAT_TYPE(mrb, lat)) {
    case LAT_UNKNOWN:
      return FALSE;
    case LAT_SET:
      return ary_include_lat(mrb, LAT(lat)->elems, val);
    case LAT_CONST:
      return mrb_equal(mrb, lat, val);
    case LAT_DYNAMIC:
      return TRUE;
    default:
      NOT_REACHABLE();
  }
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

  hpc_assert(LAT_HAS_TYPE(mrb, lat1, LAT_SET));
  hpc_assert(LAT_HAS_TYPE(mrb, lat2, LAT_SET));

  /* TODO: check instance variables */
  {
    mrb_value elems1 = LAT(lat1)->elems;
    mrb_value elems2 = LAT(lat2)->elems;
    if (RARRAY_LEN(elems1) != RARRAY_LEN(elems2))
      return FALSE;
    mrb_value *ary1 = RARRAY_PTR(elems1);
    int i, len = RARRAY_LEN(elems1);
    for (i = 0; i < len; i++) {
      if (!ary_include_lat(mrb, elems2, ary1[i]))
        return FALSE;
    }
    return TRUE;
  }
}

/* Returns true if lat1 <= lat2 in the lattice of types */
static int
lat_le(mrb_state *mrb, mrb_value lat1, mrb_value lat2)
{
  if (LAT_HAS_TYPE(mrb, lat1, LAT_UNKNOWN))
    return TRUE;
  if (LAT_HAS_TYPE(mrb, lat2, LAT_DYNAMIC))
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
        /* elems2 contains LAT_DYNAMIC */
        if (RARRAY_LEN(elems1) > RARRAY_LEN(elems2))
          return FALSE;
        mrb_value *ary1 = RARRAY_PTR(elems1);
        int i, len1 = RARRAY_LEN(elems1);
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

/*
  const_lat is a Class object
  Call this directly when a mruby object is known to be a class
  (e.g. an element of another LAT_SET)
*/
static int
lat_set_add_const(mrb_state *mrb, mrb_value lat, mrb_value klass)
{
  hpc_assert(LAT_HAS_TYPE(mrb, lat, LAT_SET));
  hpc_assert(!LAT_P(mrb, klass) && mrb_obj_class(mrb, klass) == mrb->class_class);

  if (lat_include(mrb, lat, klass))
    return FALSE;

  mrb_ary_push(mrb, LAT(lat)->elems, klass);
  return TRUE;
}

/*
  val is a constant value occurred in the target code
*/
static int
lat_set_add(mrb_state *mrb, mrb_value lat, mrb_value val)
{
  return lat_set_add_const(mrb, lat, mrb_obj_value(mrb_obj_class(mrb, val)));
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
  if (!LAT_HAS_TYPE(mrb, lat, LAT_SET))
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
  if (!LAT_HAS_TYPE(mrb, lat, LAT_SET))
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
  if (!LAT_HAS_TYPE(mrb, lat, LAT_SET))
    return FALSE;

  mrb_value elems = LAT(lat)->elems;
  if (RARRAY_LEN(elems) != 4)
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
lat_merge_set(mrb_state *mrb, mrb_value set1, mrb_value set2)
{
  mrb_value *ary = RARRAY_PTR(LAT(set2)->elems);
  int i, len = RARRAY_LEN(LAT(set2)->elems);

  hpc_assert(LAT_HAS_TYPE(mrb, set1, LAT_SET));
  hpc_assert(LAT_HAS_TYPE(mrb, set2, LAT_SET));

  set1 = lat_clone(mrb, set1);
  for (i = 0; i < len; i++)
    lat_set_add_const(mrb, set1, ary[i]);
  return set1;
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
      return lat_merge_set(mrb, val1, val2);
    default:
      NOT_REACHABLE();
  }
}

/* Compiler main */

typedef mrb_ast_node node;
typedef struct mrb_parser_state parser_state;
void parser_dump(mrb_state *mrb, node *tree, int offset);

#define sym(x) ((mrb_sym)(intptr_t)(x))
#define hirsym(x) ((HIR*)(intptr_t)(x))

static void*
compiler_palloc(hpc_state *p, size_t size)
{
  void *m = mrb_pool_alloc(p->pool, size);
  if (!m) longjmp(p->jmp, 1);
  return m;
}

static void
cons_free_gen(hpc_state *p, HIR *cons)
{
  cons->cdr = p->cells;
  p->cells = cons;
}
#define cons_free(c) cons_free_gen(p, (c))

static HIR*
cons_gen(hpc_state *p, HIR *car, HIR *cdr)
{
  HIR *c;
  if (p->cells) {
    c = p->cells;
    p->cells = p->cells->cdr;
  }
  else {
    c = (HIR *)compiler_palloc(p, sizeof(HIR));
  }

  c->car = car;
  c->cdr = cdr;
  c->lineno = 0;
  c->lat = lat_unknown;
  return c;
}
#define cons(a,b) cons_gen(p, (a), (b))
#define list1(a)          cons((a), 0)
#define list2(a,b)        cons((a), cons((b), 0))
#define list3(a,b,c)      cons((a), cons((b), cons((c), 0)))
#define list4(a,b,c,d)    cons((a), cons((b), cons((c), cons((d), 0))))
#define list5(a,b,c,d,e)  cons((a), cons((b), cons((c), cons((d), cons((e), 0)))))

void dump_varlist(mrb_state *mrb, HIR *list);

int
length(HIR *list)
{
  int c = 0;
  while (list) {
    c++;
    list = list->cdr;
  }
  return c;
}

static HIR*
append(hpc_state *p, HIR *list1, HIR *list2)
{
  HIR *orig_list1 = list1;
  if (!list1)
    return list2;

  while (list1->cdr)
    list1 = list1->cdr;
  list1->cdr = list2;
  return orig_list1;
}

static mrb_sym
attrsym(hpc_state *p, mrb_sym a)
{
  const char *name;
  size_t len;
  char *name2;

  name = mrb_sym2name_len(p->mrb, a, &len);
  name2 = (char *)compiler_palloc(p, len+1);
  memcpy(name2, name, len);
  name2[len] = '=';
  name2[len+1] = '\0';

  return mrb_intern2(p->mrb, name2, len+1);
}

hpc_state*
hpc_state_new(mrb_state *mrb)
{
  mrb_pool *pool;
  hpc_state *p;
  static const hpc_state hpc_state_zero = { 0 };

  pool = mrb_pool_open(mrb);
  if (!pool) return 0;
  p = (hpc_state*)mrb_pool_alloc(pool, sizeof(hpc_state));
  if (!p) return 0;

  *p = hpc_state_zero;
  p->mrb = mrb;
  p->pool = pool;
  return p;
}

static HIR*
new_lvar(hpc_state *p, mrb_sym sym, mrb_value lat)
{
  HIR *var = cons((HIR*)HIR_LVAR, hirsym(sym));
  var->lat = lat;
  return var;
}

static HIR*
new_ivar(hpc_state *p, mrb_sym sym)
{
  HIR *var = cons((HIR*)HIR_IVAR, hirsym(sym));
  var->lat = lat_unknown;
  return var;
}

static HIR*
dup_lvar(hpc_state *p, HIR *lvar)
{
  HIR *hir = cons(lvar->car, lvar->cdr);
  hir->lat = lat_clone(p->mrb, lvar->lat);
  return hir;
}

static HIR*
new_gvar(hpc_state *p, mrb_sym sym, mrb_value lat)
{
  HIR *var = cons((HIR*)HIR_GVAR, hirsym(sym));
  var->lat = lat;
  return var;
}

static HIR*
new_lvardecl(hpc_state *p, HIR *type, mrb_sym sym, HIR *val)
{
  return list4((HIR*)HIR_LVARDECL, type, hirsym(sym), val);
}

static HIR*
new_gvardecl(hpc_state *p, HIR *type, mrb_sym sym, HIR *val)
{
  return list4((HIR*)HIR_GVARDECL, type, hirsym(sym), val);
}

static HIR*
new_pvardecl(hpc_state *p, HIR *type, mrb_sym sym)
{
  return list3((HIR*)HIR_PVARDECL, type, hirsym(sym));
}

static HIR*
new_fundecl(hpc_state *p, mrb_sym class_name, mrb_sym sym, HIR *type, HIR *params, HIR *body)
{
  return list5((HIR*)HIR_FUNDECL, type, cons(hirsym(class_name), hirsym(sym)), params, body);
}

static HIR*
new_scope(hpc_state *p, HIR *lv, HIR *body)
{
  return cons((HIR*)HIR_SCOPE, cons(lv, body));
}

static HIR*
new_block(hpc_state *p, HIR *stmts)
{
  return list2((HIR*)HIR_BLOCK, stmts);
}

static HIR*
new_empty(hpc_state *p)
{
  HIR *hir = list1((HIR*)HIR_EMPTY);
  hir->lat = mrb_nil_value();
  return hir;
}

static HIR*
new_int(hpc_state *p, char *text, int base, mrb_int val)
{
  HIR *lit = list3((HIR*)HIR_INT, (HIR*)text, (HIR*)(intptr_t)base);
  lit->lat = mrb_fixnum_value(val);
  return lit;
}

static HIR*
new_int_const(hpc_state *p, mrb_int val)
{
  char *int_lit = (char *)compiler_palloc(p, 32);
  HIR *lit;
  sprintf(int_lit, "%d", val);
  lit = list3((HIR*)HIR_INT, (HIR*)int_lit, (HIR*)10);
  lit->lat = mrb_fixnum_value(val);
  return lit;
}

static HIR*
new_float(hpc_state *p, char *text, int base, double val)
{
  HIR *lit = list3((HIR*)HIR_FLOAT, (HIR*)text, (HIR*)(intptr_t)base);
  lit->lat = mrb_float_value(val);
  return lit;
}

static HIR*
new_ifelse(hpc_state *p, HIR* cond, HIR* ifthen, HIR* ifelse)
{

  HIR *hir = list4((HIR*)HIR_IFELSE, cond, ifthen, ifelse);
  if (ifthen && ifelse) {
    hir->lat = lat_join(p->mrb, ifthen->lat, ifelse->lat);
  } else if (ifthen) {
    hir->lat = lat_join(p->mrb, ifthen->lat, mrb_nil_value());
  } else if (ifelse) {
    hir->lat = lat_join(p->mrb, ifelse->lat, mrb_nil_value());
  } else {
    NOT_REACHABLE();
  }
  return hir;
}

static HIR*
new_return_value(hpc_state *p, HIR *exp)
{
  HIR *hir = list2((HIR*)HIR_RETURN, exp);
  hir->lat = exp->lat;
  return hir;
}

static HIR*
new_return_void(hpc_state *p)
{
  HIR *hir = list1((HIR*)HIR_RETURN);
  hir->lat = mrb_nil_value();
  return hir;
}

static HIR*
new_assign(hpc_state *p, HIR *lhs, HIR *rhs)
{
  HIR *hir = list3((HIR*)HIR_ASSIGN, lhs, rhs);
  mrb_p(p->mrb, lhs->lat);
  lhs->lat = lat_join(p->mrb, lhs->lat, rhs->lat);
  hir->lat = rhs->lat;
  return hir;
}

static HIR*
new_str(hpc_state *p, char *str, int length)
{
  HIR *hir = list3((HIR*)HIR_STRING, (HIR*)str, (HIR*)(intptr_t)length);
  hir->lat = mrb_str_new(p->mrb, str, length);
  return hir;
}

static HIR*
new_cond_op(hpc_state *p, HIR *cond, HIR *t, HIR *f)
{
  HIR *hir = list4((HIR*)HIR_COND_OP, cond, t, f);
  hir->lat = lat_join(p->mrb, t->lat, f->lat);
  return hir;
}

static HIR*
new_prim(hpc_state *p, enum hir_primitive_type t)
{
  HIR *hir = cons((HIR*)HIR_PRIM, (HIR*)t);
  switch (t) {
  case HPTYPE_NIL:
    hir->lat = mrb_nil_value();
    break;
  case HPTYPE_FALSE:
    hir->lat = mrb_false_value();
    break;
  case HPTYPE_TRUE:
    hir->lat = mrb_true_value();
    break;
  }
  return hir;
}

/* FIXME: super is not supported now */
static HIR*
new_defclass(hpc_state *p, mrb_sym name, mrb_sym super)
{
  HIR *hir = list3((HIR*)HIR_DEFCLASS, hirsym(name), hirsym(super));
  hir->lat = mrb_voidp_value(p->mrb->class_class);
  return hir;
}

static HIR*
new_simple_type(hpc_state *p, enum hir_type_kind kind)
{
  return list1((HIR*)kind);
}

static HIR*
new_ptr_type(hpc_state *p, HIR *base)
{
  return list2((HIR*)HTYPE_PTR, base);
}

static HIR*
new_func_type(hpc_state *p, HIR *ret, HIR *params)
{
  return list3((HIR*)HTYPE_FUNC, ret, params);
}

static HIR*
new_typedef_type(hpc_state *p, const char *name)
{
  return list2((HIR*)HTYPE_TYPEDEF, (HIR*)name);
}

static HIR *void_type;
static HIR *value_type;
static HIR *sym_type;
static HIR *char_type;
static HIR *int_type;
static HIR *float_type;
static HIR *string_type;
static HIR *mrb_state_ptr_type;

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
  hpc_state *hpc;
  mrb_pool *mpool;
  jmp_buf jmp;

  struct scope *prev;

  HIR *defs;
  int inherit_defs;
  HIR *lv;
  HIR *iv;
  mrb_sym class_name;
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

  HIR *current_self;
} hpc_scope;

static int
hir_len(HIR *tree)
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

static double
readint_float(hpc_scope *s, const char *p, int base)
{
  const char *e = p + strlen(p);
  double f = 0;
  int n;

  if (*p == '+') p++;
  while (p < e) {
    char c = *p;
    c = tolower((unsigned char)c);
    for (n=0; n<base; n++) {
      if (mrb_digitmap[n] == c) {
        f *= base;
        f += n;
        break;
      }
    }
    if (n == base) {
      hpc_error(s, "malformed readint input");
    }
    p++;
  }
  return f;
}

static mrb_int
readint_mrb_int(hpc_scope *s, const char *p, int base, int neg, int *overflow)
{
  const char *e = p + strlen(p);
  mrb_int result = 0;
  int n;

  if (*p == '+') p++;
  while (p < e) {
    char c = *p;
    c = tolower((unsigned char)c);
    for (n=0; n<base; n++) {
      if (mrb_digitmap[n] == c) {
        break;
      }
    }
    if (n == base) {
      hpc_error(s, "malformed readint input");
    }

    if (neg) {
      if ((MRB_INT_MIN + n)/base > result) {
        *overflow = TRUE;
        return 0;
      }
      result *= base;
      result -= n;
    }
    else {
      if ((MRB_INT_MAX - n)/base < result) {
        *overflow = TRUE;
        return 0;
      }
      result *= base;
      result += n;
    }
    p++;
  }
  *overflow = FALSE;
  return result;
}

static hpc_scope*
scope_new(hpc_state *p, hpc_scope *prev, HIR *lv, int inherit_defs, int inherit_lv)
{
  static const hpc_scope hpc_scope_zero = { 0 };
  mrb_pool *pool = mrb_pool_open(p->mrb);
  hpc_scope *s = (hpc_scope *)mrb_pool_alloc(pool, sizeof(hpc_scope));

  if (!s) hpc_error(prev, "mrb_pool_alloc failed");
  *s = hpc_scope_zero;
  s->hpc = p;
  s->mrb = p->mrb;
  s->mpool = pool;

  s->current_self = new_lvar(p, mrb_intern_cstr(p->mrb, "__self__"),
      mrb_top_self(p->mrb));

  if (!prev) return s;

  if (inherit_defs) {
    s->defs = prev->defs;
    s->iv   = prev->iv;
    s->class_name = prev->class_name;
  }
  if (inherit_lv) {
    /* append prev->lv after lv, not to destroy prev->lv */
    s->lv = append(p, lv, prev->lv);
  } else {
    s->lv = lv;
  }
  s->inherit_defs = inherit_defs;

  s->prev = prev;
  s->ainfo = -1;
  s->mscope = 0;
  s->sp += hir_len(lv) + 1;  /* +1 for self */
  s->nlocals = s->sp;
  s->ai = mrb_gc_arena_save(p->mrb);

  s->filename = prev->filename;
  s->lineno = prev->lineno;

  return s;
}

static void
scope_finish(hpc_scope *s)
{
  mrb_state *mrb = s->mrb;
  if (s->inherit_defs) {
    s->prev->defs = s->defs;
    s->prev->iv   = s->iv;
  }
  mrb_gc_arena_restore(mrb, s->ai);
  mrb_pool_close(s->mpool);
}

static hpc_scope*
scope_dup(hpc_scope *s)
{
  hpc_state *p = s->hpc;
  mrb_pool *pool = mrb_pool_open(p->mrb);
  hpc_scope *s2 = (hpc_scope *)mrb_pool_alloc(pool, sizeof(hpc_scope));
  if (!s2) hpc_error(s, "mrb_pool_alloc failed");

  memcpy(s2, s, sizeof(hpc_scope));

  s2->mpool = pool;

  /* TODO: clone state of global and instance variables */

  HIR *new_lv = 0, *last = 0, *lv = s->lv;
  while (lv) {
    if (!new_lv)
      new_lv = last = cons(dup_lvar(p, lv->car), 0);
    else {
      last->cdr = cons(dup_lvar(p, lv->car), 0);
      last = last->cdr;
    }
    lv = lv->cdr;
  }

  s2->lv = new_lv;
  s2->ai = mrb_gc_arena_save(p->mrb);
  return s2;
}

static void
join_scope(hpc_scope *s1, hpc_scope *s2)
{
  HIR *lv1;
  HIR *lv2;

  /* TODO: merge global and instance variables */

  lv1 = s1->lv;
  lv2 = s2->lv;

  while (lv1) {
    mrb_value lat1, lat2;
    int unknown1, unknown2;
    lat1 = lv1->car->lat;
    lat2 = lv2->car->lat;

    unknown1 = LAT_HAS_TYPE(s1->mrb, lat1, LAT_UNKNOWN);
    unknown2 = LAT_HAS_TYPE(s2->mrb, lat2, LAT_UNKNOWN);

    if (unknown1 != unknown2) {
      if (unknown1)
        lat1 = mrb_nil_value();
      else
        lat2 = mrb_nil_value();
    }

    lv1->car->lat = lat_join(s1->mrb, lat1, lat2);
    lv1 = lv1->cdr;
    lv2 = lv2->cdr;
  }
  scope_finish(s2);
}

static struct RProc*
search_abst_interp(mrb_state *mrb, struct RClass *klass, mrb_sym mid)
{
  struct RProc *m;
  m = mrb_method_search_vm(mrb, &klass, mid);
  if (!m) {
    /* TODO: emulate method_missing */
    NOT_IMPLEMENTED();
  }
  if (MRB_PROC_CFUNC_P(m)) {
    m = get_interp(m);
    if (!m)
      mrb_raisef(mrb, E_NOTIMP_ERROR, "Abstract interpreter for %S",
          mrb_symbol_value(mid));
    return m;
  } else {
    NOT_IMPLEMENTED();
  }
}

static HIR *typing(hpc_scope *s, node *tree);

static HIR*
infer_type(hpc_state *p, mrb_value lat)
{
  /* TODO: infer type from lattice */
  puts("Infer type:");
  mrb_p(p->mrb, lat);
  return value_type;
}

static HIR*
lvs_to_decls(hpc_state *p, HIR *lvs)
{
  HIR *lv_decls = 0;

  while (lvs) {
    mrb_sym sym = sym(lvs->car->cdr);
    lv_decls = cons(new_lvardecl(p, infer_type(p, lvs->car->lat), sym, new_empty(p)), lv_decls);
    lvs = lvs->cdr;
  }

  return lv_decls;
}

/*
  list: list of HIR_GVAR or HIR_LVAR
  return: 0 if not found
 */
static HIR*
find_var_list(hpc_state *p, HIR *list, mrb_sym needle)
{
  while (list) {
    if (sym(list->car->cdr) == needle) {
      return list->car;
    }
    list = list->cdr;
  }
  return 0;
}

static HIR*
lookup_lvar(hpc_scope *s, mrb_sym sym)
{
  HIR* var = find_var_list(s->hpc, s->lv, sym);
  if (!var)
    NOT_REACHABLE();
  return var;
}

static HIR*
lookup_ivar(hpc_scope *s, mrb_sym sym)
{
  hpc_state *p = s->hpc;
  HIR* var = find_var_list(p, s->iv, sym);
  if (!var) {
    var = new_ivar(p, sym);
    s->iv = cons(var, s->iv);
  }
  return var;
}

static void
register_class_definition(hpc_state *p, mrb_sym name, node *tree)
{
  /* TODO: override (define the same class twice) */
  p->classes = cons(cons(hirsym(name), (HIR *)tree), p->classes);
}

static HIR*
insert_return_at_last(hpc_state *p, HIR *hir)
{
  if (!hir)
    return hir;
  switch ((intptr_t)hir->car) {
  case HIR_INIT_LIST:
  case HIR_GVARDECL:
  case HIR_LVARDECL:
  case HIR_PVARDECL:
  case HIR_FUNDECL:
    NOT_REACHABLE();
    return hir;
  case HIR_SCOPE:
    hir->cdr->cdr = insert_return_at_last(p, hir->cdr->cdr);
    return hir;
  case HIR_BLOCK:
    {
      HIR *last = hir->cdr->car;
      while (last->cdr)
        last = last->cdr;
      last->car = insert_return_at_last(p, last->car);
    }
    return hir;
  case HIR_ASSIGN:
    return new_block(p, list2(hir, new_return_value(p, hir->cdr->car)));
  case HIR_IFELSE:
    hir->cdr->cdr->car = insert_return_at_last(p, hir->cdr->cdr->car);
    hir->cdr->cdr->cdr->car = insert_return_at_last(p, hir->cdr->cdr->cdr->car);
    return hir;
  case HIR_DOALL:
  case HIR_WHILE:
    return new_block(p, list2(hir, new_return_void(p)));
  case HIR_BREAK:
  case HIR_CONTINUE:
    NOT_REACHABLE();
  case HIR_RETURN:
    return hir;
  case HIR_EMPTY:
    return new_return_void(p);
  case HIR_INT:
  case HIR_FLOAT:
  case HIR_LVAR:
  case HIR_GVAR:
  case HIR_IVAR:
  case HIR_CALL:
    return new_return_value(p, hir);
  default:
    NOT_IMPLEMENTED();
  }
}

static HIR*
typing_scope(hpc_scope *s, node *tree)
{
  HIR* hir;
  HIR* lv = 0;
  node* n = tree->car;
  hpc_state *p = s->hpc;

  while (n) {
    if (n->car)
      lv = cons(new_lvar(p, sym(n->car), lat_unknown), lv);
    n = n->cdr;
  }

  hpc_scope *scope = scope_new(p, s, lv, TRUE, TRUE);
  hir = new_scope(p, lvs_to_decls(p, scope->lv), typing(scope, tree->cdr));
  scope_finish(scope);
  return hir;
}

static HIR*
typing_args(hpc_scope *s, node *args)
{
  int n = 0;
  HIR *hir = 0, *last = 0;
  hpc_state *p = s->hpc;
  while (args) {
    if (n >= 127 || (intptr_t)args->car->car == NODE_SPLAT) {
      /* splat mode */
      NOT_IMPLEMENTED();
    }
    /* normal mode */

    if (hir)
      last->cdr = cons(typing(s, args->car), 0);
    else
      hir = last = cons(typing(s, args->car), 0);
    n++;
    args = args->cdr;
  }
  return hir;
}

#define PRIMCALL_ARGC_MAX 16

static HIR *
typing_prim_call(hpc_scope *s, struct RProc *interp, HIR *recv, mrb_sym mid, HIR *args, node *blk)
{
  mrb_value *argv, argv_s[PRIMCALL_ARGC_MAX];
  mrb_value ret;
  int i, argc = hir_len(args);

  if (argc > PRIMCALL_ARGC_MAX)
    argv = (mrb_value *)mrb_malloc(s->mrb, sizeof(mrb_value)*argc);
  else
    argv = argv_s;

  for (i = 0; i < argc; i++) {
    argv[i] = args->car->lat;
    args = args->cdr;
  }

  if (blk)
    NOT_IMPLEMENTED();

  ret = mrb_proccall_with_block(s->mrb, recv->lat, interp, mid, argc, argv, mrb_nil_value());
  mrb_p(s->mrb, ret);
  NOT_IMPLEMENTED();
}

static HIR*
typing_prim_add(hpc_scope *s, HIR *recv, HIR *args)
{
  hpc_assert(!args->cdr);
  NOT_IMPLEMENTED();
}

static HIR*
typing_prim_sub(hpc_scope *s, HIR *recv, HIR *args)
{
  hpc_assert(!args->cdr);
  NOT_IMPLEMENTED();
}

static HIR*
typing_prim_mul(hpc_scope *s, HIR *recv, HIR *args)
{
  hpc_assert(!args->cdr);
  NOT_IMPLEMENTED();
}

static HIR*
typing_prim_div(hpc_scope *s, HIR *recv, HIR *args)
{
  hpc_assert(!args->cdr);
  NOT_IMPLEMENTED();
}

static HIR*
typing_prim_lt(hpc_scope *s, HIR *recv, HIR *args)
{
  hpc_assert(!args->cdr);
  NOT_IMPLEMENTED();
}

static HIR*
typing_prim_ltasgn(hpc_scope *s, HIR *recv, HIR *args)
{
  hpc_assert(!args->cdr);
  NOT_IMPLEMENTED();
}

static HIR*
typing_prim_gt(hpc_scope *s, HIR *recv, HIR *args)
{
  hpc_assert(!args->cdr);
  NOT_IMPLEMENTED();
}

static HIR*
typing_prim_gtasgn(hpc_scope *s, HIR *recv, HIR *args)
{
  hpc_assert(!args->cdr);
  NOT_IMPLEMENTED();
}

static HIR*
typing_prim_equal(hpc_scope *s, HIR *recv, HIR *args)
{
  hpc_assert(!args->cdr);
  NOT_IMPLEMENTED();
}

static HIR*
typing_call0(hpc_scope *s, struct RClass *klass, HIR *recv, mrb_sym mid, HIR *args,
    node *blk)
{
  struct RProc *interp = 0;
  size_t len;
  const char *name = mrb_sym2name_len(s->mrb, mid, &len);

  mrb_p(s->mrb, recv->lat);
  mrb_p(s->mrb, mrb_symbol_value(mid));
  mrb_p(s->mrb, mrb_obj_value(klass));

  if (len == 1 && name[0] == '+')
    return typing_prim_add(s, recv, args);
  else if (len == 1 && name[0] == '-')
    return typing_prim_sub(s, recv, args);
  else if (len == 1 && name[0] == '*')
    return typing_prim_mul(s, recv, args);
  else if (len == 1 && name[0] == '/')
    return typing_prim_div(s, recv, args);
  else if (len == 1 && name[0] == '<')
    return typing_prim_lt(s, recv, args);
  else if (len == 2 && name[0] == '<' && name[1] == '=')
    return typing_prim_ltasgn(s, recv, args);
  else if (len == 1 && name[0] == '>')
    return typing_prim_gt(s, recv, args);
  else if (len == 2 && name[0] == '>' && name[1] == '=')
    return typing_prim_gtasgn(s, recv, args);
  else if (len == 2 && name[0] == '=' && name[1] == '=')
    return typing_prim_equal(s, recv, args);

  interp = search_abst_interp(s->mrb, klass, mid);
  if (interp)
    return typing_prim_call(s, interp, recv, mid, args, blk);
  NOT_IMPLEMENTED();
}

static int
find_params(node *params, mrb_sym sym)
{
  while (params) {
    if (sym(params->car->cdr) == sym)
      return TRUE;
    params = params->cdr;
  }
  return FALSE;
}

/*
  return: (hpc_scope *scope . HIR* body)
    scope is not closed, if you do not need scope, close it
 */
static HIR*
typing_block(hpc_scope *prev_scope, node *lv_tree, node *margs, node *n_body, int inherit_lvar)
{
  hpc_state *p = prev_scope->hpc;
  hpc_scope *scope;
  HIR *body, *lv = 0, *non_pv_lv = 0;

  while (lv_tree) {
    if (lv_tree->car) {
      mrb_sym sym = sym(lv_tree->car);
      HIR *lvar = new_lvar(p, sym, lat_unknown);
      lv = cons(lvar, lv);
      if (!find_params(margs, sym)) {
        non_pv_lv = cons(lvar, non_pv_lv);
      }
    }
    lv_tree = lv_tree->cdr;
  }

  scope = scope_new(p, prev_scope, lv, TRUE, inherit_lvar);
  body = new_scope(p, lvs_to_decls(p, non_pv_lv), typing(scope, n_body));

  return cons((HIR*)scope, body);
}

static HIR*
typing_call_raw(hpc_scope *s, mrb_sym name, HIR *args_prefix, node *tree, HIR *args_suffix)
{
  HIR *args, *last;
  hpc_state *p = s->hpc;

  args = last = args_prefix;
  while (last->cdr)
    last = last->cdr;

  if (tree) {
    /* mandatory arg */
    node *argtree = tree->car;
    while (argtree) {
      last->cdr = cons(typing(s, argtree->car), 0);
      last = last->cdr;
      argtree = argtree->cdr;
    }
    /* block arg */
    if (tree->cdr) {
      if (name != mrb_intern_cstr(p->mrb, "times"))
        NOT_IMPLEMENTED();

      HIR* recv = args_prefix->car;
      HIR* hir;
      mrb_sym counter = sym(tree->cdr->cdr->car->car);
      hpc_state *p = s->hpc;

      HIR * result = typing_block(s, tree->cdr->cdr->car, tree->cdr->cdr->cdr->car->car, tree->cdr->cdr->cdr->cdr->car, TRUE);
      scope_finish((hpc_scope *)result->car);

      hir = list5((HIR *)HIR_DOALL, hirsym(counter), new_int_const(p, 0), recv, result->cdr);
      hir->lat = recv->lat;
      return hir;
    }
  }

  last->cdr = args_suffix;

  return cons((HIR*)HIR_CALL, cons(hirsym(name), args));
}

static HIR*
typing_call(hpc_scope *s, node *tree)
{
  HIR *recv = typing(s, tree->car);
  mrb_sym name = sym(tree->cdr->car);
  hpc_state *p = s->hpc;

  return typing_call_raw(s, name, list1(recv), tree->cdr->cdr->car,  0);
}

static HIR*
typing_assign(hpc_scope *s, node *tree)
{
  hpc_state *p = s->hpc;
  node *lhs = tree->car;
  HIR *rhs = typing(s, tree->cdr);
  switch ((intptr_t)lhs->car) {
  case NODE_GVAR:
  case NODE_LVAR:
  case NODE_CONST:
  case NODE_IVAR:
    return new_assign(p, typing(s, lhs), rhs);
  case NODE_CALL:
    {
      HIR *recv = typing(s, lhs->cdr->car);
      return typing_call_raw(s, attrsym(p, sym(lhs->cdr->cdr->car)), list1(recv), lhs->cdr->cdr->cdr->car,  list1(rhs));
    }
  default:
    NOT_IMPLEMENTED();
  }
}

static HIR*
typing_def(hpc_scope *s, node *ast)
{
  hpc_state *p = s->hpc;
  mrb_sym name = sym(ast->car);
  node *mandatory_params = ast->cdr->cdr->car->car;
  node *lv_tree = ast->cdr->car;
  node *n_body = ast->cdr->cdr->cdr->car;
  HIR *params = 0, *body, *last, *param;
  hpc_scope *scope;
  mrb_sym self_sym;

  HIR *result = typing_block(s, lv_tree, mandatory_params, n_body, FALSE);

  scope = (hpc_scope *)result->car;
  self_sym = sym(scope->current_self->cdr);
  scope_finish(scope);

  body = result->cdr;

  body = insert_return_at_last(p, body);

  /*
    args
    TODO: - support other than mrb_value (using info in scope)
          - optional and block args
  */
  /* TODO: define self type from scope->current_self->lat */
  params = last = cons(new_pvardecl(p, value_type, self_sym), 0);

  while (mandatory_params) {
    param = new_pvardecl(p, value_type, sym(mandatory_params->car->cdr));
    last->cdr = cons(param, 0);
    last = last->cdr;
    mandatory_params = mandatory_params->cdr;
  }

  /* TODO: inspect return type? */
  return new_fundecl(p, s->class_name, name,
      new_func_type(p, value_type, params), params, body);
}

static void
update_function_map(hpc_state *p, HIR *fundecl)
{
  HIR *map = p->function_map;
  HIR *class_name  = fundecl->cdr->cdr->car->car;
  HIR *method_name = fundecl->cdr->cdr->car->cdr;
  int arg_count = length(fundecl->cdr->cdr->cdr->car)-1;

  while (map) {
    if (map->car->car->car == method_name &&
        (intptr_t)map->car->car->cdr == arg_count) {
      map->car->cdr = cons(class_name, map->car->cdr);
      return;
    }
    map = map->cdr;
  }
  /* not found */
  p->function_map = cons(cons(cons(method_name, (HIR*)arg_count),
                              list1(class_name)),
                         p->function_map);
}

static void
add_def(hpc_scope *s, node *tree)
{
  hpc_state *p = s->hpc;
  HIR *fundecl = typing_def(s, tree);
  s->defs = cons(fundecl, s->defs);
  update_function_map(p, fundecl);
}

static void
collect_class_defs(hpc_scope *s, mrb_sym name, node *body)
{
  hpc_state *p = s->hpc;
  hpc_scope *class_scope = scope_new(p, s, 0, FALSE, FALSE);
  HIR *defs;

  class_scope->class_name = name;
  typing(class_scope, body); /* collect defs */

  defs = class_scope->defs;
  while (defs) {
    s->defs = cons(defs->car, s->defs);
    defs = defs->cdr;
  }

  scope_finish(class_scope);
}

static HIR*
typing(hpc_scope *s, node *tree)
{
  if (!tree) return 0;
  hpc_state *p = s->hpc;
  s->lineno = tree->lineno;
  int type = (intptr_t)tree->car;
  //parser_dump(p->mrb, tree, 0);
  tree = tree->cdr;
  switch (type) {
    case NODE_SCOPE:
      return typing_scope(s, tree);
    case NODE_BEGIN:
      {
        HIR *stmts = 0, *last = 0, *stmt;
        while (tree) {
          stmt = typing(s, tree->car);
          if (!stmts)
            stmts = last = cons(stmt, 0);
          else {
            last->cdr = cons(stmt, 0);
            last = last->cdr;
          }
          tree = tree->cdr;
        }
        return new_block(p, stmts);
      }
    case NODE_CALL:
    case NODE_FCALL:            /* when receiver is self */
      return typing_call(s, tree);
    case NODE_INT:
      {
        char *txt = (char*)tree->car;
        int base = (intptr_t)tree->cdr->car;
        mrb_int i;
        int overflow;
        i = readint_mrb_int(s, txt, base, FALSE, &overflow);
        if (overflow) {
          double f = readint_float(s, txt, base);
          return new_float(p, txt, base, f);
        } else {
          return new_int(p, txt, base, i);
        }
      }
    case NODE_FLOAT:
      return new_float(p, (char *)tree, 10, str_to_mrb_float((char *)tree));
    case NODE_DEF:
      /* This node will be translated later using information of call-sites */
      add_def(s, tree);
      return new_empty(p);
    case NODE_IF:
      {
        HIR *ifthen = 0, *ifelse = 0, *cond = typing(s, tree->car);
        hpc_scope *s2 = scope_dup(s);
        if (tree->cdr->car)
          ifthen = typing(s, tree->cdr->car);
        if (tree->cdr->cdr->car)
          ifelse = typing(s2, tree->cdr->cdr->car);
        join_scope(s, s2);
        return new_ifelse(p, cond, ifthen, ifelse);
      }
    case NODE_LVAR:
      return lookup_lvar(s, sym(tree));
    case NODE_IVAR:
      return lookup_ivar(s, sym(tree));
    case NODE_SELF:
      return s->current_self;
    case NODE_RETURN:
      if (tree) {
        return new_return_value(p, typing(s, tree));
      } else {
        return new_return_void(p);
      }
    case NODE_ASGN:
      return typing_assign(s, tree);
    case NODE_CONST:
    case NODE_GVAR:
      {
        HIR *gvar = find_var_list(p, p->gvars, sym(tree));
        if (!gvar) {
          gvar = new_gvar(p, sym(tree), lat_unknown);
          p->gvars = cons(gvar, p->gvars);
        }
        return gvar;
      }
    case NODE_MODULE:
      return new_empty(p);
      NOT_IMPLEMENTED();
    case NODE_CLASS:
      {
        mrb_sym class_name = sym(tree->car->cdr);
        HIR *rhs = new_defclass(p, class_name, 0);
        register_class_definition(p, class_name, tree);
        collect_class_defs(s, class_name, tree->cdr->cdr->car->cdr);
        return new_assign(p, new_gvar(p, class_name, lat_unknown), rhs);
      }
    case NODE_STR:
      return new_str(p, (char*)tree->car, (int)(intptr_t)tree->cdr);
    case NODE_AND:
      /* (lhs ? rhs : lhs) */
      {
        HIR *lhs = typing(s, tree->car);
        HIR *rhs = typing(s, tree->cdr);
        return new_cond_op(p, lhs, rhs, lhs);
      }
    case NODE_OR:
      /* (lhs ? lhs : rhs) */
      {
        HIR *lhs = typing(s, tree->car);
        HIR *rhs = typing(s, tree->cdr);
        return new_cond_op(p, lhs, lhs, rhs);
      }
    case NODE_FALSE:
      return new_prim(p, HPTYPE_FALSE);
    case NODE_TRUE:
      return new_prim(p, HPTYPE_TRUE);
    case NODE_NEGATE:
      /* NEGATE is only for literal */
      {
        HIR *child = typing(s, tree);
        char *old_lit = (char *)child->cdr->car;
        char *new_lit = compiler_palloc(p, strlen(old_lit) + 2);
        hpc_assert((intptr_t)child->car == HIR_INT
                   || (intptr_t)child->car == HIR_FLOAT);
        sprintf(new_lit, "-%s", old_lit);
        child->cdr->car = (HIR *)new_lit;
        return child;
      }
    default:
      NOT_REACHABLE();
  }
}

/* return a raw list of decls (fundecl, global decl) */
static HIR*
compile(hpc_state *p, node *ast)
{
  HIR *topdecls;
  hpc_scope *scope = scope_new(p, 0, 0, FALSE, FALSE);
  parser_dump(p->mrb, ast, 0);
  HIR *main_body = typing(scope, ast);

  HIR *params = list2(
      new_pvardecl(p, value_type, sym(scope->current_self->cdr)),
      new_pvardecl(p, mrb_state_ptr_type, mrb_intern(p->mrb, "mrb"))
      );
  HIR *main_fun = new_fundecl(p, 0, mrb_intern(p->mrb, "compiled_main"),
      new_func_type(p, void_type, params), params, main_body);

  topdecls = cons(main_fun, 0);

  while (scope->defs) {
    /* FIXME: this management of defs possibly has bug */
    HIR *tree = scope->defs->car;
    scope->defs = scope->defs->cdr;
    topdecls = cons(tree, topdecls);
  }

  while (p->gvars) {
    HIR *gvar = p->gvars->car;
    topdecls = cons(new_gvardecl(p, infer_type(p, gvar->lat), sym(gvar->cdr), new_empty(p)),
                    topdecls);
    p->gvars = p->gvars->cdr;
  }

  mrb_pool_close(scope->mpool);
  return topdecls;
}

HIR*
hpc_compile_file(hpc_state *s, FILE *rfp, mrbc_context *c)
{
  puts("Compiling...");

  mrb_state *mrb = s->mrb;  /* It is necessary for E_SYNTAX_ERROR macro */
  parser_state *p = mrb_parse_file(s->mrb, rfp, c);
  HIR *ret;

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

  ret = compile(s, p->tree);
  mrb_parser_free(p);
  return ret;
}

void
init_hpc_compiler(hpc_state *p)
{
  lat_class = mrb_define_class(p->mrb, "Lattice", p->mrb->object_class);
  lat_unknown = lat_new(p->mrb, LAT_UNKNOWN);
  lat_dynamic = lat_new(p->mrb, LAT_DYNAMIC);
  mrb_define_method(p->mrb, lat_class, "inspect", lat_inspect, ARGS_NONE());

  void_type   = new_simple_type(p, HTYPE_VOID);
  value_type  = new_simple_type(p, HTYPE_VALUE);
  sym_type    = new_simple_type(p, HTYPE_SYM);
  char_type   = new_simple_type(p, HTYPE_CHAR);
  int_type    = new_simple_type(p, HTYPE_INT);
  float_type  = new_simple_type(p, HTYPE_FLOAT);
  string_type = new_simple_type(p, HTYPE_STRING);
  mrb_state_ptr_type = new_ptr_type(p, new_typedef_type(p, "mrb_state"));
}

/* for debugging */
void
dump_varlist(mrb_state *mrb, HIR *list)
{
  while (list) {
    puts(mrb_sym2name(mrb, sym(list->car->cdr)));
    list = list->cdr;
  }
}
