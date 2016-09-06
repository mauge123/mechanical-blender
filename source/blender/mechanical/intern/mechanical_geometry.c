/*
 *
*/

#include "BKE_editmesh.h"
#include "MEM_guardedalloc.h"


#include "BLI_math.h"


#include "mechanical_utils.h"
#include "mechanical_geometry.h"
#include "prec_math.h"
#include "mesh_dimensions.h"


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


static bool mechanical_check_edge_line (BMEditMesh *UNUSED(em), BMEdge *e) {
	BMIter iter;
	BMFace *efa;
	float *prev_fno = NULL;

	if (eq_v3v3_prec(e->v1->co, e->v2->co)) {
		return false;
	}

	BM_ITER_ELEM(efa, &iter, e, BM_FACES_OF_EDGE) {
		if (prev_fno && parallel_v3u_v3u(efa->no,prev_fno)) {
			break;
		}
		if (prev_fno) {
			float d = dot_v3v3(efa->no, prev_fno);
			d *= d;
			if (d > 0.9f) {
				// Consider tangent faces
				break;
			}
		}
		prev_fno = efa->no;
	}

	return (efa == NULL);
}

static bool mechanical_follow_edge_loop_test_circle(BMEditMesh *UNUSED(em), BMEdge *e, BMVert *v1, BMVert *v2, BMVert *current, void *data) {
	test_circle_data *cdata = data;
	float *center = cdata->center;
	float n_center[3];

	if (eq_v3v3_prec(v1->co, v2->co) || eq_v3v3_prec(v1->co, current->co) || eq_v3v3_prec(v2->co, current->co)) {
		return false;
	}

	return ( !eq_v3v3_prec(current->co,center) &&
		center_of_3_points(n_center, v1->co, v2->co, current->co) &&
		eq_v3v3_prec(n_center, center) &&
		angle_v3v3v3 (e->v1->co, center, e->v2->co) < MAX_ANGLE_EDGE_FROM_CENTER_ON_ARC);
}

static bool mechanical_follow_edge_loop_test_line(BMEditMesh *em, BMEdge *e, BMVert *v1, BMVert *v2, BMVert *current, void *data) {
	float *dir = data;

	if (eq_v3v3_prec(v1->co, v2->co)) {
		return false;
	}

	return point_on_axis(v1->co,dir,current->co) && mechanical_check_edge_line(em,e);
}

