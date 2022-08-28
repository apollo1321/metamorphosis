#!/bin/sh

docker run -d \
  -it \
  --name ceq-dev \
  --mount type=bind,source="$(pwd)"/..,target=/app \
  ceq-dev

docker exec -it ceq-dev bash
