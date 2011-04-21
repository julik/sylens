#!/bin/sh
make -f Makefile64.mac clean && make -f Makefile64.mac && cp SyLens.dylib ~/.nuke/ &&  /Applications/Nuke6.1v2/Nuke6.1v2.app/Nuke6.1v2 sample.nk 