static BMVert* mechanical_follow_edge_loop(BMEditMesh *em, BMEdge *e1, BMEdge *e2, BMVert *v1, BMVert *v2, BMVert *v3,
                                           bool (*mechanical_follow_edge_loop_test_func) (
                                               BMEditMesh*, BMEdge*, BMVert*, BMVert*, BMVert*, void *
                                           ),
                                           BMVert* *r_voutput, int* r_vcount, BMEdge* *r_eoutput, int* r_ecount, void *data)
{
	BMEdge *e;
	BMVert *first;
	BMVert *current;
	BMIter iter;
	BMesh *bm = em->bm;

	*r_vcount = 0;
	*r_ecount = 0;

	first = v1;
	current = v3;

	r_voutput[(*r_vcount)++] = v1;
	r_voutput[(*r_vcount)++] = v2;
	r_voutput[(*r_vcount)++] = v3;
	r_eoutput[(*r_ecount)++] = e1;
	r_eoutput[(*r_ecount)++] = e2;

	BM_elem_flag_enable(e1, BM_ELEM_TAG);
	BM_elem_flag_enable(e2, BM_ELEM_TAG);
	while (current && current != first) {
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test (e, BM_ELEM_TAG)) {
				continue;
			}
			if (e->v1 == current) {
				BM_elem_flag_enable(e, BM_ELEM_TAG);
				r_eoutput[(*r_ecount)] = e;
				current = e->v2;
				break;
			} else if (e->v2 == current) {
				BM_elem_flag_enable(e, BM_ELEM_TAG);
				r_eoutput[(*r_ecount)] = e;
				current = e->v1;
				break;
			}
		}
		if (current != first) {
			if (e && mechanical_follow_edge_loop_test_func (em, e, v1,v2,current,data)) {
				(*r_ecount)++;
				r_voutput[(*r_vcount)++] = current;
			} else {
				if (e) {
					BM_elem_flag_disable(e, BM_ELEM_TAG);
				}
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
                                     BMVert* *r_voutput, int* r_vcount, BMEdge* *r_eoutput, int *r_ecount, float r_center[])
{
	int type = 0;
	float dir[3];

	if (eq_v3v3_prec(v1->co, v2->co) || eq_v3v3_prec(v1->co, v3->co) || eq_v3v3_prec(v2->co, v3->co)) {
		return 0;
	}

	sub_v3_v3v3_prec(dir,v2->co,v1->co);
	normalize_v3_prec(dir);
	float a1, a2;
	test_circle_data cdata;
	*r_vcount = 0;
	*r_ecount = 0;

	if (center_of_3_points(r_center, v1->co, v2->co, v3->co) &&
		(a1 = angle_v3v3v3 (v1->co,r_center,v2->co)) < MAX_ANGLE_EDGE_FROM_CENTER_ON_ARC &&
	    (a2 = angle_v3v3v3 (v2->co,r_center,v3->co)) < MAX_ANGLE_EDGE_FROM_CENTER_ON_ARC &&
	    eq_ff_prec(a1,a2) &&
	    !point_on_axis_prec(v1->co,dir,v3->co)) {

		copy_v3_v3(cdata.center, r_center);
		cdata.angle = a1;

		mechanical_find_edge_loop_start(em, &e1, &e2, &v1, &v2, &v3,
		                                mechanical_follow_edge_loop_test_circle, &cdata);

		if (mechanical_follow_edge_loop (em, e1, e2, v1, v2, v3,
		                                 mechanical_follow_edge_loop_test_circle,
		                                 r_voutput, r_vcount, r_eoutput, r_ecount, &cdata)) {
			(*r_ecount)++;  //Closing edge
			type = BM_GEOMETRY_TYPE_CIRCLE;
		} else {
			type = BM_GEOMETRY_TYPE_ARC;
		}
	}
	if (*r_vcount <= 3) {
		// Invalid, needs more vertices
		type =0;
		BM_elem_flag_disable(e1, BM_ELEM_TAG);
		BM_elem_flag_disable(e2, BM_ELEM_TAG);
	}
	return type;
}

static int mechanical_follow_line(BMEditMesh *em, BMEdge *e1, BMEdge *e2, BMVert *v1, BMVert *v2, BMVert *v3,
                                     BMVert* *r_voutput, int* r_vcount, BMEdge* *r_eoutput, int* r_ecount)
{
	float dir[3];

	if (eq_v3v3_prec(v1->co, v2->co)) {
		return 0;
	}

	sub_v3_v3v3_prec(dir, v2->co, v1->co);
	normalize_v3(dir);

	if (point_on_axis(v1->co,dir,v3->co) && mechanical_check_edge_line(em,e1) && mechanical_check_edge_line(em,e2)) {

		mechanical_find_edge_loop_start(em, &e1, &e2, &v1, &v2, &v3,
		                                mechanical_follow_edge_loop_test_line, dir);

		mechanical_follow_edge_loop(em,e1,e2,v1,v2,v3,mechanical_follow_edge_loop_test_line,r_voutput,r_vcount, r_eoutput, r_ecount,dir);
		return BM_GEOMETRY_TYPE_LINE;
	}
	return 0;
}


static int mechanical_geometry_follow_data(BMEditMesh *em, BMEdge *e1, BMEdge *e2, BMVert *v1, BMVert *v2, BMVert *v3,
             BMVert* *r_voutput, int* r_vcount, BMEdge* *r_eoutput, int* r_ecount, float r_center[])
{
	int type = 0;
	type = mechanical_follow_circle(em,e1, e2, v1, v2, v3, r_voutput, r_vcount, r_eoutput, r_ecount, r_center);
	if (type == 0) {
		type = mechanical_follow_line(em,e1,e2,v1,v2,v3,r_voutput, r_vcount, r_eoutput, r_ecount);
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
	BMEdge *(*edges) = MEM_callocN(sizeof(BMEdge*)*bm->totedge,"mechanical_circle_output");
	int vcount=0, ecount=0;
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
														&(*verts), &vcount, &(*edges), &ecount, center);
					}else if (e2->v1 == e1->v2) {
						type = mechanical_geometry_follow_data(em,e1, e2, e1->v1, e1->v2, e2->v2,
						                                &(*verts), &vcount, &(*edges), &ecount, center);
					}else if (e2->v2 == e1->v1) {
						type = mechanical_geometry_follow_data(em,e1, e2, e1->v2, e1->v1, e2->v1,
						                                &(*verts), &vcount, &(*edges), &ecount, center);
					}else if (e2->v2 == e1->v2) {
						type = mechanical_geometry_follow_data(em,e1, e2, e1->v1, e1->v2, e2->v1,
						                                &(*verts), &vcount, &(*edges), &ecount, center);
					}
				}
			}

			if (e2 == NULL) {
				// No conection Consider line
				// Check the face normals
				if (mechanical_check_edge_line(em, e1)) {
					verts[0] = e1->v1;
					verts[1] = e1->v2;
					edges[0] = e1;

					vcount = 2;
					ecount = 1;
					type = BM_GEOMETRY_TYPE_LINE;
				}
			}

			if (type) {

				BMGeom *egm = BLI_mempool_alloc(bm->gpool);
				bm->totgeom++;

				egm->head.htype = BM_GEOMETRY;
				egm->head.hflag = 0;
				egm->head.bm = bm;

				egm->totverts = vcount;
				egm->totedges = ecount;
				egm->v = MEM_callocN(sizeof(BMVert*)*egm->totverts,"geometry vertex pointer array");
				egm->e = MEM_callocN(sizeof(BMEdge*)*egm->totedges,"geometry edge pointer array");
				egm->geometry_type = type;
				memcpy(egm->v,verts,egm->totverts*sizeof(BMVert*));
				memcpy(egm->e,edges,egm->totedges*sizeof(BMEdge*));

				switch (type) {
					case BM_GEOMETRY_TYPE_CIRCLE:
					{
						copy_v3_v3(egm->center,center);
						normal_tri_v3(egm->axis, egm->v[0]->co, egm->v[1]->co, egm->v[2]->co);
						break;
					}
					case BM_GEOMETRY_TYPE_ARC:
					{
						copy_v3_v3(egm->center,center);
						normal_tri_v3(egm->axis, egm->v[0]->co, egm->v[1]->co, egm->v[2]->co);
						copy_v3_v3(egm->start, egm->v[0]->co);
						copy_v3_v3(egm->end, egm->v[egm->totverts-1]->co);
						arc_mid_point(egm);

						break;
					}
					case BM_GEOMETRY_TYPE_LINE:
					{
						sub_v3_v3v3(egm->axis,egm->v[0]->co,egm->v[1]->co);
						normalize_v3(egm->axis);
						copy_v3_v3(egm->start, egm->v[0]->co);
						copy_v3_v3(egm->end, egm->v[(egm->totverts)-1]->co);
						mid_of_2_points(egm->mid, egm->start, egm->end);
						break;
					}
					default:
						// Not valid
						break;
				}
			}
		}
	}
	MEM_freeN(&(*verts));
	MEM_freeN(&(*edges));
}

