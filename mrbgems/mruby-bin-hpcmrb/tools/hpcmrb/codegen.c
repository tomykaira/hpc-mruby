#include "hpcmrb.h"
#include <stdint.h>
#include <string.h>

#define CADR(x) ((x)->cdr->car)
#define CADDR(x) ((x)->cdr->cdr->car)
#define CADDDR(x) ((x)->cdr->cdr->cdr->car)
#define CADDDDR(x) ((x)->cdr->cdr->cdr->cdr->car)

#define TYPE(x) ((intptr_t)((x)->car))
#define DECLP(t) (t == HIR_GVARDECL || t == HIR_LVARDECL || \
                  t == HIR_PVARDECL)

#define sym(x) ((mrb_sym)(intptr_t)(x))

#define PUTS(str) (fputs((str), c->wfp))
#define PUTS_INDENT {                           \
    int __i;                                    \
    for (__i = 0; __i < c->indent; ++__i) {     \
      PUTS("\t");                               \
    }                                           \
}

#define INDENT_PP (c->indent ++)
#define INDENT_MM (c->indent --)

typedef struct {
  mrb_state *mrb;
  FILE *wfp;
  int indent;
} hpc_codegen_context;

static void put_decl(hpc_codegen_context *c, HIR *decl);
static void put_exp(hpc_codegen_context *c, HIR *exp);
static void put_statement(hpc_codegen_context *c, HIR *stat, int no_brace);
int length(HIR *list);

static void
put_header(hpc_codegen_context *c)
{
  PUTS("/* Generated by HPC mruby compiler */\n");
  PUTS("#include <stdio.h>\n");
  PUTS("#include <stdlib.h>\n");
  PUTS("#include \"mruby.h\"\n\n");
  PUTS("#include \"mruby/variable.h\"\n\n");
  PUTS("#include \"mruby/class.h\"\n\n");
  PUTS("#include \"builtin.h\"\n\n");
  PUTS("extern mrb_state * mrb;");
}

static void
puts_noescape(hpc_codegen_context *c, const char *str)
{
  int i = 0;
  int len = strlen(str);
  for (i = 0; i < len; ++i) {
    switch (str[i]) {
    case '\a':
      PUTS("\\a");
      break;
    case '\b':
      PUTS("\\b");
      break;
    case 'f':
      PUTS("\f");
      break;
    case '\n':
      PUTS("\\n");
      break;
    case '\r':
      PUTS("\\r");
      break;
    case '\t':
      PUTS("\\t");
      break;
    case '\\':
      PUTS("\\\\");
      break;
    case '\"':
      PUTS("\\\"");
      break;
    case '\'':
      PUTS("\\\'");
      break;
    default:
      putc(str[i], c->wfp);
      break;
    }
  }
}

static void
put_int(hpc_codegen_context *c, int i)
{
  char len[32];
  sprintf(len, "%d", i);
  PUTS(len);
}

static void
put_type(hpc_codegen_context *c, HIR *kind)
{
  enum hir_type_kind k = (intptr_t)kind->car;

  switch (k) {
    case HTYPE_VOID:
      PUTS("void");
      return;
    case HTYPE_VALUE:
      PUTS("mrb_value");
      return;
    case HTYPE_SYM:
      PUTS("mrb_sym");
      return;
    case HTYPE_CHAR:
      PUTS("char");
      return;
    case HTYPE_INT:
      PUTS("int");
      return;
    case HTYPE_FLOAT:
#ifdef MRB_USE_FLOAT
      PUTS("float");
#else
      PUTS("double");
#endif
      return;
    case HTYPE_STRING:
      PUTS("char *");
      return;
    case HTYPE_PTR:
      put_type(c, CADR(kind));
      PUTS(" *");
      return;
    case HTYPE_ARRAY:
    case HTYPE_FUNC:
      NOT_REACHABLE();
      return;
    case HTYPE_TYPEDEF:
      PUTS((char *)kind->cdr->car);
      return;
    default:
      NOT_REACHABLE();
  }
}

