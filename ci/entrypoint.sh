#! /bin/bash

set -x

# script always exits with non-zero codes for some reason...
source /opt/qt*/bin/qt*-env.sh

exec "$@"
