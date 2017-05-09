
#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_listbase.h"

#include "DNA_mesh_types.h"

#include "ED_mesh.h"

#include "BKE_editmesh.h"

#include "UI_resources.h"

#include "bmesh.h"

#include "mesh_dimensions.h"
#include "mesh_references.h"
#include "mechanical_utils.h"
#include "prec_math.h"

#include "MEM_guardedalloc.h"


static void set_dimension_start_end(BMesh *bm, BMDim *edm, Scene *scene);

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
	}
	return ret;
}

//Return dimension between two vertex
float get_dimension_value(BMDim *edm){
	float v=0;
	switch (edm->mdim->dim_type) {
		case DIM_TYPE_LINEAR:
			//v = len_v3v3(edm->v[0]->co, edm->v[1]->co);
			v = len_v3v3(edm->mdim->start, edm->mdim->end);
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
static void apply_dimension_linear_value_exec( float *a, float *b, float value, float *r_res, const float dir[]){
	float proj[3], vect[3];
	float r_value;

	sub_v3_v3v3(vect,a,b);

	project_v3_v3v3(proj,vect,dir);
	r_value = value - len_v3(proj);

	copy_v3_v3(r_res, dir);
	mul_v3_fl(r_res,r_value);

	add_v3_v3v3(a,a, r_res);
}


static void apply_dimension_linear_value(BMesh *bm, BMDim *edm, float value, int constraints) {

	float v[3], n[3];

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
		sub_v3_v3v3(d_dir,edm->mdim->end,edm->mdim->start);
		normalize_v3(d_dir);
		tag_vertexs_on_coplanar_faces(bm, p, d_dir);
		tag_vertexs_on_plane(bm, p , d_dir);
	}
	// Untag dimensions vertex
	untag_dimension_necessary_verts(edm);

		sub_v3_v3v3(n, edm->mdim->end, edm->mdim->start);
		normalize_v3(n);

	// Update Dimension Verts
	if(edm->mdim->dir == DIM_DIR_RIGHT){
		apply_dimension_linear_value_exec(edm->v[1]->co,edm->v[0]->co, value, v, n);
	}else if(edm->mdim->dir== DIM_DIR_LEFT){
		apply_dimension_linear_value_exec(edm->v[0]->co,edm->v[1]->co, value, v, n);
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
                                       float *axis, float value, int constraints) {
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

static void apply_dimension_angle(BMesh *bm, BMDim *edm, float value, int constraints) {
	float axis[3], ncenter[3], v[3], pp[3];
	float d; //Difential to move

	float p[3], r[3];
	float d_dir[3]; // Dimension dir

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

	if (constraints & DIM_PLANE_CONSTRAINT) {
		tag_vertexs_on_coplanar_faces(bm, p, d_dir);
		tag_vertexs_on_plane(bm, p, d_dir);
	}

	// Untag dimensions vertex
	untag_dimension_necessary_verts(edm);

	if (edm->mdim->dir == DIM_DIR_RIGHT) {
		apply_dimension_angle_exec(bm,edm->v[2],edm->mdim->center, axis,d,constraints);
		if (edm->mdim->dim_type == DIM_TYPE_ANGLE_4P){
			apply_dimension_angle_exec(bm, edm->v[3],edm->mdim->center, axis,d,constraints);
		}
	}
	if (edm->mdim->dir == DIM_DIR_LEFT) {
		d = d*(-1);
		apply_dimension_angle_exec(bm, edm->v[0],edm->mdim->center, axis, d,constraints);
		if (edm->mdim->dim_type == DIM_TYPE_ANGLE_4P){
			apply_dimension_angle_exec(bm, edm->v[1],edm->mdim->center, axis,d, constraints);
		}
	}

	// To get correct dimension value in case of nexts steps (BOTH SIDES)
	set_dimension_start_end(bm, edm, NULL);

	//Rotate tagged points against axis
	BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_TAG)) {

			sub_v3_v3v3(v,eve->co, edm->mdim->center);

			project_v3_v3v3(ncenter,v, axis);
			add_v3_v3(ncenter,edm->mdim->center);

			apply_dimension_angle_exec(bm, eve,ncenter, axis, d, constraints);

			// Reset tag
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}
	}
}

/**
 * @brief apply_dimension_concentric_constraint
 * @param bm
 * @param edm
 * @param v_in
 * @see http://www.mechanicalblender.org/modules/static/doc/apply_dimension_concentric_constraint.svg
 */
