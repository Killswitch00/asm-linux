#!/bin/sh -e

[ ! -d autom4te.cache ] || rm -fr autom4te.cache

autoreconf --force --install

echo "Now run ./configure and then make"
