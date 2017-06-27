/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/transform/transform_input.c
 *  \ingroup edtransform
 */


#include <stdlib.h>
#include <math.h>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_editmesh.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "WM_types.h"

#include "transform.h"
#include "mesh_references.h"

#include "MEM_guardedalloc.h" 

/* ************************** INPUT FROM MOUSE *************************** */

static void InputVector(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
#ifdef WITH_MECHANICAL_EXIT_TRANSFORM_MODAL
	if (t->spacetype == SPACE_VIEW3D){
		float p[3] = {0,0,0};
		float fmval[2] = {(float) mval[0], (float) mval[1]};
		int imval[2] = {(int) mval[0], (int) mval[1]};
		float vec[3];

#ifdef WITH_MECHANICAL_GRAB_FIX
		getViewDepthPoint(t, p);
#endif
// WITH_MECHANICAL_CREATE_ON_REFERENCE_PLANE
		BMEditMesh *em = NULL;
		if (t->obedit && t->obedit->type == OB_MESH) {
			em = BKE_editmesh_from_object(t->obedit);
		}
		if (t->obedit && em) {
			BMReference *erf = BM_reference_at_index_find(em->bm,((View3D*)t->view)->refplane-1);
			if (erf && reference_plane_project_input (t->obedit, erf, t->ar, t->view, imval, vec)) {
				// Ok
			} else {
				ED_view3d_win_to_3d(t->view, t->ar, p, fmval, vec);
			}
		} else {
			ED_view3d_win_to_3d(t->view, t->ar, p, fmval, vec);
		}
		sub_v3_v3v3(output, vec, t->iloc);
	} else {
		convertViewVec(t, output, mval[0] - mi->imval[0], mval[1] - mi->imval[1]);
	}
#else
	convertViewVec(t, output, mval[0] - mi->imval[0], mval[1] - mi->imval[1]);
#endif
}

static void InputSpring(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
	double dx, dy;
	float ratio;
#ifdef WITH_MECHANICAL_EXIT_TRANSFORM_MODAL
	if (t->spacetype == SPACE_VIEW3D){
		float p[3],center[2];
		getViewDepthPoint(t,p);
		projectFloatView(t, p,center);
		dx = ((double)center[0] - mval[0]);
		dy = ((double)center[1] - mval[1]);
	} else {
		dx = ((double)mi->center[0] - mval[0]);
		dy = ((double)mi->center[1] - mval[1]);
	}
#else
	dx = ((double)mi->center[0] - mval[0]);
	dy = ((double)mi->center[1] - mval[1]);
#endif
	ratio = hypot(dx, dy) / (double)mi->factor;

	output[0] = ratio;
}

static void InputSpringFlip(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
	InputSpring(t, mi, mval, output);

	/* flip scale */
	/* values can become really big when zoomed in so use longs [#26598] */
	if ((int64_t)((int)mi->center[0] - mval[0]) * (int64_t)((int)mi->center[0] - mi->imval[0]) +
	    (int64_t)((int)mi->center[1] - mval[1]) * (int64_t)((int)mi->center[1] - mi->imval[1]) < 0)
	{
		output[0] *= -1.0f;
	}
}

static void InputSpringDelta(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
	InputSpring(t, mi, mval, output);
	output[0] -= 1.0f;
}

static void InputTrackBall(TransInfo *UNUSED(t), MouseInput *mi, const double mval[2], float output[3])
{
	output[0] = (float)(mi->imval[1] - mval[1]);
	output[1] = (float)(mval[0] - mi->imval[0]);

	output[0] *= mi->factor;
	output[1] *= mi->factor;
}

static void InputHorizontalRatio(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
	const int winx = t->ar ? t->ar->winx : 1;

	output[0] = ((mval[0] - mi->imval[0]) / winx) * 2.0f;
}

