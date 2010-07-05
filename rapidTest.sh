#!/bin/sh
make -f Makefile.mac clean && make -f Makefile.mac && cp SyLens.dylib ~/.nuke/ && nukex sample.nk 