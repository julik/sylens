// For max/min on containers
#include <algorithm>
#include "DDImage/Thread.h"
#include "SyDistorter.h"
#include <iostream>

SyDistorter::SyDistorter()
{
	set_coefficients(0.0f, 0.0f, 1.78);
	center_shift_u_ = 0;
	center_shift_v_ = 0;
}

U64 SyDistorter::compute_hash()
{
	Hash h;
	h.append(k_);
	h.append(k_cube_);
	h.append(aspect_);
	h.append(center_shift_u_);
	h.append(center_shift_v_);
	return h.value();
}

void SyDistorter::recompute_if_needed()
{
	// We have a shared data structure (the lookup table vector),
	// when recomputing it we need to lock the world.
	// http://forums.thefoundry.co.uk/phpBB2/viewtopic.php?t=5955
	Lock* lock = new Lock;
	lock->lock();
	
	int new_hash = compute_hash();
	if(new_hash != hash) {
		hash = new_hash;
		recompute();
	}
	
	lock->unlock();
	delete lock;
}

void SyDistorter::set_aspect(double a)
{
	aspect_ = a;
}

void SyDistorter::set_coefficients(double k, double k_cube, double aspect)
{
	k_ = k;
	k_cube_ = k_cube;
	aspect_ = aspect;
	
	recompute_if_needed();
}

void SyDistorter::set_center_shift(double u, double v)
{
	center_shift_u_ = u;
	center_shift_v_ = v;
}

SyDistorter::~SyDistorter()
{
	// Clear out the LUT
	std::vector<LutTuple*>::iterator tuple_it;
	for(tuple_it = lut.begin(); tuple_it != lut.end(); tuple_it++) {
		delete(*tuple_it);
	}
	// Then the LUT gets deleted
}

void SyDistorter::remove_disto(Vector2& pt)
{
	// Bracket in centerpoint adjustment
	pt.x += center_shift_u_;
	pt.y += center_shift_v_;
	
	double x = pt.x * aspect_;
	double rd = sqrt((fabs(x) * fabs(x)) + (fabs(pt.y) * fabs(pt.y)));
	double inv_f = undistort(rd);
	
	pt.x = pt.x / inv_f;
	pt.y = pt.y / inv_f;
	
	pt.x -= center_shift_u_;
	pt.y -= center_shift_v_;
}

double SyDistorter::undistort(double radius_distorted)
{
	if(radius_distorted < lut.back()->r_distorted) {
		undistort_sampled(radius_distorted);
	} else {
		undistort_approximated(radius_distorted);
	}
}

/* If there is no available value in our LUT we are dealing with image outside of our original coordinates.
All we can do at this point is approximate towards it and compare the resulting distorted radius
with our given one. This has a number of disadvantages. First of all, it's going to get slower the
more we go to the outside of the image frame since the same difference in radius will produce a bigger
distortion. Second, it's kind of slow. However, not many points outside of the image will be estimated that way
but only a handful, so this method will end up unused most of the time.
*/
double SyDistorter::undistort_approximated(double rp)
{
	double r, f, approx_rp, delta;
	r = 0.1f;
	const double inc = 0.001f;
	while(r < 1000) { // Make sure the loop completes at some point
		r += inc;
		f = distort_radial(r);
		approx_rp = r / f;
		if(approx_rp > rp) {
			// We found the upper bound, so we can now go back one step and interpolate
			// between it and the upper bound F.
			r -= inc;
			double left_f = distort_radial(r);
			double left_rp = r / left_f;
			return lerp(rp, left_rp, approx_rp, left_f, f);
		}
	}
	
	// FAIL!
	return 1.0f;
}

double SyDistorter::undistort_sampled(double rd)
{
	std::vector<LutTuple*>::iterator tuple_it;
	LutTuple* left = NULL;
	LutTuple* right = NULL;
	
	for(tuple_it = lut.begin(); 
		tuple_it != lut.end() && !(left && right);
		tuple_it++) {
			
		if((*tuple_it)->r_distorted < rd) {
			left = *tuple_it;
		}
		if ((*tuple_it)->r_distorted > rd) {
			right = *tuple_it;
		}
	}
	return lerp(rd, left->r_distorted, right->r_distorted, left->f, right->f);
}

void SyDistorter::apply_disto(Vector2& pt)
{
	float x = pt.x * aspect_;
	float r = sqrt(x * x + (pt.y * pt.y));
	
	std::vector<LutTuple*>::iterator tuple_it;
	LutTuple* left = NULL;
	LutTuple* right = NULL;
	
	// Find the neihgbouring defined points in the LUT
	for(tuple_it = lut.begin(); 
		tuple_it != lut.end() && !(left && right); 
		tuple_it++) {
		if((*tuple_it)->r < r) {
			left = *tuple_it;
		}
		if ((*tuple_it)->r > r) {
			right = *tuple_it;
		}
	}
	
	float f;
	
	// If we could not find neighbour points just compute it
	if(left && right) {
		// TODO: spline interpolation instead of linear
		f = lerp(r, left->r, right->r, left->f, right->f);
	} else {
		f = distort_radial(r);
	}
	
	// Bracket in centerpoint adjustment
	// move camera gate -> distort -> move camera gate back
	pt.x -= center_shift_u_;
	pt.y -= center_shift_v_;
	
	pt.x = pt.x * f;
	pt.y = pt.y * f;
	
	pt.x += center_shift_u_;
	pt.y += center_shift_v_;
}

