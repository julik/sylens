static const char* const CLASS = "SyGeo";
static const char* const HELP = "This node will undistort the XY coordinates of the vertices.\n"
	"if it's input object. It's best for working with Cards with \"image aspect\" enabled.\n"
	"Contact me@julik.nl if you need help with the plugin.";

#include "VERSION.h"

#include "DDImage/ModifyGeo.h"
#include "DDImage/Scene.h"
#include "DDImage/Knob.h"
#include "DDImage/Knobs.h"
#include "SyDistorter.cpp"
#include <sstream>
#include <iostream>

using namespace DD::Image;


class SyGeo : public GeoOp
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

	SyGeo(Node* node) : GeoOp(node)
	{
	}
	
	void append(Hash& hash) {
		// Knobs that change the SyLens algo
		hash.append(distorter.compute_hash());
		hash.append(VERSION);
		GeoOp::append(hash); // the super called he wants his pointers back
	}
	
	void knobs(Knob_Callback f)
	{
		GeoOp::knobs(f);
		distorter.knobs_with_aspect(f);
		
		Divider(f, 0);
		std::ostringstream ver;
		ver << "SyGeo v." << VERSION;
		Text_knob(f, ver.str().c_str());
	}

	void get_geometry_hash()
	{
		// Get all hashes up-to-date
		GeoOp::get_geometry_hash();
		geo_hash[Group_Points].append(distorter.compute_hash());
	}
	
	void _validate(bool for_real)
	{
		distorter.recompute_if_needed();
		input0()->validate(for_real);
		return GeoOp::_validate(for_real);
	}
	
	void undistort_points_of(int obj_idx, GeometryList& out)
	{
		// Save the pointer to the source point list first. We pull
		// it from the GeoInfo for the object we are processing
		GeoInfo& object = out[obj_idx];
		const PointList* src_pts = object.point_list();
		
		// Allocate the destination points, they will be blanked out
		PointList* dest_points = out.writable_points(obj_idx);
		
		// Copy points from source to destination, removing distortion
		// in the process:
		for (unsigned i = 0; i < dest_points->size(); i++) {
			Vector3 v = (*src_pts)[i];
			
			// The Card has it's vertices in the [-0.5, 0.5] space with aspect applied
			Vector2 xy(v.x * 2, v.y * 2);
			distorter.remove_disto(xy);
			
			Vector3& dest_v = (*dest_points)[i];
			dest_v.z = v.z;
			dest_v.x = xy.x / 2.0f;
			dest_v.y = xy.y / 2.0f;
		}
	}
	
	void geometry_engine(Scene& scene, GeometryList& out)
	{
		input0()->get_geometry(scene, out);
		
		unsigned num_objects = out.objects();
		for(unsigned i = 0; i < num_objects; i++) {
			undistort_points_of(i, out);
		}
	}
	
	// Needed to make the object selectable
	void select_geometry(ViewerContext* ctx, GeometryList &scene_objects)
	{
		input0()->select_geometry(ctx, scene_objects);
	}
};

static Op* build(Node* node)
{
	return new SyGeo(node);
}
const Op::Description SyGeo::description(CLASS, build);

