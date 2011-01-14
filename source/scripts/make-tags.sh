#!/bin/sh

# Generate Emacs TAGS file for C and Java files in the current directory
# and below it.

# (Can't assume we have -print0.)
find $(pwd) -name \*.c -o -name \*.h -o -name \*.java | xargs etags -a
