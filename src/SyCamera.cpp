/*

This node implements the second part of the workflow described here
http://forums.thefoundry.co.uk/phpBB2/viewtopic.php?p=1915&sid=497b22394ab62d0bf8bd7a35fa229913

*/

static const char* const CLASS = "SyCamera";
static const char* const HELP = "Camera with Syntheyes lens distortion built in. Contact me@julik.nl if you need help with the plugin.";

#include "VERSION.h"

#include "DDImage/CameraOp.h"
#include "DDImage/Scene.h"
#include "DDImage/Knob.h"
#include "DDImage/Knobs.h"
#include "DDImage/Format.h"
#include <sstream>

// for our friend printf
extern "C" {
#include <stdio.h>
}

#include "SyDistorter.cpp"

using namespace DD::Image;

class SyCamera : public CameraOp
{
public:
	static const Description description;

	double k_coeff, k_cube, k_aspect;
	SyDistorter distorter;
	
	const char* Class() const
	{
		return CLASS;
	}
	
	const char* node_help() const
	{
		return HELP;
	}

	SyCamera(Node* node) : CameraOp(node)
	{
		k_coeff = 0.0f;
		k_cube = 0.0f;
		k_aspect = 1.78f; // TODO: retreive from Format
	}
	
	void append(Hash& hash)
	{
		hash.append(VERSION);
		hash.append(__DATE__);
		hash.append(__TIME__);
		distorter.append(hash);
		CameraOp::append(hash);
	}
	
	void lens_knobs(Knob_Callback f)
	{
		Tab_knob(f, "SyLens");
		
		Knob* _kKnob = Float_knob( f, &k_coeff, "k" );
		_kKnob->label("k");
		_kKnob->tooltip("Set to the same distortion as calculated by Syntheyes");
		_kKnob->set_range(-0.5f, 0.5f, true);
		
		Knob* _kCubeKnob = Float_knob( f, &k_cube, "kcube" );
		_kCubeKnob->label("kcube");
		_kCubeKnob->tooltip("Set to the same cubic distortion as applied in the Image Preparation in Syntheyes");
		
		Knob* _aKnob = Float_knob( f, &k_aspect, "aspect" );
		_aKnob->label("aspect");
		_aKnob->tooltip("Set to the aspect of your distorted plate (like 1.78 for 16:9)");
		
		Divider(f, 0);
		
		std::ostringstream ver;
		ver << "SyCamera v." << VERSION << " " << __DATE__ << " " << __TIME__;
		Text_knob(f, ver.str().c_str());
	}
	
	void distort_p(Vector4& pt)
	{
		distorter.set_coefficients(k_coeff, k_cube, k_aspect);
		
		// Divide out the W coordinate
		Vector2 uv(pt.x / pt.w, pt.y / pt.w);
		
		// Distortion applies to TOTALLY everything in the scene, isolated
		// by an XY plane at the camera's eye (only vertices in the front of the cam
		// are processed. This means vertices far outside the frustum 
		// might bend backwards!. So what we will do is culling away all
		// the vertices that are too far out of the camera frustum.
		// We want to cull away all the vertices that are outside the frustum, plus a good bit
		// of a cushion. First we compute where the corners will land.
		Vector2 max_corner(1.0f, k_aspect);
		distorter.apply_disto(max_corner);
		
		// Then we add 1 as a cushio to isolate the distortion to 2X the maximum corners.
		max_corner.x += 1.0f;
		max_corner.y += 1.0f;
		
		if(fabs(uv.x) < max_corner.x && fabs(uv.y) < max_corner.y) {
			// Apply the distortion since the vector is ALREADY in the -1..1 space.
			distorter.apply_disto(uv);
		}
		
		// And assign to the referenced vector multiplying by w.
		pt.x = uv.x * pt.w;
		pt.y = uv.y * pt.w;
	}
	
	/* The vertex shader that does lens disto.
	By default it does this (using the point-local PL vector, which is 3-dimensional)
		for (int i=0; i < n; ++i) {
			Vector4 p_clip = transforms->matrix(LOCAL_TO_CLIP).transform(v[i].PL(), 1);
			// or alternatively transforms->matrix(LOCAL_TO_CLIP).transform(v[i].P());
			// but this destroys the proper Z/W relationship and motion vectors won't come out.
			v[i].P() = transforms->matrix(CLIP_TO_TO_SCREEN).transform(v[i].P());
		}
	*/
	static void sy_camera_nlens_func(Scene* scene, CameraOp* cam, MatrixArray* transforms, VArray* v, int n, void*)
	{
		SyCamera* sy_cam = dynamic_cast<SyCamera*>(cam);
		for (int i=0; i < n; ++i) {
			// We need to apply distortion in clip space, so do that. We will perform it on
			// point local values, not on P because we want our Z and W to be computed out
			// correctly for the motion vectors
			Vector4 ps = transforms->matrix(LOCAL_TO_CLIP).transform(v[i].PL(), 1);
			sy_cam->distort_p(ps);
			// and transform to screen space, assign to position
			Vector4 ps_screen = transforms->matrix(CLIP_TO_SCREEN).transform(ps);
			v[i].P() = ps_screen;
		}
	}
	
	LensNFunc* lensNfunction(int mode) const
	{
		if (mode == LENS_PERSPECTIVE) {
			return sy_camera_nlens_func;
		}
		return CameraOp::lensNfunction(mode);
	}

};

static Op* build(Node* node)
{
	return new SyCamera(node);
}
const Op::Description SyCamera::description(CLASS, build);

