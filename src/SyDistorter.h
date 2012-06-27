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
   
	// The cubic parameter will usually have the opposite sign of the main distortion (ie one is positive, the other negative).
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
	
	// Returns the current aspect
	double aspect();
	
	// Sets centerpoint shifts
	void set_center_shift(double u, double v);
	
	// Removes distortion in-place from the Vector2 at the passed reference.
	// The passed vector should be in the [-1..1, -1..1] coordinates used in Syntheyes
	void remove_disto(Vector2&);
	
	// Applies distortion in-place to the Vector2 at the passed reference.
	// The passed vector should be in the [-1..1, -1..1] coordinates used in Syntheyes
	void apply_disto(Vector2&);
	
	// Applies distortion to the passed Nuke UV UVW Vector4 coordinates at the passed reference.
	// The UV coordinates should be premultiplied by the W component and be in the [0..1, 0..1] coordinates
	void distort_uv(Vector4& uv);
	
	// Generates knobs into the passed knob callback, but without the aspect control
	// The knobs will control the variables in the object directly
	void knobs(Knob_Callback f);

	// Generates knobs into the passed knob callback, including the aspect knov
	// The knobs will control the variables in the object directly
	void knobs_with_aspect(Knob_Callback f);
	
	// Call this from _validate(). This will, if necessary, update the internal LUT
	// used by the distortion algorithm.
	void recompute_if_needed();
	
	// Returns the hash of all the distortion controls. This hash value can be used to
	// uniquely classify the distortion model
	U64 compute_hash();

	
private:
	double lerp(double x, double left_x, double right_x, double left_y, double right_y);
	double undistort(double);
	double undistort_sampled(double);
	double undistort_approximated(double);
	double distort_sampled(double);
	double distort_radial(double);
	void recompute();
	void clear_lut();
};
