#!/bin/sh

RBENV_ROOT=`rbenv root`

targets="1.9.3-p392 2.0.0-p0 jruby-1.7.3 rbx-2.0.0-rc1 topaz-dev"

for version in $targets; do
  echo $version

  bin=`RBENV_VERSION=$version rbenv which ruby`

  if [ $? -eq 0 ]; then
    time -p $bin $1
  fi

  echo ""
done
