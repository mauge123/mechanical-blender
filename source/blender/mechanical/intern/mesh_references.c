
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_mesh_types.h"

#include "ED_mesh.h"

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
