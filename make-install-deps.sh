#!/bin/bash

mkdir deps
cd deps 

git clone https://github.com/paullouisageneau/libdatachannel.git
cd libdatachannel
git submodule update --init --recursive --depth 1
cmake -B build
cd build
make -j2
sudo make install