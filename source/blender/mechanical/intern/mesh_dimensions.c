
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_mesh_types.h"

#include "ED_mesh.h"

#include "BKE_editmesh.h"

#include "UI_resources.h"

#include "bmesh.h"

#include "mesh_dimensions.h"
#include "mechanical_utils.h"

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
		case DIM_TYPE_RADIUS:
			v = len_v3v3(edm->v[0]->co, edm->center);
			break;
		case DIM_TYPE_ANGLE_3P:
			v = RAD2DEG(angle_v3v3v3(edm->v[0]->co,edm->v[1]->co,edm->v[2]->co));
			break;
		default:
			BLI_assert(0);
	}

	return v;
}


static void apply_dimension_diameter_from_center(BMesh *bm, BMDim *edm, float value, int constraints) {

	BLI_assert (ELEM(edm->dim_type,DIM_TYPE_DIAMETER, DIM_TYPE_RADIUS));

	float axis[3],v[3], ncenter[3];
	float curv = get_dimension_value(edm);

	BMIter iter;
	BMVert* eve;

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
			mul_v3_fl(v,value);
			add_v3_v3v3(eve->co,ncenter,v);

			// Reset tag
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}
	}
}

/**
 * @brief apply_dimension_linear_value_exec
 * @param va
 * @param vb  Vertex to be modified
 * @param value
 * @param r_res  The vector of translation
 */
static void apply_dimension_linear_value_exec( float *a, float *b, float value, float *r_res){
	float vect [3];
	float prev[3];
	copy_v3_v3(prev,a);
	sub_v3_v3v3(vect,a,b);
	normalize_v3(vect);
	mul_v3_fl(vect,value);
	add_v3_v3v3(a,b, vect);
	sub_v3_v3v3(r_res, a, prev);
}


static void apply_dimension_linear_value(BMesh *bm, BMDim *edm, float value, int constraints) {

	float v[3];

	BMIter iter;
	BMVert* eve;

	BLI_assert (edm->dim_type == DIM_TYPE_LINEAR);

	if (edm->dir == 0){
		// Both directions
		float len=get_dimension_value(edm);
		float len2=(value-len)/2;
		edm->dir = 1;
		apply_dimension_linear_value(bm, edm,(len+len2), constraints);
		edm->dir = -1;
		apply_dimension_linear_value(bm, edm,value,constraints);
		edm->dir = 0;
		return;
	}

	if (constraints & DIM_PLANE_CONSTRAINT) {
		float p[3], d_dir[3];
		if (edm->dir == -1) {
			copy_v3_v3(p, edm->v[0]->co);
		} else if (edm->dir == 1) {
			copy_v3_v3(p, edm->v[1]->co);
		}
		sub_v3_v3v3(d_dir,edm->v[0]->co,edm->v[1]->co);
		normalize_v3(d_dir);
		tag_vertexs_on_coplanar_faces(bm, p, d_dir);
	}
	// Untag dimensions vertex
	BM_elem_flag_disable(edm->v[0], BM_ELEM_TAG);
	BM_elem_flag_disable(edm->v[1], BM_ELEM_TAG);


	// Update Dimension Verts
	if(edm->dir==1){
		apply_dimension_linear_value_exec(edm->v[1]->co,edm->v[0]->co, value, v);
	}else if(edm->dir==-1){
		apply_dimension_linear_value_exec(edm->v[0]->co,edm->v[1]->co, value, v);
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

static void apply_dimension_angle_exec (float *a, float *center, float *axis, float value) {
	float rot[3], v[3];
	sub_v3_v3v3(v,a,center);
	rotate_v3_v3v3fl(rot, v,axis, DEG2RAD(value));
	add_v3_v3v3(a,rot,center);
}

static void apply_dimension_angle(BMesh *bm, BMDim *edm, float value, int constraints) {
	float axis[3], ncenter[3], v[3];
	float d; //Difential to move

	BMIter iter;
	BMVert* eve;

	BLI_assert (edm->dim_type == DIM_TYPE_ANGLE_3P);

	get_dimension_plane(axis,edm);
	d = value - get_dimension_value(edm);

	if (edm->dir == 0) {
		// Both sides
		edm->dir = 1;
		apply_dimension_angle(bm, edm,value - d/2.0f, constraints);
		edm->dir = -1;
		apply_dimension_angle(bm, edm,value,constraints);
		edm->dir = 0;
		return;
	}

	if (constraints & DIM_PLANE_CONSTRAINT) {
		float p[3], r[3], d_dir[3];
		if (edm->dir == 1) {
			copy_v3_v3(p, edm->v[2]->co);
			sub_v3_v3v3(r, edm->v[2]->co, edm->v[1]->co);
		} else if (edm->dir == -1) {
			copy_v3_v3(p, edm->v[0]->co);
			sub_v3_v3v3(r, edm->v[0]->co, edm->v[1]->co);
		}
		cross_v3_v3v3(d_dir,axis,r);
		normalize_v3(d_dir);
		tag_vertexs_on_coplanar_faces(bm, p, d_dir);
	}

	// Untag dimensions vertex
	BM_elem_flag_disable(edm->v[0], BM_ELEM_TAG);
	BM_elem_flag_disable(edm->v[1], BM_ELEM_TAG);
	BM_elem_flag_disable(edm->v[2], BM_ELEM_TAG);

	if (edm->dir == 1) {
		apply_dimension_angle_exec(edm->v[2]->co,edm->v[1]->co, axis,d);
	}
	if (edm->dir == -1) {
		d = d*(-1);
		apply_dimension_angle_exec(edm->v[0]->co,edm->v[1]->co, axis, d);
	}

	//Rotate tagged points against axis
	BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_TAG)) {

			sub_v3_v3v3(v,eve->co, edm->v[1]->co);

			project_v3_v3v3(ncenter,v, axis);
			add_v3_v3(ncenter,edm->v[1]->co);

			apply_dimension_angle_exec(eve->co,ncenter, axis,d);

			// Reset tag
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}
	}
}

