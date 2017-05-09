/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Jaume Bellet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_extrude.c
 *  \ingroup bmesh
 *
 * Extrude faces and solidify.
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_buffer.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "mesh_dimensions.h"

#include "intern/bmesh_operators_private.h" /* own include */

enum {
	EXT_INPUT   = 1,
	EXT_KEEP    = 2,
	EXT_DEL     = 4,
	EXT_OUT		= 8
};


/*
 * Create Dimension operators
 *
 */

static BMVert** create_dimension_from_geometry_4p_angle(BMesh *UNUSED(bm), BMOperator *op){
	BMOIter siter;
	BMVert *(*v_arr) = NULL;
	BMGeom *egm, *egm2;

	egm = BMO_iter_new(&siter, op->slots_in, "geom", BM_GEOMETRY);
	egm2 = BMO_iter_step(&siter);
	if (egm && egm2 &&
	    egm->geometry_type == BM_GEOMETRY_TYPE_LINE &&
	    egm2->geometry_type == BM_GEOMETRY_TYPE_LINE)
	{
		v_arr = MEM_mallocN(sizeof (BMVert*)*4, "BMVert temp array");
		v_arr[0] = egm->v[0];
		v_arr[1] = egm->v[egm->totverts-1];
		v_arr[2] = egm2->v[0];
		v_arr[3] = egm2->v[egm2->totverts-1];
	}
	return v_arr;

}

static BMVert** create_dimension_from_geometry_3p_angle(BMesh *UNUSED(bm), BMOperator *op){

	BMOIter siter;
	BMVert *(*v_arr) = NULL;
	BMGeom *egm, *egm2;

	egm = BMO_iter_new(&siter, op->slots_in, "geom", BM_GEOMETRY);
	egm2 = BMO_iter_step(&siter);
	if (egm && egm2 &&
	    egm->geometry_type == BM_GEOMETRY_TYPE_LINE &&
	    egm2->geometry_type == BM_GEOMETRY_TYPE_LINE)
	{
		BMVert *v1_1 = egm->v[0];
		BMVert *v1_2 = egm->v[egm->totverts-1];
		BMVert *v2_1 = egm2->v[0];
		BMVert *v2_2 = egm2->v[egm2->totverts-1];
		if (v1_1 == v2_1) {
			v_arr = MEM_mallocN(sizeof (BMVert*)*3, "BMVert temp array");
			v_arr[0] = v1_2;
			v_arr[1] = v1_1;
			v_arr[2] = v2_2;
		} else if (v1_1 == v2_2) {
			v_arr = MEM_mallocN(sizeof (BMVert*)*3, "BMVert temp array");
			v_arr[0] = v1_2;
			v_arr[1] = v1_1;
			v_arr[2] = v2_1;
		}else if (v1_2 == v2_1) {
			v_arr = MEM_mallocN(sizeof (BMVert*)*3, "BMVert temp array");
			v_arr[0] = v1_1;
			v_arr[1] = v1_2;
			v_arr[2] = v2_2;
		}
		else if (v1_2 == v2_2) {
			v_arr = MEM_mallocN(sizeof (BMVert*)*3, "BMVert temp array");
			v_arr[0] = v1_1;
			v_arr[1] = v1_2;
			v_arr[2] = v2_1;
		}
	}
	return v_arr;

}


void bmo_create_dimension_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMVert *v;
	BMIter iter;
	BMGeom *egm;
	BMVert *(*v_arr) = NULL;
	BMDim *d;
	BMOpSlot *op_verts_slot = BMO_slot_get(op->slots_in, "verts");
	BMOpSlot *op_geom_slot = BMO_slot_get(op->slots_in, "geom");
	MDim *mdm = BMO_slot_ptr_get(op->slots_in, "dimension");
	int n=0;
	int type = mdm->dim_type = BMO_slot_int_get(op->slots_in,"dim_type");

// WITH_MECHANICAL_GEOMETRY
	if (op_geom_slot->len > 0) {
		// Dimension from Geometry
		if (type == DIM_TYPE_LINEAR) {
			for (egm = BMO_iter_new(&siter, op->slots_in, "geom", BM_GEOMETRY); egm; egm = BMO_iter_step(&siter)) {
				if (egm->geometry_type == BM_GEOMETRY_TYPE_LINE)
				{
					// Only two vertices
					v_arr = MEM_mallocN(sizeof (BMVert*)*2, "BMVert temp array");
					v_arr[0] = egm->v[0];
					v_arr[1] = egm->v[egm->totverts-1];
					d = BM_dim_create(bm, v_arr, 2, type, NULL, BM_CREATE_SET_DEFAULT_DATA, mdm);
					BMO_dim_flag_enable(bm, d, EXT_KEEP);
					MEM_freeN (v_arr);
				}
			}
		}
		else if (ELEM(type, DIM_TYPE_DIAMETER, DIM_TYPE_RADIUS)) {
			for (egm = BMO_iter_new(&siter, op->slots_in, "geom", BM_GEOMETRY); egm; egm = BMO_iter_step(&siter)) {
				if (ELEM(egm->geometry_type, BM_GEOMETRY_TYPE_ARC, BM_GEOMETRY_TYPE_CIRCLE)) {
					d = BM_dim_create(bm, egm->v, egm->totverts, type, NULL, BM_CREATE_SET_DEFAULT_DATA, mdm);
					BMO_dim_flag_enable(bm, d, EXT_KEEP);
				}
			}
		}else if (type == DIM_TYPE_ANGLE_3P) {
			v_arr = create_dimension_from_geometry_3p_angle (bm, op);
			if (v_arr) {
				d = BM_dim_create(bm, v_arr, 3, type, NULL, BM_CREATE_SET_DEFAULT_DATA, mdm);
				BMO_dim_flag_enable(bm, d, EXT_KEEP);
				MEM_freeN (v_arr);
			}
		}
		else if (type == DIM_TYPE_ANGLE_4P) {
			v_arr = create_dimension_from_geometry_4p_angle (bm, op);
			if (v_arr) {
				d = BM_dim_create(bm, v_arr, 4, type, NULL, BM_CREATE_SET_DEFAULT_DATA, mdm);
				BMO_dim_flag_enable(bm, d, EXT_KEEP);
				MEM_freeN (v_arr);
			}
		}
	} else {
		// Dimension from Vertices
		v_arr = MEM_mallocN(sizeof (BMVert*)*op_verts_slot->len, "BMVert temp array");
		for (v = BMO_iter_new(&siter, op->slots_in, "verts", BM_VERT); v; v = BMO_iter_step(&siter), n++) {
			v_arr[n] = v;
		}
		d = BM_dim_create(bm, v_arr,op_verts_slot->len, type, NULL, BM_CREATE_USE_SELECT_ORDER | BM_CREATE_SET_DEFAULT_DATA, mdm);
		BMO_dim_flag_enable(bm, (BMDim *)d, EXT_KEEP);
		MEM_freeN (v_arr);
	}

	mdm->value = get_dimension_value(d);
	dimension_data_update(bm, d, NULL);

	BM_elem_flag_enable(d, BM_ELEM_SELECT);

	BM_ITER_MESH(d, &iter, bm, BM_DIMS_OF_MESH) {
		if (BMO_dim_flag_test(bm,d, EXT_KEEP)) {
			BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "dim.out", BM_DIM, EXT_KEEP);
			BMO_dim_flag_disable(bm, d, EXT_KEEP);
		}
	}
}

