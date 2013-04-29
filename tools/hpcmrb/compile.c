#include <ctype.h>
#include <string.h>
#include "hpcmrb.h"
#include "mruby/array.h"
#include "mruby/data.h"
#include "mruby/string.h"
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
  c->type = lat_unknown;
  return c;
}
#define cons(a,b) cons_gen(p, (a), (b))
#define list1(a)          cons((a), 0)
#define list2(a,b)        cons((a), cons((b), 0))
#define list3(a,b,c)      cons((a), cons((b), cons((c), 0)))
#define list4(a,b,c,d)    cons((a), cons((b), cons((c), cons((d), 0))))
#define list5(a,b,c,d,e)  cons((a), cons((b), cons((c), cons((d), cons((e), 0)))))

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
  var->type = lat;
  return var;
}

static HIR*
new_fundecl(hpc_state *p, mrb_sym sym, HIR *params, HIR *body, HIR *type)
{
  return list5((HIR*)HIR_FUNDECL, hirsym(sym), params, body, type);
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
new_int(hpc_state *p, char *text, int base, mrb_int val)
{
  HIR *lit = list3((HIR*)HIR_INT, (HIR*)text, (HIR*)(intptr_t)base);
  lit->type = mrb_fixnum_value(val);
  return lit;
}

static HIR*
new_float(hpc_state *p, char *text, int base, double val)
{
  HIR *lit = list3((HIR*)HIR_FLOAT, (HIR*)text, (HIR*)(intptr_t)base);
  lit->type = mrb_float_value(val);
  return lit;
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

static HIR *void_type;
static HIR *value_type;
static HIR *sym_type;
static HIR *char_type;
static HIR *int_type;
static HIR *float_type;
static HIR *string_type;

static void
init_types(hpc_state *p)
{
  void_type   = new_simple_type(p, HTYPE_VOID);
  value_type  = new_simple_type(p, HTYPE_VALUE);
  sym_type    = new_simple_type(p, HTYPE_SYM);
  char_type   = new_simple_type(p, HTYPE_CHAR);
  int_type    = new_simple_type(p, HTYPE_INT);
  float_type  = new_simple_type(p, HTYPE_FLOAT);
  string_type = new_simple_type(p, HTYPE_STRING);
}

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

  HIR *lv;
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
scope_new(hpc_state *p, hpc_scope *prev, HIR *lv)
{
  static const hpc_scope hpc_scope_zero = { 0 };
  mrb_pool *pool = mrb_pool_open(p->mrb);
  hpc_scope *s = (hpc_scope *)mrb_pool_alloc(pool, sizeof(hpc_scope));

  if (!s) hpc_error(s, "mrb_pool_alloc failed");
  *s = hpc_scope_zero;
  s->hpc = p;
  s->mrb = p->mrb;
  s->mpool = pool;
  if (!prev) return s;
  s->prev = prev;
  s->ainfo = -1;
  s->mscope = 0;
  s->lv = lv;
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
  mrb_gc_arena_restore(mrb, s->ai);
  mrb_pool_close(s->mpool);
}

static HIR *typing(hpc_scope *s, node *tree);

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

  hpc_scope *scope = scope_new(p, s, lv);
  hir = new_scope(p, scope->lv, typing(scope, tree->cdr));
  scope_finish(scope);
  return hir;
}

static HIR*
typing(hpc_scope *s, node *tree)
{
  if (!tree) return 0;
  hpc_state *p = s->hpc;
  s->lineno = tree->lineno;
  int type = (intptr_t)tree->car;
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
          else
            last->cdr = cons(stmt, 0);
          last = stmt;
          tree = tree->cdr;
        }
        return new_block(p, stmts);
      }
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
    default:
      NOT_REACHABLE();
  }
}

static HIR*
compile(hpc_state *p, node *ast)
{
  hpc_scope *scope = scope_new(p, 0, 0);

  parser_dump(p->mrb, ast, 0);

  HIR *main_body = typing(scope, ast);

  mrb_pool_close(scope->mpool);

  return new_fundecl(p, mrb_intern(p->mrb, "compiled_main"), 0, main_body,
      new_func_type(p, void_type, 0));
}

HIR*
hpc_compile_file(hpc_state *s, FILE *rfp, mrbc_context *c)
{
  puts("Compiling...");

  mrb_state *mrb = s->mrb;  /* It is necessary for E_SYNTAX_ERROR macro */
  parser_state *p = mrb_parse_file(s->mrb, rfp, c);

  init_types(p);

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
  HIR *tree = compile(s, p->tree);
  mrb_parser_free(p);
  return tree;
}

void
init_hpc_compiler(mrb_state *mrb)
{
  lat_class = mrb_define_class(mrb, "Lattice", mrb->object_class);
  lat_unknown = lat_new(mrb, LAT_UNKNOWN);
  lat_dynamic = lat_new(mrb, LAT_DYNAMIC);
  mrb_define_method(mrb, lat_class, "inspect", lat_inspect, ARGS_NONE());
}
