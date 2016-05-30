/*
 *
*/

#include "BKE_editmesh.h"
#include "MEM_guardedalloc.h"


#include "BLI_math.h"


#include "mechanical_utils.h"
#include "mechanical_geometry.h"

static bool mechanical_follow_edge_loop_test_circle(BMEditMesh *em, BMEdge *e, BMVert *v1, BMVert *v2, BMVert *current, void *data);

static void mechanical_find_edge_loop_start(BMEditMesh *em,
                                        BMEdge **e1, BMEdge **e2, BMVert **v1, BMVert **v2, BMVert **v3,
                                        bool (*mechanical_follow_edge_loop_test_func) (BMEditMesh*, BMEdge*, BMVert*, BMVert*, BMVert*, void *),
                                        void *data)
{
	BMVert *first = *v1;
	BMVert *current = *v3, *temp;
	BMesh *bm= em->bm;
	BMEdge *e, *e_curr = *e2, *e_prev = *e1;
	BMIter iter;

	while (current && current != first) {
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test (e, BM_ELEM_TAG)) {
				continue;
			}
			if (e == e_prev || e == e_curr) {
				continue;
			}

			if (e->v1 == current) {
				current = e->v2;
				break;
			}
			if (e->v2 == current) {
				current = e->v1;
				break;
			}
		}
		if (current != first) {
			if (e && mechanical_follow_edge_loop_test_func(em, e,*v1,*v2,current,data)) {
				*v1 = *v2;
				*v2 = *v3;
				*v3 = current;
				e_prev = e_curr;
				e_curr = e;
				// Ok.
			} else {
				current = NULL;
			}
		}
	}

	// Invert
	*e2 = e_prev;
	*e1 = e_curr;
	temp = *v1;
	*v1 = *v3;
	*v3 = temp;

}


static bool mechanical_check_edge_line (BMEditMesh *em, BMEdge *e) {
	BMFace *efa = NULL;
	BMIter iter, f_iter;
	BMEdge *e2 = NULL;
	float *prev_fno = NULL;

	// Check the edge is not shared between coplanar faces
	BM_ITER_MESH(efa, &iter, em->bm, BM_FACES_OF_MESH) {
		BM_ITER_ELEM(e2,&f_iter,efa, BM_EDGES_OF_FACE) {
			if (e2 == e) {
				if (prev_fno && parallel_v3u_v3u(efa->no,prev_fno)) {
					break;
				}
				prev_fno = efa->no;
			}
		}
		if (e2 != NULL) {
			break;
		}
	}

	return (efa == NULL);
}

static bool mechanical_follow_edge_loop_test_circle(BMEditMesh *UNUSED(em), BMEdge *e, BMVert *v1, BMVert *v2, BMVert *current, void *data) {
	float *center = data;
	float n_center[3];

	return (center_of_3_points(n_center, v1->co, v2->co, current->co) &&
	len_v3v3(n_center, center) < DIM_GEOMETRY_PRECISION &&
    angle_v3v3v3 (e->v1->co, center, e->v2->co) < MAX_ANGLE_EDGE_FROM_CENTER_ON_ARC);
}

static bool mechanical_follow_edge_loop_test_line(BMEditMesh *em, BMEdge *e, BMVert *v1, BMVert *UNUSED(v2), BMVert *current, void *data) {
	float *dir = data;
	return point_on_axis(v1->co,dir,current->co) && mechanical_check_edge_line(em,e);
}

