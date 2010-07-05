extern "C" {

#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <string.h>
}


#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstring>
#include <iomanip>
#include <cstdlib> 

// Nuke-Data
#include "DDImage/Iop.h"
#include "DDImage/NukeWrapper.h"
#include "DDImage/Row.h"
#include "DDImage/Tile.h"
#include "DDImage/Pixel.h"
#include "DDImage/Filter.h"
#include "DDImage/Knobs.h"
#include "DDImage/Knob.h"
#include "DDImage/Vector2.h"
#include "DDImage/DDMath.h"

using namespace DD::Image;

#define MAX_ITERATIONS 400
#define FLOAT_TOL 5.0e-7
#define STR_EQUAL 0

static const char* const CLASS = "sy_LensUndistort";
static const char* const HELP =
"This plugin distorts and undistorts footage according"
"to the lens distortion model used by Syntheyes";

class sy_LensUndistort : public Iop
{
	Filter filter;
	
	// Input with and height for input(0)
	unsigned int _inputWidth;
	unsigned int _inputHeight;

	// The size of the filmback we are actually sampling from
	unsigned int _extWidth;
	unsigned int _extHeight;
	
	// When we extend the image and get undistorted coordinates, we need to add these
	// values to the pixel offset
	unsigned int _shiftX;
	unsigned int _shiftY;
	
	unsigned int _padding;
	
	// Image aspect and NOT the pixel aspect Nuke furnishes us
	double _aspect;
	
	double kCoeff;
	double kCubeCoeff;
	double kUnCrop;
	
	// used to store input-format
	Format _format;
	
public:
	sy_LensUndistort( Node *node ) : Iop ( node )
	{
		kCoeff = -0.01826;
		kCubeCoeff = 0.0f;
		kUnCrop = 0.038f;
		_aspect = 1.33f;
		_shiftX = 0;
		_shiftY = 0;
	}
	
	~sy_LensUndistort () { }
	
	void _validate( bool ) {
		filter.initialize();
		copy_info();
		set_out_channels( Mask_All );
		
		// Compute the aspect from the input format
		Format f = input0().format();
		_inputWidth = f.width();
		_inputHeight = f.height();
		_aspect = float(f.width()) / float(f.height()) *  f.pixel_aspect();
		
		_padding = ceil(_inputWidth * kUnCrop);
		
		// Compute the sampled width and height
		_extWidth = uncrop(_inputWidth);
		_extHeight = uncrop(_inputHeight);
		_shiftX = _extWidth - _inputWidth;
		_shiftY = _extHeight - _inputHeight;
		
		Box extended = Box(_padding * -1, _padding * -1, _extWidth, _extHeight);
		info_.merge(extended);
	}
	
	// Uncrop an integer dimension with a Syntheyes crop factor
	int uncrop(int dimension) {
		return ceil(dimension + (dimension * kUnCrop * 2));
	}
	
	// request the entire image to have access to every pixel, plus padding
	void _request(int x, int y, int r, int t, ChannelMask channels, int count)
	{
		ChannelSet c1(channels); in_channels(0,c1);
		input0().request(x - _padding, y - _padding, r + _padding, t + _padding, channels, count);
	}

	// create UI
	void knobs( Knob_Callback f )
	{
		Knob* _kCoeffKnob = Float_knob( f, &kCoeff, "kCoeff_knob" );
		_kCoeffKnob->label("K coeff");
		_kCoeffKnob->tooltip("Set to the same K coefficient as applied by Syntheyes");

		Knob* _kCubeKnob = Float_knob( f, &kCubeCoeff, "kCube_knob" );
		_kCubeKnob->label("Cubic coeff");
		_kCubeKnob->tooltip("Set to the same cubic distortion as applied by Syntheyes");

		Knob* _kUncropKnob = Float_knob( f, &kUnCrop, "Crop_knob" );
		_kUncropKnob->label("Uncrop expansion");
		_kUncropKnob->tooltip("Set this to the same uncrop value as applied by Syntheyes, it will be the same on all sides");
		
		// Add the filter selection menu that comes from the filter obj itself
		filter.knobs( f );
	}
	
