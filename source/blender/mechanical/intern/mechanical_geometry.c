/*
 *
*/

#include "BKE_editmesh.h"
#include "MEM_guardedalloc.h"


#include "BLI_math.h"


#include "mechanical_utils.h"
#include "mechanical_geometry.h"


static int mechanical_follow_circle (BMEditMesh *em, BMVert *v1, BMVert *v2, BMVert *v3,
                                     BMVert* *r_output, int* r_count, float center[])
{
	float n_center[3];
	BMEdge *e;
	BMVert *first = v1;
	BMVert *current = v3;
	BMIter iter;
	BMesh *bm = em->bm;

	*r_count =0;

	r_output[(*r_count)++] = v1;
	r_output[(*r_count)++] = v2;
	r_output[(*r_count)++] = v3;


	if (center_of_3_points(center, v1->co, v2->co, v3->co)) {
		while (current && current != first) {
			BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
				if (BM_elem_flag_test (e, BM_ELEM_TAG)) {
					continue;
				}
				if (e->v1 == current) {
					current = e->v2;
					break;
				} else if (e->v2 == current) {
					current = e->v1;
					break;
				}
			}
			if (current != first) {
				if (e &&
					center_of_3_points(n_center, v1->co, v2->co, current->co) &&
					len_v3v3(n_center, center) < DIM_GEOMETRY_PRECISION)
				{
					r_output[(*r_count)++] = current;
					BM_elem_flag_enable(e, BM_ELEM_TAG);
				} else {
					current = NULL;
				}
			}
		}
	}
	if (*r_count > MIN_NUM_VERTS_CIRCLE) {
		return current ? BM_REFERENCE_TYPE_CIRCLE : BM_REFERENCE_TYPE_ARC;
	} else {
		return 0;
	}
}

static void mechanical_find_mesh_circles (BMEditMesh *em)
{
	BMEdge *e1;
	BMEdge *e2;
	BMIter iter1;
	BMIter iter2;
	BMesh *bm = em->bm;
	int type;

	// Max size is total count of verts
	BMVert *(*data) = MEM_callocN(sizeof(BMVert*)*bm->totvert,"mechanical_circle_output");
	int count;
	float center[3];

	BM_ITER_MESH (e1, &iter1, bm, BM_EDGES_OF_MESH) {
		BM_elem_flag_disable(e1, BM_ELEM_TAG);
	}

	BM_ITER_MESH (e1, &iter1, bm, BM_EDGES_OF_MESH) {
		if (!BM_elem_flag_test (e1, BM_ELEM_TAG)) {
			BM_elem_flag_enable(e1, BM_ELEM_TAG);
			memcpy(&iter2,&iter1, sizeof (BMIter));
			type = 0;
			// Continue the edge
			while (!type && (e2 = BM_iter_step(&iter2))) {
				if (!BM_elem_flag_test (e2, BM_ELEM_TAG)) {
					if (e2->v1 == e1->v1) {
						BM_elem_flag_enable(e2, BM_ELEM_TAG);
						type = mechanical_follow_circle(em,e1->v2, e1->v1, e2->v2, &(*data), &count, center);
						if (!type) {
							BM_elem_flag_disable(e2, BM_ELEM_TAG);
						}
					}else if (e2->v1 == e1->v2) {
						BM_elem_flag_enable(e2, BM_ELEM_TAG);
						type = mechanical_follow_circle(em,e1->v1, e1->v2, e2->v2, &(*data), &count, center);
						if (!type) {
							BM_elem_flag_disable(e2, BM_ELEM_TAG);
						}
					}else if (e2->v2 == e1->v1) {
						BM_elem_flag_enable(e2, BM_ELEM_TAG);
						type = mechanical_follow_circle(em,e1->v2, e1->v1, e2->v1, &(*data), &count, center);
						if (!type) {
							BM_elem_flag_disable(e2, BM_ELEM_TAG);
						}
					}else if (e2->v2 == e1->v2) {
						BM_elem_flag_enable(e2, BM_ELEM_TAG);
						type = mechanical_follow_circle(em,e1->v1, e1->v2, e2->v1, &(*data), &count, center);
						if (!type) {
							BM_elem_flag_disable(e2, BM_ELEM_TAG);
						}
					}
				}
			}
			switch (type) {
				case BM_REFERENCE_TYPE_CIRCLE:
				case BM_REFERENCE_TYPE_ARC:
				{
					BMElemGeom *egm = 	BLI_mempool_alloc(bm->gpool);
					bm->totgeom++;
					copy_v3_v3(egm->center,center);
					egm->totverts = count;
					egm->v = MEM_callocN(sizeof(BMVert*)*egm->totverts,"geometry vertex pointer array");
					egm->geometry_type = type;
					memccpy(egm->v,data,egm->totverts,sizeof(BMVert*));
					break;
				}
				default:
					// Not valid
					break;
			}
		}
	}

	BM_ITER_MESH (e1, &iter1, bm, BM_EDGES_OF_MESH) {
		BM_elem_flag_disable(e1, BM_ELEM_TAG);
	}

	MEM_freeN(&(*data));
}


void mechanical_calc_edit_mesh_geometry (BMEditMesh *em) {
	mechanical_find_mesh_circles(em);
}
