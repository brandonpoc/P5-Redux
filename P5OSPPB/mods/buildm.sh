#!/bin/bash

cd lib
./build.sh
cd ../um_tst
./build.sh
cd ../registrar
./build.sh
cd ../vesa
./build.sh
cd ../init
./build.sh
cd ../v86
./build.sh
cd ../pci
./build.sh
cd ../uhci
./build.sh
cd ../idle
./build.sh
cd ../wyg
./build.sh
cd ../ps2
./build.sh
cd ../uart
./build.sh
cd ..