static void apply_dimension_concentric_constraint (BMesh *bm, BMDim *edm, float *v_in){
	float constraint_axis[3], r_original[3], r_modified[3];
	float rad[3], r_rad[3];
	float c_original[3], c_modified[3];
	float r1, r2;
	float n[3];
	int i =0;
	BMVert *eve, *eve1;
	BMIter iter, iter1;
	BMReference *erf = BM_reference_at_index_find(bm, edm->mdim->axis-1);
	BLI_assert(erf->type == BM_REFERENCE_TYPE_AXIS);
	sub_v3_v3v3(constraint_axis,erf->v1, erf->v2);
	normalize_v3(constraint_axis);

	tag_vertexs_affected_by_dimension(bm,edm);

	BM_ITER_MESH_INDEX (eve1, &iter1, bm, BM_VERTS_OF_MESH, i) {
		if (!BM_elem_flag_test_bool(eve1, BM_ELEM_TAG)) {
			continue;
		}
		if (!equals_v3v3(&v_in[i*3],eve1->co)) {
			// Changed apply changes on concetric
			v_perpendicular_to_axis (r_original, &v_in[i*3], erf->v1, constraint_axis);
			add_v3_v3v3(c_original, &v_in[i*3], r_original);
			normal_tri_v3(n,erf->v1, erf->v2, &v_in[i*3]);
			if (point_on_plane_prec(erf->v1, n, eve1->co)) {
				v_perpendicular_to_axis (r_modified, eve1->co, erf->v1, constraint_axis);
				add_v3_v3v3(c_modified,eve1->co, r_modified);
				r1 = len_v3(r_original);
				r2 = len_v3(r_modified);

				BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
					if (!BM_elem_flag_test_bool(eve, BM_ELEM_TAG)) {
						if (point_on_plane_prec(eve->co,constraint_axis,c_original)) {
							// OK
							v_perpendicular_to_axis (rad, eve->co, erf->v1, constraint_axis);
							if (eq_ff_prec(len_v3(rad), r1)) {
								copy_v3_v3(r_rad,rad);
								normalize_v3_length(r_rad,-r2);
								add_v3_v3v3(eve->co, c_modified, r_rad);
							}
						}
					}
				}
			}
		}

	}

	BM_ITER_MESH(eve, &iter, bm, BM_VERTS_OF_MESH) {
		BM_elem_flag_disable(eve, BM_ELEM_TAG);
	}
}

