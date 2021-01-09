#!/bin/bash

if [ -f "./output/logfile.txt" ]; then
    while read line; do echo $line; done < ./output/logfile.txt
else
    echo "$0:Error" 1>&2
fi