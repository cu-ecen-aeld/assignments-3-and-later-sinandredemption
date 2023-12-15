#!/bin/sh
writefile=$1
writestr=$2
if [ -z "${writefile}" ] || [ -z "${writestr}" ]; then
    echo "Error: Please specify both the file and string to write." >&2
    exit 1
fi
touch ${writefile}
echo ${writestr} > ${writefile}
if [ $? -ne 0 ]; then
    echo "Failed to create or overwrite file" >&2
    exit 1
fi
exit 0