static void
put_symbol(hpc_codegen_context *c, HIR *hir)
{
  mrb_sym sym = (intptr_t)hir;
  const char * name = mrb_sym2name(c->mrb, sym);
  PUTS(name);
}

/* Take two heads from hir as type and var */
static void
put_variable(hpc_codegen_context *c, HIR *hir)
{
  HIR *type = hir->car;
  enum hir_type_kind k = (intptr_t)type->car;
  HIR *var = CADR(hir);

  switch (k) {
  case HTYPE_ARRAY:
    {
      int indices[1024];
      int length, i;

      /* link basetypes */
      for (length = 0; TYPE(type) == HTYPE_ARRAY; length++) {
        indices[length] = (intptr_t)CADDR(type);
        type = CADR(type);
      }
      put_type(c, type);
      PUTS(" ");
      put_symbol(c, var);
      for (i = 0; i <= length; i ++) {
        char buf[32];
        sprintf(buf, "[%d]", indices[i]);
        PUTS(buf);
      }
    }
    return;
  case HTYPE_FUNC:
    /* function pointer? */
    NOT_IMPLEMENTED();
  default:
    put_type(c, type);
    PUTS(" ");
    put_symbol(c, var);
    return;
  }
}

static int
put_operator_name(hpc_codegen_context *c, mrb_sym sym)
{
  size_t len;
  int i;
  const char *name = mrb_sym2name_len(c->mrb, sym(sym), &len);
  static const char table[][2][32] = {
    {"+", "num_add"},
    {"-", "num_sub"},
    {"*", "num_mul"},
    {"/", "num_div"},
    {"<", "num_lt"},
    {"<=", "num_le"},
    {">", "num_gt"},
    {">=", "num_ge"},
    {"==", "num_eq"},
    {"!", "mrb_bob_not"},
    {"[]", "hpc_ary_aget"},
    {"[]=", "hpc_ary_aset"},
    {"-@", "num_uminus"},
    {"", ""}
  };

  for (i = 0; strlen(table[i][0]); ++i) {
    if (strlen(table[i][0]) == len && strncmp(table[i][0], name, len) == 0) {
      PUTS(table[i][1]);
      return 1;
    }
  }
  return 0;
}

static int
put_invalid_name(hpc_codegen_context *c, mrb_sym sym)
{
  size_t len;
  const char *name = mrb_sym2name_len(c->mrb, sym(sym), &len);

  if (name[len-1] == '=') {
    char new_name[32];
    strncpy(new_name, name, len-1);
    new_name[len-1] = '\0';
    PUTS(new_name);
    PUTS("_set");
    return 1;
  }

  return 0;
}

static void
put_function_name(hpc_codegen_context *c, HIR *sym)
{
  if (! put_operator_name(c, sym(sym)) &&
      ! put_invalid_name(c, sym(sym))) {
    put_symbol(c, sym);
  }
}

static void
put_unique_function_name(hpc_codegen_context *c, HIR* class_name, HIR* method_name)
{
  if (class_name) {
    put_symbol(c, class_name);
    PUTS("_");
    put_function_name(c, method_name);
  } else {
    put_function_name(c, method_name);
  }
}

static void
put_call_function_name(hpc_codegen_context *c, HIR* method_name, int arg_count)
{
  put_function_name(c, method_name); PUTS("_"); put_int(c, arg_count);
}

static void
put_ivar_name(hpc_codegen_context *c, HIR *hir)
{
  mrb_sym sym = (intptr_t)hir;
  const char * name = mrb_sym2name(c->mrb, sym);

  PUTS("mrb_intern(mrb, \"");
  PUTS(name + 1);                /* skip @ */
  PUTS("\")");
}

