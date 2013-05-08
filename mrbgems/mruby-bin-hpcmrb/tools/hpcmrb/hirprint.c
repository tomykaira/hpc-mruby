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
#define PUTS_INDENT do {                        \
    int __i;                                    \
    for (__i = 0; __i < c->indent; ++__i) {     \
      PUTS("\t");                               \
    }                                           \
} while(0)

#define INDENT_PP (c->indent ++)
#define INDENT_MM (c->indent --)

typedef struct {
  mrb_state *mrb;
  FILE *wfp;
  int indent;
} hpc_hirprint_context;

static void put_exp(hpc_hirprint_context *c, HIR *exp);
static void put_statement(hpc_hirprint_context *c, HIR *stat);

static void
put_type(hpc_hirprint_context *c, HIR *kind)
{
  enum hir_type_kind k = (intptr_t) kind->car;

  switch(k){
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
    PUTS("float");
    return;
  case HTYPE_STRING:
    PUTS("string");
    return;
  case HTYPE_PTR:
    put_type(c, CADR(kind));
    PUTS("ptr");
    return;
  case HTYPE_ARRAY:
  case HTYPE_FUNC:
    NOT_REACHABLE();
    return;
  case HTYPE_TYPEDEF:
    PUTS((char *)CADR(kind));
    return;
  default:
    NOT_REACHABLE();
  }
}

static void
put_symbol(hpc_hirprint_context *c, HIR *hir)
{
  mrb_sym sym = (intptr_t)hir;
  const char * name = mrb_sym2name(c->mrb, sym);
  PUTS(name);
}

static void
put_variable(hpc_hirprint_context *c, HIR *hir)
{
  HIR *type = hir->car;
  enum hir_type_kind k = (intptr_t) type->car;
  HIR *var = CADR(hir);

  switch(k){
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
    {
      INDENT_PP;
      PUTS((char*)CADR(type));
      type = CADDR(type);
      while(type){
        PUTS(" ");
        put_statement(c, type->car);
        type = type->cdr;
      }
      INDENT_MM;
    }
    return;
  default:
    put_type(c, type);
    PUTS(" ");
    put_symbol(c, var);
    return;
  }
}

void put_vardecl(hpc_hirprint_context *c, struct HIR* tree, char *vartype)
{
  INDENT_PP;
  PUTS("(:");
  PUTS(vartype);
  put_type(c, CADR(tree));
  PUTS(" ");
  put_symbol(c, CADDR(tree));

  if(CADDDR(tree)){
    PUTS(" ");
    put_statement(c, CADDDR(tree));
  }

  PUTS(")");
  INDENT_MM;
}

static void
put_decl(hpc_hirprint_context *c, HIR *decl)
{
  PUTS_INDENT;
  switch(TYPE(decl)){
  case HIR_GVARDECL:
    PUTS("(:HIR_GVARDECL ");
    put_variable(c, decl->cdr);
    if (TYPE(CADDDR(decl)) != HIR_EMPTY) {
      put_exp(c, CADDDR(decl));
    }
    PUTS(")");
    return;
  case HIR_LVARDECL:
    PUTS("(:HIR_LVARDECL ");
    put_variable(c, decl->cdr);
    if (TYPE(CADDDR(decl)) != HIR_EMPTY) {
      put_exp(c, CADDDR(decl));
    }
    PUTS(")");
    return;
  case HIR_PVARDECL:
    PUTS("(:HIR_PVARDECL ");
    put_variable(c, decl->cdr);
    PUTS(")");
    return;
  case HIR_FUNDECL:
    {
      PUTS("(:HIR_FUNDECL ");
      HIR *funtype = CADR(decl);
      HIR *params = CADDDR(decl);
      hpc_assert((intptr_t)funtype->car == HTYPE_FUNC);
      put_type(c, CADR(funtype));
      PUTS(" ");
      put_symbol(c, CADDR(decl));
      PUTS("\n");
      INDENT_PP;
      while(params){
        hpc_assert((intptr_t)params->car->car == HIR_PVARDECL);
        put_decl(c, params->car);
        params = params->cdr;
        if (params) {
          PUTS("\n");
        }
      }
      PUTS("\n");
      put_statement(c, CADDDDR(decl));
      INDENT_MM;
      PUTS(")");
    }
    return;
  default:
    NOT_REACHABLE();
  }
}

