#include "mruby.h"

mrb_value num_add_1(int val, mrb_value, mrb_value);

mrb_value num_addi_1(int val, mrb_value, mrb_int);

mrb_value num_sub_1(int val, mrb_value, mrb_value);

mrb_value num_subi_1(int val, mrb_value, mrb_int);

mrb_value num_mul_1(int val, mrb_value, mrb_value);

mrb_value num_div_1(int val, mrb_value, mrb_value);

mrb_value num_xor_1(int val, mrb_value a, mrb_value b);
mrb_value num_lshift_1(int val, mrb_value a, mrb_value b);
mrb_value num_rshift_1(int val, mrb_value a, mrb_value b);
mrb_value num_and_1(int val, mrb_value a, mrb_value b);
mrb_value num_mod_1(int val, mrb_value a, mrb_value b);

mrb_value num_eq_1(int val, mrb_value, mrb_value);

mrb_value num_lt_1(int val, mrb_value, mrb_value);

mrb_value num_le_1(int val, mrb_value, mrb_value);

mrb_value num_gt_1(int val, mrb_value, mrb_value);

mrb_value num_ge_1(int val, mrb_value, mrb_value);

mrb_value puts_1(int val, mrb_value __self__, mrb_value n);
mrb_value print_1(int val, mrb_value __self__, mrb_value n);

mrb_value hpc_ary_aget_1(int val, mrb_value __self__, mrb_value index);
mrb_value hpc_ary_aset_2(int val, mrb_value __self__, mrb_value index, mrb_value value);

mrb_value sqrt_1(int val, mrb_value __self__, mrb_value);
mrb_value num_uminus_0(int val, mrb_value __self__);
mrb_value to_i_0(int val, mrb_value __self__);
mrb_value to_f_0(int val, mrb_value __self__);
mrb_value chr_0(int val, mrb_value __self__);
mrb_value cos_1(int val, mrb_value __self__, mrb_value);
mrb_value sin_1(int val, mrb_value __self__, mrb_value);
mrb_value to_s_0(int val, mrb_value __self__);
mrb_value bob_not_0(int val, mrb_value __self__);