static void
put_fundecl_decl(hpc_codegen_context *c, HIR *decl)
{
  HIR *funtype = CADR(decl);
  HIR *params = CADDDR(decl);
  hpc_assert((intptr_t)funtype->car == HTYPE_FUNC);
  put_type(c, CADR(funtype));
  PUTS("\n");
  put_unique_function_name(c, CADDR(decl)->car, CADDR(decl)->cdr);
  PUTS("(");
  while (params) {
    hpc_assert((intptr_t)params->car->car == HIR_PVARDECL);
    put_decl(c, params->car);
    params = params->cdr;
    if (params) {
      PUTS(", ");
    }
  }
  PUTS(")");
}

static void
put_decl(hpc_codegen_context *c, HIR *decl)
{
  PUTS_INDENT;
  switch (TYPE(decl)) {
    case HIR_GVARDECL:
    case HIR_LVARDECL:
      put_variable(c, decl->cdr);
      if (TYPE(CADDDR(decl)) != HIR_EMPTY) {
        PUTS(" = ");
        put_exp(c, CADDDR(decl));
      }
      PUTS(";\n");
      return;
    case HIR_PVARDECL:
      put_variable(c, decl->cdr);
      return;
    case HIR_FUNDECL:
      {
        put_fundecl_decl(c, decl);
        PUTS("\n{\n");
        INDENT_PP;
        put_statement(c, CADDDDR(decl), TRUE);
        INDENT_MM;
        PUTS("}\n\n");
      }
      return;
    default:
      NOT_REACHABLE();
  }
}

/*
  Output:
    some_exp
  w/o newline, w/o trailing semicolon
 */
static void
put_exp(hpc_codegen_context *c, HIR *exp)
{
  switch (TYPE(exp)) {
    case HIR_PRIM:
      switch ((enum hir_primitive_type)exp->cdr) {
      case HPTYPE_NIL:
        PUTS("mrb_nil_value()");
        return;
      case HPTYPE_FALSE:
        PUTS("mrb_false_value()");
        return;
      case HPTYPE_TRUE:
        PUTS("mrb_true_value()");
        return;
      }
    case HIR_INT:
      PUTS("mrb_fixnum_value(");
      PUTS((char *)CADR(exp));
      PUTS(")");
      return;
    case HIR_FLOAT:
      PUTS("mrb_float_value(");
      PUTS((char *)CADR(exp));
      PUTS(")");
      return;
    case HIR_STRING:
      PUTS("mrb_str_new(mrb, \"");
      puts_noescape(c, (char *)CADR(exp));
      PUTS("\", ");
      put_int(c, (intptr_t)CADDR(exp));
      PUTS(")");
      return;
    case HIR_LVAR:
    case HIR_GVAR:
      put_symbol(c, exp->cdr);
      return;
    case HIR_IVAR:
      PUTS("mrb_iv_get(mrb, __self__, ");
      put_ivar_name(c, exp->cdr);
      PUTS(")");
      break;
    case HIR_CALL:
      /* FIXME: expects
         - func is symbol
         - args are expressions
       */
      {
        HIR *args = exp->cdr->cdr;
        put_call_function_name(c, CADR(exp), length(args)-1);
        PUTS("(");
        while (args) {
          put_exp(c, args->car);
          args = args->cdr;
          if (args) {
            PUTS(", ");
          }
        }
      }
      PUTS(")");
      return;
    case HIR_COND_OP:
      PUTS("( mrb_bool(");
      put_exp(c, CADR(exp));
      PUTS(") ? ");
      put_exp(c, CADDR(exp));
      PUTS(" : ");
      put_exp(c, CADDDR(exp));
      PUTS(" )");
      return;
    case HIR_DEFCLASS:
      /* mrb_define_class(mrb, NAME, mrb->object_class) */
      PUTS("mrb_obj_value(mrb_define_class(mrb, \"");
      put_symbol(c, CADR(exp));
      PUTS("\", mrb->object_class))");
      return;
    case HIR_INIT_LIST:
      PUTS("{");
      {
        HIR *values = exp->cdr;
        while (values) {
          put_exp(c, values->car);
          values = values->cdr;
          if (values)
            PUTS(", ");
        }
      }
      PUTS("}");
    case HIR_BLOCK:
      /* FIXME: ruby: (stat1; stat2; stat3) return the result of stat3 */
      {
        HIR *exps = exp->cdr->car;
        PUTS("(");
        while (exps) {
          put_exp(c, exps->car);
          if (exps->cdr)
            PUTS(", ");
          exps = exps->cdr;
        }
        PUTS(")");
      }
      return;
    default:
      NOT_REACHABLE();
  }
}

