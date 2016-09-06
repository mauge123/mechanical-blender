/*
 *
*/

#include "BKE_editmesh.h"
#include "MEM_guardedalloc.h"
#include "BLI_math.h"

#include "mechanical_geometry.h"
#include "mesh_dimensions.h"




int get_max_geom_points(BMEditMesh *em)
{
	BMGeom *egm;
	BMIter iter;
	int num=0;
	BM_ITER_MESH (egm, &iter, em->bm, BM_GEOMETRY_OF_MESH) {
		switch (egm->geometry_type) {
			case BM_GEOMETRY_TYPE_LINE:
				num=num+GEO_SNAP_POINTS_PER_LINE;
			case BM_GEOMETRY_TYPE_ARC:
				num=num+GEO_SNAP_POINTS_PER_ARC;

			break;
			case BM_GEOMETRY_TYPE_CIRCLE:
				num=num+GEO_SNAP_POINTS_PER_CIRCLE;
			break;
			default:
				num=0;
			break;
		}
	}
	return num;
}

/***
 * ASSUMES EQUAL ANGLES FOR AL VERTS
 *
 */
void arc_mid_point(BMGeom *egm){
		float mid[3], r;
		int num= (egm->totverts)/2;

		BLI_assert(egm->geometry_type == BM_GEOMETRY_TYPE_ARC);

		if((egm->totverts)%2 ==0){
			r=len_v3v3(egm->start, egm->center);
			mid_of_2_points(mid,egm->v[num]->co, egm->v[num-1]->co);
			sub_v3_v3v3(mid, mid, egm->center);
			normalize_v3(mid);
			mul_v3_fl(mid, r);
			add_v3_v3v3(egm->mid, egm->center, mid);

		}else{
			copy_v3_v3(egm->mid, egm->v[num]->co);
		}
}
