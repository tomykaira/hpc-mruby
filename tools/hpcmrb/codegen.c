#include "hpcmrb.h"
#include <stdint.h>

#define CADR(x) ((x)->cdr->car)
#define CADDR(x) ((x)->cdr->cdr->car)
#define CADDDR(x) ((x)->cdr->cdr->cdr->car)
#define CADDDDR(x) ((x)->cdr->cdr->cdr->cdr->car)

#define TYPE(x) ((intptr_t)((x)->car))
#define DECLP(t) (t == HIR_GVARDECL || t == HIR_LVARDECL || \
                  t == HIR_PVARDECL)

#define PUTS(str) (fputs((str), c->wfp))

#define INDENT_PP (c->indent ++)
#define INDENT_MM (c->indent --)

typedef struct {
  mrb_state *mrb;
  FILE *wfp;
  int indent;
} hpc_codegen_context;

static void put_exp(hpc_codegen_context *c, HIR *exp);
static void put_statement(hpc_codegen_context *c, HIR *stat);

static void
put_header(hpc_codegen_context *c)
{
  PUTS("/* Generated by HPC mruby compiler */\n");
  PUTS("#include <stdio.h>\n");
  PUTS("#include <stdlib.h>\n");
  PUTS("#include \"mruby.n\"\n");
}

static void
put_type(hpc_codegen_context *c, HIR *kind)
{
  enum hir_type_kind k = (intptr_t)kind->car;

  switch (k) {
    case HTYPE_VOID:
      PUTS("void");
    case HTYPE_VALUE:
      PUTS("mrb_value");
    case HTYPE_SYM:
      PUTS("mrb_sym");
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
      PUTS(kind->cdr->car);
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

static void
put_decl(hpc_codegen_context *c, HIR *decl)
{
  switch (TYPE(decl)) {
    case HIR_GVARDECL:
    case HIR_LVARDECL:
      put_variable(c, decl->cdr);
      if (TYPE(CADDR(decl)) != HIR_EMPTY) {
        PUTS(" = ");
        put_exp(c, CADDR(decl));
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
        put_statement(c, CADDDDR(decl));
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
    case HIR_INT:
    case HIR_FLOAT:
      PUTS((char *)CADR(exp));
      return;
    case HIR_LVAR:
    case HIR_GVAR:
      put_symbol(c, CADR(exp));
      return;
    case HIR_CALL:
      /* FIXME: expects
         - func is symbol
         - args are expressions
       */
      put_symbol(c, CADR(exp));
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
put_statement(hpc_codegen_context *c, HIR *stat)
{
  switch (TYPE(stat)) {
    case HIR_SCOPE:
      {
        /* FIXME: redundant {} when HIR_SCOPE has HIR_BLOCK as a child */
        HIR *decls = CADR(stat);
        HIR *inner_stat = stat->cdr->cdr;
        PUTS("{\n");
        INDENT_PP;
        if (decls) {
          while (decls) {
            put_decl(c, decls->car);
            decls = decls->cdr;
          }
          PUTS("\n");
        }
        put_statement(c, inner_stat);
        INDENT_MM;
        PUTS("}\n");
      }
      return;
    case HIR_BLOCK:
      {
        HIR *stats = stat->cdr->car;
        PUTS("{\n");
        INDENT_PP;
        while (stats) {
          put_statement(c, stats->car);
          stats = stats->cdr;
        }
        INDENT_MM;
        PUTS("}\n");
      }
      return;
    case HIR_ASSIGN:
      /* FIXME: assuming that lhs is sym, rhs is exp */
      put_symbol(c, CADR(stat));
      PUTS(" = ");
      put_exp(c, CADDR(stat));
      PUTS(";\n");
      return;
    case HIR_IFELSE:
      PUTS("if (");
      PUTS(")");
      put_statement(c, CADDR(stat));
      if (CADDDR(stat)) {
        PUTS("else");
        put_statement(c, CADDDR(stat));
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

        PUTS("for (");
        put_symbol(c, sym); PUTS(" = "); put_exp(c, low); PUTS("; ");
        put_symbol(c, sym); PUTS(" < "); put_exp(c, high); PUTS("; ");
        puts("++"); put_symbol(c, sym); PUTS(")");
        put_statement(c, CADDDDR(stat));
      }
      return;
    case HIR_WHILE:
      PUTS("while (");
      put_exp(c, CADR(stat));
      PUTS(") ");
      put_statement(c, CADDR(stat));
      return;
    case HIR_BREAK:
      PUTS("break;\n");
      return;
    case HIR_CONTINUE:
      PUTS("continue;\n");
      return;
    case HIR_RETURN:
      if (stat->cdr) {
        PUTS("return ");
        put_exp(c, CADR(stat));
        PUTS(";\n");
      } else {
        PUTS("return;\n");
      }
      return;
    case HIR_EMPTY:
      /* EMPTY occurs anywhere */
      return;
    case HIR_CALL:
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

  c.mrb = s->mrb;
  c.wfp = stdout;

  put_header(&c);
  /* toplevel is declaration of compiled_main */
  put_decl(&c, hir);

  //while (hir) {
  //  put_decl(fp, hir->car);
  //  hir = hir->cdr;
  //}

  return mrb_fixnum_value(0);
}
