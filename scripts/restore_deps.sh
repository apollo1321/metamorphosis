
set -e

build_type=$1

rsync -P ${HOME}/.cmake/_deps build-${build_type}/ -r --include "*/" --include "*.tar.gz" --exclude "*"
