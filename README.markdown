# SyLens for Nuke

These plugins help to prep images that have been undistorted using the Syntheyes matchmoving software. It implements the same undistortion algorithm as the one embedded in Syntheyes and explained [here.][1]

The currently provided plugins are:

* SyLens for images, footage and roto (works on all channels)
* SyUV for undistorting projected UVs in your geometry
* SyGeo for undistorting your geometry itself (primarily cards)
* SyShader for undistorting in texture space (material modifier)
* SyCamera for rendering from ScanlineRender with lens redistortion baked in

To get started right away, have a look at the sample.nk test file included with the plugin.

## Installation

Download a packaged release from the [releases page.](https://github.com/julik/sylens/releases) Then, drop the directory into your `$HOME/.nuke` and
add it to your `init.py` which is in `$HOME/.nuke` like so:

    nuke.pluginAddPath('sylens-311') # For the unzipped release
    
When you start Nuke the next time there is going to be an extra icon in your node toolbar.

## The SyLens node

SyLens node works with image inputs.

### Undistorting footage 

Punch in the lens distortion into the lens settings panel

![Kappa Value][2]

or let Syntheyes compute it for you...

![Auto Disto][3]

Then just punch in the computed distortion value into the k knob

![Just K][4]

You will probably note that the bounding box of your output will stick outside the format - this is perfectly fine, read below how to deal with that

### Redistorting footage

After all is done you might want to redistort either your whole comp or only the piece of CG that came to you from 3D (since it would have been rendered from the undistorted film back size). To redistort, plug your oversize plate into a copy of SyLens with the "output" switch set to "apply disto". *See to it that other settings - k, kcube - stay the same!*

![SyLens controls][17]


### Explanation of the UI controls

#### output

When set to *remove disto*, SyLens will remove lens distortion. When set to *apply disto* SyLens will apply lens distortion

#### k

Quartic distortion coefficient. This is calculated by Syntheyes and needs to be punched in here.

#### kcube

Cubic distortion modifier. If you used this in Syntheyes you can apply it here as well.

#### ushift

Sometimes you are dealing with off-center lens distortion. This can occur when a lens is fitted onto the camera but not properly centered onto the sensor (some lens adapters are especially susceptible to this, like the anamorphic Alexa fittings). Apply some margin here to shift your distortion midpoint left or right with regards to the center of your digital plate.

#### vshift

Sometimes you are dealing with off-center lens distortion. This can occur when a lens is fitted onto the camera but not properly centered onto the sensor (some lens adapters are especially susceptible to this, like the anamorphic Alexa fittings). Apply some margin here to shift your distortion midpoint up or down with regards to the center of your digital plate.

#### filter

This selects the filtering algorithm used for sampling the source image, pick one that gives a better-looking result

#### trim bbox

When you apply distortion to the image, the bounding box that SyLens receives will usually grow. For example, when reintroducing distortion, there will be overflow outside of the image. When you are compositing redistorted items onto the source you generally don't want to have this overscan. When you enable *trim bbox* the size of the bounding box will be reduced to fit within the actual output format, and no overscan pixels will be output or computed.

#### grow format

Sometimes it's not that handy to have an overflow bounding box in the standard format - for example, when you need to render your images out for matte painting.
So we provide a shortcut checkbox that you can use to create outputs bigger than the original plate. This will mess up other calculations that depend on the correct
field of view of the camera, so be careful with that one. It will only have effect on images that have barrel distortion.

![Cropping workflow][5]

#### debug info

You can see what SyLens is doing. When you enable this, debug info will be written to STDOUT. If you start Nuke from the terminal then this terminal will contain all the relevant output.

### Standard projection workflow caveats

SyLens creates images which have overflow bounding box, that is - **bounding box that extends outside the image format.** For that reason creating a roundtrip projection setup needs a little work to get right.

To spare you some grief, here's how your DAG should look:

![Projection Dag][6]

I am highlighting the special parameters from the node bin which differ from the defaults.

Important points:

*   This relies on using Nuke's "overscan" (bbox that goes outside of the image format). Enable "Show overscan" in the viewer context menu to actually **see** what's in there.
*   Project3d will only project the image outside of the format with "crop" checkbox disabled
*   You need to render some overscan from your ScanlineRenderer to fill the bbox covered area. By default the renderer only renders within the format
*   Use your cameras as usual (do not change the field of view)

Alternatively, render with a [SyCamera node][7] instead of the standard camera and get even better results if your meshes permit it.

## The SyUV node

The SyUV node is a modifier for the input geometry. It will undistort your image in UV space so that you can preserve filtering steps.

It is built to account for *projected UVs*. 

### Workflow with UV distortion

Typically you would be using it like this:

![SyUV DAG][8]

Here we project the UVs from our projection camera.

Make sure that the camera you are projecting UVs from has it's **vertical aperture** set correctly. If you are seeing mismatches between your projected UVs and the actual image, this is the culprit.

Afterwards, apply the *SyUV* node. You can also apply it to a plain Card, or you can apply it to an arbitrary mesh - what is important is having the UVs projected from your camera, not the standard UVs or unwraps.

Create the node and dial in the controls just as the [main SyLens plugin.][9]

The undistorted image will look like this in the viewport. **Note that to make efficient use of SyUV your geometry needs to be relatively dense** since we can only apply undistortion at the vertices of the image. Everything within one quad/triangle will be interpolated in a linear fashion.

![SyUV undistortion in the viewport][10]

### Explanation of the UI controls

The UI controls of SyUV are very similar to the standard SyLens plugin.

![SyUV controls in the node bin][11]k

#### k

Quartic distortion coefficient. This is calculated by Syntheyes and needs to be punched in here.

#### kcube

Cubic distortion modifier. If you used this in Syntheyes you can apply it here as well.

#### aspect

The Syntheyes algorithm **requires** the aspect ratio of your distorted plate. This cannot be automatically deciphered from the UVs so you need to dial it in manually. Use your image's aspect ratio.

#### ushift

Sometimes you are dealing with off-center lens distortion. This can occur when a lens is fitted onto the camera but not properly centered onto the sensor (some lens adapters are especially susceptible to this, like the anamorphic Alexa fittings). Apply some margin here to shift your distortion midpoint left or right with regards to the center of your digital plate.

#### vshift

Sometimes you are dealing with off-center lens distortion. This can occur when a lens is fitted onto the camera but not properly centered onto the sensor (some lens adapters are especially susceptible to this, like the anamorphic Alexa fittings). Apply some margin here to shift your distortion midpoint up or down with regards to the center of your digital plate.

#### uv attrib

Nuke allows for arbitrary UV attributes to be added to the main geometry. If you want to manipulate a non-standard UV channel punch it's name in here. Normally you would leave this parameter at it's default setting.

## The SyGeo node

SyGeo undistorts the vertex coordinates in your geometry. It's primarily designed to undistort Card nodes that you place your footage on (with **image aspect** enabled).

### Workflow with SyGeo

Typically you would be using it like this:

![SyGeo DAG][18]

Here we pump the footage into a Card node, which then gets straightened. This is analogous
to the distortion parameters of the Card node itself.

### Explanation of the UI controls

#### k

Quartic distortion coefficient. This is calculated by Syntheyes and needs to be punched in here.

#### kcube

Cubic distortion modifier. If you used this in Syntheyes you can apply it here as well.

#### aspect

The Syntheyes algorithm **requires** the aspect ratio of your distorted plate. This cannot be automatically deciphered from the input geometry so you need to dial it in manually. Use your image's aspect ratio.

#### ushift

Sometimes you are dealing with off-center lens distortion. This can occur when a lens is fitted onto the camera but not properly centered onto the sensor (some lens adapters are especially susceptible to this, like the anamorphic Alexa fittings). Apply some margin here to shift your distortion midpoint left or right with regards to the center of your digital plate.

#### vshift

Sometimes you are dealing with off-center lens distortion. This can occur when a lens is fitted onto the camera but not properly centered onto the sensor (some lens adapters are especially susceptible to this, like the anamorphic Alexa fittings). Apply some margin here to shift your distortion midpoint up or down with regards to the center of your digital plate.

#### scale

This defines the relationship between the coordinates in the camera frustum and your geometry. When using SyGeo
with Card nodes, for example, the factor is set to 2 since the outermost left and right points are offset by 0.5 from
the origin of the Card. The Syntheyes algorithm, however, needs the coordinates to be in the -1..1 range so there is a
scaling operation involved when doing distortions. For Card nodes you can leave this parameter at it's default setting.

## The SyCamera node

The SyCamera node performs distortion in camera space. You can use it just like you would use a standard Nuke camera node. However, once plugged into the render node it will distort the rendered output according to the Syntheyes algorithm.

Here's how a render looks with the standard Nuke camera:

![Cam Std][12]

...and here's how it looks rendered with the SyCamera:

![Cam Sy][13]

The only difference with the standard Nuke camera is the addition of the SyLens controls tab. Use it the same way you would be using SyLens.

![Cam Controls][14]

Note that in order to achieve good redistortion **you need to have enough vertices in your geometry.** 
That is, Nuke's lens distortion function is not a **fragment shader**, but a **vertex shader** - it transforms the actual vertices
in the geometry as opposed to requested pixels.

For example, a Cylinder having only one span vertically only distorts at the caps with straight lines being traced between vertices:

![Cam Few Subdivs][15]

..while a well-subdivided Cylinder will distort like this:

![Cam Many Subdivs][16]

Note that the Syntheyes algorithm **requires** the aspect ratio of your distorted plate. Therefore it is **imperative** that your 
`haperture` and `vaperture` parameters are set correctly. The Syntheyes script that exports Nuke files takes care of this by default,
but if you create a SyCamera from scratch you will need to take care of them yourself.


### Explanation of the UI controls

#### k

Quartic distortion coefficient. This is calculated by Syntheyes and needs to be punched in here.

#### kcube

Cubic distortion modifier. If you used this in Syntheyes you can apply it here as well.

#### ushift

Sometimes you are dealing with off-center lens distortion. This can occur when a lens is fitted onto the camera but not properly centered onto the sensor (some lens adapters are especially susceptible to this, like the anamorphic Alexa fittings). Apply some margin here to shift your distortion midpoint left or right with regards to the center of your digital plate.

#### vshift

Sometimes you are dealing with off-center lens distortion. This can occur when a lens is fitted onto the camera but not properly centered onto the sensor (some lens adapters are especially susceptible to this, like the anamorphic Alexa fittings). Apply some margin here to shift your distortion midpoint up or down with regards to the center of your digital plate.

## The SyShader node

SyShader is a Material modifier that can be applied before any Nuke shader. For example, adding a SyShader
before a Project3D will undistort the projected texture. SyShader can be used as a *vertex shader* or as 
a *fragment shader*. Vertex shader is dependent on the geometry, but it will provide good undistortion with
SyCamera. Fragment shader works on the actual pixels of the texture (so you can use it even with low-density geo),
but it will not provide good redistortion with SyCamera. It might nevertheless be useful if you are using
it to undistort the plates and say do a texture extraction.

## Building the plugins

Consult `BUILD_INSTRUCTIONS.md` in the `src` directory of the plugin for exact build instructions.

## Testing the plugins

See the scripts in the sample_scripts directory.

## Credits

The plugins are based on the 3DE distortion plugin by Matti Grüner.

Contributors, build maintainers, sponsors and beta-testers of SyLens are:

* Julik Tarkhanov and HecticElectric, Amsterdam
* Ivan Busquets
* Marijn Eken
* Dmitri Korshunov
* Jonathan Egstad
* Miklos Kozary and Elefant Studios, Zürich
* Sebastian Elsner

The beautiful landscape shot used in the test script has been provided by Tim Parshikov,
Mikhail Mestezky and the "Drug Druga" production company.

## Questions and remarks

For questions and comments shoot a mail to me \_at\_ julik.nl

## License

    Copyright (c) 2010-2014 Julik Tarkhanov
    
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

 [1]: http://www.ssontech.com/content/lensalg.htm
 [2]: https://github.com/julik/sylens/raw/master/images/kappa_value.png
 [3]: https://github.com/julik/sylens/raw/master/images/auto_disto.png
 [4]: https://github.com/julik/sylens/raw/master/images/just_k.png
 [5]: https://github.com/julik/sylens/raw/master/images/SyLens_Figures_03.png
 [6]: https://github.com/julik/sylens/raw/master/images/projection_dag.png
 [7]: #sycamera
 [8]: https://github.com/julik/sylens/raw/master/images/syuv_dag.png
 [9]: #iop
 [10]: https://github.com/julik/sylens/raw/master/images/syuv_undisto.png
 [11]: https://github.com/julik/sylens/raw/master/images/syuv_controls.png
 [12]: https://github.com/julik/sylens/raw/master/images/cam_std.png
 [13]: https://github.com/julik/sylens/raw/master/images/cam_sy.png
 [14]: https://github.com/julik/sylens/raw/master/images/cam_controls.png
 [15]: https://github.com/julik/sylens/raw/master/images/cam_few_subdivs.png
 [16]: https://github.com/julik/sylens/raw/master/images/cam_many_subdivs.png
 [17]: https://github.com/julik/sylens/raw/master/images/sylens_controls.png
 [18]: https://github.com/julik/sylens/raw/master/images/sygeo.png
 [19]: https://github.com/julik/sylens/raw/master/images/local_space.png
 [20]: https://github.com/julik/sylens/raw/master/images/global_space.png
