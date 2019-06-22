#!/usr/bin/env bash

# make gravity
make clean gravity OPENSSL_ENABLED=${OPENSSL_ENABLED}

# run example
rm -f examples/http/main.json
gravity -c examples/http/main.gravity -o examples/http/main.json
gravity -x examples/http/main.json