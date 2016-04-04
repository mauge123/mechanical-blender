/*
 *
*/


#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_mesh_types.h"

#include "BKE_editmesh.h"

#include "bmesh.h"

#include "mesh_dimensions.h"
#include "mechanical_utils.h"


void tag_vertexs_on_coplanar_faces(BMesh *bm, float *point, float* dir){
	BMFace *f;
	BMVert *eve;
	BMIter iter, viter;
	float p[3],vec[3], d;

	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		d = dot_v3v3(dir,f->no);
		if (fabs(d*d -1) < DIM_CONSTRAINT_PRECISION) {
			// Coplanar?
			int ok =1;
			BM_ITER_ELEM (eve, &viter, f, BM_VERTS_OF_FACE) {
				sub_v3_v3v3(vec, point, eve->co);
				normalize_v3(vec);
				project_v3_v3v3(p,vec,dir);
				if (len_squared_v3(p) > DIM_CONSTRAINT_PRECISION) {
					ok = 0;
					break;
				}
			}
			if (ok) {
				BM_ITER_ELEM (eve, &viter, f, BM_VERTS_OF_FACE) {
					BM_elem_flag_enable(eve, BM_ELEM_TAG);
				}
			}
		}
	}
}
