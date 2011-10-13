#!/bin/sh

# Generate Emacs TAGS file for C and Java files in the current directory
# and below it.

# (Can't assume we have -print0.)
# Ignore the 3rparty stuff if run in source.
rm -f TAGS
find $(pwd) -name \*.c -o -name \*.h -o -name \*.java |
  grep -v '#' | grep -v 3rdparty | xargs etags -a
