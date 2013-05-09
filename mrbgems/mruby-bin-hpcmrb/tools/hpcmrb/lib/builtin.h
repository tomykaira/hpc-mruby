#include "mruby.h"

mrb_value num_add_1(mrb_value, mrb_value);

mrb_value num_addi_1(mrb_value, mrb_int);

mrb_value num_sub_1(mrb_value, mrb_value);

mrb_value num_subi_1(mrb_value, mrb_int);

mrb_value num_mul_1(mrb_value, mrb_value);

mrb_value num_div_1(mrb_value, mrb_value);

mrb_value num_eq_1(mrb_value, mrb_value);

mrb_value num_lt_1(mrb_value, mrb_value);

mrb_value num_le_1(mrb_value, mrb_value);

mrb_value num_gt_1(mrb_value, mrb_value);

mrb_value num_ge_1(mrb_value, mrb_value);

mrb_value puts_1(mrb_value __self__, mrb_value n);
mrb_value print_1(mrb_value __self__, mrb_value n);

mrb_value hpc_ary_aget_1(mrb_value __self__, mrb_value index);
mrb_value hpc_ary_aset_2(mrb_value __self__, mrb_value index, mrb_value value);
