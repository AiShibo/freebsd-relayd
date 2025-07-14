#!/bin/sh

for f in ./crashes/*; do
    if [ -f "$f" ]; then
        echo "Running with input: $f"
        ./relayd -d < "$f"
    fi
done