/*
  Output:
    some
    what
    statements\n
 */
static void
put_statement(hpc_codegen_context *c, HIR *stat, int no_brace)
{
  switch (TYPE(stat)) {
    case HIR_SCOPE:
      {
        HIR *decls = CADR(stat);
        HIR *inner_stat = stat->cdr->cdr;
        if (!no_brace) {
          PUTS_INDENT;
          PUTS("{\n");
          INDENT_PP;
        }
        if (decls) {
          while (decls) {
            put_decl(c, decls->car);
            decls = decls->cdr;
          }
          PUTS("\n");
        }
        put_statement(c, inner_stat, TRUE);
        if (!no_brace) {
          INDENT_MM;
          PUTS_INDENT;
          PUTS("}\n");
        }
      }
      return;
    case HIR_BLOCK:
      {
        HIR *stats = stat->cdr->car;
        if (!no_brace) {
          PUTS_INDENT;
          PUTS("{\n");
          INDENT_PP;
        }
        while (stats) {
          put_statement(c, stats->car, TRUE);
          stats = stats->cdr;
        }
        if (!no_brace) {
          INDENT_MM;
          PUTS_INDENT;
          PUTS("}\n");
        }
      }
      return;
    case HIR_ASSIGN:
      /* lhs is HIR_LVAR or HIR_GVAR,
         rhs is exp */
      PUTS_INDENT;
      switch (TYPE(CADR(stat))) {
      case HIR_LVAR:
      case HIR_GVAR:
        put_symbol(c, CADR(stat)->cdr);
        PUTS(" = ");
        put_exp(c, CADDR(stat));
        break;
      case HIR_IVAR:
        PUTS("mrb_iv_set(mrb, __self__, ");
        put_ivar_name(c, CADR(stat)->cdr);
        PUTS(", ");
        put_exp(c, CADDR(stat));
        PUTS(")");
        break;
      }
      PUTS(";\n");
      return;
    case HIR_IFELSE:
      PUTS_INDENT;
      PUTS("if ( mrb_bool(");
      put_exp(c, CADR(stat));
      PUTS(") )\n");
      if (CADDR(stat)) {
        put_statement(c, CADDR(stat), FALSE);
      } else {
        PUTS_INDENT;
        PUTS("{}\n");
      }
      if (CADDDR(stat)) {
        PUTS_INDENT;
        PUTS("else");
        put_statement(c, CADDDR(stat), FALSE);
      }
      return;
    case HIR_DOALL:
      /* FIXME: expects
         - var is mrb_sym
         - var is defined at the top of this block
         - low, high are exp
         - body is a statement
      */
      {
        HIR *low  = CADDR(stat);
        HIR *high = CADDDR(stat);
        HIR *sym = CADR(stat);
        char counter[32], last[32];

        sprintf(counter, "__%s", mrb_sym2name(c->mrb, sym(sym)));
        sprintf(last, "__%s_last", mrb_sym2name(c->mrb, sym(sym)));

        if (!no_brace) {
          INDENT_PP;
          PUTS_INDENT;
          PUTS("{\n");
        }
        PUTS_INDENT;
        PUTS("mrb_int "); PUTS(counter); PUTS(" = ");
        PUTS("mrb_fixnum("); put_exp(c, low); PUTS(");\n");
        PUTS_INDENT;
        PUTS("mrb_int "); PUTS(last); PUTS(" = ");
        PUTS("mrb_fixnum("); put_exp(c, high); PUTS(");\n");
        PUTS_INDENT;
        PUTS("for (; ");
        PUTS(counter); PUTS(" < "); PUTS(last); PUTS("; ");
        PUTS("++"); PUTS(counter); PUTS(") {\n");
        INDENT_PP;
        PUTS_INDENT;
        PUTS("mrb_value "); put_symbol(c, sym); PUTS(" = "); PUTS("mrb_fixnum_value("); PUTS(counter); PUTS(");\n");
        put_statement(c, CADDDDR(stat), TRUE);
        INDENT_MM;
        PUTS_INDENT;
        PUTS("}\n");
        if (!no_brace) {
          INDENT_MM;
          PUTS_INDENT;
          PUTS("}\n");
        }
      }
      return;
    case HIR_WHILE:
      PUTS_INDENT;
      PUTS("while (");
      put_exp(c, CADR(stat));
      PUTS(") ");
      put_statement(c, CADDR(stat), FALSE);
      return;
    case HIR_BREAK:
      PUTS_INDENT;
      PUTS("break;\n");
      return;
    case HIR_CONTINUE:
      PUTS_INDENT;
      PUTS("continue;\n");
      return;
    case HIR_RETURN:
      PUTS_INDENT;
      if (stat->cdr) {
        PUTS("return ");
        put_exp(c, CADR(stat));
        PUTS(";\n");
      } else {
        PUTS("return mrb_nil_value();\n");
      }
      return;
    case HIR_EMPTY:
      return;
    case HIR_CALL:
      PUTS_INDENT;
      put_exp(c, stat);
      PUTS(";\n");
      return;
    default:
      NOT_REACHABLE();
  }
}

