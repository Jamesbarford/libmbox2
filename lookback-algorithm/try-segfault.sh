#!/usr/bin/env bash

set -e

for ((i=0;i<1000;++i)); do
    ./test
    echo "---${i}---"
done
