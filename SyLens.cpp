/*
*	This plugin uses the lens distortion model provided by Russ Anderson of the SynthEyes camera tracker
*	fame. It is made so that it's output is _identical_ to the Image Preparation tool of the SY camera tracker.
*	so you can use it INSTEAD of the footage Syntheyes prerenders.
*	It is largely based on tx_nukeLensDistortion by Matti Gruener of Trixter Film and Rising Sun fame. However,
*	For filtering we also use the standard filtering methods provided by Nuke.
*	
*	Written by Julik Tarkhanov in Amsterdam in 2010. me(at)julik.nl
*	with kind support by HecticElectric.
*	The beautiful Crimean landscape shot provided by Tim Parshikov, 2010.
*	
*	The code has some more comments than it's 3DE counterpart since we have to do some things that the other plugin
*	did not (like expanding the image output)
*/

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

static const char* const CLASS = "SyLens";
static const char* const HELP =  "This plugin undistorts footage according"
" to the lens distortion model used by Syntheyes";
static const char* const VERSION = "0.0.2";
static const char* const mode_names[] = { "undistort", "redistort", 0 };

class SyLens : public Iop
{
	//Nuke statics
	
	const char* Class() const { return CLASS; }
	const char* node_help() const { return HELP; }
	static const Iop::Description description;
	
	Filter filter;
	
	enum { UNDIST, REDIST };
	
	// Input with and height for input0
	unsigned int _plateWidth;
	unsigned int _plateHeight;

	// The size of the filmback we are actually sampling from
	unsigned int _extWidth;
	unsigned int _extHeight;
	
	// When we extend the image and get undistorted coordinates, we need to add these
	// values to the pixel offset
	unsigned int _paddingW;
	unsigned int _paddingH;
	
	// Image aspect and NOT the pixel aspect Nuke furnishes us
	double _aspect;
	
	// Stuff drive bu knobbz
	double kCoeff;
	double kCubeCoeff;
	double kUnCrop;
	bool kDbg;
	int kMode;
	
	int _lastScanlineSize;
	
	// Used to store the output format. This needs to be kept between
	// calls to _request and cannot reside on the stack, so... 
	Format _outFormat;
	
public:
	SyLens( Node *node ) : Iop ( node )
	{
		kMode = UNDIST;
		kCoeff = -0.01826;
		kCubeCoeff = 0.0f;
		kUnCrop = 0.038f;
		_aspect = 1.33f;
		_lastScanlineSize = 0;
		_paddingW = 0;
		_paddingH = 0;
		kDbg = false;
	}
	
	~SyLens () { }
	
	void _computeAspects() {
		// Compute the aspect from the input format
		Format f = input0().format();
		
		double sc = 1.0f + (2.0f * kUnCrop);
		
		if(kMode == UNDIST) {
			_plateWidth = f.width();
			_plateHeight = f.height();
			_extWidth = ceil(float(_plateWidth) * sc);
			_extHeight = ceil(float(_plateHeight) * sc);
		} else {
			_plateWidth = floor( float(f.width()) / sc);
			_plateHeight = floor( float(f.height()) / sc);
			_extWidth = f.width();
			_extHeight = f.height();
		}
		
		_paddingW = (_extWidth - _plateWidth) / 2;
		_paddingH = (_extHeight - _plateHeight) / 2;
		_aspect = float(_plateWidth) / float(_plateHeight) *  f.pixel_aspect();
		
		if(kDbg) printf("SyLens: true plate window with uncrop will be %dx%d\n", _extWidth, _extHeight);
	}
	
	// Here we need to expand the image
	void _validate(bool for_real)
	  {
		filter.initialize();
		input0().validate(for_real);
		copy_info();
		set_out_channels(Mask_All);
		
		info_.black_outside(true);
		
		_computeAspects();    
		
		if(kDbg) printf("SyLens: _validate info box to  %dx%d\n", _extWidth, _extHeight);
		
		int ow, oh;
		
		if(kMode == UNDIST) {
			ow = _extWidth;
			oh = _extHeight;
		} else {
			ow = _plateWidth;
			oh = _plateHeight;
		}
		
		// Crucial. Define the format in the info_ - this is what Nuke uses
		// to know how big OUR output will be. We also pretty much NEED to store it
		// in an instance var because we cannot keep it on the stack (segfault!)
		_outFormat = Format(ow, oh, input0().format().pixel_aspect());
		info_.format( _outFormat );
		
		// And also enforce the bounding box AS WELL
		Box obox = Box(0,0, ow, oh);
		if(kMode == UNDIST) {
			info_.merge(obox);
		} else {
			info_.intersect(obox);
		}
		
		if(kDbg) printf("SyLens: ext output will be %dx%d\n", _outFormat.width(), _outFormat.height());
	}
	
