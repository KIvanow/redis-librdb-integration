#!/bin/bash

# Exit on error
set -e

LIBRDB_DIR="./deps/librdb"

# Create deps directory if it doesn't exist
mkdir -p deps

# Clone librdb if it doesn't exist
if [ ! -d "$LIBRDB_DIR" ]; then
    git clone https://github.com/redis/librdb.git "$LIBRDB_DIR"
fi

# Enter librdb directory
cd "$LIBRDB_DIR"

# Initialize and update submodules (for hiredis)
git submodule update --init

# Build librdb
make CFLAGS='-flto=auto'

# Install to local project directory instead of system-wide
make CFLAGS='-flto=auto' install PREFIX="$(pwd)/../../local"
