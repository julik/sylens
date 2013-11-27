# Building SyLens

SyLens can be built on any OS supported by Nuke, provided you have the right compiler version, the Nuke
version that you are building for and the right system headers.

We don't use Boost or other third-party libraries, and the libraries should build cleanly for Nuke 6 as well.

## On OS X

On OS X you will need to have GCC 4.0. Be careful - this is an *old* GCC, but you have to use the right version.
Also, `clang` that ships as a replacement for GCC will *not* work (surprise!).

The best way to ensure you have everything is to install the
[command-line tools](http://stackoverflow.com/questions/9353444/how-to-use-install-gcc-on-mac-os-x-10-8-xcode-4-4) 
which include the actual GCC binaries.

You will also need to link against the 10.6 SDK.

Once you got everything in place use the included Makefile for OSX:

    make -f Makefile.Nuke7.mac

You can supply your own path to the Nuke headers by overriding the NDKDIR envar:

    make -f Makefile.Nuke7.mac NDKDIR=/Applications/Nuke7.0v9/Nuke7.0v9.app/Contents/MacOS
   
If it completes without errors you are all set. The Makefile is very short and can be changed to your liking if you need to.

## On Linux

On Linux you are going to need GCC 4.1.2. This is incidentially the same version you use for building Maya plugins
for Maya 2013.

Be aware that because you also have to link against the 4.1.2 standard library headers it's *very* problematic to build
on Ubuntu 12.04 - this is a known issue. Thank the providers of VFX software for using, hm, *modern* compilers.

Anyhow, once you got gcc-4.1.2 set up, building is not much more difficult than on the Mac:

    make -f Makefile.Nuke7.mac
    
You can supply your override for where Nuke is installed on your system:
    
    make -f Makefile.Nuke7.mac NDKDIR=/usr/local/thefoundry/Nuke7.0v2

If that completes without errors you are pretty much guaranteed to have your libraries on.