double SyDistorter::distort_radial(double r)
{
	double r2 = r * r;
	double f;
	// Skipping the square root speeds things up if we don't need it
	if (fabs(k_cube_) > 0.00001) {
		f = 1 + r2*(k_ + k_cube_ * r);
	} else {
		f = 1 + r2*(k_);
	}
	return f;
}

void SyDistorter::distort_uv(Vector4& uv)
{
	/* 
	
	Citing Jonathan Egstad here:
	
	   http://www.mail-archive.com/nuke-dev@support.thefoundry.co.uk/msg00904.html
	
	W in a uv coordinate is normally 1.0 unless you've projected a point to produce the uv. In that case w
	needs to be divided out of the uv values to get the real uv, and this is not done until the poly
	interpolation/shading stage to preserve correct perspective.
	
	*/
	
	// UV's go 0..1. SY imageplane coordinates go -1..1
	const double factor = 2;
	// Centerpoint is in the middle.
	const double centerpoint_shift_in_uv_space = 0.5f;
	
	// Move the coordinate by 0.5 since Syntheyes assume 0
	// to be in the optical center of the image, and then scale them to -1..1
	double x = ((uv.x / uv.w) - centerpoint_shift_in_uv_space) * factor;
	double y = ((uv.y / uv.w) - centerpoint_shift_in_uv_space) * factor;
	double z = sqrt(x*x + y*y);
	
	Vector2 syntheyes_uv(x, y);
	
	// Call the SY algo
	apply_disto(syntheyes_uv);
	
	syntheyes_uv.x = (syntheyes_uv.x / factor) + centerpoint_shift_in_uv_space;
	syntheyes_uv.y = (syntheyes_uv.y / factor) + centerpoint_shift_in_uv_space;
	
	uv.set(syntheyes_uv.x * uv.w, syntheyes_uv.y * uv.w, z, uv.w);
}

double SyDistorter::aspect()
{
	return aspect_;
}

double SyDistorter::lerp(const double x, const double left_x, const double right_x, const double left_y, const double right_y)
{
	double dx = right_x - left_x;
	double dy = right_y - left_y;
	double t = (x - left_x) / dx;
	return left_y + (dy * t);
}

// Creates knobs related to lens distortion, but without aspect control
// The caller should then set the aspect by itself
void SyDistorter::knobs( Knob_Callback f)
{
	// For info on knob flags see Knob.h
	const int KNOB_ON_SEPARATE_LINE = 0x1000;
	const int KNOB_HIDDEN = 0x0000000000040000;
	
	Knob* _kKnob = Float_knob( f, &k_, "k" );
	_kKnob->label("k");
	_kKnob->tooltip("Set to the same distortion as applied by Syntheyes");
	_kKnob->set_range(-0.3f, 0.3f, false);
	
	Knob* _kCubeKnob = Float_knob( f, &k_cube_, "kcube" );
	_kCubeKnob->label("cubic k");
	_kCubeKnob->tooltip("Set to the same cubic distortion as applied by Syntheyes");
	_kCubeKnob->set_range(-0.1f, 0.1f, false);
	
	Knob* _uKnob = Float_knob( f, &center_shift_u_, "ushift" );
	_uKnob->label("horizontal shift");
	_uKnob->tooltip("Set this to the X window offset if your optical center is off the centerpoint.");
	_uKnob->set_range(-1.0f, 1.0f, true);
	
	Knob* _vKnob = Float_knob( f, &center_shift_v_, "vshift" );
	_vKnob->label("vertical shift");
	_vKnob->tooltip("Set this to the Y window offset if your optical center is off the centerpoint.");
	_vKnob->set_range(-1.0f, 1.0f, true);
}

// Creates knobs related to lens distortion including the aspect knob
void SyDistorter::knobs_with_aspect( Knob_Callback f)
{
	knobs(f);
	Knob* _aKnob = Float_knob( f, &aspect_, "aspect" );
	_aKnob->label("aspect");
	_aKnob->tooltip("Set to the aspect of your distorted plate (like 1.78 for 16:9)");
}

// The number of discrete points we sample on the radius of the distortion.
// The rest is going to be interpolated
static const unsigned int STEPS = 64;

// Updates the internal lookup table
void SyDistorter::recompute()
{
	double r = 0;
	// Max radius will be the original radius at the top-right corner
	double max_r = sqrt((aspect_ * aspect_) + 1);
	double increment = max_r / float(STEPS);
	
	// Clear out the LUT elements so that they don't leak. We could use std::auto_ptr
	// as well...
	std::vector<LutTuple*>::iterator tuple_it;
	for(tuple_it = lut.begin(); tuple_it != lut.end(); tuple_it++) {
		delete (*tuple_it);
	}
	lut.clear();
	
	lut.push_back(new LutTuple(0,1));
	for(unsigned i = 0; i < STEPS; i++) {
		r += increment;
		lut.push_back(new LutTuple(r, distort_radial(r)));
	}
	
	return;
}
