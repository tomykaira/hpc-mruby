#include "hpcmrb.h"
#include <stdint.h>

#if 0

#define CADR(x) ((x)->cdr->car)
#define CADDR(x) ((x)->cdr->cdr->car)
#define CADDDR(x) ((x)->cdr->cdr->cdr->car)

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
  hpc_assert(TYPE(kind) == HIR_TYPE);

  enum hir_type_kind k = (intptr_t)CADR(kind);

  switch (k) {
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
      PUTS("void *");
      return;
    case HTYPE_ARRAY:
      PUTS("void **");
      return;
    default:
      NOT_IMPLEMENTED();
  }
}

static void
put_symbol(hpc_codegen_context *c, HIR *hir)
{
  mrb_sym sym = (intptr_t)hir;
  const char * name = mrb_sym2name(c->mrb, sym);
  PUTS(name);
}

/*
  FIXME:
  def ::= (symbol . type)

  Output:
      int foo
 */
static void
put_var_definition(hpc_codegen_context *c, HIR *def)
{
  put_type(c, CADR(def));
  PUTS(" ");
  put_symbol(c, def->car);
}

static void
put_decl(hpc_codegen_context *c, HIR *decl)
{
  switch (TYPE(decl)) {
    case HIR_GVARDECL:
      /* FIXME: what is 'var'? */
      put_var_definition(c, CADR(decl));
      PUTS(" = ");
      put_exp(c, CADDR(decl));
      PUTS(";\n");
      return;
    case HIR_LVARDECL:
      put_var_definition(c, CADR(decl));
      PUTS(" = ");
      put_exp(c, CADDR(decl));
      PUTS(";\n");
      return;
    case HIR_PVARDECL:
      put_var_definition(c, CADR(decl));
      PUTS(";\n");
      return;
    case HIR_FUNDECL:
      NOT_IMPLEMENTED();
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
  char buf[1024];
  switch (TYPE(exp)) {
    case HIR_EMPTY:
      /* FIXME */
      return;
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
        while (decls) {
          put_decl(c, decls->car);
          decls = decls->cdr;
        }
        PUTS("\n");
        put_statement(c, inner_stat);
        INDENT_MM;
        PUTS("}\n");
      }
      return;
    case HIR_BLOCK:
      {
        HIR *stats = stat->cdr;
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
         - low, high are int (TODO: it can be variable)
         - body is a statement
      */
      {
        int low  = (intptr_t)CADDR(stat);
        int high = (intptr_t)CADDDR(stat);
        mrb_sym sym = (intptr_t)CADR(stat);
        const char * var_name = mrb_sym2name(c->mrb, sym);
        char buf[1024];

        hpc_assert(low <= high);

        sprintf(buf, "for (%s = %d; %s < %d; %s++)",
                var_name, low, var_name, high, var_name);
        PUTS(buf);
        put_statement(c, stat->cdr->cdr->cdr->cdr->car);
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
      PUTS("return;\n");
      return;
    default:
      NOT_REACHABLE();
  }
}

#endif

mrb_value
hpc_generate_code(hpc_state *s, FILE *wfp, HIR *hir, mrbc_context *__c)
{
#if 0
  hpc_codegen_context c;

  puts("Generating C-program...");

  c.mrb = s->mrb;
  c.wfp = wfp;

  put_header(&c);

  //while (hir) {
  //  put_decl(fp, hir->car);
  //  hir = hir->cdr;
  //}

  return mrb_fixnum_value(0);
#endif
  return mrb_fixnum_value(0);
}
