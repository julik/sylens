// SyShader : A Nuke plugin to distort UV coordinates at a shader stage
//
// Ivan Busquets - 2012

static const char* const CLASS = "SyShader";
static const char* const HELP = "A UV-modifying shader to remove"
		"lens distortion from the input texture "
		"based on the lens distortion model used by Syntheyes."
		;

#include "VERSION.h"

#include <DDImage/Material.h>
#include <DDImage/ViewerContext.h>
#include <DDImage/Pixel.h>
#include <DDImage/Knobs.h>
#include <DDImage/Scene.h>
#include <DDImage/Iop.h>
#include <DDImage/Tile.h>
#include <DDImage/gl.h>
#include <sstream>
#include "SyDistorter.cpp"


using namespace DD::Image;


static const char* const k_shader_types[] = {
		"vertex shader",
		"fragment shader",
		NULL
};


using namespace DD::Image;

class SyShader : public Material
{
	int kShaderType;

private:

	double k_coeff, k_cube, centerpoint_shift_u_, centerpoint_shift_v_;
	// The distortion engine
	SyDistorter distorter_;


public:

	static const Description description;
	const char* Class() const {return CLASS;}
	const char* node_help() const {return HELP;}

	SyShader(Node* node) : Material(node)
	{
		k_coeff = 0.0f;
		k_cube = 0.0f;
		kShaderType = 0;

	}


	void _validate(bool for_real) {

		Format f = input0().format();
		double _aspect;
		_aspect = float(f.width()) / float(f.height()) *  f.pixel_aspect();

		// Set the coefficients for the distorter
		distorter_.set_coefficients(k_coeff, k_cube, _aspect);
		distorter_.set_center_shift(centerpoint_shift_u_, centerpoint_shift_v_);
		Material::_validate(for_real);

	}

	/*virtual*/
	void vertex_shader(VertexContext& vtx) {

		if (kShaderType == 0) {

			Vector4& uv = vtx.vP.UV();
			distorter_.undistort_uv(uv);
		}

		input0().vertex_shader(vtx);
	}

	/*virtual*/
	void fragment_shader(const VertexContext& vtx, Pixel& out) {

		if (kShaderType == 1) {
			VertexContext new_vtx(vtx);
			Vector4& uv = new_vtx.vP.UV();
			distorter_.undistort_uv(uv);
			input0().fragment_shader(new_vtx,out);
		}

		else {
			input0().fragment_shader(vtx, out);
		}
	}


	bool shade_GL(ViewerContext* ctx, GeoInfo& geo)
	{

		// Not doing any distortion in the GL view
		// Passing the input's shade_GL instead
		return input0().shade_GL(ctx, geo);

	}

	void knobs(Knob_Callback f)
	{

		Knob* _shaderType = Enumeration_knob(f, &kShaderType, k_shader_types, "shader_type");
		_shaderType->tooltip(
				"vertex shader:\n"
				"    Each vertex's UV is modified according to\n"
				"    the distortion parameters.\n"
				"    Surfaces between vertices are linearly interpolated."

				"\n\nfragment shader:\n"
				"    Distortion is applied for each fragment (pixel).\n"
				"    Use this mode if you need an accurate result\n"
				"    regardless of how dense the geometry is.");

		Knob* _kKnob = Float_knob( f, &k_coeff, "k" );
		_kKnob->label("k");
		_kKnob->tooltip("Set to the same distortion as calculated by Syntheyes");
		_kKnob->set_range(-0.5f, 0.5f, true);

		Knob* _kCubeKnob = Float_knob( f, &k_cube, "kcube" );
		_kCubeKnob->label("kcube");
		_kCubeKnob->tooltip("Set to the same cubic distortion as applied in the Image Preparation in Syntheyes");
		_kCubeKnob->set_range(-0.4f, 0.4f, true);

		Knob* _uKnob = Float_knob( f, &centerpoint_shift_u_, "ushift" );
		_uKnob->label("horizontal shift");
		_uKnob->tooltip("Set this to the X window offset if your optical center is off the centerpoint.");

		Knob* _vKnob = Float_knob( f, &centerpoint_shift_v_, "vshift" );
		_vKnob->label("vertical shift");
		_vKnob->tooltip("Set this to the Y window offset if your optical center is off the centerpoint.");

		Divider(f, 0);

		std::ostringstream ver;
		ver << "SyShader v." << VERSION << " " << __DATE__ << " " << __TIME__;
		Text_knob(f, ver.str().c_str());
	}

};

static Op* build(Node* node)
{
	return new SyShader(node);
}
const Op::Description SyShader::description(CLASS, build);

