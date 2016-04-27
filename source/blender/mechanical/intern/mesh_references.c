
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_mesh_types.h"

#include "ED_mesh.h"
#include "ED_view3d.h"

#include "BKE_editmesh.h"

#include "UI_resources.h"

#include "bmesh.h"

#include "mesh_references.h"
#include "mechanical_utils.h"


void reference_plane_matrix (BMReference *erf, float mat[3][3]) {
	float x[3],y[3],z[3];
	sub_v3_v3v3(x, erf->v2, erf->v1);
	v_perpendicular_to_axis(y,erf->v1,erf->v3,x);
	cross_v3_v3v3(z,x,y);

	copy_v3_v3(mat[0],x);
	copy_v3_v3(mat[1],y);
	copy_v3_v3(mat[2],z);

	normalize_m3(mat);
}

bool reference_plane_project_input (BMReference *erf, ARegion *ar, View3D *v3d, const int mval[2], float r_co[3]) {

	float ray_origin[3], ray_normal[3], ray_start[3];
	float dist = 0.0f;
	float fmval[2] = {(float)mval[0],(float)mval[1]};

	if (!ED_view3d_win_to_ray_ex(
		        ar, v3d,
		        fmval, ray_origin, ray_normal, ray_start, true))
	{
		return false;
	}

	if (isect_ray_tri_epsilon_v3(ray_origin, ray_normal, erf->v1, erf->v2, erf->v3, &dist, NULL, FLT_EPSILON)
	    || isect_ray_tri_epsilon_v3(ray_origin, ray_normal, erf->v1, erf->v3, erf->v4, &dist, NULL, FLT_EPSILON)) {

		madd_v3_v3v3fl(r_co,ray_origin,ray_normal,dist);
	} else {
		return false;
	}

	return true;
}
