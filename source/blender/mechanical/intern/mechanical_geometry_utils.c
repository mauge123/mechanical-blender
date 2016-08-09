/*
 *
*/

#include "BKE_editmesh.h"
#include "MEM_guardedalloc.h"

#include "mechanical_geometry.h"


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
