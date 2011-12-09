/*
*	This plugin uses the lens distortion model provided by Russ Anderson of the SynthEyes camera tracker
*	fame. It is made so that it's output is _identical_ to the Image Preparation tool of the SY camera tracker.
*	so you can use it INSTEAD of the footage Syntheyes prerenders.
*	It is largely based on tx_nukeLensDistortion by Matti Gruener of Trixter Film and Rising Sun Films.
*	The code has however been simplified and some features not present in the original version have been added.
*	
*	Written by Julik Tarkhanov in Amsterdam in 2010-2011 with kind support by HecticElectric.
*	I thank the users for their continued support and bug reports.
*	For questions mail me(at)julik.nl
*
*	The beautiful Crimean landscape shot used in the test script is provided by Tim Parshikov
* and Mikhail Mestezky, 2010.
*	
*	The code has some more comments than it's 3DE counterpart since we have to do some things that the other plugin
*	did not
*/

// for our friend printf
extern "C" {
#include <stdio.h>
}

// For max/min on containers
#include <algorithm>

#include "DDImage/Iop.h"
#include "DDImage/Row.h"
#include "DDImage/Pixel.h"
#include "DDImage/Filter.h"
#include "DDImage/Knobs.h"

using namespace DD::Image;

static const char* const CLASS = "SyLens";
static const char* const HELP =  "This plugin undistorts footage according"
" to the lens distortion model used by Syntheyes";
static const char* const VERSION = "1.0.0";
static const char* const mode_names[] = { "undistort", "redistort", 0 };

class SyLens : public Iop
{
	//Nuke statics
	
	const char* Class() const { return CLASS; }
	const char* node_help() const { return HELP; }
	static const Iop::Description description;
	
	Filter filter;
	
	enum { UNDIST, REDIST };
	
	// The original size of the plate that we distort
	unsigned int _plateWidth, _plateHeight;

	// The size of the filmback we are actually sampling from
	unsigned int _extWidth, _extHeight;
	
	// Image aspect and NOT the pixel aspect Nuke furnishes us
	double _aspect;
	
	// Stuff driven by knobbz
	double kCoeff, kCubeCoeff, kUnCrop;
	bool kDbg, kTrimToFormat, kOnlyFormat;
	int kMode;
	
	int _lastScanlineSize;
	
	// Used to store the output format. This needs to be kept between
	// calls to _request and cannot reside on the stack, so... 
	Format _outFormat;
	
public:
	SyLens( Node *node ) : Iop ( node )
	{
		// IMPORTANT! these are the knob defaults. If someone has a plugin in
		// his script and did not change these values this is the values they will expect.
		// Therefore THESE values have to be here for the life of the plugin, through versions to come.
		// These are sane defaults - they showcase distortion and uncrop but do not use cubics.
		// cast in stone BEGIN {{
		kMode = UNDIST;
		kCoeff = -0.01826;
		kCubeCoeff = 0.0f;
		kUnCrop = 0.038f;
		kDbg = false;
		kTrimToFormat = false;
		kOnlyFormat = false;
		// }} END
		
		_aspect = 1.33f;
		_lastScanlineSize = 0;
	}
	
	~SyLens () { }
	
	void _computeAspects();
	void _validate(bool for_real);
	void _request(int x, int y, int r, int t, ChannelMask channels, int count);
	void engine( int y, int x, int r, ChannelMask channels, Row& out );
	void knobs( Knob_Callback f);
	int knob_changed(Knob* k);
	
	// Hashing for caches. We append our version to the cache hash, so that when you update
	// the plugin all the caches will(should?) be flushed automatically
	void append(Hash& hash) {
		hash.append(VERSION);
		hash.append(__DATE__);
		hash.append(__TIME__);
		Iop::append(hash); // the super called he wants his pointers back
	}
	
private:
	
	int round(double x);
	double toUv(double, int);
	double fromUv(double, int);
	void vecToUV(Vector2&, Vector2&, int, int);
	void vecFromUV(Vector2&, Vector2&, int, int);
	void distortVector(Vector2& uvVec, double k, double kcube);
	void distortVectorIntoSource(Vector2& vec);
	void undistortVectorIntoDest(Vector2& vec);
	void Remove(Vector2& vec);
};