static int
has_null_class(HIR *classes)
{
  while (classes) {
    if (!classes->car) return TRUE;
    classes = classes->cdr;
  }
  return FALSE;
}

/* mrb_value funname_1(mrb_value __self__, mrb_value arg1) */
static void
put_map_decl(hpc_codegen_context *c, const mrb_sym name, const int arg_count)
{
  char buf[1024] = "";
  int i;

  PUTS("mrb_value\n");
  put_call_function_name(c, (HIR*)(intptr_t)name, arg_count);
  PUTS("(mrb_value __self__");
  for (i = 0; i < arg_count; ++i) {
    sprintf(buf, ", mrb_value arg%d", i);
    PUTS(buf);
  }
  PUTS(")");
}

/*
  this does not handle initialize

  mrb_value funname_1(mrb_value __self__, mrb_value arg1) {
    struct RClass *c = mrb_obj_class(mrb, __self__);
    if (c == mrb_class_get(mrb, "FirstClass")) {
      FirstClass_funname(__self__, arg1);
    } else if (c == mrb_class_get(mrb, "SecondClass")) {
      SecondClass_funname(__self__, arg1);
    } else {
      mrb_funcall(mrb, __self__, "funname", 1, arg1);
    }
  }
 */
static void
put_multiplexers(hpc_codegen_context *c, hpc_state *s)
{
  HIR * map = s->function_map;
  mrb_state *mrb = c->mrb;

  while (map) {
    char buf[1024] = "", arglist[1024] = "";
    HIR* elm = map->car;
    HIR* method = elm->car;
    HIR* classes = elm->cdr;
    map = map->cdr;

    if (sym(method->car) == mrb->init_sym)
      continue;

    const int arg_count = (intptr_t)method->cdr;
    int i;

    put_map_decl(c, sym(method->car), arg_count);
    PUTS("\n{\n");
    for (i = 0; i < arg_count; ++i) {
      sprintf(arglist, "%s, arg%d", arglist, i);
    }

    if (has_null_class(classes)) {
      PUTS("\treturn ");
      put_unique_function_name(c, classes->car, method->car);
      PUTS("(__self__");
      PUTS(arglist);
      PUTS(");\n}");

      continue;
    }

    PUTS("\tstruct RClass *c = mrb_obj_class(mrb, __self__);\n");

    while (classes) {
      PUTS("\tif (c == mrb_class_get(mrb, \"");
      put_symbol(c, classes->car);
      PUTS("\")) {\n\t\treturn ");
      put_unique_function_name(c, classes->car, method->car);
      PUTS("(__self__");
      PUTS(arglist);
      PUTS(");\n\t} else ");

      classes = classes->cdr;
    }

    PUTS("{\n");
    sprintf(buf, "\t\treturn mrb_funcall(mrb, __self__, \"%s\", %d%s);\n",
            mrb_sym2name(mrb, sym(method->car)), arg_count, arglist);
    PUTS(buf);
    PUTS("\t}\n}\n\n");
  }
}

