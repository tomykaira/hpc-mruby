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

int
main(int argc, char **argv)
{
  mrb = mrb_open();
  int ret = -1;

  /*
  int i;
  mrb_value ARGV;

  if (mrb == NULL) {
    fputs("Invalid mrb_state, exiting mruby\n", stderr);
    return EXIT_FAILURE;
  }

  argc--;
  argv++;

  ARGV = mrb_ary_new_capa(mrb, argc);
  for (i = 0; i < argc; i++) {
    mrb_ary_push(mrb, ARGV, mrb_str_new(mrb, argv[i], strlen(argv[i])));
  }
  mrb_define_global_const(mrb, "ARGV", ARGV);
  */
  /* mrb_gv_set(mrb, mrb_intern2(mrb, "$0", 2), ...) */

  compiled_main(mrb_fixnum_value(0), mrb);

  mrb_close(mrb);

  return ret;
  /* return n == 0 ? EXIT_SUCCESS : EXIT_FAILURE; */
}
