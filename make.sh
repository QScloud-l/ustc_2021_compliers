#!/bin/bash
cd build
make -j
make install
cd ../tests/3-ir-gen
./eval.py
vim eval_result
