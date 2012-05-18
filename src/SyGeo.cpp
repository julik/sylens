static const char* const CLASS = "SyGeo";
static const char* const HELP = "This node will undistort the XY coordinates of the vertices\n"
	"Contact me@julik.nl if you need help with the plugin.";

#include "VERSION.h"

#include "DDImage/ModifyGeo.h"
#include "DDImage/Scene.h"
#include "DDImage/Knob.h"
#include "DDImage/Knobs.h"
#include <sstream>
#include "SyDistorter.cpp"

using namespace DD::Image;

class SyGeo : public ModifyGeo
{
private:
	
	// The distortion engine
	SyDistorter distorter;
	
public:

	static const Description description;
	
	const char* Class() const
	{
		return CLASS;
	}
	
	const char* node_help() const
	{
		return HELP;
	}

	SyGeo(Node* node) : ModifyGeo(node)
	{
	}
	
	void append(Hash& hash) {
		// Knobs that change the SyLens algo
		hash.append(distorter.compute_hash());
		hash.append(VERSION);
		ModifyGeo::append(hash); // the super called he wants his pointers back
	}
	
	void knobs(Knob_Callback f)
	{
		ModifyGeo::knobs(f);
		
		distorter.knobs_with_aspect(f);
		
		Divider(f, 0);
		std::ostringstream ver;
		ver << "SyGeo v." << VERSION;
		Text_knob(f, ver.str().c_str());
	}

	void get_geometry_hash()
	{
		// Get all hashes up-to-date
		ModifyGeo::get_geometry_hash();
		// Knobs that change the SyLens algo:
		geo_hash[Group_Points].append(distorter.compute_hash());
	}
	
	void _validate(bool for_real)
	{
		distorter.recompute_if_needed();
		return ModifyGeo::_validate(for_real);
	}
	
	void modify_geometry(int obj, Scene& scene, GeometryList& out)
	{
		PointList* points = out.writable_points(obj);
		const unsigned n = points->size();
		// Transform points:
		for (unsigned i = 0; i < n; i++) {
			Vector3& v = (*points)[i];
			Vector2 xy(v.x, v.y);
			distorter.remove_disto(xy);
			v.x = xy.x;
			v.y = xy.y;
		}
	}
};

static Op* build(Node* node)
{
	return new SyGeo(node);
}
const Op::Description SyGeo::description(CLASS, build);