static void InputHorizontalAbsolute(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
	float vec[3];

	InputVector(t, mi, mval, vec);
	project_v3_v3v3(vec, vec, t->viewinv[0]);

	output[0] = dot_v3v3(t->viewinv[0], vec) * 2.0f;
}

static void InputVerticalRatio(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
	const int winy = t->ar ? t->ar->winy : 1;

	output[0] = ((mval[1] - mi->imval[1]) / winy) * 2.0f;
}

static void InputVerticalAbsolute(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
	float vec[3];

	InputVector(t, mi, mval, vec);
	project_v3_v3v3(vec, vec, t->viewinv[1]);

	output[0] = dot_v3v3(t->viewinv[1], vec) * 2.0f;
}

void setCustomPoints(TransInfo *UNUSED(t), MouseInput *mi, const int mval_start[2], const int mval_end[2])
{
	int *data;

	mi->data = MEM_reallocN(mi->data, sizeof(int) * 4);
	
	data = mi->data;

	data[0] = mval_start[0];
	data[1] = mval_start[1];
	data[2] = mval_end[0];
	data[3] = mval_end[1];
}

static void InputCustomRatioFlip(TransInfo *UNUSED(t), MouseInput *mi, const double mval[2], float output[3])
{
	double length;
	double distance;
	double dx, dy;
	const int *data = mi->data;
	
	if (data) {
		int mdx, mdy;
		dx = data[2] - data[0];
		dy = data[3] - data[1];

		length = hypot(dx, dy);

		mdx = mval[0] - data[2];
		mdy = mval[1] - data[3];

		distance = (length != 0.0) ? (mdx * dx + mdy * dy) / length : 0.0;

		output[0] = (length != 0.0) ? (double)(distance / length) : 0.0;
	}
}

static void InputCustomRatio(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
	InputCustomRatioFlip(t, mi, mval, output);
	output[0] = -output[0];
}

struct InputAngle_Data {
	double angle;
	double mval_prev[2];
};

static void InputAngle(TransInfo *UNUSED(t), MouseInput *mi, const double mval[2], float output[3])
{
	struct InputAngle_Data *data = mi->data;
	double dx2 = mval[0] - (double)mi->center[0];
	double dy2 = mval[1] - (double)mi->center[1];
	double B = sqrt(dx2 * dx2 + dy2 * dy2);

	double dx1 = data->mval_prev[0] - (double)mi->center[0];
	double dy1 = data->mval_prev[1] - (double)mi->center[1];
	double A = sqrt(dx1 * dx1 + dy1 * dy1);

	double dx3 = mval[0] - data->mval_prev[0];
	double dy3 = mval[1] - data->mval_prev[1];

	/* use doubles here, to make sure a "1.0" (no rotation) doesn't become 9.999999e-01, which gives 0.02 for acos */
	double deler = (((dx1 * dx1 + dy1 * dy1) +
	                 (dx2 * dx2 + dy2 * dy2) -
	                 (dx3 * dx3 + dy3 * dy3)) / (2.0 * (((A * B) != 0.0) ? (A * B) : 1.0)));
	/* ((A * B) ? (A * B) : 1.0) this takes care of potential divide by zero errors */

	float dphi;

	dphi = saacos((float)deler);
	if ((dx1 * dy2 - dx2 * dy1) > 0.0) dphi = -dphi;

	/* If the angle is zero, because of lack of precision close to the 1.0 value in acos
	 * approximate the angle with the opposite side of the normalized triangle
	 * This is a good approximation here since the smallest acos value seems to be around
	 * 0.02 degree and lower values don't even have a 0.01% error compared to the approximation
	 */
	if (dphi == 0) {
		double dx, dy;

		dx2 /= A;
		dy2 /= A;

		dx1 /= B;
		dy1 /= B;

		dx = dx1 - dx2;
		dy = dy1 - dy2;

		dphi = sqrt(dx * dx + dy * dy);
		if ((dx1 * dy2 - dx2 * dy1) > 0.0) dphi = -dphi;
	}

	data->angle += ((double)dphi) * (mi->precision ? (double)mi->precision_factor : 1.0);

	data->mval_prev[0] = mval[0];
	data->mval_prev[1] = mval[1];

	output[0] = data->angle;
}

