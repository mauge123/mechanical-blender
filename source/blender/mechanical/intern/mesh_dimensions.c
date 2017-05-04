
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "DNA_mesh_types.h"

#include "ED_mesh.h"

#include "BKE_editmesh.h"

#include "UI_resources.h"

#include "bmesh.h"

#include "mesh_dimensions.h"
#include "mechanical_utils.h"


static void set_dimension_start_end(BMDim *edm);

bool valid_constraint_setting(BMDim *edm, int constraint) {
	bool ret = false;
	switch (constraint) {
		case DIM_PLANE_CONSTRAINT:
			ret = ELEM(edm->mdim->dim_type,DIM_TYPE_LINEAR,DIM_TYPE_ANGLE_3P,DIM_TYPE_ANGLE_4P);
			break;
		case DIM_AXIS_CONSTRAINT:
			ret = ELEM(edm->mdim->dim_type,DIM_TYPE_RADIUS, DIM_TYPE_DIAMETER);
			break;
		case DIM_ALLOW_SLIDE_CONSTRAINT:
			ret = ELEM(edm->mdim->dim_type,DIM_TYPE_ANGLE_3P,DIM_TYPE_ANGLE_4P);
			break;
		case DIM_CONCENTRIC_CONSTRAINT:
			ret = (edm->mdim->axis > 0);
			break;
	}
	return ret;
}

//Return dimension between two vertex
float get_dimension_value(BMDim *edm){
	float v=0;
	switch (edm->mdim->dim_type) {
		case DIM_TYPE_LINEAR:
			v = len_v3v3(edm->v[0]->co, edm->v[1]->co);
			break;
		case DIM_TYPE_DIAMETER:
			v = len_v3v3(edm->v[0]->co, edm->mdim->center)*2;
			break;
		case DIM_TYPE_RADIUS:
			v = len_v3v3(edm->v[0]->co, edm->mdim->center);
			break;
		case DIM_TYPE_ANGLE_3P:
			v = RAD2DEG(angle_v3v3v3(edm->mdim->start,edm->mdim->center,edm->mdim->end));
			break;
		case DIM_TYPE_ANGLE_4P:
			v = RAD2DEG(angle_v3v3v3(edm->v[0]->co,edm->mdim->center, edm->v[3]->co));
			break;
		default:
			BLI_assert(0);
	}

	return v;
}

int get_necessary_dimension_verts (int dim_type) {
	static int min_vert[] = {0,
	                  2, // DIM_TYPE_LINEAR
	                  3, // DIM_TYPE_DIAMETER
	                  3,  // DIM_TYPE_RADIUS
	                  3, // DIM_TYPE_ANGLE_3P       
	                  4, // DIM_TYPE_ANGLE_4P
	                 };
	return min_vert[dim_type];
}


static void untag_dimension_necessary_verts(BMDim *edm) {
	for (int i=0;i<get_necessary_dimension_verts(edm->mdim->dim_type);i++) {
		BM_elem_flag_disable(edm->v[i], BM_ELEM_TAG);
	}
}


static void apply_dimension_radius_from_center_exec(float* point, float *center, float *axis, float inc) {
	float ncenter[3], v[3];
	float curr;

	sub_v3_v3v3(v,point, center);
	project_v3_v3v3(ncenter,v, axis);
	add_v3_v3(ncenter,center);

	sub_v3_v3v3(v,point, ncenter);
	curr = len_v3(v);

	normalize_v3(v);
	mul_v3_fl(v,curr+inc);
	add_v3_v3v3(point,ncenter,v);
}

