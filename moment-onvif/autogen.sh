#!/bin/sh

# Simplistic autogen.sh.
# It is this simple because I don't (yet?) get what else could this script do.
#    - DMS

aclocal                   || exit 1
libtoolize --copy --force || exit 1
autoconf                  || exit 1
automake -acf             || exit 1

echo "You may now run ./configure && make"