static void InputAngleSpring(TransInfo *t, MouseInput *mi, const double mval[2], float output[3])
{
	float toutput[3];

	InputAngle(t, mi, mval, output);
	InputSpring(t, mi, mval, toutput);

	output[1] = toutput[0];
}

void initMouseInput(TransInfo *UNUSED(t), MouseInput *mi, const float center[2], const int mval[2], const bool precision)
{
	mi->factor = 0;
	mi->precision = precision;

	mi->center[0] = center[0];
	mi->center[1] = center[1];

	mi->imval[0] = mval[0];
	mi->imval[1] = mval[1];

	mi->post = NULL;
}

static void calcSpringFactor(MouseInput *mi)
{
	mi->factor = sqrtf(((float)(mi->center[1] - mi->imval[1])) * ((float)(mi->center[1] - mi->imval[1])) +
	                   ((float)(mi->center[0] - mi->imval[0])) * ((float)(mi->center[0] - mi->imval[0])));

	if (mi->factor == 0.0f) {
		mi->factor = 1.0f; /* prevent Inf */
	}
}

void initMouseInputMode(TransInfo *t, MouseInput *mi, MouseInputMode mode)
{
	/* incase we allocate a new value */
	void *mi_data_prev = mi->data;

#ifdef WITH_MECHANICAL_EXIT_TRANSFORM_MODAL
	// Virtual Code is based on imval
	// On exit transform_modal we are based on real 3D loc
	mi->use_virtual_mval = false;
#else
	mi->use_virtual_mval = true;
#endif
	mi->precision_factor = 1.0f / 10.0f;

	switch (mode) {
		case INPUT_VECTOR:
			mi->apply = InputVector;
			t->helpline = HLP_NONE;
			break;
		case INPUT_SPRING:
			calcSpringFactor(mi);
			mi->apply = InputSpring;
			t->helpline = HLP_SPRING;
			break;
		case INPUT_SPRING_FLIP:
			calcSpringFactor(mi);
			mi->apply = InputSpringFlip;
			t->helpline = HLP_SPRING;
			break;
		case INPUT_SPRING_DELTA:
			calcSpringFactor(mi);
			mi->apply = InputSpringDelta;
			t->helpline = HLP_SPRING;
			break;
		case INPUT_ANGLE:
		case INPUT_ANGLE_SPRING:
		{
			struct InputAngle_Data *data;
			mi->use_virtual_mval = false;
			mi->precision_factor = 1.0f / 30.0f;
			data = MEM_callocN(sizeof(struct InputAngle_Data), "angle accumulator");
			data->mval_prev[0] = mi->imval[0];
			data->mval_prev[1] = mi->imval[1];
			mi->data = data;
			if (mode == INPUT_ANGLE) {
				mi->apply = InputAngle;
			}
			else {
				calcSpringFactor(mi);
				mi->apply = InputAngleSpring;
			}
			t->helpline = HLP_ANGLE;
			break;
		}
		case INPUT_TRACKBALL:
			mi->precision_factor = 1.0f / 30.0f;
			/* factor has to become setting or so */
			mi->factor = 0.01f;
			mi->apply = InputTrackBall;
			t->helpline = HLP_TRACKBALL;
			break;
		case INPUT_HORIZONTAL_RATIO:
			mi->apply = InputHorizontalRatio;
			t->helpline = HLP_HARROW;
			break;
		case INPUT_HORIZONTAL_ABSOLUTE:
			mi->apply = InputHorizontalAbsolute;
			t->helpline = HLP_HARROW;
			break;
		case INPUT_VERTICAL_RATIO:
			mi->apply = InputVerticalRatio;
			t->helpline = HLP_VARROW;
			break;
		case INPUT_VERTICAL_ABSOLUTE:
			mi->apply = InputVerticalAbsolute;
			t->helpline = HLP_VARROW;
			break;
		case INPUT_CUSTOM_RATIO:
			mi->apply = InputCustomRatio;
			t->helpline = HLP_NONE;
			break;
		case INPUT_CUSTOM_RATIO_FLIP:
			mi->apply = InputCustomRatioFlip;
			t->helpline = HLP_NONE;
			break;
		case INPUT_NONE:
		default:
			mi->apply = NULL;
			break;
	}

	/* if we've allocated new data, free the old data
	 * less hassle then checking before every alloc above */
	if (mi_data_prev && (mi_data_prev != mi->data)) {
		MEM_freeN(mi_data_prev);
	}

	/* bootstrap mouse input with initial values */
	applyMouseInput(t, mi, mi->imval, t->values);
}

