
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_mesh_types.h"

#include "BKE_editmesh.h"

#include "UI_resources.h"

#include "bmesh.h"

#include "mesh_dimensions.h"

//Return dimension between two vertex
float get_dimension_value(BMDim *edm){
	float v=0;
	switch (edm->dim_type) {
		case DIM_TYPE_LINEAR:
			v = len_v3v3(edm->v[0]->co, edm->v[1]->co);
			break;
		case DIM_TYPE_DIAMETER:
			v = len_v3v3(edm->v[0]->co, edm->center)*2;
			break;
	}

	return v;
}

//Apply to the selected direction a dimension value
void apply_dimension_direction_value( BMVert *va, BMVert *vb, float value, float *res){
	float vect [3];
	float prev[3];
	copy_v3_v3(prev,va->co);
	sub_v3_v3v3(vect,va->co,vb->co);
	normalize_v3(vect);
	mul_v3_fl(vect,value);
	add_v3_v3v3(va->co,vb->co, vect);
	sub_v3_v3v3(res, va->co, prev);
}


static void apply_dimension_diameter_value(Mesh *me, BMDim *edm, float value, int constraints) {

	BLI_assert (edm->dim_type == DIM_TYPE_DIAMETER);

	float axis[3],v[3], ncenter[3];
	float curv = get_dimension_value(edm);

	BMIter iter;
	BMVert* eve;
	BMEditMesh* em = me->edit_btmesh;
	BMesh *bm = em->bm;


	get_dimension_plane(axis, edm);

	if (constraints & DIM_AXIS_CONSTRAINT) {

		BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
			sub_v3_v3v3(v,eve->co, edm->center);
			project_v3_v3v3(ncenter,v, axis);
			add_v3_v3(ncenter,edm->center);

			if ((len_v3v3 (eve->co, ncenter) - curv) <  DIM_CONSTRAINT_PRECISION){
				BM_elem_flag_enable(eve, BM_ELEM_TAG);
			}
		}
	}


	// Update related Verts
	BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_TAG)) {
			sub_v3_v3v3(v,eve->co, edm->center);

			project_v3_v3v3(ncenter,v, axis);
			add_v3_v3(ncenter,edm->center);

			sub_v3_v3v3(v,eve->co, ncenter);

			normalize_v3(v);
			mul_v3_fl(v,value/2.0f);
			add_v3_v3v3(eve->co,ncenter,v);

			// Reset tag
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}
	}





}

static void apply_dimension_linear_value(Mesh *me, BMDim *edm, float value, int constraints) {

	float v[3];

	BMIter iter;
	BMVert* eve;
	BMEditMesh* em = me->edit_btmesh;
	BMesh *bm = em->bm;

	BLI_assert (edm->dim_type == DIM_TYPE_LINEAR);

	if (edm->dir == 0){
		// Both directions
		float len=get_dimension_value(edm);
		float len2=(value-len)/2;
		edm->dir = 1;
		apply_dimension_value (me, edm,(len+len2), constraints);
		edm->dir = -1;
		apply_dimension_value (me, edm,value,constraints);
		edm->dir = 0;
		return;
	}

	if (constraints & DIM_PLANE_CONSTRAINT) {
		BMFace *f;
		BMIter viter;
		float vec[3];
		float d_dir[3], p[3];

		sub_v3_v3v3(d_dir,edm->v[0]->co,edm->v[1]->co);
		normalize_v3(d_dir);
		BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
			float d = dot_v3v3(d_dir,f->no);
			if (fabs(d*d -1) < DIM_CONSTRAINT_PRECISION) {
				// Coplanar?
				int ok =1;
				BM_ITER_ELEM (eve, &viter, f, BM_VERTS_OF_FACE) {
					if (edm->v[0] ==  eve || edm->v[1] == eve) {
						continue;
					}
					if (edm->dir == -1) {
						sub_v3_v3v3(vec, edm->v[0]->co, eve->co);
					} else if (edm->dir == 1) {
						sub_v3_v3v3(vec, edm->v[1]->co, eve->co);
					}
					normalize_v3(vec);
					project_v3_v3v3(p,vec,d_dir);
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
	// Untag dimensions vertex
	BM_elem_flag_disable(edm->v[0], BM_ELEM_TAG);
	BM_elem_flag_disable(edm->v[1], BM_ELEM_TAG);


	// Update Dimension Verts
	if(edm->dir==1){
		apply_dimension_direction_value(edm->v[1],edm->v[0], value, v);
	}else if(edm->dir==-1){
		apply_dimension_direction_value(edm->v[0],edm->v[1], value,v);
	}

	// Update related Verts
	BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_TAG)) {
			add_v3_v3(eve->co, v);
			// Reset tag
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}
	}

}

void apply_dimension_value (Mesh *me, BMDim *edm, float value, int constraints) {

	BMIter iter;
	BMVert* eve;
	BMEditMesh* em = me->edit_btmesh;
	BMesh *bm = em->bm;

	// Tag all elements to be affected by change
	BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
			BM_elem_flag_enable(eve, BM_ELEM_TAG);
		} else {
			// Disable as default
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}
	}

	// Tag vertexs of dimension
	for (int i=0;i<edm->totverts;i++) {
		// Tag elements
		BM_elem_flag_enable(edm->v[i], BM_ELEM_TAG);
	}

	switch (edm->dim_type) {
		case DIM_TYPE_LINEAR:
			apply_dimension_linear_value (me,edm,value,constraints);
			break;
		case DIM_TYPE_DIAMETER:
			apply_dimension_diameter_value (me,edm,value,constraints);
	}

	//EDBM_mesh_normals_update(em);
}

