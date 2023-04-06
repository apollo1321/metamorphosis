#!/bin/bash

set -e

build_dir=$1

mkdir -p ${HOME}/.cmake/_deps
rsync -P ${build_dir}/_deps/ ~/.cmake/_deps -r --include "*/" --include "*.tar.gz" --exclude "*" ${HOME}/.cmake/_deps/