void setInputPostFct(MouseInput *mi, void (*post)(struct TransInfo *t, float values[3]))
{
	mi->post = post;
}

void applyMouseInput(TransInfo *t, MouseInput *mi, const int mval[2], float output[3])
{
	double mval_db[2];

	if (mi->use_virtual_mval) {
#ifdef WITH_MECHANICAL
		// virtual m_val dues to bad results on scene movements, selected elements can move out of view during scene movements
		BLI_assert(false);
#endif
		/* update accumulator */
		double mval_delta[2];

		mval_delta[0] = (mval[0] - mi->imval[0]) - mi->virtual_mval.prev[0];
		mval_delta[1] = (mval[1] - mi->imval[1]) - mi->virtual_mval.prev[1];

		mi->virtual_mval.prev[0] += mval_delta[0];
		mi->virtual_mval.prev[1] += mval_delta[1];

		if (mi->precision) {
			mval_delta[0] *= (double)mi->precision_factor;
			mval_delta[1] *= (double)mi->precision_factor;
		}

		mi->virtual_mval.accum[0] += mval_delta[0];
		mi->virtual_mval.accum[1] += mval_delta[1];

		mval_db[0] = mi->imval[0] + mi->virtual_mval.accum[0];
		mval_db[1] = mi->imval[1] + mi->virtual_mval.accum[1];
	}
	else {
		mval_db[0] = mval[0];
		mval_db[1] = mval[1];
#ifdef WITH_MECHANICAL_EXIT_TRANSFORM_MODAL
		if (mi->precision) {
			int m_delta[2];
			sub_v2_v2v2_int(m_delta,mval, mi->precision_start);
			mval_db[0] -= ((double)m_delta[0])*(1.0-mi->precision_factor);
			mval_db[1] -= ((double)m_delta[1])*(1.0-mi->precision_factor);
		}
#endif
	}


	if (mi->apply != NULL) {
		mi->apply(t, mi, mval_db, output);
	}

	if (mi->post) {
		mi->post(t, output);
	}
}

eRedrawFlag handleMouseInput(TransInfo *t, MouseInput *mi, const wmEvent *event)
{
	eRedrawFlag redraw = TREDRAW_NOTHING;

	switch (event->type) {
		case LEFTSHIFTKEY:
		case RIGHTSHIFTKEY:
			if (event->val == KM_PRESS) {
				t->modifiers |= MOD_PRECISION;
				/* shift is modifier for higher precision transforn */
#ifdef WITH_MECHANICAL_EXIT_TRANSFORM_MODAL
				if (!mi->precision) {
					copy_v2_v2_int(mi->precision_start, event->mval);
				}
#endif
				mi->precision = 1;
				redraw = TREDRAW_HARD;
			}
			else if (event->val == KM_RELEASE) {
				t->modifiers &= ~MOD_PRECISION;
				mi->precision = 0;
				redraw = TREDRAW_HARD;
			}
			break;
	}

	return redraw;
}
