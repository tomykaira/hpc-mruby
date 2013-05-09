#include "mruby.h"

mrb_value num_add(mrb_value, mrb_value);

mrb_value num_addi(mrb_value, mrb_int);

mrb_value num_sub(mrb_value, mrb_value);

mrb_value num_subi(mrb_value, mrb_int);

mrb_value num_mul(mrb_value, mrb_value);

mrb_value num_div(mrb_value, mrb_value);

mrb_value num_eq(mrb_value, mrb_value);

mrb_value num_lt(mrb_value, mrb_value);

mrb_value num_le(mrb_value, mrb_value);

mrb_value num_gt(mrb_value, mrb_value);

mrb_value num_ge(mrb_value, mrb_value);

mrb_value puts_1(mrb_value __self__, mrb_value n);
mrb_value print_1(mrb_value __self__, mrb_value n);

mrb_value hpc_ary_aget(mrb_value __self__, mrb_value index);
mrb_value hpc_ary_aset(mrb_value __self__, mrb_value index, mrb_value value);