static void apply_dimension_radius_from_center(BMesh *bm, BMDim *edm, float value, int constraints) {

	BLI_assert (ELEM(edm->mdim->dim_type,DIM_TYPE_DIAMETER, DIM_TYPE_RADIUS));

	float axis[3],v[3], ncenter[3], p[3];
	float curv = get_dimension_value(edm); // Current Value
	float inc;

	BMIter iter;
	BMVert* eve;

	if (edm->mdim->dim_type == DIM_TYPE_DIAMETER) {
		curv/=2.0f;
	}
	inc = value-curv;


	get_dimension_plane(axis, p, edm);

	tag_vertexs_affected_by_dimension (bm, edm);

	if (constraints & DIM_AXIS_CONSTRAINT) {

		BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(eve,BM_ELEM_TAG)) {
				continue;
			}
			sub_v3_v3v3(v,eve->co, edm->mdim->center);
			project_v3_v3v3(ncenter,v, axis);
			add_v3_v3(ncenter,edm->mdim->center);

			if (fabs((len_v3v3 (eve->co, ncenter) - curv)) <  DIM_CONSTRAINT_PRECISION){
				BM_elem_flag_enable(eve, BM_ELEM_TAG);
			}
		}
	}

	// Update related Verts
	BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_TAG)) {
			apply_dimension_radius_from_center_exec(eve->co,edm->mdim->center,axis,inc);
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

	BLI_assert (edm->mdim->dim_type == DIM_TYPE_LINEAR);

	if (edm->mdim->dir == DIM_DIR_BOTH){
		// Both directions
		float len=get_dimension_value(edm);
		float len2=(value-len)/2;
		edm->mdim->dir = DIM_DIR_RIGHT;
		apply_dimension_linear_value(bm, edm,(len+len2), constraints);
		edm->mdim->dir = DIM_DIR_LEFT;
		apply_dimension_linear_value(bm, edm,value,constraints);
		edm->mdim->dir = DIM_DIR_BOTH;
		return;
	}

	tag_vertexs_affected_by_dimension (bm, edm);

	if (constraints & DIM_PLANE_CONSTRAINT) {
		float p[3], d_dir[3];
		if (edm->mdim->dir == DIM_DIR_LEFT) {
			copy_v3_v3(p, edm->v[0]->co);
		} else if (edm->mdim->dir == DIM_DIR_RIGHT) {
			copy_v3_v3(p, edm->v[1]->co);
		}
		sub_v3_v3v3(d_dir,edm->v[0]->co,edm->v[1]->co);
		normalize_v3(d_dir);
		tag_vertexs_on_coplanar_faces(bm, p, d_dir);
		tag_vertexs_on_plane(bm, p , d_dir);
	}
	// Untag dimensions vertex
	untag_dimension_necessary_verts(edm);


	// Update Dimension Verts
	if(edm->mdim->dir == DIM_DIR_RIGHT){
		apply_dimension_linear_value_exec(edm->v[1]->co,edm->v[0]->co, value, v);
	}else if(edm->mdim->dir== DIM_DIR_LEFT){
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

static void apply_dimension_angle_exec(BMesh *bm, BMVert *eve, float *center,
                                       float *axis, float value, int constraints, float *constraint_axis) {
	float rot[3], delta[3], r[3], p[3];
	BMVert *v,*v2;
	BMFace *f;
	BMIter iterv;
	BMIter iterf, iterf2;
	float dir[3];

	copy_v3_v3(p, eve->co);  //Store original point
	sub_v3_v3v3(delta, eve->co, center);
	cross_v3_v3v3(dir,delta,axis);
	normalize_v3(dir);
	rotate_v3_v3v3fl(rot, delta, axis, DEG2RAD(value));
	add_v3_v3v3(eve->co, rot, center);

	if (constraints & DIM_ALLOW_SLIDE_CONSTRAINT) {
		if (constraint_axis) {
			if (isect_line_plane_v3(r, center, eve->co, p, constraint_axis)){
				copy_v3_v3(eve->co,r);
			}
		} else {
			// DIM_TYPE_ANGLE_3P
			// DIM_TYPE_ANGLE_4P
			BM_ITER_MESH (f, &iterv, bm, BM_FACES_OF_MESH) {
				BM_ITER_ELEM (v, &iterf, f, BM_VERTS_OF_FACE) {
					if (v == eve) {
						if (perpendicular_v3_v3(axis, f->no)) {
							if (!parallel_v3u_v3u(dir,f->no)) {
								if (!point_on_axis(center,axis,eve->co)) {
									// Fin a vertex on face not matching eve
									BM_ITER_ELEM (v2, &iterf2, f, BM_VERTS_OF_FACE) {
										if (v2 != eve) break;
									}
									if (isect_line_plane_v3(r, center, eve->co, v2->co, f->no)){
										copy_v3_v3(eve->co,r);
									}
								}
							}
						}
					}
				}
			}
		}
	}

}

static void apply_dimension_angle(BMesh *bm, BMDim *edm, float value, int constraints) {
	float axis[3], ncenter[3], v[3], pp[3];
	float d; //Difential to move

	float p[3], r[3];
	float d_dir[3]; // Dimension dir

	float constraint_axis[3];
	float *p_constraint_axis = NULL;
	float ccenter[3],ccenter1[3],ccenter2[3]; // Centers coordinates
	float r_constraint, r_constraint_c;

	bool has_axis = false;

	BMIter iter;
	BMVert* eve;

	BLI_assert (ELEM(edm->mdim->dim_type,DIM_TYPE_ANGLE_3P,DIM_TYPE_ANGLE_4P));

	get_dimension_plane(axis, pp, edm);
	d = value - get_dimension_value(edm);

	if (edm->mdim->dir == DIM_DIR_BOTH) {
		// Both sides
		edm->mdim->dir = DIM_DIR_RIGHT;
		apply_dimension_angle(bm, edm,value - d/2.0f, constraints);
		edm->mdim->dir = DIM_DIR_LEFT;
		apply_dimension_angle(bm, edm,value,constraints);
		edm->mdim->dir = DIM_DIR_BOTH;
		return;
	}

	if (edm->mdim->dimension_flag & DIMENSION_FLAG_ANGLE_COMPLEMENTARY) {
		d = -d;
	}


	// Get Dimension dir
	if (edm->mdim->dir == DIM_DIR_RIGHT) {
		copy_v3_v3(p, edm->mdim->end);
		sub_v3_v3v3(r, edm->mdim->end, edm->mdim->center);
	} else if (edm->mdim->dir == DIM_DIR_LEFT) {

		copy_v3_v3(p, edm->mdim->start);
		sub_v3_v3v3(r, edm->mdim->start, edm->mdim->center);
	}
	cross_v3_v3v3(d_dir,axis,r);
	normalize_v3(d_dir);

	tag_vertexs_affected_by_dimension (bm, edm);

	if ((constraints & DIM_PLANE_CONSTRAINT) &&  (~constraints &  DIM_CONCENTRIC_CONSTRAINT )) {
		tag_vertexs_on_coplanar_faces(bm, p, d_dir);
		tag_vertexs_on_plane(bm, p, d_dir);
	}

	if (edm->mdim->axis > 0) {

		/*int i_ofs =  (edm->mdim->dim_type ==  DIM_TYPE_ANGLE_3P_CON) ? 3 : 4;
		float *t = (edm->mdim->dir == DIM_DIR_RIGHT) ? edm->v[2]->co : edm->v[0]->co;

		if (center_of_3_points (ccenter, edm->v[i_ofs]->co, edm->v[i_ofs+1]->co, edm->v[i_ofs+2]->co))
		{
			float rcenter1[3],rcenter2[3]; // Vector radius on centers

			normal_tri_v3(constraint_axis,edm->v[i_ofs]->co, edm->v[i_ofs+1]->co, edm->v[i_ofs+2]->co);

			v_perpendicular_to_axis(ccenter1, ccenter, t, constraint_axis);
			sub_v3_v3v3(ccenter1, t, ccenter1);

			sub_v3_v3v3(rcenter1,ccenter1, t);
			r_constraint = len_v3(rcenter1);
			normalize_v3(rcenter1);

			v_perpendicular_to_axis (ccenter2, ccenter1, edm->mdim->center, constraint_axis);
			sub_v3_v3v3(ccenter2, edm->mdim->center, ccenter2);

			sub_v3_v3v3(rcenter2,ccenter2,edm->mdim->center);
			r_constraint_c = len_v3(rcenter2);
			normalize_v3(rcenter2);

			// Invert constraint center radi if v rcenter1 and rcenter2 are opposite
			r_constraint_c *= dot_v3v3(rcenter1,rcenter2);

			p_constraint_axis = constraint_axis;
			has_axis = true;

		}*/

		/*if (constraints & DIM_CONCENTRIC_CONSTRAINT && has_axis) {
			BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
				float delta = fabs(point_dist_to_axis (ccenter1, constraint_axis,eve->co));
				if ((delta - r_constraint)< DIM_CONSTRAINT_PRECISION &&
					point_on_plane(t,constraint_axis,eve->co))
				{
					BM_elem_flag_enable(eve, BM_ELEM_TAG);
				}
			}

		}*/
	}


	// Untag dimensions vertex
	untag_dimension_necessary_verts(edm);

	if (!has_axis) {
		// Untag vertex not in plane
		BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
			if (BM_elem_flag_test(eve, BM_ELEM_TAG)) {
				if (!point_on_plane (edm->mdim->center,d_dir,eve->co)) {
					BM_elem_flag_disable(eve, BM_ELEM_TAG);
				}
			}
		}
	}

	if (edm->mdim->dir == DIM_DIR_RIGHT) {
		apply_dimension_angle_exec(bm,edm->v[2],edm->mdim->center, axis,d,constraints, p_constraint_axis);
		if (edm->mdim->dim_type == DIM_TYPE_ANGLE_4P){
			apply_dimension_angle_exec(bm, edm->v[3],edm->mdim->center, axis,d,constraints, p_constraint_axis);
		}
	}
	if (edm->mdim->dir == DIM_DIR_LEFT) {
		d = d*(-1);
		apply_dimension_angle_exec(bm, edm->v[0],edm->mdim->center, axis, d,constraints, p_constraint_axis);
		if (edm->mdim->dim_type == DIM_TYPE_ANGLE_4P){
			apply_dimension_angle_exec(bm, edm->v[1],edm->mdim->center, axis,d, constraints, p_constraint_axis);
		}
	}

	// To get correct dimension value in case of nexts steps (BOTH SIDES)
	set_dimension_start_end(edm);

	//Rotate tagged points against axis
	BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_TAG)) {

			if (has_axis && (fabs(point_dist_to_axis (ccenter1, constraint_axis,eve->co) - r_constraint))< DIM_CONSTRAINT_OVERRIDE)
			{
				float naxis[3];

				sub_v3_v3v3(v,eve->co, ccenter2);
				v_perpendicular_to_axis(ncenter,ccenter2,v,constraint_axis);
				normalize_v3(ncenter);
				// r_constraint_c has sign
				mul_v3_fl(ncenter,r_constraint_c);
				add_v3_v3(ncenter,ccenter2);
				if (edm->mdim->dir == DIM_DIR_LEFT) {
					normal_tri_v3(naxis,ccenter1,ccenter2,ncenter);
				} else {
					normal_tri_v3(naxis,ccenter2,ccenter1,ncenter);
				}

				apply_dimension_angle_exec(bm, eve,ncenter, naxis,d,constraints, p_constraint_axis);

			} else {

				sub_v3_v3v3(v,eve->co, edm->mdim->center);
	
				project_v3_v3v3(ncenter,v, axis);
				add_v3_v3(ncenter,edm->mdim->center);
	
				apply_dimension_angle_exec(bm, eve,ncenter, axis,d,constraints, p_constraint_axis);
			}
	
			// Reset tag
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}
	}
}

