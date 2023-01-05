#!/usr/bin/env bash

set -e

HEADER_LINE="DownstreamTlsContext();"
SOURCE_LINE="XdsListenerResource::DownstreamTlsContext::DownstreamTlsContext() {}"

if [[ $(sed -n 82p $1) != $HEADER_LINE ]]; then
  sed -ibak "81a $HEADER_LINE" $1
fi

if [[ $(sed -n 66p $2) != $SOURCE_LINE ]]; then
  sed -ibak "65a $SOURCE_LINE" $2
fi
