#!/usr/bin/env bash

mkdir -vp ../out

# echo $(pwd)

rm ../out/*

clang++ tests.cpp -std=c++14 -O3 -o ../out/tests.out

../out/tests.out example.xml
