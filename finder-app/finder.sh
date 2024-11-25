#!/bin/sh
# Check if both arguments are provided
if [ $# -ne 2 ]
then
    echo "Exactly two parameters should be provided" >&2
    exit 1
fi
filesdir=$1
searchstr=$2
# Verify that the first argument is a directory
if [ ! -d $filesdir ]
then
    echo "The given path does not represent a directory on the filesystem." >&2
    exit 1
fi
# Get all files in the directory (and subdirectories) and count them. Save this number to 'nr_of_files' variable.
nr_of_files=$(find ${filesdir} -type f | wc -l)
# Search for occurrences of searchstr string across all files in the directory (and subdirectories). Count these and save them to 'matching_lines' variable.
matching_lines=$(grep -r "${searchstr}" $filesdir | wc -l)
# Print results
echo "The number of files are ${nr_of_files} and the number of matching lines are ${matching_lines}" 
exit 0