void apply_dimension_value (BMesh *bm, BMDim *edm, float value, int constraints) {

	BMIter iter;
	BMVert* eve;

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
			apply_dimension_linear_value(bm,edm,value,constraints);
			break;
		case DIM_TYPE_DIAMETER:
			apply_dimension_diameter_from_center(bm,edm,value/2.0f,constraints);
			break;
		case DIM_TYPE_RADIUS:
			apply_dimension_diameter_from_center(bm,edm,value,constraints);
			break;
		case DIM_TYPE_ANGLE_3P:
			apply_dimension_angle(bm,edm,value,constraints);
			break;
		default:
			BLI_assert(0);
	}

	BM_mesh_normals_update(bm);
}

BMDim* get_selected_dimension(BMesh *bm){
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


void get_dimension_mid(float mid[3],BMDim *edm){
	mid_of_2_points(mid, edm->v[0]->co, edm->v[1]->co);
}

void get_dimension_plane (float p[3], BMDim *edm){
	float m[3], r[3];

	BLI_assert (edm->totverts >= 3);
	BLI_assert (ELEM(edm->dim_type,DIM_TYPE_DIAMETER,DIM_TYPE_RADIUS));
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

static void set_dimension_start_end (BMDim *edm) {
	float v[3];
	switch (edm->dim_type) {
		case DIM_TYPE_LINEAR:
			add_v3_v3v3(edm->start,edm->fpos, edm->v[0]->co);
			add_v3_v3v3(edm->end, edm->fpos, edm->v[1]->co);
			break;
		case DIM_TYPE_DIAMETER:
			sub_v3_v3v3(v,edm->fpos, edm->center);
			normalize_v3(v);
			mul_v3_fl(v,get_dimension_value(edm)/2.0f);
			add_v3_v3v3(edm->start,edm->center,v);
			sub_v3_v3v3(edm->end, edm->center,v);
			break;
		case DIM_TYPE_RADIUS:
			sub_v3_v3v3(v,edm->fpos, edm->center);
			normalize_v3(v);
			mul_v3_fl(v,get_dimension_value(edm));
			copy_v3_v3(edm->start,edm->center);
			add_v3_v3v3(edm->end, edm->center, v);
			break;
		case DIM_TYPE_ANGLE_3P:
			sub_v3_v3v3(edm->start,edm->v[0]->co,edm->v[1]->co);
			add_v3_v3(edm->start,edm->v[1]->co);
			sub_v3_v3v3(edm->end,edm->v[2]->co,edm->v[1]->co);
			add_v3_v3(edm->end,edm->v[1]->co);
			break;
		default:
			BLI_assert(0);
	}
}

void dimension_data_update (BMDim *edm) {
	float axis[3], v1[3],v2[3], l;

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
			set_dimension_start_end(edm);

			// Set txt pos acording pos factor
			sub_v3_v3v3(edm->dpos, edm->end, edm->start);
			mul_v3_fl(edm->dpos, edm->dpos_fact);
			add_v3_v3(edm->dpos, edm->start);

			break;
		case DIM_TYPE_DIAMETER:
		case DIM_TYPE_RADIUS:
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

			set_dimension_start_end(edm);

			// Set txt pos acording pos factor
			sub_v3_v3v3(edm->dpos, edm->end, edm->start);
			mul_v3_fl(edm->dpos, edm->dpos_fact);
			add_v3_v3(edm->dpos, edm->start);
			break;
		case DIM_TYPE_ANGLE_3P:
			get_dimension_plane(v1,edm);
			project_plane_v3_v3v3(edm->fpos, edm->fpos ,v1);
			set_dimension_start_end(edm);

			// Set txt pos acording pos factor
			sub_v3_v3v3(v1,edm->start,edm->v[1]->co);
			normalize_v3(v1);
			sub_v3_v3v3(v2,edm->end,edm->v[1]->co);
			cross_v3_v3v3(axis,v1,v2);
			normalize_v3(v2);

			// Set txt pos acording pos factor
			mul_v3_fl(v1,len_v3(edm->fpos));
			rotate_v3_v3v3fl(edm->dpos,v1,axis, acos(dot_v3v3(v1,v2))*edm->dpos_fact);
			add_v3_v3(edm->dpos, edm->v[1]->co);
			break;
		default:
			BLI_assert(0);
	}
}