static BMVert* mechanical_follow_edge_loop(BMEditMesh *em, BMEdge *e1, BMEdge *e2, BMVert *v1, BMVert *v2, BMVert *v3,
                                           bool (*mechanical_follow_edge_loop_test_func) (
                                               BMEditMesh*, BMEdge*, BMVert*, BMVert*, BMVert*, void *
                                           ),
                                           BMVert* *r_voutput, int* r_vcount, void *data)
{
	BMEdge *e;
	BMVert *first;
	BMVert *current;
	BMIter iter;
	BMesh *bm = em->bm;

	*r_vcount =0;

	first = v1;
	current = v3;

	r_voutput[(*r_vcount)++] = v1;
	r_voutput[(*r_vcount)++] = v2;
	r_voutput[(*r_vcount)++] = v3;

	BM_elem_flag_enable(e1, BM_ELEM_TAG);
	BM_elem_flag_enable(e2, BM_ELEM_TAG);
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
			if (e && mechanical_follow_edge_loop_test_func (em, e, v1,v2,current,data)) {
				r_voutput[(*r_vcount)++] = current;
				BM_elem_flag_enable(e, BM_ELEM_TAG);
			} else {
				current = NULL;
			}
		}
	}
	return current;
}

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
static int mechanical_follow_circle(BMEditMesh *em, BMEdge *e1, BMEdge *e2, BMVert *v1, BMVert *v2, BMVert *v3,
                                     BMVert* *r_voutput, int* r_vcount, float r_center[])
{
	float dir[3];
	sub_v3_v3v3(dir,v2->co,v1->co);
	normalize_v3(dir);

	if (center_of_3_points(r_center, v1->co, v2->co, v3->co) &&
		angle_v3v3v3 (v1->co,r_center,v2->co) < MAX_ANGLE_EDGE_FROM_CENTER_ON_ARC &&
	    !point_on_axis(v1->co,dir,v3->co)) {

		mechanical_find_edge_loop_start(em, &e1, &e2, &v1, &v2, &v3,
		                                mechanical_follow_edge_loop_test_circle, r_center);

		if (mechanical_follow_edge_loop (em, e1, e2, v1, v2, v3,
		                                 mechanical_follow_edge_loop_test_circle,
		                                 r_voutput, r_vcount, r_center)) {
			return BM_GEOMETRY_TYPE_CIRCLE;
		} else {
			return BM_GEOMETRY_TYPE_ARC;
		}

	}
	return 0;
}

static int mechanical_follow_line(BMEditMesh *em, BMEdge *e1, BMEdge *e2, BMVert *v1, BMVert *v2, BMVert *v3,
                                     BMVert* *r_voutput, int* r_vcount)
{
	float dir[3];

	sub_v3_v3v3(dir, v2->co, v1->co);
	normalize_v3(dir);

	if (point_on_axis(v1->co,dir,v3->co) && mechanical_check_edge_line(em,e1) && mechanical_check_edge_line(em,e2)) {

		mechanical_find_edge_loop_start(em, &e1, &e2, &v1, &v2, &v3,
		                                mechanical_follow_edge_loop_test_line, dir);

		mechanical_follow_edge_loop (em,e1,e2,v1,v2,v3,mechanical_follow_edge_loop_test_line,r_voutput,r_vcount,dir);
		return BM_GEOMETRY_TYPE_LINE;
	}
	return 0;
}


static int mechanical_geometry_follow_data(BMEditMesh *em, BMEdge *e1, BMEdge *e2, BMVert *v1, BMVert *v2, BMVert *v3,
             BMVert* *r_voutput, int* r_vcount, float r_center[])
{
	int type = 0;
	type = mechanical_follow_circle(em,e1, e2, v1, v2, v3, r_voutput, r_vcount, r_center);
	if (type == 0) {
		type = mechanical_follow_line(em,e1,e2,v1,v2,v3,r_voutput, r_vcount);
	}
	return type;
}