void apply_dimension_value (BMesh *bm, BMDim *edm, float value, ToolSettings *ts) {

	int constraints = (edm->mdim->constraints & DIM_CONSTRAINT_OVERRIDE) ? edm->mdim->constraints : ts->dimension_constraints;

	switch (edm->mdim->dim_type) {
		case DIM_TYPE_LINEAR:
			apply_dimension_linear_value(bm,edm,value,constraints);
			break;
		case DIM_TYPE_DIAMETER:
			apply_dimension_radius_from_center(bm,edm,value/2.0f,constraints);
			break;
		case DIM_TYPE_RADIUS:
			apply_dimension_radius_from_center(bm,edm,value,constraints);
			break;
		case DIM_TYPE_ANGLE_3P:
		case DIM_TYPE_ANGLE_4P:
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
		BM_ITER_MESH(edm, &iter, bm, BM_DIMS_OF_MESH) {
			if ( BM_elem_flag_test(edm, BM_ELEM_SELECT)) {
				break;
			}
		}
	}
	return edm;
}


void get_dimension_mid(float mid[3],BMDim *edm){
	switch (edm->mdim->dim_type)
	{
		case DIM_TYPE_LINEAR:
			mid_of_2_points(mid, edm->v[0]->co, edm->v[1]->co);
			break;
		default:
			BLI_assert(0);
	}
}

