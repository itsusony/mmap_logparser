#! /bin/sh

rm -f bin/parser 2>/dev/null
gcc -Wall -o bin/parser parser.c
