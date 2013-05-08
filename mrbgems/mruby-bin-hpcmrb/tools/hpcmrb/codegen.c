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

static void put_exp(hpc_codegen_context *c, HIR *exp);
static void put_statement(hpc_codegen_context *c, HIR *stat, int no_brace);

static void
put_header(hpc_codegen_context *c)
{
  PUTS("/* Generated by HPC mruby compiler */\n");
  PUTS("#include <stdio.h>\n");
  PUTS("#include <stdlib.h>\n");
  PUTS("#include \"mruby.n\"\n\n");
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
put_name_for_sym(hpc_codegen_context *c, mrb_sym sym)
{
  size_t len;
  int i;
  const char *name = mrb_sym2name_len(c->mrb, (mrb_sym)(intptr_t)sym, &len);
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
    {"[]", "mrb_ary_aget"},
    {"[]=", "mrb_ary_aset"},
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

static void
put_function_name(hpc_codegen_context *c, HIR *sym)
{
  if (! put_name_for_sym(c, (mrb_sym)(intptr_t)sym)) {
    put_symbol(c, sym);
  }
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
        HIR *funtype = CADR(decl);
        HIR *params = CADDDR(decl);
        hpc_assert((intptr_t)funtype->car == HTYPE_FUNC);
        put_type(c, CADR(funtype));
        PUTS("\n");
        put_symbol(c, CADDR(decl));
        PUTS("(");
        while (params) {
          hpc_assert((intptr_t)params->car->car == HIR_PVARDECL);
          put_decl(c, params->car);
          params = params->cdr;
          if (params) {
            PUTS(", ");
          }
        }
        PUTS(")\n");
        PUTS("{\n");
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
      {
        char len[32];
        sprintf(len, "%d", (int)(intptr_t)CADDR(exp));
        PUTS(len);
      }
      PUTS(")");
      return;
    case HIR_LVAR:
    case HIR_GVAR:
      put_symbol(c, exp->cdr);
      return;
    case HIR_CALL:
      /* FIXME: expects
         - func is symbol
         - args are expressions
       */
      put_function_name(c, CADR(exp));
      PUTS("(");
      {
        HIR *args = exp->cdr->cdr;
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
      hpc_assert(TYPE(CADR(stat)) == HIR_LVAR
                 || TYPE(CADR(stat)) == HIR_GVAR);
      put_symbol(c, CADR(stat)->cdr);
      PUTS(" = ");
      put_exp(c, CADDR(stat));
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

        PUTS_INDENT;
        PUTS("for (");
        put_symbol(c, sym); PUTS(" = "); put_exp(c, low); PUTS("; ");
        put_symbol(c, sym); PUTS(" < "); put_exp(c, high); PUTS("; ");
        PUTS("++"); put_symbol(c, sym); PUTS(")\n");
        put_statement(c, CADDDDR(stat), FALSE);
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
        PUTS("return;\n");
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

mrb_value
hpc_generate_code(hpc_state *s, FILE *wfp, HIR *hir, mrbc_context *__c)
{
  hpc_codegen_context c;

  puts("Generating C-program...");

  c.indent = 0;
  c.mrb = s->mrb;
  c.wfp = stdout;

  put_header(&c);
  /* toplevel is a list of decls */
  while (hir) {
    put_decl(&c, hir->car);
    hir = hir->cdr;
  }

  return mrb_fixnum_value(0);
}