// Does not consider added data expand the geometry , eg lines
static bool mechanical_test_circle(BMVert *v1, BMVert *v2, BMVert *v3, void *data) {
	float *center = data;
	float n_center[3];

	return (center_of_3_points(n_center, v1->co, v2->co, v3->co) &&
			eq_v3v3_prec(n_center, center));
}

static bool mechanical_test_line(BMVert *v1, BMVert *UNUSED(v2), BMVert *v3, void *data) {
	float *dir = data;
	return point_on_axis(v1->co,dir,v3->co);

}

static bool mechanical_check_geometry(BMEditMesh *em, BMGeom *egm, bool (*mechanical_geometry_func) (
                                          BMVert*, BMVert*, BMVert*, void *), void *data)
{
	if (egm->totverts > 2) {
		BMVert *v1 = egm->v[0];
		BMVert *v2 = egm->v[1];
		BMVert *current = egm->v[2];
		int i;

		BM_elem_flag_enable(v1, BM_ELEM_TAG);
		BM_elem_flag_enable(v2, BM_ELEM_TAG);

		for (i=2;current && i<egm->totverts;i++) {
			if (mechanical_geometry_func(v1, v2, current, data))
			{
				BM_elem_flag_enable(current, BM_ELEM_TAG);
				current++;
			} else {
				current = NULL;
			}
		}
		if (current) {
			// check for edge expanding the new geometry
			BMEdge *e;
			BMIter iter;
			BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
				if (BM_elem_flag_test (e, BM_ELEM_TAG)) {
					continue;
				}
				if (e->v1 == egm->v[0] || e->v1 == egm->v[egm->totverts-1]) {
					if (mechanical_geometry_func(v1, v2, e->v1, data)) {
						current =NULL;
					}
				} else if (e->v2 == egm->v[0] || e->v2 == egm->v[egm->totverts-1]) {
					if (mechanical_geometry_func(v1, v2, e->v2, data)) {
						current =NULL;
					}
				}
			}
		}
		if (current == NULL) {
			//reset
			for (i=i-1;i>=0;i--) {
				BM_elem_flag_disable(egm->v[i], BM_ELEM_TAG);
			}
		}

		return (current != NULL);

	} else {
		return false;
	}
}

