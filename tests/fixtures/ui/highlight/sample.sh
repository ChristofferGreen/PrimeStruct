#!/bin/sh
# deploy script
name="world"
if [ -n "$name" ]; then
  echo "hello ${name}" 'literal' $HOME
fi
