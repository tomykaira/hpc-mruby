#include "mruby.h"
#include "mruby/proc.h"
#include "mruby/array.h"
#include "mruby/string.h"
#include "mruby/compile.h"
#include "mruby/dump.h"
#include "mruby/variable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern void compiled_main(mrb_value, mrb_state *);

mrb_state *mrb;

FILE *debug_fp = NULL;
int
main(int argc, char **argv)
{
  mrb = mrb_open();
  mrb->gc_disabled = TRUE;
  debug_fp = fopen("result.ppm", "w");

  compiled_main(mrb_top_self(mrb), mrb);

  fclose(debug_fp);
  mrb_close(mrb);

  return 0;
  /* return n == 0 ? EXIT_SUCCESS : EXIT_FAILURE; */
}
