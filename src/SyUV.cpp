/*

This node implements the first part of the workflow described here
http://forums.thefoundry.co.uk/phpBB2/viewtopic.php?p=1915&sid=497b22394ab62d0bf8bd7a35fa229913

*/

static const char* const CLASS = "SyUV";
static const char* const HELP = "This node will apply the Syntheyes undistortion in the UV space of your geometry. "
	"Use ProjectUV or just a card to get a basic geo, and apply this node - your UVs will be undistorted. "
	"Contact me@julik.nl if you need help with the plugin.";

#include "VERSION.h"

#include "DDImage/ModifyGeo.h"
#include "DDImage/Scene.h"
#include "DDImage/Knob.h"
#include "DDImage/Knobs.h"
#include <sstream>
#include "SyDistorter.cpp"

using namespace DD::Image;

class SyUV : public ModifyGeo
{
private:
	
	const char* uv_attrib_name;
	
	// The distortion engine
	SyDistorter distorter;
	
	// Group type ptr detected in keep_uvs()
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

	SyUV(Node* node) : ModifyGeo(node)
	{
		uv_attrib_name = "uv";
	}
	
	void append(Hash& hash) {
		hash.append(VERSION);
		
		// Knobs that change the SyLens algo
		hash.append(distorter.compute_hash());
		hash.append(uv_attrib_name);
		ModifyGeo::append(hash); // the super called he wants his pointers back
	}
	
	void knobs(Knob_Callback f)
	{
		ModifyGeo::knobs(f);
		
		distorter.knobs_with_aspect(f);
		
		Knob* uv_attr_name_knob = String_knob(f, &uv_attrib_name, "uv_attrib_name");
		uv_attr_name_knob->label("uv attrib name");
		uv_attr_name_knob->tooltip("Set to the name of the UV attribute you want to modify (default is usually good)");
		
		Divider(f, 0);
		std::ostringstream ver;
		ver << "SyUV v." << VERSION;
		Text_knob(f, ver.str().c_str());
	}

	void get_geometry_hash()
	{
		// Get all hashes up-to-date
		ModifyGeo::get_geometry_hash();
		
		// Knobs that change the SyLens algo:
		geo_hash[Group_Points].append(distorter.compute_hash());
		geo_hash[Group_Points].append(uv_attrib_name);
	}
	
	void _validate(bool for_real)
	{
		distorter.recompute_if_needed();
		return ModifyGeo::_validate(for_real);
	}
	
	// This is needed to preserve UVs which are already there
	void keep_uvs(int index, GeoInfo& info, GeometryList& out)
	{
		
		// get the original uv attribute used to restore untouched uv coordinate
		const AttribContext* context = info.get_attribcontext(uv_attrib_name);
		AttributePtr uv_original = context ? context->attribute : AttributePtr();
		
		if(!uv_original){
			Op::error( "Missing \"%s\" channel from geometry", uv_attrib_name );
			return;
		}

		t_group_type = context->group; // texture coordinate group type

		// we have two possibilities:
		// the uv coordinate are stored in Group_Points or in Group_Vertices way
		// sanity check
		assert(t_group_type == Group_Points || t_group_type == Group_Vertices);
		
		// create a buffer to write on it
		Attribute* uv = out.writable_attribute(index, t_group_type, uv_attrib_name, VECTOR4_ATTRIB);
		assert(uv);

		// copy all original texture coordinate if available
		if (uv_original){
			// sanity check
			assert(uv->size() == uv_original->size());

			for (unsigned i = 0; i < uv->size(); i++) {
				uv->vector4(i) = uv_original->vector4(i);
			}
		}
	}
	
	// Apply distortion to each element in the passed vertex or point attribute
	void distort_each_element_in_attribute(Attribute* attr, unsigned const int num_of_elements)
	{
		if(!attr) return;
		for (unsigned point_idx = 0; point_idx < num_of_elements; point_idx++) {
			distorter.distort_uv(attr->vector4(point_idx));
		}
	}
	
	void modify_geometry(int obj, Scene& scene, GeometryList& out)
	{
		// Call the engine on all the caches:
		for (unsigned i = 0; i < out.objects(); i++) {
			GeoInfo& info = out[i];
			
			// Copy over old UV attributes
			keep_uvs(i, info, out);
			
			// Reusable pointer for the attribute we are going to be writing to
			Attribute* uv;
			
			// Copy over pt attributes
			uv = out.writable_attribute(i, Group_Points, uv_attrib_name, VECTOR4_ATTRIB);
			distort_each_element_in_attribute(uv, info.points());
			
			// If the previously detected group type is vertex attribute we need to distort it as well
			// since vertex attribs take precedence and say a Sphere in Nuke has vertex attribs
			// as opposed to point attribs :-( so justified double work here
			if(t_group_type == Group_Vertices) {
				uv = out.writable_attribute(i, Group_Vertices, uv_attrib_name, VECTOR4_ATTRIB);
				distort_each_element_in_attribute(uv, info.vertices()); // Copy over vertex attributes
			}
		}
	}
};

static Op* build(Node* node)
{
	return new SyUV(node);
}
const Op::Description SyUV::description(CLASS, build);

