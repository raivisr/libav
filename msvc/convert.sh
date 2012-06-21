#!/bin/bash
file=$1
if test x$file = x; then
    echo "Please give a file to convert."
    exit 1
fi

echo "Converting ${file}"
~/test ${file} 2>/dev/null | sed -e "s/\r\n/\n/" > ${file}.2
diff -u ${file} ${file}.2

if test x$2 != x1; then
    echo "Give a second argument of 1 to overwrite."
    rm ${file}.2
else
    mv ${file}.2 ${file}
fi