void apply_dimension_value (BMesh *bm, BMDim *edm, float value, ToolSettings *ts) {

	int constraints = (edm->mdim->constraints & DIM_CONSTRAINT_OVERRIDE) ? edm->mdim->constraints : ts->dimension_constraints;
	float *v_in = NULL;
	int i =0;
	BMVert *eve;
	BMIter iter;

	if (edm->mdim->axis > 0) {
		v_in = MEM_callocN(sizeof(float)*3*bm->totvert, "Original Coordinates");
		BM_ITER_MESH_INDEX (eve, &iter, bm, BM_VERTS_OF_MESH, i) {
			copy_v3_v3(&v_in[i*3],eve->co);
		}
	}


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

	if (edm->mdim->axis > 0) {
		apply_dimension_concentric_constraint (bm, edm, v_in);
		MEM_freeN (v_in);
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

static void set_dimension_start_end(BMesh *UNUSED(bm), BMDim *edm, Scene *scene) {

	switch (edm->mdim->dim_type) {
		case DIM_TYPE_LINEAR:
			add_v3_v3v3(edm->mdim->start,edm->mdim->fpos, edm->v[0]->co);
			add_v3_v3v3(edm->mdim->end, edm->mdim->fpos, edm->v[1]->co);

			if (scene && edm->mdim->dimension_flag & DIMENSION_FLAG_TS_ALIGNED) {
				float normal[3], perp[3], pos[3];
				get_dimension_transform_orientation_plane(normal, edm, scene);

				cross_v3_v3v3(perp, normal, edm->mdim->fpos);
				normalize_v3(perp);

				sub_v3_v3v3(pos,edm->mdim->end, edm->mdim->start);
				project_v3_v3v3(pos,pos,perp);
				add_v3_v3v3(edm->mdim->end, edm->mdim->start, pos);
			}

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

void dimension_data_update (BMesh *bm, BMDim *edm, Scene *scene) {
	float axis[3], v1[3],v2[3], l;

	edm->mdim->value_pr = edm->mdim->value;

	switch (edm->mdim->dim_type) {
		case DIM_TYPE_LINEAR:

			if (scene) {
				if (edm->mdim->dimension_flag & DIMENSION_FLAG_TS_ALIGNED) {
					float n[3], zero[3]={0}, mat[3][3];
					float x[3],y[3],z[3];
					float lx, ly, lz;

					get_dimension_transform_orientation_plane(n,edm,scene);
					project_v3_plane(edm->mdim->fpos,n,zero);

					//set on axis
					get_dimension_transform_orientation_matrix (mat, edm, scene);

					project_v3_v3v3(x,edm->mdim->fpos,mat[0]);
					project_v3_v3v3(y,edm->mdim->fpos,mat[1]);
					project_v3_v3v3(z,edm->mdim->fpos,mat[2]);

					lx = len_squared_v3(x);
					ly = len_squared_v3(y);
					lz = len_squared_v3(z);

					if (lx > ly && lx > lz) {
						copy_v3_v3(edm->mdim->fpos, x);
					} else if (ly > lx && ly > lz) {
						copy_v3_v3(edm->mdim->fpos, y);
					} else if (lz > lx && lz > ly) {
						copy_v3_v3(edm->mdim->fpos, z);
					}

				} else {
					// Set dimension always perpendicular to edge
					l = len_v3(edm->mdim->fpos);
					sub_v3_v3v3(axis, edm->v[0]->co, edm->v[1]->co);
					normalize_v3(axis);
					project_v3_v3v3(v1,edm->mdim->fpos,axis);
					sub_v3_v3(edm->mdim->fpos,v1);
					normalize_v3(edm->mdim->fpos);
					mul_v3_fl (edm->mdim->fpos, l);
				}
			}

			// Baseline
			set_dimension_start_end(bm, edm, scene);


			// Set txt pos acording pos factor
			sub_v3_v3v3(edm->mdim->dpos, edm->mdim->end, edm->mdim->start);
			mul_v3_fl(edm->mdim->dpos, edm->mdim->dpos_fact);
			add_v3_v3(edm->mdim->dpos, edm->mdim->start);

			break;
		case DIM_TYPE_DIAMETER:
			set_dimension_center(edm);
			ensure_fpos_on_plane(edm);
			set_dimension_start_end(bm, edm, scene);


			// Set txt pos acording pos factor
			sub_v3_v3v3(edm->mdim->dpos, edm->mdim->end, edm->mdim->start);
			mul_v3_fl(edm->mdim->dpos, edm->mdim->dpos_fact);
			add_v3_v3(edm->mdim->dpos, edm->mdim->start);
			break;
		case DIM_TYPE_RADIUS:
			set_dimension_center(edm);
			ensure_fpos_on_plane(edm);

			set_dimension_start_end(bm, edm, scene); // First

			// Set txt pos acording pos factor
			sub_v3_v3v3(edm->mdim->dpos, edm->mdim->end, edm->mdim->center);
			mul_v3_fl(edm->mdim->dpos, edm->mdim->dpos_fact);
			add_v3_v3(edm->mdim->dpos, edm->mdim->start);
			break;
		case DIM_TYPE_ANGLE_3P:
		case DIM_TYPE_ANGLE_4P:
			ensure_fpos_on_plane(edm);

			set_dimension_center(edm);  // First
			set_dimension_start_end(bm, edm, scene);

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

void get_dimension_transform_orientation_matrix (float mat[][3], BMDim *edm, Scene *scene) {
	if (edm->mdim->ts ==  0) {
		// Global
		copy_m3_m4(mat,edm->mdim->ob->imat);

	} else if (edm->mdim->ts == 1) {
		// Object
		unit_m3(mat);
	} else {
		TransformOrientation *ts = BLI_findlink(&scene->transform_spaces, edm->mdim->ts-2);
		copy_m3_m3(mat, ts->mat);
	}
}

void get_dimension_transform_orientation_plane (float normal[3], BMDim *edm, Scene *scene) {
	float mat[3][3];

	get_dimension_transform_orientation_matrix(mat, edm, scene);

	switch (edm->mdim->ts_plane) {
		case DIM_TS_PLANE_X:
			copy_v3_v3(normal,mat[0]);
			break;
		case DIM_TS_PLANE_Y:
			copy_v3_v3(normal,mat[1]);
			break;
		case DIM_TS_PLANE_Z:
			copy_v3_v3(normal,mat[2]);
			break;
	}
}
