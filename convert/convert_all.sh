#!/bin/sh
for file in *.jpg *.jpeg; do
    if [ -f "$file" ]; then
        python ./convert.py "$file"
    fi
done
