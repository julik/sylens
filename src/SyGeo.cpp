static const char* const CLASS = "SyGeo";
static const char* const HELP = "This node will undistort the XY coordinates of the vertices.\n"
	"if it's input object. It's best for working with Cards with \"image aspect\" enabled.\n"
	"Note that the input Card should not have any transforms since the distortion happens in the world space.\n"
	"Contact me@julik.nl if you need help with the plugin.";

#include "VERSION.h"

#include "DDImage/ModifyGeo.h"
#include "DDImage/Scene.h"
#include "DDImage/Knob.h"
#include "DDImage/Knobs.h"
#include "SyDistorter.cpp"
#include <sstream>

using namespace DD::Image;

class SyGeo : public ModifyGeo
{
private:
	
	// The distortion engine
	SyDistorter distorter;
	const char* point_local_attrib_name;
	
	// Group type ptr
	DD::Image::GroupType t_group_type;
	
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
		point_local_attrib_name = "pw";
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
	
	// This is needed to preserve UVs which are already there
	void keep_point_locals(int index, GeoInfo& info, GeometryList& out)
	{
		
		// get the original point_local attribute used to restore untouched point_local coordinate
		const AttribContext* context = info.get_attribcontext(point_local_attrib_name);
		AttributePtr point_local_original = context ? context->attribute : AttributePtr();
		
		if(!point_local_original){
			Op::error( "Missing \"%s\" channel from geometry", point_local_attrib_name );
			return;
		}

		t_group_type = context->group; // texture coordinate group type

		// we have two possibilities:
		// the point_local coordinate are stored in Group_Points or in Group_Vertices way
		// sanity check
		assert(t_group_type == Group_Points || t_group_type == Group_Vertices);
		
		// create a buffer to write on it
		Attribute* point_local = out.writable_attribute(index, t_group_type, point_local_attrib_name, VECTOR4_ATTRIB);
		assert(point_local);

		// copy all original texture coordinate if available
		if (point_local_original){
			// sanity check
			assert(point_local->size() == point_local_original->size());

			for (unsigned i = 0; i < point_local->size(); i++) {
				point_local->vector4(i) = point_local_original->vector4(i);
			}
		}
	}
	
	// Apply distortion to each element in the passed vertex or point attribute
	void distort_each_element_in_attribute(Attribute* attr, unsigned const int num_of_elements)
	{
		if(!attr) return;
		for (unsigned point_idx = 0; point_idx < num_of_elements; point_idx++) {
			Vector3& v = attr->vector3(point_idx);
			// The Card has it's vertices in the [-0.5, 0.5] space with aspect applied
			Vector2 xy(v.x * 2, v.y * 2);
			distorter.remove_disto(xy);
			v.x = xy.x / 2.0f;
			v.y = xy.y / 2.0f;
		}
	}
	
	void modify_geometry(int obj, Scene& scene, GeometryList& out)
	{
		// Call the engine on all the caches:
		for (unsigned i = 0; i < out.objects(); i++) {
			GeoInfo& info = out[i];
			
			// Copy over old UV attributes
			keep_point_locals(i, info, out);
			
			// Reusable pointer for the attribute we are going to be writing to
			Attribute* point_local;
			
			// Copy over pt attributes
			point_local = out.writable_attribute(i, Group_Points, point_local_attrib_name, VECTOR3_ATTRIB);
			distort_each_element_in_attribute(point_local, info.points());
			
			// If the previously detected group type is vertex attribute we need to distort it as well
			// since vertex attribs take precedence and say a Sphere in Nuke has vertex attribs
			// as opposed to point attribs :-( so justified double work here
			if(t_group_type == Group_Vertices) {
				point_local = out.writable_attribute(i, Group_Vertices, point_local_attrib_name, VECTOR3_ATTRIB);
				distort_each_element_in_attribute(point_local, info.vertices()); // Copy over vertex attributes
			}
		}
	}
};

static Op* build(Node* node)
{
	return new SyGeo(node);
}
const Op::Description SyGeo::description(CLASS, build);