void get_dimension_plane (float v[3], float p[3], BMDim *edm){
	float m[3]={0}, r[3]={0};
	switch (edm->mdim->dim_type) {
		case DIM_TYPE_LINEAR:
			BLI_assert(0); // Not possible
			break;
		case DIM_TYPE_DIAMETER:
		case DIM_TYPE_RADIUS:
			sub_v3_v3v3(m,edm->v[0]->co,edm->mdim->center);
			normalize_v3(m);
			sub_v3_v3v3(r,edm->v[1]->co,edm->mdim->center);
			normalize_v3(r);
			if (parallel_v3u_v3u(m,r)) {
				// Parallel vector, use next
				sub_v3_v3v3(r,edm->v[2]->co,edm->mdim->center);
				normalize_v3(r);
			}
			break;
		case DIM_TYPE_ANGLE_3P:
			sub_v3_v3v3(m,edm->v[0]->co,edm->mdim->center);
			sub_v3_v3v3(r,edm->v[2]->co,edm->mdim->center);
			break;
		case DIM_TYPE_ANGLE_4P:
			sub_v3_v3v3(m,edm->v[0]->co,edm->mdim->center);
			sub_v3_v3v3(r,edm->v[3]->co,edm->mdim->center);
			break;
		default:
			BLI_assert(0);
	}
	cross_v3_v3v3(v,m,r);
	normalize_v3(v);
	copy_v3_v3(p,edm->v[0]->co);
}