static void mechanical_calc_edit_mesh_geometry(BMEditMesh *em)
{
	BMEdge *e1;
	BMEdge *e2;
	BMIter iter1;
	BMIter iter2;
	BMesh *bm = em->bm;
	int type;

	// Max size is total count of verts
	BMVert *(*verts) = MEM_callocN(sizeof(BMVert*)*bm->totvert,"mechanical_circle_output");
	int vcount;
	float center[3];

	BM_ITER_MESH (e1, &iter1, bm, BM_EDGES_OF_MESH) {
		if (!BM_elem_flag_test (e1, BM_ELEM_TAG)) {
			memcpy(&iter2,&iter1, sizeof (BMIter));
			type = 0;
			// Continue the edge
			while (!type && (e2 = BM_iter_step(&iter2))) {
				if (!BM_elem_flag_test (e2, BM_ELEM_TAG)) {
					if (e2->v1 == e1->v1) {
						type = mechanical_geometry_follow_data(em,e1, e2, e1->v2, e1->v1, e2->v2,
														&(*verts), &vcount, center);
					}else if (e2->v1 == e1->v2) {
						type = mechanical_geometry_follow_data(em,e1, e2, e1->v1, e1->v2, e2->v2,
						                                &(*verts), &vcount, center);
					}else if (e2->v2 == e1->v1) {
						type = mechanical_geometry_follow_data(em,e1, e2, e1->v2, e1->v1, e2->v1,
						                                &(*verts), &vcount, center);
					}else if (e2->v2 == e1->v2) {
						type = mechanical_geometry_follow_data(em,e1, e2, e1->v1, e1->v2, e2->v1,
						                                &(*verts), &vcount, center);
					}
				}
			}

			if (e2 == NULL) {
				// No conection Consider line
				// Check the face normals
				if (mechanical_check_edge_line(em, e1)) {
					verts[0] = e1->v1;
					verts[1] = e1->v2;
					vcount = 2;
					type = BM_GEOMETRY_TYPE_LINE;
				}
			}

			switch (type) {
				case BM_GEOMETRY_TYPE_CIRCLE:
				case BM_GEOMETRY_TYPE_ARC:
				{
					BMElemGeom *egm = BLI_mempool_alloc(bm->gpool);
					bm->totgeom++;
					copy_v3_v3(egm->center,center);
					egm->totverts = vcount;
					egm->v = MEM_callocN(sizeof(BMVert*)*egm->totverts,"geometry vertex pointer array");
					egm->geometry_type = type;
					memcpy(egm->v,verts,egm->totverts*sizeof(BMVert*));
					normal_tri_v3(egm->axis, egm->v[0]->co, egm->v[1]->co, egm->v[2]->co);
					break;
				}
				case BM_GEOMETRY_TYPE_LINE:
				{
					BMElemGeom *egm = BLI_mempool_alloc(bm->gpool);
					bm->totgeom++;
					egm->totverts = vcount;
					egm->v = MEM_callocN(sizeof(BMVert*)*egm->totverts,"geometry vertex pointer array");
					egm->geometry_type = type;
					memcpy(egm->v,verts,egm->totverts*sizeof(BMVert*));
					sub_v3_v3v3(egm->axis,egm->v[0]->co,egm->v[1]->co);
					normalize_v3(egm->axis);
					break;
				}
				default:
					// Not valid
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
	int i;

	BM_elem_flag_enable(v1, BM_ELEM_TAG);
	BM_elem_flag_enable(v2, BM_ELEM_TAG);

	for (i=2;current && i<egm->totverts;i++) {
		if (center_of_3_points(n_center, v1->co, v2->co, current->co) &&
			len_v3v3(n_center, egm->center) < DIM_GEOMETRY_PRECISION &&
		    angle_v3v3v3 (egm->v[i]->co,egm->center,egm->v[i-1]->co) < MAX_ANGLE_EDGE_FROM_CENTER_ON_ARC)
		{
			BM_elem_flag_enable(current, BM_ELEM_TAG);
			current++;
		} else {
			current = NULL;
		}
	}
	if (current == NULL) {
		//reset
		for (i=i-1;i>=0;i--) {
			BM_elem_flag_disable(egm->v[i], BM_ELEM_TAG);
		}
	}
	return (current != NULL);
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



