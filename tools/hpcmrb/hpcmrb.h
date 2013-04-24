#ifndef HPCMRB_H
#define HPCMRB_H

#if defined(__cplusplus)
extern "C" {
#endif

#include <stdio.h>
#include "mruby.h"
#include "mruby/compile.h"

/* High-level intermediate representation. */
struct HIR {
};


struct HIR *hpc_compile_file(mrb_state*, FILE*, mrbc_context*);
mrb_value hpc_generate_code(mrb_state*, FILE*, struct HIR*, mrbc_context*);

#if defined(__cplusplus)
}  /* extern "C" { */
#endif

#endif  /* HPCMRB_H */
