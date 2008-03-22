#!/bin/sh
aclocal
autoreconf
automake --add-missing --copy
autoreconf
libtoolize -f --automake

