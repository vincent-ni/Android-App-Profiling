#!/bin/bash

g++-4.2 -O2 -DNDEBUG -fasm-blocks main.cpp -L. -I/opt/local/include/opencv -I/opt/local/include -L/opt/local/lib -L../build -lcv -lcxcore -lhighgui -limagefilter -lgomp -lpthread