void mid_of_2_points (float *mid, float *p1, float *p2) {
	sub_v3_v3v3(mid, p2, p1);
	mul_v3_fl(mid,0.5);
	add_v3_v3(mid, p1);
}

static void set_dimension_angle_start_end(BMDim *edm, float *a, float *b) {
	float len;
	len = len_v3(edm->mdim->fpos);
	sub_v3_v3v3(edm->mdim->start,a,edm->mdim->center);
	normalize_v3(edm->mdim->start);
	mul_v3_fl(edm->mdim->start,len);
	add_v3_v3(edm->mdim->start,edm->mdim->center);
	sub_v3_v3v3(edm->mdim->end,b,edm->mdim->center);
	normalize_v3(edm->mdim->end);
	mul_v3_fl(edm->mdim->end,len);
	add_v3_v3(edm->mdim->end,edm->mdim->center);

	if (edm->mdim->dimension_flag & DIMENSION_FLAG_ANGLE_COMPLEMENTARY ) {
		BLI_assert (edm->mdim->dim_type == DIM_TYPE_ANGLE_3P);
		sub_v3_v3(edm->mdim->start, edm->mdim->center);
		sub_v3_v3v3(edm->mdim->start, edm->mdim->center, edm->mdim->start);
	}
}

static void set_dimension_diameter_start_end(BMDim* edm) {
	float v[3];
	copy_v3_v3(v,edm->mdim->fpos);
	normalize_v3(v);
	mul_v3_fl(v,get_dimension_value(edm)/2.0f);
	add_v3_v3v3(edm->mdim->start,edm->mdim->center,v);
	sub_v3_v3v3(edm->mdim->end, edm->mdim->center,v);
}

static void set_dimension_radius_start_end(BMDim* edm) {
	float v[3];
	copy_v3_v3(v, edm->mdim->fpos);
	normalize_v3(v);
	mul_v3_fl(v, get_dimension_value(edm));
	add_v3_v3v3(edm->mdim->end, edm->mdim->center, v);
	copy_v3_v3(edm->mdim->start, edm->mdim->center);
}

static void set_dimension_start_end(BMDim *edm) {
	switch (edm->mdim->dim_type) {
		case DIM_TYPE_LINEAR:
			add_v3_v3v3(edm->mdim->start,edm->mdim->fpos, edm->v[0]->co);
			add_v3_v3v3(edm->mdim->end, edm->mdim->fpos, edm->v[1]->co);
			break;
		case DIM_TYPE_DIAMETER:
			set_dimension_diameter_start_end(edm);
			break;
		case DIM_TYPE_RADIUS:
			set_dimension_radius_start_end(edm);
			break;
		case DIM_TYPE_ANGLE_3P:
			set_dimension_angle_start_end(edm, edm->v[0]->co, edm->v[2]->co);
			break;
		case DIM_TYPE_ANGLE_4P:
			set_dimension_angle_start_end(edm, edm->v[0]->co, edm->v[3]->co);
			break;
		default:
			BLI_assert(0);
	}
}