static void
put_function_name(hpc_hirprint_context *c, HIR *sym)
{
  size_t len;
  const char *name = mrb_sym2name_len(c->mrb, (mrb_sym)(intptr_t)sym, &len);

  if (len == 1 && name[0] == '+')  {
    PUTS("num_add");
  }
  else if (len == 1 && name[0] == '-')  {
    PUTS("num_sub");
  }
  else if (len == 1 && name[0] == '*')  {
    PUTS("num_mul");
  }
  else if (len == 1 && name[0] == '/')  {
    PUTS("num_div");
  }
  else if (len == 1 && name[0] == '<')  {
    PUTS("num_lt");
  }
  else if (len == 2 && name[0] == '<' && name[1] == '=')  {
    PUTS("num_le");
  }
  else if (len == 1 && name[0] == '>')  {
    PUTS("num_gt");
  }
  else if (len == 2 && name[0] == '>' && name[1] == '=')  {
    PUTS("num_ge");
  }
  else if (len == 2 && name[0] == '=' && name[1] == '=')  {
    PUTS("num_eq");
  }
  else if (len == 1 && name[0] == '!')  {
    PUTS("mrb_bob_not");
  }
  else {
    put_symbol(c, sym);
  }
}

static void
put_exp(hpc_hirprint_context *c, HIR *exp)
{
  PUTS_INDENT;
  switch (TYPE(exp)) {
  case HIR_PRIM:
    PUTS("(:HIR_PRIM . ");
    switch ((enum hir_primitive_type)exp->cdr) {
    case HPTYPE_NIL:
      PUTS("mrb_nil_value)");
      return;
    case HPTYPE_FALSE:
      PUTS("mrb_false_value)");
      return;
    case HPTYPE_TRUE:
      PUTS("mrb_true_value)");
      return;
    }
  case HIR_INT:
    {
      char buff[100];
      PUTS("(:HIR_INT ");
      PUTS((char *)CADR(exp));
      sprintf(buff, " %d)", (int)((intptr_t)CADDR(exp)));
      PUTS(buff);
    }
    return;
  case HIR_FLOAT:
    {
      char buff[32];
      PUTS("(:HIR_FLOAT ");
      PUTS((char *)CADR(exp));
      sprintf(buff, " %d)", (int)(intptr_t)CADDR(exp));
      PUTS(buff);
    }
    return;
  case HIR_STRING:
    {
      char len[32];
      PUTS("(:HIR_STRING ");
      PUTS((char *)CADR(exp));
      sprintf(len, " %d)", (int)(intptr_t)CADDR(exp));
      PUTS(len);
    }
    return;
  case HIR_LVAR:
    PUTS("(:HIR_LVAR . ");
    put_symbol(c, exp->cdr);
    PUTS(")");
    return;
  case HIR_GVAR:
    PUTS("(:HIR_GVAR . ");
    put_symbol(c, exp->cdr);
    PUTS(")");
    return;
  case HIR_CALL:
    /* FIXME: expects
       - func is symbol
       - args are expressions
    */
    PUTS("(:HIR_CALL ");
    put_function_name(c, CADR(exp));
    {
      INDENT_PP;
      HIR *args = exp->cdr->cdr;
      while (args) {
        PUTS("\n");
        put_exp(c, args->car);
        args = args->cdr;
      }
      INDENT_MM;
    }
    PUTS(")");
    return;
  case HIR_COND_OP:
    PUTS("(:HIR_COND_OP\n");
    INDENT_PP;
    put_exp(c, CADR(exp));
    PUTS("\n");
    put_exp(c, CADDR(exp));
    PUTS("\n");
    put_exp(c, CADDDR(exp));
    PUTS(")");
    INDENT_MM;
    return;
  case HIR_INIT_LIST:
    PUTS("(:HIR_INIT_LIST ");
    {
      HIR *values = exp->cdr;
      while (values) {
        put_exp(c, values->car);
        values = values->cdr;
        if (values)
          PUTS(" ");
      }
    }
    PUTS(")");
    return;
  case HIR_BLOCK:
    /* FIXME: ruby: (stat1; stat2; stat3) return the result of stat3 */
    {
      HIR *exps = exp->cdr->car;
      PUTS("(:HIR_BLOCK");
      INDENT_PP;
      while (exps) {
        PUTS("\n");
        put_exp(c, exps->car);
        if (exps->cdr)
          PUTS(" ");
        exps = exps->cdr;
      }
      INDENT_MM;
      PUTS(")");
    }
    return;
  default:
    NOT_REACHABLE();
  }
}

