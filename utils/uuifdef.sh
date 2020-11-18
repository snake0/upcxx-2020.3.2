#!/bin/bash

# This utility generates (hopefully unique) ifdef/define guards 
# suitable for pasting into header files.

x="_$(uuidgen|tr \\- _)"
echo "#ifndef $x"
echo "#define $x"
echo "#endif"
