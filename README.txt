# INFO

This plugin uses the lens distortion model provided by Russ Anderson of the SynthEyes camera tracker
fame. It is made so that it's output is _identical_ to the Image Preparation tool of the SY camera tracker.
It is largely based on tx_nukeLensDistortion by Matti Gruener.

## USAGE
See Manual.html for instructions. To install, drop that into your .nuke
directory in the home dir and do "All plugins->Update". Afterwards
type SyLens in the tab menu and you will get'em.

## TESTING
Use the included sample.nk script. The following values should produce identical distortion and bounds for Source.jpg
to match Sample.jpg.

K distortion of -0.01826
crop of 0.038 on all sides

This file also contains a number of other examples and check sets.

# CREDITS

Written by Julik Tarkhanov in Amsterdam in 2010. me(at)julik.nl
with kind support by HecticElectric.
Largely based on tx_nukeLensDistortion by Matti Gruener.
The beautiful landscape shot provided by Tim Parshikov, 2010.

# LICENSE

Copyright (c) 2010 Julik Tarkhanov

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.