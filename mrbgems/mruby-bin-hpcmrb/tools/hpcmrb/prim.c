#include "hpcmrb.h"
#include "mruby.h"
#include "mruby/class.h"
#include "mruby/proc.h"
#include "mruby/variable.h"

/* Abstrac interpreters for builtin methods. */

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
  int ai = mrb_gc_arena_save(mrb);
  p = mrb_method_search_vm(mrb, &c, mid);
  hpc_assert(p);

  q = mrb_proc_new_cfunc(mrb, func);
  q->target_class = c;
  p->interp = q;

  mrb_gc_arena_restore(mrb, ai);
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
  add_interp(mrb, mrb->kernel_module, "__printstr__", prim_printstr, ARGS_REQ(1));
}