void set_dimension_center (BMDim *edm) {
	float a[3];
	switch (edm->mdim->dim_type) {
		case DIM_TYPE_LINEAR:
			zero_v3(edm->mdim->center);
			break;
		case DIM_TYPE_DIAMETER:
		case DIM_TYPE_RADIUS:
			// Recalc dimension center
			if (center_of_3_points (edm->mdim->center, edm->v[0]->co, edm->v[1]->co, edm->v[2]->co)) {
				// Ok
			} else if (center_of_3_points (edm->mdim->center, edm->v[1]->co, edm->v[0]->co, edm->v[2]->co)) {
				// Ok
			} else {
				// Somthing is going grong!
				BLI_assert (0);
			}
			break;
		case DIM_TYPE_ANGLE_3P:
			copy_v3_v3(edm->mdim->center, edm->v[1]->co);
			break;
		case DIM_TYPE_ANGLE_4P:
			isect_line_line_v3(
			            edm->v[0]->co,
						edm->v[1]->co,
						edm->v[2]->co,
						edm->v[3]->co,
						edm->mdim->center,
						a // Should match center if cooplanar
			        );
			break;
	}
}

static void ensure_fpos_on_plane (BMDim *edm){
	float n[3],p[3];
	// Set f pos always on dimension plane!
	// fpos is related to dimension center
	get_dimension_plane(n,p, edm);
	add_v3_v3(edm->mdim->fpos,edm->mdim->center);
	project_v3_plane(edm->mdim->fpos, n, p);
	sub_v3_v3(edm->mdim->fpos,edm->mdim->center);
}

void dimension_data_update (BMDim *edm) {
	float axis[3], v1[3],v2[3], l;

	edm->mdim->value_pr = edm->mdim->value;

	switch (edm->mdim->dim_type) {
		case DIM_TYPE_LINEAR:
			// Set dimension always perpendicular to edge
			l = len_v3(edm->mdim->fpos);
			sub_v3_v3v3(axis, edm->v[0]->co, edm->v[1]->co);
			normalize_v3(axis);
			project_v3_v3v3(v1,edm->mdim->fpos,axis);
			sub_v3_v3(edm->mdim->fpos,v1);
			normalize_v3(edm->mdim->fpos);
			mul_v3_fl (edm->mdim->fpos, l);


			// Baseline
			set_dimension_start_end(edm);

			// Set txt pos acording pos factor
			sub_v3_v3v3(edm->mdim->dpos, edm->mdim->end, edm->mdim->start);
			mul_v3_fl(edm->mdim->dpos, edm->mdim->dpos_fact);
			add_v3_v3(edm->mdim->dpos, edm->mdim->start);

			break;
		case DIM_TYPE_DIAMETER:
			set_dimension_center(edm);
			ensure_fpos_on_plane(edm);
			set_dimension_start_end(edm);


			// Set txt pos acording pos factor
			sub_v3_v3v3(edm->mdim->dpos, edm->mdim->end, edm->mdim->start);
			mul_v3_fl(edm->mdim->dpos, edm->mdim->dpos_fact);
			add_v3_v3(edm->mdim->dpos, edm->mdim->start);
			break;
		case DIM_TYPE_RADIUS:
			set_dimension_center(edm);
			ensure_fpos_on_plane(edm);

			set_dimension_start_end(edm); // First

			// Set txt pos acording pos factor
			sub_v3_v3v3(edm->mdim->dpos, edm->mdim->end, edm->mdim->center);
			mul_v3_fl(edm->mdim->dpos, edm->mdim->dpos_fact);
			add_v3_v3(edm->mdim->dpos, edm->mdim->start);
			break;
		case DIM_TYPE_ANGLE_3P:
		case DIM_TYPE_ANGLE_4P:
			ensure_fpos_on_plane(edm);

			set_dimension_center(edm);  // First
			set_dimension_start_end(edm);

			// Set txt pos acording pos factor
			sub_v3_v3v3(v1,edm->mdim->start,edm->mdim->center);
			normalize_v3(v1);
			sub_v3_v3v3(v2,edm->mdim->end,edm->mdim->center);
			normalize_v3(v2);

			cross_v3_v3v3(axis,v1,v2);


			// Set txt pos acording pos factor
			mul_v3_fl(v1,len_v3(edm->mdim->fpos));
			rotate_v3_v3v3fl(edm->mdim->dpos,v1,axis, angle_normalized_v3v3(v1,v2)*edm->mdim->dpos_fact);
			add_v3_v3(edm->mdim->dpos, edm->mdim->center);
			break;
		default:
			BLI_assert(0);
	}
}
