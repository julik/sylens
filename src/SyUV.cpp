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
	
	double k_coeff, k_cube, aspect, centerpoint_shift_u_, centerpoint_shift_v_;
	const char* uv_attrib_name;
	
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

	SyUV(Node* node) : ModifyGeo(node)
	{
		k_coeff = 0.0f;
		k_cube = 0.0f;
		uv_attrib_name = "uv";
		aspect = 1.78f; // TODO: retreive from script defaults
	}
	
	void append(Hash& hash) {
		hash.append(VERSION);
		hash.append(__DATE__);
		hash.append(__TIME__);
		
		// Knobs that change the SyLens algo
		hash.append(k_coeff);
		hash.append(k_cube);
		hash.append(aspect);
		hash.append(uv_attrib_name);
		
		ModifyGeo::append(hash); // the super called he wants his pointers back
	}
	
	void knobs(Knob_Callback f)
	{
		ModifyGeo::knobs(f);
		
		Knob* _kKnob = Float_knob( f, &k_coeff, "k" );
		_kKnob->label("k");
		_kKnob->tooltip("Set to the same distortion as calculated by Syntheyes");
		_kKnob->set_range(-0.5f, 0.5f, true);
		
		Knob* _kCubeKnob = Float_knob( f, &k_cube, "kcube" );
		_kCubeKnob->label("kcube");
		_kCubeKnob->tooltip("Set to the same cubic distortion as applied in the Image Preparation in Syntheyes");
		
		Knob* _aKnob = Float_knob( f, &aspect, "aspect" );
		_aKnob->label("aspect");
		_aKnob->tooltip("Set to the aspect of your distorted texture (like 1.78 for 16:9)");
		
		Knob* _uvAttributeName = String_knob(f, &uv_attrib_name, "uv_attrib_name");
		_uvAttributeName->label("uv attrib name");
		_uvAttributeName->tooltip("Set to the name of the UV attribute you want to modify (default is usually good)");
		
		Knob* _uKnob = Float_knob( f, &centerpoint_shift_u_, "ushift" );
		_uKnob->label("horizontal shift");
		_uKnob->tooltip("Set this to the X window offset if your optical center is off the centerpoint.");
		
		Knob* _vKnob = Float_knob( f, &centerpoint_shift_v_, "vshift" );
		_vKnob->label("vertical shift");
		_vKnob->tooltip("Set this to the Y window offset if your optical center is off the centerpoint.");
		
		Divider(f, 0);
		
		std::ostringstream ver;
		ver << "SyUV v." << VERSION << " " << __DATE__ << " " << __TIME__;
		Text_knob(f, ver.str().c_str());
	}

	void get_geometry_hash()
	{
		// Get all hashes up-to-date
		ModifyGeo::get_geometry_hash();
		
		// Knobs that change the SyLens algo:
		geo_hash[Group_Points].append(k_coeff);
		geo_hash[Group_Points].append(k_cube);
		geo_hash[Group_Points].append(aspect);
		geo_hash[Group_Points].append(uv_attrib_name);
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

		DD::Image::GroupType t_group_type = context->group; // texture coordinate group type

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
	
	void modify_geometry(int obj, Scene& scene, GeometryList& out)
	{
		const char* uv_attrib_name = "uv";
		
		distorter.set_coefficients(k_coeff, k_cube, aspect);
		distorter.set_center_shift(centerpoint_shift_u_, centerpoint_shift_v_);
		
		// Call the engine on all the caches:
		for (unsigned i = 0; i < out.objects(); i++) {
			GeoInfo& info = out[i];
			
			// Copy over old UV attributes
			keep_uvs(i, info, out);
			
			// TODO: investigate difference between vertex and point UVs
			
			// Create a point attribute
			Attribute* uv = out.writable_attribute(i, Group_Points, uv_attrib_name, VECTOR4_ATTRIB);
			if(!uv) return;
			
			for (unsigned p = 0; p < info.points(); p++) {
				distort_point(uv->vector4(p));
			}
		}
	}
	
	// Perform a transform in UV space. We need to adjust ALL of the UV coordinates at
	// once, and there are four of them.
	// To achieve what we want we need to DISTORT the UV coordinates. Once they are distorted
	// the end projection will be straightened since it will be sampling from the distorted grid.
	void distort_point(Vector4& pt)
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
		double x = ((pt.x / pt.w) - centerpoint_shift_in_uv_space) * factor;
		double y = ((pt.y / pt.w) - centerpoint_shift_in_uv_space) * factor;
		
		Vector2 syntheyes_uv(x, y);
		
		// Call the SY algo
		distorter.apply_disto(syntheyes_uv);
		
		syntheyes_uv.x = ((syntheyes_uv.x / factor) + centerpoint_shift_in_uv_space) * pt.w;
		syntheyes_uv.y = ((syntheyes_uv.y / factor) + centerpoint_shift_in_uv_space) * pt.w;
		
		pt.set(syntheyes_uv.x, syntheyes_uv.y, pt.z, pt.w);
	}
};

static Op* build(Node* node)
{
	return new SyUV(node);
}
const Op::Description SyUV::description(CLASS, build);