/*
 * Dimension data operators
 *
 */

static void dimension_data_select(BMesh *bm, BMOperator *op) {
	BMOIter siter;
	BMDim *edm;

	edm = BMO_iter_new(&siter, op->slots_in, "dims", BM_DIM);
	for (; edm; edm = BMO_iter_step(&siter)) {
		for (int i=0; i< edm->totverts; i++) {
			// Just flag, select occurs later
			BMO_vert_flag_enable(bm, edm->v[i], EXT_OUT);
		}
	}
}

static void dimension_data_reset(BMesh *bm, BMOperator *op) {
	BMOIter siter;
	BMDim *edm;
	BMVert **vv;

	edm = BMO_iter_new(&siter, op->slots_in, "dims", BM_DIM);
	for (; edm; edm = BMO_iter_step(&siter)) {
		int count = get_necessary_dimension_verts(edm->mdim->dim_type);
		vv = MEM_callocN(sizeof (BMVert*)*count, "Replaced mdim vertices array");
		for (int i= 0; i< edm->totverts; i++) {
			if (i<count) {
				vv[i] = edm->v[i];
			} else {
				// Flag to unselect
				BMO_vert_flag_enable(bm, edm->v[i], EXT_OUT);
			}
		}
		MEM_freeN(edm->v);
		edm->v = vv;
		edm->totverts = count;
	}
}

static void dimension_data_set(BMesh *bm, BMOperator *op) {
	BMOIter diter, viter;
	BMDim *edm;
	BMVert **vv;
	BMVert *v;
	int i =0;

	edm = BMO_iter_new(&diter, op->slots_in, "dims", BM_DIM);
	for (; edm; edm = BMO_iter_step(&diter)) {
		int necessary = get_necessary_dimension_verts(edm->mdim->dim_type);
		int total = necessary;

		v = BMO_iter_new(&viter, op->slots_in, "verts", BM_VERT);
		for (; v; v = BMO_iter_step(&viter)) {
			// do not add already set as necessary verts
			for (i=0;i<necessary && (v != edm->v[i]) ;i++);

			if (i == necessary) {
				BMO_vert_flag_enable(bm, v, EXT_KEEP);
				total++;
			}
		}
		vv = MEM_callocN(sizeof (BMVert*)* total, "Replaced mdim vertices array");

		//set the necessary ones
		for (i= 0; i< necessary ; i++) {
			vv[i] = edm->v[i];
			BMO_vert_flag_enable(bm, vv[i], EXT_OUT);
		}

		//set the new ones
		v = BMO_iter_new(&viter, op->slots_in, "verts", BM_VERT);
		for (; v; v = BMO_iter_step(&viter)) {
			if (BMO_vert_flag_test(bm, v, EXT_KEEP)) {
				BMO_vert_flag_disable (bm, v, EXT_KEEP);
				BMO_vert_flag_enable(bm, v, EXT_OUT);
				vv[i] = v;
				i++;
			} else {
				//was a necessary vert
			}

		}

		MEM_freeN(edm->v);
		edm->v = vv;
		edm->totverts = total;
	}
}

void bmo_dimension_data_exec(BMesh *bm, BMOperator *op)
{
	BMVert *v;
	BMIter iter;

 	switch (BMO_slot_int_get(op->slots_in,"action")) {
		case DIM_DATA_ACTION_SELECT:
			dimension_data_select(bm,op);
			break;
		case DIM_DATA_ACTION_RESET:
			dimension_data_reset(bm,op);
			break;
		case DIM_DATA_ACTION_SET:
			dimension_data_set(bm,op);
			break;
	}

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "verts.out", BM_VERT, EXT_OUT);

	BM_ITER_MESH(v, &iter, bm, BM_VERTS_OF_MESH) {
		if (BMO_vert_flag_test(bm,v, EXT_OUT)) {
			BMO_vert_flag_disable(bm, v, EXT_OUT);
		}
	}


}