// Since we do not need channel selectors or masks, we can use our raw Iop
// directly instead of putting it into a NukeWrapper. Besides, the mask input on
// the NukeWrapper cannot be disabled even though the Foundry doco says it can
// (Foundry bug #12598)
static Iop* SyLensCreate( Node* node ) {
	return new SyLens(node);
}

// The second item is ignored because all a compsitor dreams of is writing fucking init.py
// every time he installs a plugin
const Iop::Description SyLens::description(CLASS, "Transform/SyLens", SyLensCreate);

// Syntheyes uses UV coordinates that start at the optical center of the image,
// and go -1,1. Nuke offers a UV option on Format that goes from 0 to 1, but it's not
// exactly what we want
double SyLens::toUv(double absValue, int absSide)
{
	double x = (absValue / (double)absSide) - 0.5f;
	return x * 2;
}

double SyLens::fromUv(double uvValue, int absSide) {
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
}

// This is still a little wrongish but less wrong than before
void SyLens::undistortVectorIntoDest(Vector2& absXY) {
	Vector2 uvXY(0, 0);
	vecToUV(absXY, uvXY, _extWidth, _extHeight);
	Remove(uvXY);
	vecFromUV(absXY, uvXY, _extWidth, _extHeight);
}

// THIS IS SEMI-WRONG! Ported over from distort.szl, does not honor cubic distortion
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
		r += (err / slope);
	}
	f = r / rp;
	
	// For the pixel in the middle of the image the F can
	// be NaN, so check for that and leave it be.
	// http://stackoverflow.com/questions/570669/checking-if-a-double-or-float-is-nan-in-c
	// If a NaN F does crop up it creates a black pixel in the image - not something we love
	if(f == f) {
		pt.x = pt.x * f;
		pt.y = pt.y * f;
	}
}


