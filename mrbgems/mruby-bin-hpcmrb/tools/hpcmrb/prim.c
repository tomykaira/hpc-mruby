#include "hpcmrb.h"
#include "mruby.h"
#include "mruby/class.h"
#include "mruby/khash.h"
#include "mruby/proc.h"
#include "mruby/variable.h"

/* Abstrac interpreters for builtin methods. */

#define ptr_hash_func(mrb,key) (khint_t)(((intptr_t)key)>>33^((intptr_t)key)^((intptr_t)key)<<11)
#define ptr_hash_equal(mrb,a, b) (a == b)

KHASH_DECLARE(interp, struct RProc*, struct RProc*, 1)
KHASH_DEFINE(interp, struct RProc*, struct RProc*, 1, ptr_hash_func, ptr_hash_equal)

static khash_t(interp) *interp_tbl;

static mrb_value
prim_printstr(mrb_state *mrb, mrb_value self)
{
  mrb_value argv;
  mrb_get_args(mrb, "o", &argv);
  return argv;
}

static void
add_interp_id(mrb_state *mrb, struct RClass *c, mrb_sym mid, mrb_func_t func, mrb_aspec aspec)
{
  struct RProc *p, *q;
  khiter_t k;
  int ai = mrb_gc_arena_save(mrb);
  p = mrb_method_search_vm(mrb, &c, mid);
  hpc_assert(p);

  q = mrb_proc_new_cfunc(mrb, func);
  q->target_class = c;

  k = kh_put(interp, interp_tbl, p);
  kh_value(interp_tbl, k) = q;

  mrb_gc_arena_restore(mrb, ai);
}

struct RProc*
get_interp(struct RProc *key)
{
  khiter_t k;
  k = kh_get(interp, interp_tbl, key);
  if (k != kh_end(interp_tbl))
    return kh_value(interp_tbl, k);
  return 0;
}

static void
add_interp(mrb_state *mrb, struct RClass *c, const char *name, mrb_func_t func, mrb_aspec aspec)
{
  add_interp_id(mrb, c, mrb_intern(mrb, name), func, aspec);
}

void
init_prim_interpreters(hpc_state *p)
{
  mrb_state *mrb = p->mrb;
  interp_tbl = kh_init(interp, mrb);
  add_interp(mrb, mrb->kernel_module, "__printstr__", prim_printstr, ARGS_REQ(1));
}
