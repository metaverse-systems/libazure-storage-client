#!/bin/bash

libtoolize \
&& aclocal \
&& automake --add-missing -c \
&& autoconf
