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
private:
	double max_corner_u_, max_corner_v_;
	bool distortion_enabled;
	double _obsolete_aspect;
	
public:
	static const Description description;

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
		distortion_enabled = 1;
	}
	
	void append(Hash& hash)
	{
		hash.append(VERSION);
		hash.append(distorter.compute_hash());
		CameraOp::append(hash);
	}
	
	void _validate(bool for_real)
	{
		CameraOp::_validate(for_real);
		
		// Avoid recomputing things when not necessary
		if(0 == distortion_enabled) return;
		
		// Set the distortion aspect based on haperture/vaperture correlation.
		// For this to work haperture/vaperture must be set correctly
		double asp = film_width() / film_height();
		debug("Disto autoaspect (haperture/vaperture) %0.5f", asp);
		distorter.set_aspect(asp);
		distorter.recompute_if_needed();
		update_distortion_limits();
	}
	
	/* This is a virtual method on every CameraOp made exactly for this purpose, it's called from within
	the standard knobs() */
	void lens_knobs(Knob_Callback f)
	{
		Tab_knob(f, "SyLens");
		distorter.knobs(f);
		
		// Allow bypass
		Knob* k_bypass = Bool_knob( f, &distortion_enabled, "disto_enabled");
		k_bypass->label("enable distortion");
		k_bypass->tooltip("You can deactivate this to suppress redistortion (if you want to use SyCamera without any distortion but do not want to change the parameters)");
		
		Divider(f, 0);
		Text_knob(f, "Make sure that your haperture/vaperture are set to the correct aspect!");
		
		Divider(f, 0);
		
		std::ostringstream ver;
		ver << "SyCamera v." << VERSION;
		Text_knob(f, ver.str().c_str());
		
		// TODO: to be removed in SyLens 4.
		// For compatibility with older scripts, add an "aspect" knob
		Knob* k_obsolete_aspect = Float_knob(f, &_obsolete_aspect, "aspect");
		// ...do not show it
		k_obsolete_aspect->set_flag(Knob::INVISIBLE);
		// ...and make it disappear when copy-pasted or saved out.
		k_obsolete_aspect->set_flag(Knob::DO_NOT_WRITE);
	}

	/*
		Distortion applies to TOTALLY everything in the scene, isolated
		by an XY plane at the camera's eye (only vertices in the front of the cam
		are processed). This means vertices far outside the frustum 
		might bend so extremely that they come back into the image!.
		So what we will do is culling away all
		the vertices that are too far out of the camera frustum.
		A non-scientific but efficient way to do it is to compute where the extremes will be (u1v1)
		at the corners and add 1 to it, and limit our distortion to the coordinates lying inside that limit.
		This way we capture enough points and prevent the rollaround from occuring. 
		We also cache these limits based on the distortion
		kappas. Note that for EXTREME fisheyes wraparound can still occur but
		we consider it a corner case at the moment.
		What is actually needed is finding a critical point of the distortion function
		where the F(r2) changes sign, but we'll do that later.
		Maybe.
	*/
	void update_distortion_limits()
	{
		distorter.recompute_if_needed();
		Vector2 max_corner(distorter.aspect(), 1.0f);
		distorter.apply_disto(max_corner);
		max_corner_u_ = max_corner.x + 1.0f;
		max_corner_v_ = max_corner.y + 1.0f;
	}
	
	// Distort a point in clip space. It's almost like a UV point (with embedded W)
	// but it's already in the right Syntheyes coordinate system
	void distort_p(Vector4& pt)
	{
		// Divide out the W coordinate
		Vector2 uv(pt.x / pt.w, pt.y / pt.w);
		
		if(fabs(uv.x) < max_corner_u_ && fabs(uv.y) < max_corner_v_) {
			// Apply the distortion since the vector is ALREADY in the -1..1 space.
			distorter.apply_disto(uv);
		}
		
		// And assign to the referenced vector multiplying by w.
		pt.x = uv.x * pt.w;
		pt.y = uv.y * pt.w;
	}
	
	/* The vertex shader that does lens disto.
	By default it does this (using the point-local PL vector, which is 3-dimensional)
	Note that we go from PL (point-local) to P (point-global)
		for (int i=0; i < n; ++i) {
			Vector4 p_clip = transforms->matrix(LOCAL_TO_CLIP).transform(v[i].PL());
			v[i].P() = transforms->matrix(CLIP_TO_TO_SCREEN).transform(p_clip);
		}
	*/
	static void sy_camera_nlens_func(Scene* scene, CameraOp* cam, MatrixArray* transforms, VArray* v, int n, void*)
	{
		SyCamera* sy_cam = dynamic_cast<SyCamera*>(cam);
		for (int i=0; i < n; ++i) {
			// We need to apply distortion in clip space, so do that. We will perform it on
			// point local values, not on P because we want our Z and W to be computed out
			// correctly for the motion vectors.
			// We do it in clip space (vertices in a distorted camera frustum centered on the middle of the frustum)
			Vector4 ps = transforms->matrix(LOCAL_TO_CLIP).transform(v[i].PL(), 1);
			// Perform the disto magic
			sy_cam->distort_p(ps);
			// and transform to screen space, assign to position. Note that in screen space
			// objects are in pixel coordinates already, relative to the bottom-left corner
			Vector4 ps_screen = transforms->matrix(CLIP_TO_SCREEN).transform(ps);
			v[i].P() = ps_screen;
		}
	}
	
	LensNFunc* lensNfunction(int mode) const
	{
		if (mode == LENS_PERSPECTIVE && distortion_enabled) {
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

