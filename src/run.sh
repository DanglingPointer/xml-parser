#!/usr/bin/env bash

mkdir -vp ../out

clang++ tests.cpp -std=c++14 -O3 -o ../out/tests.out

../out/tests.out example.xml