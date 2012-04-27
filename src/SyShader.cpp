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
	// The distortion engine
	SyDistorter distorter;
	float _aspect;

public:

	static const Description description;
	const char* Class() const {return CLASS;}
	const char* node_help() const {return HELP;}

	SyShader(Node* node) : Material(node)
	{
		kShaderType = 0;
		_aspect = 1.0f;
	}
	
	/* virtual */
	void append(Hash& hash)
	{
		hash.append(VERSION);
		hash.append(distorter.compute_hash());
		Material::append(hash);
	}

	void _validate(bool for_real) {

		Format f = input0().format();
		_aspect = float(f.width()) / float(f.height()) *  f.pixel_aspect();
		distorter.set_aspect(_aspect);
		distorter.recompute_if_needed();
		Material::_validate(for_real);
	}

	/*virtual*/
	void vertex_shader(VertexContext& vtx) {

		if (kShaderType == 0) {

			Vector4& uv = vtx.vP.UV();
			distorter.distort_uv(uv);
		}

		input0().vertex_shader(vtx);
	}
	
	/* Request some more image - TODO */
	/*virtual*/
//	void _request(int x, int y, int r, int t, ChannelMask channels, int count)
//	{
//		Vector2 corner(_aspect, 1);
//		distorter.apply_disto(corner);
//		float x_mult = 1 / corner.x;
//		float y_mult = 1 / corner.y;
//		Material::_request(ceil(x * x_mult), ceil(y * y_mult), ceil(r * x_mult), ceil(t * y_mult), channels, count);
//	}

	/*virtual*/
	void fragment_shader(const VertexContext& vtx, Pixel& out) {

		if (kShaderType == 1) {
			VertexContext new_vtx(vtx);
			Vector4& uv = new_vtx.vP.UV();
			distorter.distort_uv(uv);
			input0().fragment_shader(new_vtx,out);
		} else {
			input0().fragment_shader(vtx, out);
		}
	}
	
	
	// TODO: write me! now the display is botched
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
				"    Surfaces between vertices are linearly interpolated.\n"
				"    The result can be redistorted by SyCamera."

				"\n\nfragment shader:\n"
				"    Distortion is applied for each fragment (pixel).\n"
				"    Use this mode if you need an accurate result\n"
				"    regardless of how dense the geometry is.\n"
				"    The result will NOT be redistorted by SyCamera.");
		
		distorter.knobs(f);
		
		Divider(f, 0);
		std::ostringstream ver;
		ver << "SyShader v." << VERSION;
		Text_knob(f, ver.str().c_str());
	}

};

static Op* build(Node* node)
{
	return new SyShader(node);
}
const Op::Description SyShader::description(CLASS, build);

