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
	double r2, f, rp;
	LutTuple(double nr2, double nf2) {
		r2 = nr2;
		f = nf2;
		rp = r2 * f;
	}
};

typedef std::vector<LutTuple*> Lut;

class SyDistorter
{

private:

	double k_, k_cube_, aspect_;
	Lut lut;
	
public:

	SyDistorter();
	~SyDistorter();
	
	void set_coefficients(double k, double k_cube, double aspect);
	void remove_disto(Vector2&);
	void apply_disto(Vector2&);
	void append(Hash&);

private:
	double lerp(double x, double left_x, double right_x, double left_y, double right_y);
	double undistort_sampled(double);
	double distort_sampled(double);
	double distort_radial(double);
	void recompute();
};