	// Request the same source area upstream. We pad in the output during engine() call
	void _request(int x, int y, int r, int t, ChannelMask channels, int count)
	{
		if(kDbg) printf("SyLens: Received request %d %d %d %d\n", x, y, r, t);
		ChannelSet c1(channels); in_channels(0,c1);
		// Request the same part of the input plus padding
		input0().request(x - (_paddingW * 2), y - (_paddingW * 2), r + (_paddingW * 2), t + (_paddingH * 2), channels, count);
	}

	// nuke
	void engine( int y, int x, int r, ChannelMask channels, Row& out );
	void knobs( Knob_Callback f) {
		
		Knob* _modeSel = Enumeration_knob(f, &kMode, mode_names, "mode", "Mode");
		_modeSel->label("mode");
		_modeSel->tooltip("Pick your poison");
		
		Knob* _kKnob = Float_knob( f, &kCoeff, "k" );
		_kKnob->label("K coeff");
		_kKnob->tooltip("Set to the same distortion as applied by Syntheyes");

		Knob* _kCubeKnob = Float_knob( f, &kCubeCoeff, "kcube" );
		_kCubeKnob->label("Cubic coeff");
		_kCubeKnob->tooltip("Set to the same cubic distortion as applied by Syntheyes");
		
		Knob* _kUncropKnob = Float_knob( f, &kUnCrop, "uncrop" );
		_kUncropKnob->label("Uncrop expansion");
		_kUncropKnob->tooltip("Set this to the same uncrop value as applied by Syntheyes, it will be the same on all sides");
		
		Knob* kDbgKnob = Bool_knob( f, &kDbg, "debug");
		kDbgKnob->label("Output debug info");
		kDbgKnob->tooltip("When checked, SyLens will output various debug info to STDOUT");
		
		// Add the filter selection menu that comes from the filter obj itself
		filter.knobs( f );
		
		Divider(f, 0);
		Text_knob(f, (std::string("SyLens v.") + std::string(VERSION)).c_str());
   	}
	
	// called whenever a knob is changed
	int knob_changed(Knob* k) {
		// Dont dereference unless...
		if (k == NULL) return 1;
		
		// Touching the crop knob changes our output bounds
		if ((*k).startsWith("uncrop")) {
			_validate(false);
		}
		
		// Touching the mode changes everything
		if ((*k).startsWith("mode")) {
			_validate(false);
		}
	}
	
	
private:
	
	double toUv(double, int);
	double fromUv(double, int);
	void vecToUV(Vector2&, Vector2&, int, int);
	void vecFromUV(Vector2&, Vector2&, int, int);
	void distortVector(Vector2& uvVec, double k, double kcube);
	void distortVectorIntoSource(Vector2& vec);
	void undistortVectorIntoDest(Vector2& vec);
	int uncrop(int dimension);
	void Remove(Vector2& vec);
};

static Iop* SyLensCreate( Node* node ) {
	return ( new NukeWrapper ( new SyLens( node )))->noMix()->noMask();
}

//const Iop::Description SyLens::description ( CLASS, "Transform/SyLens", SyLensCreate );
const Iop::Description SyLens::description(CLASS, 0, SyLensCreate);

// Syntheyes uses UV coordinates that start at the optical center of the image
double SyLens::toUv(double absValue, int absSide)
{
	double x = (absValue / (double)absSide) - 0.5;
    // .2 to -.3, y is reversed and coords are double
	return x * 2;
}

double SyLens::fromUv(double uvValue, int absSide) {
    // First, start from zero (-.1 becomes .4)
	double value_off_corner = (uvValue / 2) + 0.5f;
	return absSide * value_off_corner;
}

void SyLens::vecToUV(Vector2& absVec, Vector2& uvVec, int w, int h)
{
	// Nuke coords are 0,0 on lower left
	uvVec.x = toUv(absVec.x, w);
	uvVec.y = toUv(absVec.y, h);
}

