# INFO

This plugin uses the lens distortion model provided by Russ Anderson of the SynthEyes camera tracker
fame. It is made so that it's output is _identical_ to the Image Preparation tool of the SY camera tracker.
It is largely based on tx_nukeLensDistortion by Matti Gruener.

## USAGE
See Manual.html for instructions.

## TESTING
Use the included sample.nk script. The following values should produce identical distortion and bounds for Source.jpg
to match Sample.jpg.

K distortion of -0.01826
crop of 0.038 on all sides

# CREDITS

Written by Julik Tarkhanov in Amsterdam in 2010. me(at)julik.nl
with kind support by HecticElectric.
The beautiful landscape shot provided by Tim Parshikov, 2010.

For filtering we also use the standard filtering methods provided by Nuke.

The code has some more comments than it's 3DE counterpart since we have to do some things that the other plugin
did not (like expanding the image output)
