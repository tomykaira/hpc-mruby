#ifndef HPCMRB_H
#define HPCMRB_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>
#include "mruby.h"
#include "mruby/compile.h"

#define NOT_IMPLEMENTED() mrb_bug("%s(%d): not implemented", __func__, __LINE__)
#define NOT_REACHABLE()   mrb_bug("%s(%d): not reachable here", __func__, __LINE__)

/* High-level intermediate representation. */
enum hir_type {
  /* declarations */
  HIR_GVARDECL,
  HIR_LVARDECL,
  HIR_PVARDECL,
  HIR_FUNDECL,

  HIR_INIT_LIST,

  /* statements */
  HIR_SEQ,
  HIR_ASSIGN,
  HIR_IFELSE,
  HIR_DOALL,
  HIR_WHILE,
  HIR_BREAK,
  HIR_CONTINUE,
  HIR_RETURN,

  /* expressions */
  HIR_EMPTY,
  HIR_LIT,
  HIR_VAR,
  HIR_CALL,
  HIR_ADDRESSOF,

  /* others */
  HIR_TYPE,
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

HIR *hpc_compile_file(mrb_state*, FILE*, mrbc_context*);
mrb_value hpc_generate_code(mrb_state*, FILE*, HIR*, mrbc_context*);

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif  /* HPCMRB_H */