// The image processor that works by scanline. Y is the scanline offset, x is the pix,
// r is the length of the row. We are now effectively in the undistorted coordinates, mind you!
void SyLens::engine ( int y, int x, int r, ChannelMask channels, Row& out )
{
	if(r != _lastScanlineSize) {
		if(kDbg) printf("SyLens: Rendering scanline up to %d X pix starting at %d on X\n", r, x);
		_lastScanlineSize = r;
	}
	
	if(kOnlyFormat) {
		out.erase(channels);
		return;
	}
	
	foreach(z, channels) out.writable(z);
	
	Pixel pixel(channels);
	const float sampleOff = 0.5f;
	
	Vector2 sampleFromXY(0.0f, 0.0f);
	for (; x < r; x++) {
		
		sampleFromXY = Vector2(x, y);
		if( kMode == UNDIST) {
			distortVectorIntoSource(sampleFromXY);
		} else {
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


// knobs. There is really only one thing to pay attention to - be consistent and call your knobs
// "in_snake_case_as_short_as_possible", labels are also lowercase normally
void SyLens::knobs( Knob_Callback f) {
	// For info on knob flags see Knob.h
	const int KNOB_ON_SEPARATE_LINE = 0x1000;
	const int KNOB_HIDDEN = 0x0000000000040000;
	
	Knob* _modeSel = Enumeration_knob(f, &kMode, mode_names, "mode", "Mode");
	_modeSel->label("mode");
	_modeSel->tooltip("Pick your poison");
	
	Knob* _kKnob = Float_knob( f, &kCoeff, "k" );
	_kKnob->label("k");
	_kKnob->tooltip("Set to the same distortion as applied by Syntheyes");
	
	Knob* _kCubeKnob = Float_knob( f, &kCubeCoeff, "kcube" );
	_kCubeKnob->label("cubic k");
	_kCubeKnob->tooltip("Set to the same cubic distortion as applied by Syntheyes");
	
	// TODO: this knob is to be removed from SyLens 2.
	// This is not something we condone, but it is in the older scripts, so we make it a hidden knob
	Knob* _kUncropKnob = Float_knob( f, &kUnCrop, "uncrop" );
	_kUncropKnob->set_flag(KNOB_HIDDEN);
	// Make sure uncrop is zero
	_kUncropKnob->set_range(0.0f, 0.0f, true);
	
	
	// Add the filter selection menu that comes from the filter obj itself
	filter.knobs( f );
	
	Knob* kTrimKnob = Bool_knob( f, &kTrimToFormat, "trim");
	kTrimKnob->label("trim bbox");
	kTrimKnob->tooltip("When checked, SyLens will crop the output to the format dimensions and reduce the bbox to match format exactly");
	kTrimKnob->set_flag(KNOB_ON_SEPARATE_LINE);
	
	Knob* kDbgKnob = Bool_knob( f, &kDbg, "debug");
	kDbgKnob->label("debug info");
	kDbgKnob->tooltip("When checked, SyLens will output various debug info to STDOUT");

	Knob* kOnlyFormatKnob = Bool_knob( f, &kOnlyFormat, "onlyformat");
	kOnlyFormatKnob->label("only format");
	kOnlyFormatKnob->tooltip("When checked, SyLens will only output the format that can be used as reference, but will not compute any image");
	kOnlyFormatKnob->set_flag(KNOB_ON_SEPARATE_LINE);
	
	Divider(f, 0);
	Text_knob(f, (std::string("SyLens v.") + std::string(VERSION)).c_str());
}

// called whenever a knob is changed
int SyLens::knob_changed(Knob* k) {
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
	return Iop::knob_changed(k); // Super knows better
}

// http://stackoverflow.com/questions/485525/round-for-float-in-c
int SyLens::round(double x) {
	return floor(x + 0.5);
}

// The algo works in image aspec, not the pixel aspect. We also have to take the uncrop factor
// into account.
void SyLens::_computeAspects() {
	// Compute the aspect from the input format
	Format f = input0().format();
	
	if(kMode == UNDIST) {
		_plateWidth = round(f.width());
		_plateHeight = round(f.height());
		_extWidth = ceil(float(_plateWidth));
		_extHeight = ceil(float(_plateHeight));
	} else {
		_plateWidth = floor( float(f.width()));
		_plateHeight = floor( float(f.height()));
		_extWidth = f.width();
		_extHeight = f.height();
	}
	_aspect = float(_plateWidth) / float(_plateHeight) *  f.pixel_aspect();
	if(kDbg) printf("SyLens: true plate window with uncrop will be %dx%d\n", _extWidth, _extHeight);
}

	
// Here we need to expand the image and the bounding box. This is the most important method in a plug like this so
// pay attention
void SyLens::_validate(bool for_real)
{
	// Bookkeeping boilerplate
	filter.initialize();
	input0().validate(for_real);
	copy_info();
	set_out_channels(Mask_All);
	
	info_.black_outside(true);
	
	// We need to know our aspects so prep them here
	_computeAspects();    
	
	if(kDbg) printf("SyLens: _validate info box to  %dx%d\n", _extWidth, _extHeight);
	
	// Time to define how big our output will be in terms of format. Format will always be the whole plate.
	// If we only use a bboxed piece of the image we will limit our request to that. But first of all we need to
	// compute the format of our output.
	int ow, oh;
	
	if(kMode == UNDIST) {
		ow = _extWidth;
		oh = _extHeight;
	} else {
		ow = _plateWidth;
		oh = _plateHeight;
	}
	
	// Nudge outputs to power of 2, upwards
	if(ow % 2 != 0) ow +=1;
	if(oh % 2 != 0) oh +=1;
	
	// Crucial. Define the format in the info_ - this is what Nuke uses
	// to know how big OUR output will be. We also pretty much NEED to store it
	// in an instance var because we cannot keep it on the stack (segfault!)
	_outFormat = Format(ow, oh, input0().format().pixel_aspect());
	info_.format( _outFormat );
	
	// For the case when we are working with a 8k by 4k plate with a SMALL CG pink elephant rrright in the left
	// corner we want to actually translate the bbox of the elephant to our distorted pipe downstream. So we need to
	// apply our SuperAlgorizm to the bbox as well and move the bbox downstream too.
	// Grab the bbox from the input first
	Info inf = input0().info();
	Format f = input0().format();
	
	// Just distorting the four corners of the bbox is NOT enough. We also need to find out whether
	// the bbox intersects the centerlines. Since the distortion is the most extreme at the centerlines if
	// we just take the corners we might be chopping some image away. So to get a reliable bbox we need to check
	// our padding at 6 points - the 4 extremes and where the bbox crosses the middle of the coordinates
	int xMid = ow/2;
	int yMid = oh/2;

	std::vector<Vector2*> pointsOnBbox;
	
	// Add the standard two points - LR and TR
	pointsOnBbox.push_back(new Vector2(inf.x(), inf.y()));
	pointsOnBbox.push_back(new Vector2(inf.r(), inf.t()));
	
	// Add the TL and LR as well
	pointsOnBbox.push_back(new Vector2(inf.x(), inf.t()));
	pointsOnBbox.push_back(new Vector2(inf.r(), inf.y()));
	
	// If our box intersects the midplane on X add the points where the bbox crosses centerline
	if((inf.x() < xMid) && (inf.r() > xMid)) {
		// Find the two intersections and add them
		pointsOnBbox.push_back( new Vector2(xMid, inf.y()) );
		pointsOnBbox.push_back( new Vector2(xMid, inf.t()) );
	}
	
	// If our box intersects the midplane on Y add the points where the bbox crosses centerline
	if((inf.y() < yMid) && (inf.t() > yMid)) {
		pointsOnBbox.push_back( new Vector2(inf.x(), yMid) );
		pointsOnBbox.push_back( new Vector2(inf.r(), yMid) );
	}
	

	std::vector<int> xValues;
	std::vector<int> yValues;
	
	// Here the distortion is INVERTED with relation to the pixel operation. With pixels, we need
	// to obtain the coordinate to sample FROM. However, here we need a coordinate to sample TO
	// since this is where our bbox corners are going to be in the coordinate plane of the output
	// format.
	for(unsigned int i = 0; i < pointsOnBbox.size(); i++) {
		if(kMode == UNDIST) {
			undistortVectorIntoDest(*pointsOnBbox[i]);
		} else {
			distortVectorIntoSource(*pointsOnBbox[i]);
		}
		xValues.push_back(pointsOnBbox[i]->x);
		yValues.push_back(pointsOnBbox[i]->y);
	}
	pointsOnBbox.clear();
	
	int minX, minY, maxX, maxY;
	
	// Formally speaking, we have to allocate an std::iterator first. But we wont.
	minX = *std::min_element(xValues.begin(), xValues.end());
	maxX = *std::max_element(xValues.begin(), xValues.end());
	minY = *std::min_element(yValues.begin(), yValues.end());
	maxY = *std::max_element(yValues.begin(), yValues.end());
	
	Box obox(minX, minY, maxX, maxY);
	
	// If trim is enabled we intersect our obox with the format so that there is no bounding box
	// outside the crop area. Thiis handy for redistorted material.
	if(kTrimToFormat) obox.intersect(_outFormat);
	
	if(kDbg) printf("SyLens: output format will be %dx%d\n", _outFormat.width(), _outFormat.height());
	if(kDbg) printf("SyLens: output bbox is %dx%d to %dx%d\n", obox.x(), obox.y(), obox.r(), obox.t());
	
	info_.set(obox);
}

void SyLens::_request(int x, int y, int r, int t, ChannelMask channels, int count)
{
	if(kOnlyFormat) return;
	
	if(kDbg) printf("SyLens: Received request %d %d %d %d\n", x, y, r, t);
	ChannelSet c1(channels); in_channels(0,c1);
	
	Vector2 bl(x, y), br(r, y), tr(r, t), tl(x, t);
	
	if(kMode == UNDIST) {
		distortVectorIntoSource(bl);
		distortVectorIntoSource(br);
		distortVectorIntoSource(tl);
		distortVectorIntoSource(tr);
	} else {
		undistortVectorIntoDest(bl);
		undistortVectorIntoDest(br);
		undistortVectorIntoDest(tl);
		undistortVectorIntoDest(tr);
	}
	
	// Request the same part of the input distorted. However if rounding errors have taken place 
	// it is possible that in engine() we will need to sample from the pixels slightly outside of this area.
	// If we don't request it we will get black pixels in there, so we add a small margin on all sides
	// to give us a little cushion
	const unsigned int safetyPadding = 4;
	input0().request(
		round(bl.x - safetyPadding),  
		round(bl.y  - safetyPadding),
		round(tr.x + safetyPadding),
		round(tr.y + safetyPadding),
		channels, count);
}