// text Position
void apply_txt_dimension_value(BMDim *edm, float value){

	edm->dpos_fact = value;
	sub_v3_v3v3(edm->dpos,edm->start,edm->end);
	mul_v3_fl(edm->dpos,value);
	add_v3_v3(edm->dpos, edm->end);


}

BMDim* get_selected_dimension_BMesh(BMesh *bm){
	BMDim *edm = NULL;
	BMIter iter;
	if (bm->totdimsel == 1) {
		BM_ITER_MESH (edm, &iter, bm, BM_DIMS_OF_MESH) {
			if ( BM_elem_flag_test(edm, BM_ELEM_SELECT)) {
				break;
			}
		}
	}
	return edm;
}


//Return edm.
BMDim* get_selected_dimension(BMEditMesh *em){
	BMDim *edm = NULL;
	BMesh *bm = em->bm;

	edm=get_selected_dimension_BMesh(bm);
	return edm;
}


void get_dimension_mid(float mid[3],BMDim *edm){
	mid_of_2_points(mid, edm->v[0]->co, edm->v[1]->co);
}

void get_dimension_plane (float p[3], BMDim *edm){
	float m[3], r[3];

	BLI_assert (edm->totverts >= 3);
	BLI_assert (edm->dim_type == DIM_TYPE_DIAMETER);
	sub_v3_v3v3(m,edm->v[0]->co,edm->v[1]->co);
	sub_v3_v3v3(r,edm->v[2]->co,edm->v[1]->co);
	cross_v3_v3v3(p,m,r);
	normalize_v3(p);

}


void mid_of_2_points (float *mid, float *p1, float *p2) {
	sub_v3_v3v3(mid, p2, p1);
	mul_v3_fl(mid,0.5);
	add_v3_v3(mid, p1);
}

int center_of_3_points(float *center, float *p1, float *p2, float *p3) {
	int res=0;

	float m[3], pm[3], mm[3];
	float r[3], pr[3], mr[3];
	float p21[3], p31[3];
	float p[3]; //Plane vector

	float r1[3],r2[3]; //isect results

	sub_v3_v3v3(m,p2,p1);
	sub_v3_v3v3(r,p3,p1);

	// Plane vector
	cross_v3_v3v3(p,m,r);

	// Perpendicular vectors
	cross_v3_v3v3(pm,m,p);
	cross_v3_v3v3(pr,r,p);

	// Midpoints
	mid_of_2_points(mm,p2,p1);
	mid_of_2_points(mr,p3,p1);

	//Second point
	add_v3_v3v3(p21,mm,pm);
	add_v3_v3v3(p31,mr,pr);

	if ((res = isect_line_line_v3(mm, p21, mr, p31,r1,r2)) == 1) {
		// One intersection: Ok
		copy_v3_v3(center,r1);
		return 1;
	} else if (res == 2 && len_v3v3(r1,r2) < DIM_CONSTRAINT_PRECISION) {
		copy_v3_v3(center,r1);
		return 1;
	} else {
		return 0;
	}
}

void dimension_data_update (BMDim *edm) {
	float axis[3], v1[3], l;

	switch (edm->dim_type) {
		case DIM_TYPE_LINEAR:
			// Set dimension always perpendicular to edge
			l = len_v3(edm->fpos);
			sub_v3_v3v3(axis, edm->v[0]->co, edm->v[1]->co);
			normalize_v3(axis);
			project_v3_v3v3(v1,edm->fpos,axis);
			sub_v3_v3(edm->fpos,v1);
			normalize_v3(edm->fpos);
			mul_v3_fl (edm->fpos, l);


			// Baseline
			add_v3_v3v3(edm->start,edm->fpos, edm->v[0]->co);
			add_v3_v3v3(edm->end, edm->fpos, edm->v[1]->co);


			// Set txt pos acording pos factor
			sub_v3_v3v3(edm->dpos, edm->end, edm->start);
			mul_v3_fl(edm->dpos, edm->dpos_fact);
			add_v3_v3(edm->dpos, edm->start);

			break;
		case DIM_TYPE_DIAMETER:
			// Set f pos always on dimension plane!
			get_dimension_plane(v1,edm);
			project_plane_v3_v3v3(edm->fpos, edm->fpos ,v1);

			// Recalc dimension center
			if (center_of_3_points (edm->center, edm->v[0]->co, edm->v[1]->co, edm->v[2]->co)) {
				// Ok
			} else if (center_of_3_points (edm->center, edm->v[1]->co, edm->v[0]->co, edm->v[2]->co)) {
				// Ok
			} else {
				// Somthing is going grong!
				BLI_assert (0);
			}

			// Calc start end
			sub_v3_v3v3(v1,edm->fpos, edm->center);
			normalize_v3(v1);
			mul_v3_fl(v1,get_dimension_value(edm)/2.0f);
			add_v3_v3v3(edm->start,edm->center,v1);
			sub_v3_v3v3(edm->end, edm->center,v1);

			// Set txt pos acording pos factor
			sub_v3_v3v3(edm->dpos, edm->end, edm->start);
			mul_v3_fl(edm->dpos, edm->dpos_fact);
			add_v3_v3(edm->dpos, edm->start);


			break;
	}
}
