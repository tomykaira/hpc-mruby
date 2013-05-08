#ifndef HPCMRB_H
#define HPCMRB_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>
#include "mruby.h"
#include "mruby/compile.h"
#include <setjmp.h>
#include <assert.h>

#define NOT_IMPLEMENTED() assert(0 && "not implemented")
#define NOT_REACHABLE()   assert(0 && "not reachable")
#define hpc_assert(cond)  assert(cond)

/* High-level intermediate representation. */
enum hir_type {
  /* declarations */
  HIR_GVARDECL,   /* (:HIR_GVARDECL type var value) */
  HIR_LVARDECL,   /* (:HIR_LVARDECL type var value) */
  HIR_PVARDECL,   /* (:HIR_PVARDECL type var) */
  HIR_FUNDECL,    /* (:HIR_FUNDECL type (class_sym . sym) (params...) body (classname)) */

  HIR_INIT_LIST,  /* (:HIR_INIT_LIST values...) */

  /* statements */
  HIR_SCOPE,      /* (:HIR_SCOPE (vars...) . statement) */
  HIR_BLOCK,      /* (:HIR_BLOCK (statements...)) */
  HIR_ASSIGN,     /* (:HIR_ASSIGN lhs rhs) */
  HIR_IFELSE,     /* (:HIR_IFELSE cond ifthen ifelse); ifthen and ifelse: stat */
  HIR_DOALL,      /* (:HIR_DOALL var low high body) */
  HIR_WHILE,      /* (:HIR_WHILE cond body) */
  HIR_BREAK,      /* (:HIR_BREAK) */
  HIR_CONTINUE,   /* (:HIR_CONTINUE) */
  HIR_RETURN,     /* (:HIR_RETURN) or (:HIR_RETURN exp) */

  /* expressions */
  HIR_EMPTY,      /* (:HIR_EMPTY) */
  HIR_PRIM,       /* (:HIR_PRIM . primitive_type) */
  HIR_INT,        /* (:HIR_INT text base) */
  HIR_FLOAT,      /* (:HIR_FLOAT text base) */
  HIR_STRING,     /* (:HIR_STRING text length) */
  HIR_LVAR,       /* (:HIR_LVAR . symbol) */
  HIR_GVAR,       /* (:HIR_GVAR . symbol) */
  HIR_IVAR,       /* (:HIR_IVAR . symbol) */
  HIR_CVAR,       /* (:HIR_CVAR . symbol) */
  HIR_CALL,       /* (:HIR_CALL func args...) */
  HIR_COND_OP,    /* (:HIR_COND_OP cond t f) t and f are exp */
  HIR_DEFCLASS,   /* (:HIR_DEFCLASS sym super) super is not supported */
};

enum hir_primitive_type {
  HPTYPE_NIL,
  HPTYPE_FALSE,
  HPTYPE_TRUE,
};

enum hir_type_kind {
  HTYPE_VOID,     /* (:HTYPE_VOID) */
  HTYPE_VALUE,    /* (:HTYPE_VALUE) */ /* corresponds to mrb_value */
  HTYPE_SYM,      /* (:HTYPE_SYM) */   /* corresponds to mrb_sym */

  HTYPE_CHAR,     /* (:HTYPE_CHAR) */
  HTYPE_INT,      /* (:HTYPE_INT) */
  HTYPE_FLOAT,    /* (:HTYPE_FLOAT) */ /* NB: Its precision depends on MRB_USE_FLOAT */
  HTYPE_STRING,   /* (:HTYPE_STRING) */

  HTYPE_PTR,      /* (:HTYPE_PTR basetype) */
  HTYPE_ARRAY,    /* (:HTYPE_ARRAY basetype len) */
  HTYPE_FUNC,     /* (:HTYPE_FUNC ret params) */ /* params is list of HIR_PVARDECL */

  HTYPE_TYPEDEF,  /* (:HTYPE_TYPEDEF text) */ /* for typedef-ed types */
};

enum hir_var_kind {
  HVAR_RB_GVAR, /* mruby's global variable */
  HVAR_RB_CVAR, /* mruby's constant variable */
  HVAR_GVAR,    /* global variable */
  HVAR_LVAR,    /* local variable */
};

typedef struct HIR {
  struct HIR *car, *cdr;
  mrb_value lat;
  short lineno;
} HIR;

typedef struct hpc_state {
  mrb_state *mrb;
  struct mrb_pool *pool;
  HIR *cells;
  HIR *gvars;                   /* found global variables */
  HIR *classes;                 /* found class definitions */
  HIR *function_map;            /* list ((fun name . arg count) . list (class name)) */
  short line;
  jmp_buf jmp;
} hpc_state;

void init_hpc_compiler(hpc_state *p);
void init_prim_interpreters(hpc_state *p);
struct RProc *get_interp(struct RProc *p);
hpc_state* hpc_state_new(mrb_state *mrb);
HIR *hpc_compile_file(hpc_state*, FILE*, mrbc_context*);
mrb_value hpc_generate_code(hpc_state*, FILE*, HIR*, mrbc_context*);
mrb_value hpc_dump_hir(hpc_state*, FILE*, HIR*, mrbc_context*);  

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif  /* HPCMRB_H */
