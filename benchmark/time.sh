#!/bin/sh

# call time and export in TSV format
# time.sh bench_name machine_name program_name

bench=$1
shift

machine=$1
shift

program=$1
shift

time -f"$bench	$machine	$program	%U" $*