void SyLens::vecFromUV(Vector2& absVec, Vector2& uvVec, int w, int h)
{
	// Nuke coords are 0,0 on lower left
	absVec.x = fromUv(uvVec.x, w);
	absVec.y = fromUv(uvVec.y, h);
}

// Place the UV coordinates of the distorted image in the undistorted image plane into uvVec
void SyLens::distortVector(Vector2& uvVec, double k, double kcube) {
	
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

// Get a coordinate that we need to sample from the SOURCE distorted image to get at the absXY
// values in the RESULT
void SyLens::distortVectorIntoSource(Vector2& absXY) {
	Vector2 uvXY(0, 0);
	// The gritty bits - get coordinates of the distorted pixel in the coordinates of the
	// EXTENDED film back
	vecToUV(absXY, uvXY, _extWidth, _extHeight);
	distortVector(uvXY, kCoeff, kCubeCoeff);
	vecFromUV(absXY, uvXY, _extWidth, _extHeight);
	absXY.x -= (double)_paddingW;
	absXY.y -=  (double)_paddingH;
}

// This is still a little wrongish (we use the same approximation as the one
// in distort.szl and we do all ops in reverse
void SyLens::undistortVectorIntoDest(Vector2& absXY) {
	absXY.x += (double)_paddingW;
	absXY.y += (double)_paddingH;
	Vector2 uvXY(0, 0);
	vecToUV(absXY, uvXY, _extWidth, _extHeight);
	Remove(uvXY);
	vecFromUV(absXY, uvXY, _extWidth, _extHeight);
}

// Ported over from distort.szl
void SyLens::Remove(Vector2& pt) {
	double r, rp, f, rlim, rplim, raw, err, slope;
	
	rp = sqrt(_aspect * _aspect * pt.x * pt.x + pt.y * pt.y);
	if (kCoeff < 0.0f) {
		rlim = sqrt((-1.0/3.0) / kCoeff);
		rplim = rlim * (1 + kCoeff*rlim*rlim);
		if (rp >= 0.99 * rplim) {
			f = rlim / rp;
			pt.x = pt.x * f;
			pt.y = pt.y * f;
			return;
		}
    }
	r = rp;
    for (int i = 0; i < 20; i++) {
		raw = kCoeff * r * r;
		slope = 1 + 3*raw;
		err = rp - r * (1 + raw);
		if (fabs(err) < 1.0e-10) break;
		r += err / slope;
	}
	f = r / rp;
	
	pt.x = pt.x * f;
	pt.y = pt.y * f;
}


// Uncrop an integer dimension with a Syntheyes crop factor
int SyLens::uncrop(int dimension) {
	return ceil(dimension + (dimension * kUnCrop * 2));
}

// The image processor that works by scanline. Y is the scanline offset, x is the pix,
// r is the length of the row. We are now effectively in the undistorted coordinates, mind you!
void SyLens::engine ( int y, int x, int r, ChannelMask channels, Row& out )
{
	if(r != _lastScanlineSize) {
		if(kDbg) printf("SyLens: Rendering scanline %d pix\n", r);
		_lastScanlineSize = r;
	}
	
	foreach(z, channels) out.writable(z);
	
	Pixel pixel(channels);
	const float sampleOff = 0.5f;
	
	Vector2 sampleFromXY(0.0f, 0.0f);
	for (; x < r; x++) {
		
		// If chromatics are enabled, we apply per channel adjustment to the fringe values
		if( kMode == UNDIST) {
			sampleFromXY = Vector2(x, y);
			distortVectorIntoSource(sampleFromXY);
		} else {
			// There is an offset by one scanline that appears here, we neutralise that
			sampleFromXY = Vector2(x, y);
			undistortVectorIntoDest(sampleFromXY);
		}
		
		// Sample from the input node at the coordinates
		// half a pixel has to be added here because sample() takes the first two
		// arguments as the center of the rectangle to sample. By not adding 0.5 we'd
		// have to deal with a slight offset which is *not* desired.
		input0().sample(
			sampleFromXY.x + sampleOff , sampleFromXY.y + sampleOff, 
			1.0f, 
			1.0f,
			&filter,
			pixel
		);
		
		// write the resulting pixel into the image
		foreach (z, channels)
		{
			((float*)out[z])[x] = pixel[z];
		}
	}
}