static void mechanical_check_mesh_geometry(BMEditMesh *em)
{
	BMGeom *egm;

	BMGeom *(*rem_egm);
	int rem_egm_count =0;
	BMIter iter;
	BMesh *bm = em->bm;
	bool valid;

	rem_egm = MEM_callocN(sizeof (BMGeom*)*BLI_mempool_count(bm->gpool), "geometry to be removed");

	BM_ITER_MESH (egm, &iter, bm, BM_GEOMETRY_OF_MESH) {
		valid = false;
		switch (egm->geometry_type) {
			case BM_GEOMETRY_TYPE_CIRCLE:
			case BM_GEOMETRY_TYPE_ARC:
				valid = mechanical_check_geometry(em, egm, mechanical_test_circle, egm->center);
				break;
			case BM_GEOMETRY_TYPE_LINE:
				valid = mechanical_check_geometry(em, egm, mechanical_test_line, egm->axis);
				break;
		}
		if (valid) {
			// Edges will be tagged according to vertex
		} else {
			// Remove!
#if 0
			/* Causes assert faling as changes pool count*/
			MEM_freeN(egm->v);
			MEM_freeN(egm->e);
			BLI_mempool_free(bm->gpool, egm);
			bm->totgeom--;
#endif
			rem_egm[rem_egm_count++] = egm;
		}
	}

	for (int i=0;i<rem_egm_count;i++){
		MEM_freeN(rem_egm[i]->v);
		MEM_freeN(rem_egm[i]->e);
		BLI_mempool_free(bm->gpool, rem_egm[i]);
		bm->totgeom--;
	}
	MEM_freeN(rem_egm);
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

		if (!mechanical_check_edge_line(em,e)) {
			// Ignore not valid edges
			BM_elem_flag_enable(e, BM_ELEM_TAG);
			continue;
		}

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