void
put_fun_decls(hpc_codegen_context *c, HIR *hir, HIR *map)
{
  while (hir) {
    if (TYPE(hir->car) == HIR_FUNDECL) {
      put_fundecl_decl(c, hir->car);
      PUTS(";\n");
    }
    hir = hir->cdr;
  }

  while (map) {
    HIR * method = map->car->car;
    if (c->mrb->init_sym == sym(method->car))
      goto next;
    put_map_decl(c, sym(method->car), (intptr_t)method->cdr);
    PUTS(";\n");

  next:
    map = map->cdr;
  }
}

HIR *
lookup_map(HIR *map, mrb_sym sym, int arg_count)
{
  while (map) {
    HIR *m = map->car->car;
    if (sym(m->car) == sym && (intptr_t)m->cdr == arg_count)
      return map->car->cdr;
    map = map->cdr;
  }

  return NULL;
}

/* cf. mrb_instance_new in class.c */
void
put_new_decls(hpc_codegen_context *c, HIR *map, int max_arg)
{
  int arg_count;
  mrb_sym new_sym = mrb_intern(c->mrb, "new");
  char buf[1024];

  for (arg_count = 0; arg_count < max_arg; ++arg_count) {
    char arglist[1024] = "";
    int i;
    HIR * classes = lookup_map(map, c->mrb->init_sym, arg_count);
    for (i = 0; i < arg_count; ++i) {
      sprintf(arglist, "%s, arg%d", arglist, i);
    }

    put_map_decl(c, new_sym, arg_count);
    PUTS("\n{\n");

    PUTS("\tstruct RClass *c = mrb_class_ptr(__self__);\n");
    PUTS("\tstruct RObject *o;\n");
    PUTS("\tenum mrb_vtype ttype = MRB_INSTANCE_TT(c);\n");
    PUTS("\tmrb_value obj;\n");

    PUTS("\tif (ttype == 0) ttype = MRB_TT_OBJECT;\n");
    PUTS("\to = (struct RObject*)mrb_obj_alloc(mrb, ttype, c);\n");
    PUTS("\tobj = mrb_obj_value(o);\n");

    while (classes) {
      PUTS("\tif (c == mrb_class_get(mrb, \"");
      put_symbol(c, classes->car);
      PUTS("\")) {\n\t\t");
      put_unique_function_name(c, classes->car, (HIR *)(intptr_t)c->mrb->init_sym);
      PUTS("(obj");
      PUTS(arglist);
      PUTS(");\n\t} else ");

      classes = classes->cdr;
    }

    PUTS("{\n");
    sprintf(buf, "\t\tmrb_funcall(mrb, obj, \"%s\", %d%s);\n",
            "initialize", arg_count, arglist);
    PUTS(buf);
    PUTS("\t}\n");


    PUTS("return obj;\n");

    PUTS("}\n\n");
  }
}

mrb_value
hpc_generate_code(hpc_state *s, FILE *wfp, HIR *hir, mrbc_context *__c)
{
  hpc_codegen_context c;

  puts("Generating C-program...");

  c.indent = 0;
  c.mrb = s->mrb;
  c.wfp = wfp;

  put_header(&c);
  /* toplevel is a list of decls */
  put_fun_decls(&c, hir, s->function_map);
  put_new_decls(&c, s->function_map, 4);
  while (hir) {
    put_decl(&c, hir->car);
    hir = hir->cdr;
  }
  put_multiplexers(&c, s);

  return mrb_fixnum_value(0);
}
