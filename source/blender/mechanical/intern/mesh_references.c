
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"

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

bool reference_plane_project_input (Object *ob, BMReference *erf, ARegion *ar, View3D *v3d, const int mval[2], float r_co[3]) {
#ifdef WITH_PLAYER
	// There are link issues on ED_view3d_win_to_ray_ex as is implemented on editors
	return false;
#else
	float ray_origin[3], ray_normal[3], ray_start[3];
	float dist = 0.0f;
	float fmval[2] = {(float)mval[0],(float)mval[1]};

	if (!ED_view3d_win_to_ray_ex(
		        ar, v3d,
		        fmval, ray_origin, ray_normal, ray_start, true))
	{
		return false;
	}

	float imat[4][4];
	float timat[3][3]; /* transpose inverse matrix for normals */
	float ray_start_local[3], ray_normal_local[3],ray_org_local[3];

	invert_m4_m4(imat, ob->obmat);
	transpose_m3_m4(timat, imat);

	copy_v3_v3(ray_start_local, ray_start);
	copy_v3_v3(ray_normal_local, ray_normal);
	copy_v3_v3(ray_org_local, ray_origin);

	mul_m4_v3(imat, ray_org_local);
	mul_m4_v3(imat, ray_start_local);
	mul_mat3_m4_v3(imat, ray_normal_local);

	if (!((RegionView3D *)ar->regiondata)->is_persp) {
		/* We pass a temp ray_start, set from plane dimensions, to avoid precision issues with very far
		 * away ray_start values (as returned in case of ortho view3d).
		 */
		madd_v3_v3v3fl(ray_start_local, ray_org_local, ray_normal_local,
		               -len_v3v3(erf->v1, erf->v3));
	}


	if (isect_ray_tri_epsilon_v3(ray_start_local, ray_normal_local, erf->v1, erf->v2, erf->v3, &dist, NULL, FLT_EPSILON)
	    || isect_ray_tri_epsilon_v3(ray_start_local, ray_normal_local, erf->v1, erf->v3, erf->v4, &dist, NULL, FLT_EPSILON)) {

		madd_v3_v3v3fl(r_co,ray_start_local,ray_normal_local,dist);
	} else {
		return false;
	}

	return true;
#endif
}

void reference_plane_normal(BMReference *erf, float *r) {
	normal_tri_v3(r, erf->v1, erf->v2, erf->v3);
}

void reference_plane_origin(BMReference *erf, float *origin) {
	copy_v3_v3(origin, erf->v4);
}
