/*
 *
*/

#include "BKE_editmesh.h"
#include "MEM_guardedalloc.h"


#include "BLI_math.h"


#include "mechanical_utils.h"
#include "mechanical_geometry.h"

/**
 * @brief mechanical_follow_circle
 * @param em
 * @param v1 vertice n1
 * @param v2 vertice n2
 * @param v3 vertice n3
 * @param r_output vertex pointer array
 * @param r_count number of vertex on output
 * @param r_center  center of circle
 * @return int type of data, 0 if not valid
 */
static int mechanical_follow_circle(BMEditMesh *em, BMVert *v1, BMVert *v2, BMVert *v3,
                                     BMVert* *r_output, int* r_count, float r_center[])
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


	if (center_of_3_points(r_center, v1->co, v2->co, v3->co)) {
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
					len_v3v3(n_center, r_center) < DIM_GEOMETRY_PRECISION)
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
		return current ? BM_GEOMETRY_TYPE_CIRCLE : BM_GEOMETRY_TYPE_ARC;
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
	BMVert *(*verts) = MEM_callocN(sizeof(BMVert*)*bm->totvert,"mechanical_circle_output");
	int count;
	float center[3];

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
						type = mechanical_follow_circle(em,e1->v2, e1->v1, e2->v2, &(*verts), &count, center);
						if (!type) {
							BM_elem_flag_disable(e2, BM_ELEM_TAG);
						}
					}else if (e2->v1 == e1->v2) {
						BM_elem_flag_enable(e2, BM_ELEM_TAG);
						type = mechanical_follow_circle(em,e1->v1, e1->v2, e2->v2, &(*verts), &count, center);
						if (!type) {
							BM_elem_flag_disable(e2, BM_ELEM_TAG);
						}
					}else if (e2->v2 == e1->v1) {
						BM_elem_flag_enable(e2, BM_ELEM_TAG);
						type = mechanical_follow_circle(em,e1->v2, e1->v1, e2->v1, &(*verts), &count, center);
						if (!type) {
							BM_elem_flag_disable(e2, BM_ELEM_TAG);
						}
					}else if (e2->v2 == e1->v2) {
						BM_elem_flag_enable(e2, BM_ELEM_TAG);
						type = mechanical_follow_circle(em,e1->v1, e1->v2, e2->v1, &(*verts), &count, center);
						if (!type) {
							BM_elem_flag_disable(e2, BM_ELEM_TAG);
						}
					}
				}
			}
			switch (type) {
				case BM_GEOMETRY_TYPE_CIRCLE:
				case BM_GEOMETRY_TYPE_ARC:
				{
					BMElemGeom *egm = 	BLI_mempool_alloc(bm->gpool);
					bm->totgeom++;
					copy_v3_v3(egm->center,center);
					egm->totverts = count;
					egm->v = MEM_callocN(sizeof(BMVert*)*egm->totverts,"geometry vertex pointer array");
					egm->geometry_type = type;
					memcpy(egm->v,verts,egm->totverts*sizeof(BMVert*));
					break;
				}
				default:
					// Not valid
					BM_elem_flag_disable(e1, BM_ELEM_TAG);
					break;
			}
		}
	}
	MEM_freeN(&(*verts));
}

static bool mechanical_check_circle(BMEditMesh *UNUSED(em), BMElemGeom *egm)
{
	BMVert *v1 = egm->v[0];
	BMVert *v2 = egm->v[1];
	BMVert *current = egm->v[2];
	float n_center[3];

	BM_elem_flag_enable(v1, BM_ELEM_TAG);
	BM_elem_flag_enable(v2, BM_ELEM_TAG);

	for (int i=2;current && i<egm->totverts;i++) {
		if (center_of_3_points(n_center, v1->co, v2->co, current->co) &&
			len_v3v3(n_center, egm->center) < DIM_GEOMETRY_PRECISION)
		{
			BM_elem_flag_enable(current, BM_ELEM_TAG);
			current++;
		} else {
			current = NULL;
		}
	}
	if (current == NULL) {
		//reset
		for (int i;i>=0;i--) {
			BM_elem_flag_disable(egm->v[i], BM_ELEM_TAG);
		}
	}
	return (current != NULL);
}



static void mechanical_calc_edit_mesh_geometry(BMEditMesh *em)
{
	mechanical_find_mesh_circles(em);
}


static void mechanical_check_mesh_geometry(BMEditMesh *em)
{
	BMElemGeom *egm;
	BMIter iter;
	BMesh *bm = em->bm;
	bool valid;
	BM_ITER_MESH (egm, &iter, bm, BM_GEOMETRY_OF_MESH) {
		valid = false;
		switch (egm->geometry_type) {
			case BM_GEOMETRY_TYPE_CIRCLE:
			case BM_GEOMETRY_TYPE_ARC:
				valid = mechanical_check_circle(em, egm);
				break;
		}
		if (valid) {
			// Edges will be tagged according to vertex
		} else {
			// Remove!
			MEM_freeN(egm->v);
			BLI_mempool_free(bm->gpool, egm);
			bm->totgeom--;
		}
	}

}



void mechanical_update_mesh_geometry(BMEditMesh *em)
{
	BMEdge *e;
	BMVert *v;
	BMesh *bm = em->bm;
	BMIter iter;


	BM_ITER_MESH (v, &iter, bm, BM_EDGES_OF_MESH) {
		BM_elem_flag_disable(v, BM_ELEM_TAG);
	}

	mechanical_check_mesh_geometry(em);

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		// edges verts are set as OK if checked
		if (BM_elem_flag_test (e->v1, BM_ELEM_TAG)) {
			BM_elem_flag_enable(e, BM_ELEM_TAG);
		} else {
			BM_elem_flag_disable(e, BM_ELEM_TAG);
		}
	}

	mechanical_calc_edit_mesh_geometry(em);

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		BM_elem_flag_disable(e, BM_ELEM_TAG);
	}
	BM_ITER_MESH (v, &iter, bm, BM_EDGES_OF_MESH) {
		BM_elem_flag_disable(v, BM_ELEM_TAG);
	}
}



