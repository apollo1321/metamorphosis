#!/bin/bash

set -e

build_dir=$1

rsync -P ${HOME}/.cmake/_deps ${build_dir}/ -r --include "*/" --include "*.tar.gz" --exclude "*"
