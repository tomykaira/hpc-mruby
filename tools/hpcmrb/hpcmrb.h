#ifndef HPCMRB_H
#define HPCMRB_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>
#include "mruby.h"
#include "mruby/compile.h"
#include <setjmp.h>

#define NOT_IMPLEMENTED() mrb_bug("%s(%d): not implemented", __func__, __LINE__)
#define NOT_REACHABLE()   mrb_bug("%s(%d): not reachable here", __func__, __LINE__)
#define hpc_assert(cond)  do { if (!(cond)) { NOT_REACHABLE(); } } while(0)

/* High-level intermediate representation. */
enum hir_type {
  /* declarations */
  HIR_GVARDECL,   /* (:HIR_GVARDECL var value) */
  HIR_LVARDECL,   /* (:HIR_LVARDECL var value) */
  HIR_PVARDECL,   /* (:HIR_PVARDECL var) */
  HIR_FUNDECL,    /* (:HIR_FUNDECL var body (options...)) */

  HIR_INIT_LIST,  /* (:HIR_INIT_LIST values...) */

  /* statements */
  HIR_BLOCK,      /* (:HIR_BLOCK declaration* statement*) */
  HIR_ASSIGN,     /* (:HIR_ASSIGN lhs rhs) */
  HIR_IFELSE,     /* (:HIR_IFELSE cond ifthen ifelse) */
  HIR_DOALL,      /* (:HIR_DOALL var low high body) */
  HIR_WHILE,      /* (:HIR_WHILE cond body) */
  HIR_BREAK,      /* (:HIR_BREAK) */
  HIR_CONTINUE,   /* (:HIR_CONTINUE) */
  HIR_RETURN,     /* (:HIR_RETURN) */

  /* expressions */
  HIR_EMPTY,      /* (:HIR_EMPTY) */
  HIR_INT,        /* (:HIR_INT value) */
  HIR_FLOAT,      /* (:HIR_FLOAT value) */
  HIR_VAR,        /* (:HIR_VAR mrb_sym type) */
  HIR_CALL,       /* (:HIR_CALL func args...) */

  /* others */
  HIR_TYPE,       /* (:HIR_TYPE hir_type_kind) */
};

enum hir_type_kind {
  HTYPE_VALUE, /* corresponds to mrb_value */
  HTYPE_SYM,   /* corresponds to mrb_sym */

  HTYPE_CHAR,
  HTYPE_INT,
  HTYPE_FLOAT, /* NB: Its precision depends on MRB_USE_FLOAT */
  HTYPE_STRING,

  HTYPE_PTR,
  HTYPE_ARRAY,
  HTYPE_FUNC,
};

enum hir_var_kind {
  HVAR_RB_GVAR, /* mruby's global variable */
  HVAR_RB_CVAR, /* mruby's constant variable */
  HVAR_GVAR,    /* global variable */
  HVAR_LVAR,    /* local variable */
};

typedef struct HIR {
  struct HIR *car, *cdr;
  short lineno;
} HIR;

typedef struct hpc_state {
  mrb_state *mrb;
  struct mrb_pool *pool;
  HIR *cells;
  short line;
  jmp_buf jmp;
} hpc_state;

void init_hpc_compiler(mrb_state *mrb);
hpc_state* hpc_state_new(mrb_state *mrb);
HIR *hpc_compile_file(hpc_state*, FILE*, mrbc_context*);
mrb_value hpc_generate_code(hpc_state*, FILE*, HIR*, mrbc_context*);

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif  /* HPCMRB_H */
