/*
	This plugin uses the lens distortion model provided by Russ Anderson of the SynthEyes camera tracker
	fame. It is made so that it's output is _identical_ to the Image Preparation tool of the SY camera tracker.
	so you can use it INSTEAD of the footage Syntheyes prerenders.
	
	It implements the algorithm described here http://www.ssontech.com/content/lensalg.htm
	
	It is largely based on tx_nukeLensDistortion by Matti Gruener of Trixter Film and Rising Sun Films.
	The code has however been simplified and some features not present in the original version have been added.
	
	Written by Julik Tarkhanov in Amsterdam in 2010-2011 with kind support by HecticElectric.
	I thank the users for their continued support and bug reports.
	For questions mail me(at)julik.nl
	
	The beautiful Crimean landscape shot used in the test script is provided by Tim Parshikov
	and Mikhail Mestezky, 2010.
	
	The code has some more comments than it's 3DE counterpart since we have to do some things that the other plugin
	did not
*/

// For max/min on containers
#include <algorithm>

// For string concats
#include <sstream>

#include "DDImage/Iop.h"
#include "DDImage/Row.h"
#include "DDImage/Pixel.h"
#include "DDImage/Filter.h"
#include "DDImage/Knobs.h"
#include "SyDistorter.cpp"

using namespace DD::Image;

static const char* const CLASS = "SyLens";
static const char* const HELP =  "This plugin undistorts footage according "
	"to the lens distortion model used by Syntheyes. "
	"Contact me@julik.nl if you need help with the plugin.";

#include "VERSION.h"

static const char* const output_mode_names[] = { "remove disto", "apply disto", 0 };

class SyLens : public Iop
{
	//Nuke statics
	
	const char* Class() const { return CLASS; }
	const char* node_help() const { return HELP; }
	static const Iop::Description description;
	
	Filter filter;
	
	enum { UNDIST, REDIST };
	
	// The original size of the plate that we distort
	unsigned int plate_width_, plate_height_;
	
	// The size of the output
	unsigned int out_width_, out_height_;
	
	// Image aspect and NOT the pixel aspect Nuke furnishes us
	double _aspect;
	
	// Movable centerpoint offsets
	double centerpoint_shift_u_, centerpoint_shift_v_;
	
	// Stuff driven by knobbz
	bool k_trim_bbox_to_format_, k_only_format_output_, k_grow_format_;
	int k_output;
	
	// Sampling offset
	int xShift, yShift;
	
	// The distortion engine
	SyDistorter distorter;
	
	// The output format for the node
	Format output_format;
	
public:
	SyLens( Node *node ) : Iop ( node )
	{
		k_output = UNDIST;
		_aspect = 1.33f;
		k_grow_format_ = false;
		k_trim_bbox_to_format_ = false;
		xShift = 0;
		yShift = 0;
	}
	
	void _computeAspects();
	void _validate(bool for_real);
	void _request(int x, int y, int r, int t, ChannelMask channels, int count);
	void engine( int y, int x, int r, ChannelMask channels, Row& out );
	void knobs( Knob_Callback f);
	
	// Hashing for caches. We append our version to the cache hash, so that when you update
	// the plugin all the caches will be flushed automatically
	void append(Hash& hash) {
		hash.append(VERSION);
		hash.append(distorter.compute_hash());
		hash.append(k_grow_format_);
		hash.append(k_trim_bbox_to_format_);
		hash.append(xShift);
		hash.append(yShift);
		Iop::append(hash); // the super called he wants his pointers back
	}
	
	~SyLens () { 
	}
private:
	
	int round(double x);
	double toUv(double, int);
	double fromUv(double, int);
	void absolute_px_to_centered_uv(Vector2&, int, int);
	void centered_uv_to_absolute_px(Vector2&, int, int);
	void distort_px_into_source(Vector2& vec);
	void undistort_px_into_destination(Vector2& vec);
	Box compute_needed_bbox_with_distortion(Box& source, unsigned w, unsigned h, int flag);
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
  return (((absValue - 0.5f) / (absSide - 1.0f)) - 0.5f) * 2.0f;
}

double SyLens::fromUv(double uvValue, int absSide)
{
  return (((uvValue / 2.0f) + 0.5f) * (absSide - 1.0f)) + 0.5f;
}

void SyLens::absolute_px_to_centered_uv(Vector2& xy, int w, int h)
{
	// Nuke coords are 0,0 on lower left
	xy.x = toUv(xy.x, w);
	xy.y = toUv(xy.y, h);
}

void SyLens::centered_uv_to_absolute_px(Vector2& xy, int w, int h)
{
	// Nuke coords are 0,0 on lower left
	xy.x = fromUv(xy.x, w);
	xy.y = fromUv(xy.y, h);
}