	void uncropCoordinate(Vector2& croppedSource, Vector2& uncroppedDest) {
		uncroppedDest.x = croppedSource.x - float(_shiftX);
		uncroppedDest.y = croppedSource.y - float(_shiftX);
	}

	// nuke-functions
	void engine( int y, int x, int r, ChannelMask channels, Row& out );
	const char* Class() const { return CLASS; }
	const char* node_help() const { return HELP; }
	static const Iop::Description description; 
	
	// custom functions
	double toUv(double, int);
	double fromUv(double, int);
	void vecToUV(Vector2&, Vector2&, int, int);
	void vecFromUV(Vector2&, Vector2&, int, int);
	void distortVector(Vector2& uvVec, double k, double kcube);
};

static Iop* sy_LensUndistortCreate( Node *node ) {
	return ( 
		new DD::Image::NukeWrapper(
		 	new sy_LensUndistort(node)
	   )
	)->noMix()->noMask();
}

const Iop::Description sy_LensUndistort::description ( CLASS, 0, sy_LensUndistortCreate );

// Syntheyes uses UV coordinates that start at the optical center of the image
double sy_LensUndistort::toUv(double absValue, int absSide)
{
	double x = (absValue / (double)absSide) - 0.5;
    // .2 to -.3, y is reversed and coords are double
	return x * 2;
}

double sy_LensUndistort::fromUv(double uvValue, int absSide) {
    // First, start from zero (-.1 becomes .4)
	double value_off_corner = (uvValue / 2) + 0.5f;
	return absSide * value_off_corner;
}

void sy_LensUndistort::vecToUV(Vector2& absVec, Vector2& uvVec, int w, int h)
{
	// Nuke coords are 0,0 on lower left
	uvVec.x = toUv(absVec.x, w);
	uvVec.y = toUv(absVec.y, h);
}

void sy_LensUndistort::vecFromUV(Vector2& absVec, Vector2& uvVec, int w, int h)
{
	// Nuke coords are 0,0 on lower left
	absVec.x = fromUv(uvVec.x, w);
	absVec.y = fromUv(uvVec.y, h);
}

// Place the UV coordinates of the distorted image in the undistorted image plane into uvVec
void sy_LensUndistort::distortVector(Vector2& uvVec, double k, double kcube) {
	
	float r2 = _aspect * _aspect * uvVec.x * uvVec.x + uvVec.y * uvVec.y;
	float f;
	
	// Skipping the square root munges up precision some
	if (fabs(kcube) > 0.00001) {
		f = 1 + r2*(k + kcube * sqrt(r2));
	} else {
		f = 1 + r2*(k);
	}
	
	uvVec.x = uvVec.x * f;
	uvVec.y = uvVec.y * f;
}

// The image processor that works by scanline. Y is the scanline offset, x is the pix,
// r is the length of the row. We are now effectively in the undistorted coordinates, mind you!
void sy_LensUndistort::engine ( int y, int x, int r, ChannelMask channels, Row& out )
{
	
	foreach(z, channels) out.writable(z);
	
	Pixel pixel(channels);
	
	for (; x < r; x++) {
		
		Vector2 absXY(x, y);
		Vector2 uvXY(0, 0);
		Vector2 distXY(0, 0);
		
		// Compute the DISTORTED vector for this image and sample it from the input
		vecToUV(absXY, uvXY, _inputWidth, _inputHeight);
		distortVector(uvXY, kCoeff, kCubeCoeff);
		vecFromUV(distXY, uvXY, _inputWidth, _inputHeight);
		
		// half a pixel has to be added here because sample() takes the first two
		// arguments as the center of the rectangle to sample. By not adding 0.5 we'd
		// have to deal with a slight offset which is *not* desired.
		input0().sample(distXY.x+0.5, distXY.y+0.5, 1.0f, 1.0f, &filter, pixel );
		
		// write the resulting pixel into the image
		foreach (z, channels)
		{
			((float*)out[z])[x] = pixel[z];
		}
	}
}
