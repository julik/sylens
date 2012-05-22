// For max/min on containers
#include <algorithm>

#include "DDImage/Row.h"
#include "DDImage/Pixel.h"
#include "DDImage/Filter.h"
#include "DDImage/Knobs.h"

using namespace DD::Image;

class LutTuple
{
public:
	double r, f, r_distorted;
	LutTuple(double radius, double disto_f) {
		r = radius;
		f = disto_f;
		r_distorted = radius * f;
	}
	bool operator<(LutTuple rhs) { return r < rhs.r; }
};

typedef std::vector<LutTuple*> Lut;

class SyDistorter
{

private:

	double k_, k_cube_, aspect_, center_shift_u_, center_shift_v_;
	U64 hash;
	Lut lut;
	
public:

	SyDistorter();
	~SyDistorter();
	
	// Sets the coefficients that affect the distortion lookup table.
	void set_coefficients(double k, double k_cube, double aspect);
	// Externally set the aspect
	void set_aspect(double);
	// Sets centerpoint shifts
	void set_center_shift(double u, double v);
	
	void remove_disto(Vector2&);
	void apply_disto(Vector2&);
	void distort_uv(Vector4& uv);
	
	void knobs(Knob_Callback f);
	void knobs_with_aspect(Knob_Callback f);
	
	void recompute_if_needed();
	U64 compute_hash();
	double aspect();
	
private:
	double lerp(double x, double left_x, double right_x, double left_y, double right_y);
	double undistort(double);
	double undistort_sampled(double);
	double undistort_approximated(double);
	double distort_sampled(double);
	double distort_radial(double);
	void recompute();
};