static void
put_statement(hpc_hirprint_context *c, HIR *stat)
{
  PUTS_INDENT;
  switch (TYPE(stat)) {
    case HIR_SCOPE:
      {
        PUTS("(:HIR_SCOPE");
        HIR *decls = CADR(stat);
        HIR *inner_stat = stat->cdr->cdr;
        INDENT_PP;
        if (decls) {
          while (decls) {
            PUTS("\n");
            put_decl(c, decls->car);
            decls = decls->cdr;
          }
        }
        PUTS("\n");
        put_statement(c, inner_stat);
        PUTS(")");
        INDENT_MM;
      }
      return;
    case HIR_BLOCK:
      {
        PUTS("(:HIR_BLOCK");
        HIR *stats = stat->cdr->car;
        INDENT_PP;
        while (stats) {
          PUTS("\n");
          put_statement(c, stats->car);
          stats = stats->cdr;
        }
        INDENT_MM;
        PUTS(")");
      }
      return;
    case HIR_ASSIGN:
      /* lhs is HIR_LVAR or HIR_GVAR,
         rhs is exp */
      PUTS("(:HIR_ASSIGN ");
      hpc_assert(TYPE(CADR(stat)) == HIR_LVAR
                 || TYPE(CADR(stat)) == HIR_GVAR);
      INDENT_PP;
      put_symbol(c, CADR(stat)->cdr);
      PUTS("\n");
      put_exp(c, CADDR(stat));
      INDENT_MM;
      PUTS(")");
      return;
    case HIR_IFELSE:
      PUTS("(:HIR_IFELSE\n");
      INDENT_PP;
      put_exp(c, CADR(stat));

      if (CADDR(stat)) {
        PUTS("\n");
        put_statement(c, CADDR(stat));
      }
      if (CADDDR(stat)) {
        PUTS("\n");
        put_statement(c, CADDDR(stat));
      }
      INDENT_MM;
      PUTS(")");
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

        PUTS("(:HIR_DOALL ");
        INDENT_PP;
        put_symbol(c, sym); PUTS("\n");
        put_exp(c, low); PUTS("\n");
        put_symbol(c, sym); PUTS("\n");
        put_exp(c, high); PUTS("\n");
        put_symbol(c, sym); PUTS(")\n");
        put_statement(c, CADDDDR(stat));
        PUTS(")");
      }
      return;
    case HIR_WHILE:
      PUTS("(:HIR_WHILE ");
      put_exp(c, CADR(stat));
      PUTS("\n");
      put_statement(c, CADDR(stat));
      PUTS(")");
      return;
    case HIR_BREAK:
      PUTS("(:HIR_BREAK)");
      return;
    case HIR_CONTINUE:
      PUTS("(:HIR_CONTINUE)");
      return;
    case HIR_RETURN:
      PUTS("(:HIR_RETURN");
      if (stat->cdr) {
        PUTS("\n");
        put_exp(c, CADR(stat));
      }
      PUTS(")");
      return;
    case HIR_EMPTY:
      PUTS("(:HIR_EMPTY)");
      return;
    case HIR_CALL:
      PUTS("(:HIR_CALL\n");
      INDENT_PP;
      put_exp(c, stat);
      INDENT_MM;
      PUTS(")");
      return;
    default:
      NOT_REACHABLE();
  }
}

mrb_value
hpc_dump_hir(hpc_state *s, FILE *wfp, HIR *hir, mrbc_context *__c)
{
  hpc_hirprint_context c;

  puts("Dump struct HIR");

  c.indent = 0;
  c.mrb = s->mrb;
  c.wfp = stdout;

  while(hir) {
    put_decl(&c, hir->car);
    hir = hir->cdr;
    if(hir) fputs("\n", c.wfp);
  }
  puts("");
  return mrb_fixnum_value(0);
}
