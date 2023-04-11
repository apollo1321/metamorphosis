#!/bin/bash

set -e

build_type=$1

mkdir -p ${HOME}/.cmake/_deps
rsync -P build-${build_type}/_deps/ ~/.cmake/_deps -r --include "*/" --include "*.tar.gz" --exclude "*" ${HOME}/.cmake/_deps/