/* 
This takes the given Box and the width and height of the Format the box will be
fit in. Then it applies or removes the disto from all the corner points of the bbox
AND, most importantly, from the intersections of the Box with the centerlines of the
Format. These will be only computed only if the Box actually intersects with the format
centerlines. The flag argument accepts the same UNDIST/REDIST flags.
*/
Box SyLens::compute_needed_bbox_with_distortion(Box& inf, unsigned ow, unsigned oh, int flag)
{
	// Just distorting the four corners of the bbox is NOT enough. We also need to find out whether
	// the bbox intersects the centerlines. Since the distortion is the most extreme at the centerlines if
	// we just take the corners we might be chopping some image away. So to get a reliable bbox we need to check
	// our padding at 6 points - the 4 extremes and where the bbox crosses the middle of the coordinates
	int xMid = ow/2;
	int yMid = oh/2;

	std::vector<Vector2*> pointsOnBbox;
	
	// Add the standard two points - LR and TR
	pointsOnBbox.push_back(new Vector2((float)inf.x(), (float)inf.y()));
	pointsOnBbox.push_back(new Vector2((float)inf.r(), (float)inf.t()));
	
	// Add the TL and LR as well
	pointsOnBbox.push_back(new Vector2((float)inf.x(), (float)inf.t()));
	pointsOnBbox.push_back(new Vector2((float)inf.r(), (float)inf.y()));
	
	// If our box intersects the midplane on X add the points where the bbox crosses centerline
	if((inf.x() < xMid) && (inf.r() > xMid)) {
		// Find the two intersections and add them
		pointsOnBbox.push_back( new Vector2(xMid, (float)inf.y()) );
		pointsOnBbox.push_back( new Vector2(xMid, (float)inf.t()) );
	}
	
	// If our box intersects the midplane on Y add the points where the bbox crosses centerline
	if((inf.y() < yMid) && (inf.t() > yMid)) {
		pointsOnBbox.push_back( new Vector2((float)inf.x(), yMid) );
		pointsOnBbox.push_back( new Vector2((float)inf.r(), yMid) );
	}
	
	std::vector<int> xValues;
	std::vector<int> yValues;
	
	// Apply the operation to each bounding point
	for(unsigned i = 0; i < pointsOnBbox.size(); i++) {
		if(flag == UNDIST) {
			undistort_px_into_destination(*pointsOnBbox[i]);
		} else {
			distort_px_into_source(*pointsOnBbox[i]);
		}
		xValues.push_back((int)pointsOnBbox[i]->x);
		yValues.push_back((int)pointsOnBbox[i]->y);
	}
	
	// Find the maximum coverage area for the given points
	int minX, minY, maxX, maxY;
	minX = *std::min_element(xValues.begin(), xValues.end());
	maxX = *std::max_element(xValues.begin(), xValues.end());
	minY = *std::min_element(yValues.begin(), yValues.end());
	maxY = *std::max_element(yValues.begin(), yValues.end());
	
	return Box(minX, minY, maxX, maxY);
}

// Get a coordinate that we need to sample from the SOURCE distorted image to get at the absXY
// values in the RESULT
void SyLens::distort_px_into_source(Vector2& absXY) {
	absolute_px_to_centered_uv(absXY, plate_width_, plate_height_);
	distorter.apply_disto(absXY);
	centered_uv_to_absolute_px(absXY, plate_width_, plate_height_);
}

// This is still a little wrongish but less wrong than before
void SyLens::undistort_px_into_destination(Vector2& absXY) {
	absolute_px_to_centered_uv(absXY, plate_width_, plate_height_);
	distorter.remove_disto(absXY);
	centered_uv_to_absolute_px(absXY, plate_width_, plate_height_);
}
 
// The image processor that works by scanline. Y is the scanline offset, x is the pix,
// r is the length of the row. We are now effectively in the undistorted coordinates, mind you!
void SyLens::engine ( int y, int x, int r, ChannelMask channels, Row& out )
{
	
	foreach(z, channels) out.writable(z);
	
	Pixel pixel(channels);
	const float sampleOff = 0.5f;
	
	Vector2 sampleFromXY(0.0f, 0.0f);
	for (; x < r; x++) {
		
		sampleFromXY = Vector2(x - xShift, y - yShift);
		
		if( k_output == UNDIST) {
			distort_px_into_source(sampleFromXY);
		} else {
			undistort_px_into_destination(sampleFromXY);
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
	Knob* _output_selector = Enumeration_knob(f, &k_output, output_mode_names, "output");
	_output_selector->label("output");
	_output_selector->tooltip("Pick your poison");
	
	// TODO: Remove in SyLens 4. Old mode configuration knob that we just hide
	const char* old_mode_value = "nada";
	Knob* hidden_mode = String_knob(f, &old_mode_value, "mode");
	hidden_mode->set_flag(Knob::INVISIBLE);
	hidden_mode->set_flag(Knob::DO_NOT_WRITE);
	
	distorter.knobs(f);
	filter.knobs(f);
	
	// Utility functions
	Knob* kTrimKnob = Bool_knob( f, &k_trim_bbox_to_format_, "trim");
	kTrimKnob->label("trim bbox");
	kTrimKnob->tooltip("When checked, SyLens will crop the output to the format dimensions and reduce the bbox to match format exactly");
	kTrimKnob->set_flag(Knob::STARTLINE);
	
	// Grow plate
	Knob* kGrowKnob = Bool_knob( f, &k_grow_format_, "grow");
	kGrowKnob->label("grow format");
	kGrowKnob->tooltip("When checked, SyLens will expand the actual format of the image along with the bbox."
		"\nThis is useful if you are going to do a matte painting on the output.");
	kGrowKnob->set_flag(Knob::STARTLINE);
	
	Divider(f, 0);
	
	std::ostringstream ver;
	ver << "SyLens v." << VERSION;
	Text_knob(f, ver.str().c_str());
}

// http://stackoverflow.com/questions/485525/round-for-float-in-c
int SyLens::round(double x) {
	return (int)floor(x + 0.5);
}

// The algo works in image aspec, not the pixel aspect. We also have to take the uncrop factor
// into account.
void SyLens::_computeAspects() {
	// Compute the aspect from the input format
	Format f = input0().format();
	
	plate_width_ = round(f.width());
	plate_height_ = round(f.height());

	_aspect = float(plate_width_) / float(plate_height_) *  f.pixel_aspect();
	
	debug("true plate window with uncrop will be %dx%d", plate_width_, plate_height_);
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
	
	// Do not blank away everything
	info_.black_outside(false);
	
	// We need to know our aspects so prep them here
	_computeAspects();
	
	distorter.set_aspect(_aspect);
	distorter.recompute_if_needed();
	
	// Reset pixel shifts to 0 for the case
	// that the grow plate has been disabled
	xShift = 0;
	yShift = 0;
	
	debug("_validate plate size  %dx%d", plate_width_, plate_height_);
	
	// Time to define how big our output will be in terms of format. Format will always be the whole plate.
	// If we only use a bboxed piece of the image we will limit our request to that.
	// For the case when we are working with a 8k by 4k plate with a SMALL CG pink elephant rrright in the left
	// corner we want to actually translate the bbox of the elephant to our distorted pipe downstream. So we need to
	// apply our SuperAlgorizm to the bbox as well and move the bbox downstream too.
	// Grab the bbox from the input first
	Info inf = input0().info();
	debug("Input bbox is %dx%d to %dx%d", inf.x(), inf.y(), inf.r(), inf.t());
	
	Box obox = compute_needed_bbox_with_distortion(inf, plate_width_, plate_height_, k_output);

	// Start with the input format
	output_format = input0().format();
	
	if(k_grow_format_ && k_output == UNDIST) {
		
		// We spare some extra steps and only do this step if the bounding box
		// will actually grow. To determine that, we take the corner at 0,0 (lower left)
		// and we undistort it. If the coordinates end up being negative it means that the
		// undistorted plate will be bigger than the original and we need to compute a
		// new oversize format. We need to store this format as a member to prevent Nuke from
		// crashing (otherwise the Format object goes out of scope and the _validate() of the
		// downstream node cannot get at it) 
		Vector2 corner(0,0);
		undistort_px_into_destination(corner);
		
		// If we undistort and the corner will end up outside - we have overflow
		if(corner.x < 0.0f || corner.y < 0.0f) {
			debug("Barrel distortion and plate needs to grow. Off-corner is %0.5fx%0.5f", corner.x, corner.y);
			
			xShift = (signed)fabs(corner.x);
			yShift = (signed)fabs(corner.y);
			
			// Reassign the output format to something bigger than the original
			output_format = Format(output_format.width() + (xShift * 2), output_format.height() + (yShift * 2), output_format.pixel_aspect());
			
			// Move the bounding box
			obox.move(xShift, yShift);
			
			debug("Oversize format will be %dx%d", output_format.width(), output_format.height());
			
		}
	}
	
	// If trim is enabled we intersect our obox with the format so that there is no bounding box
	// outside the crop area. Thiis handy for redistorted material.
	if(k_trim_bbox_to_format_) obox.intersect(output_format);
	
	debug("Output bbox is %dx%d to %dx%d", obox.x(), obox.y(), obox.r(), obox.t());
	
	// Set the oversize format and the bounding box
	info_.format(output_format);
	info_.set(obox);
}

void SyLens::_request(int x, int y, int r, int t, ChannelMask channels, int count)
{
	ChannelSet c1(channels); in_channels(0,c1);
	
	debug("Received request from downstream [%d,%d]x [%d,%d]", x, y, r, t);

	const signed safetyPadding = 4;
	Box requested(x, y, r, t);
	requested.move(-xShift, -yShift);
	requested.pad(safetyPadding);
	
	// Request the same part of the input, but without distortions
	Box disto_requested = compute_needed_bbox_with_distortion(requested, out_width_, out_height_, UNDIST);

	debug("Will request upstream (accounting for (re)distortion): [%d,%d] by [%d,%d]", 
		disto_requested.x(),
		disto_requested.y(),
		disto_requested.r(),
		disto_requested.t()
	);
	
	input0().request(
		disto_requested.x(),
		disto_requested.y(),
		disto_requested.r(),
		disto_requested.t(),		
		channels, count);
}
