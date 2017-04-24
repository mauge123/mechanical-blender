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
 * This file is created for Mechanical Blender
 *
 * Contributor(s): Jaume Bellet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_buffer.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "mechanical_utils.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define ELE_NEW 1

void bmo_create_reference_exec(BMesh *bm, BMOperator *op)
{
	BMReference *erf;
	float mat[4][4];
	float v1[3],v2[3],v3[3],v4[3];
	float axis[3],r[3];
	float dia;
	int ref_type;
	BMOpSlot *op_verts_slot = BMO_slot_get(op->slots_in, "verts");
	BMVert *v[3];
	BMOIter siter;

	BMO_slot_mat4_get(op->slots_in,"matrix",mat);
	dia = BMO_slot_float_get(op->slots_in,"dia");
	ref_type = BMO_slot_int_get(op->slots_in,"ref_type");

	v1[0]=-dia; v1[1]=-dia; v1[2]=0;
	v2[0]=dia;v2[1]=-dia;v2[2]=0;
	v3[0]=dia; v3[1]=dia;v3[2]=0;
	v4[0]=-dia;v4[1]=dia; v4[2]=0;

	if (op_verts_slot->len == 0) {
		/* Default, no selection, added on 3D Cursor */
		mul_m4_v3(mat, v1);
		mul_m4_v3(mat, v2);
		mul_m4_v3(mat, v3);
		mul_m4_v3(mat, v4);
	} else if (op_verts_slot->len == 1) {
		// One point translate to.
		v[0] = BMO_iter_new(&siter, op->slots_in, "verts", BM_VERT);
		copy_v3_v3(mat[3],v[0]->co);

		mul_m4_v3(mat, v1);
		mul_m4_v3(mat, v2);
		mul_m4_v3(mat, v3);
		mul_m4_v3(mat, v4);
	} else if (op_verts_slot->len == 2) {
		// Two Points: align X, maintain Z
		float x[3], y[3], z[3], mid[3];
		v[0] = BMO_iter_new(&siter, op->slots_in, "verts", BM_VERT);
		v[1] = BMO_iter_step(&siter);
		qsort(v,2, sizeof (MVert*), order_select_compare);

		sub_v3_v3v3(x,v[1]->co,v[0]->co);
		madd_v3_v3v3fl(mid, v[0]->co,x,0.5);

		copy_v3_v3(y,mat[1]);
		copy_v3_v3(z,mat[2]);

		cross_v3_v3v3(y,z,x);
		normalize_v3(y);

		copy_v3_v3(mat[0],x);
		copy_v3_v3(mat[1],y);
		copy_v3_v3(mat[2],z);
		copy_v3_v3(mat[3],mid);

		mul_m4_v3(mat, v1);
		mul_m4_v3(mat, v2);
		mul_m4_v3(mat, v3);
		mul_m4_v3(mat, v4);

	} else if (op_verts_slot->len >= 3) {
		/* From selection */
		v[0] = BMO_iter_new(&siter, op->slots_in, "verts", BM_VERT);
		v[1] = BMO_iter_step(&siter);
		v[2] = BMO_iter_step(&siter);
		qsort(v,3, sizeof (MVert*), order_select_compare);

		copy_v3_v3(v1, v[0]->co);
		copy_v3_v3(v2, v[1]->co);
		copy_v3_v3(v3, v[2]->co);

		sub_v3_v3v3(axis,v2,v1);
		normalize_v3(axis);
		v_perpendicular_to_axis(r, v1, v3, axis);
		add_v3_v3v3(v3,v2,r);
		add_v3_v3v3(v4,v1,r);
	}

	BM_mesh_elem_hflag_disable_all(bm, BM_ALL_NOLOOP, BM_ELEM_SELECT,true);

	erf = BM_reference_create(bm, ref_type, v1, v2, v3, v4, NULL, NULL, 0);

	BMO_elem_flag_enable(bm, erf, ELE_NEW);
	BM_elem_flag_enable(erf, BM_ELEM_SELECT);

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "reference.out", BM_REFERENCE, ELE_NEW);

	BM_mesh_select_mode_flush(bm); // Will update counters

}


void bmo_create_reference_axis_exec(BMesh *bm, BMOperator *op)
{
	BMReference *erf;
	float mat[4][4];
	float v1[3],v2[3],v3[3],v4[3];
	float axis[3],r[3];
	float dia;
	BMOpSlot *op_verts_slot = BMO_slot_get(op->slots_in, "verts");
	BMVert *v[3];
	BMOIter siter;

	BMO_slot_mat4_get(op->slots_in,"matrix",mat);
	dia = BMO_slot_float_get(op->slots_in,"dia");

	v1[0]=-dia; v1[1]=-dia; v1[2]=0;
	v2[0]=dia;v2[1]=-dia;v2[2]=0;
	v3[0]=dia; v3[1]=dia;v3[2]=0;
	v4[0]=-dia;v4[1]=dia; v4[2]=0;

	if (op_verts_slot->len == 0) {
		/* Default, no selection, added on 3D Cursor, Z Dir */
		mul_m4_v3(mat, v1);
		mul_m4_v3(mat, v2);
		mul_m4_v3(mat, v3);
		mul_m4_v3(mat, v4);
	} else if (op_verts_slot->len == 1) {
		// One point translate to.
		v[0] = BMO_iter_new(&siter, op->slots_in, "verts", BM_VERT);
		copy_v3_v3(mat[3],v[0]->co);

		mul_m4_v3(mat, v1);
		mul_m4_v3(mat, v2);
		mul_m4_v3(mat, v3);
		mul_m4_v3(mat, v4);
	} else if (op_verts_slot->len == 2) {
		// Two Points: align X, maintain Z
		float x[3], y[3], z[3], mid[3];
		v[0] = BMO_iter_new(&siter, op->slots_in, "verts", BM_VERT);
		v[1] = BMO_iter_step(&siter);
		qsort(v,2, sizeof (MVert*), order_select_compare);

		sub_v3_v3v3(x,v[1]->co,v[0]->co);
		madd_v3_v3v3fl(mid, v[0]->co,x,0.5);

		copy_v3_v3(y,mat[1]);
		copy_v3_v3(z,mat[2]);

		cross_v3_v3v3(y,z,x);
		normalize_v3(y);

		copy_v3_v3(mat[0],x);
		copy_v3_v3(mat[1],y);
		copy_v3_v3(mat[2],z);
		copy_v3_v3(mat[3],mid);

		mul_m4_v3(mat, v1);
		mul_m4_v3(mat, v2);
		mul_m4_v3(mat, v3);
		mul_m4_v3(mat, v4);

	} else if (op_verts_slot->len >= 3) {
		/* From selection */
		v[0] = BMO_iter_new(&siter, op->slots_in, "verts", BM_VERT);
		v[1] = BMO_iter_step(&siter);
		v[2] = BMO_iter_step(&siter);
		qsort(v,3, sizeof (MVert*), order_select_compare);

		copy_v3_v3(v1, v[0]->co);
		copy_v3_v3(v2, v[1]->co);
		copy_v3_v3(v3, v[2]->co);

		sub_v3_v3v3(axis,v2,v1);
		normalize_v3(axis);
		v_perpendicular_to_axis(r, v1, v3, axis);
		add_v3_v3v3(v3,v2,r);
		add_v3_v3v3(v4,v1,r);
	}

	BM_mesh_elem_hflag_disable_all(bm, BM_ALL_NOLOOP, BM_ELEM_SELECT,true);

	erf = BM_reference_create(bm, BM_REFERENCE_TYPE_PLANE, v1, v2, v3, v4, NULL, NULL, 0);

	BMO_elem_flag_enable(bm, erf, ELE_NEW);
	BM_elem_flag_enable(erf, BM_ELEM_SELECT);

	BMO_slot_buffer_from_enabled_flag(bm, op, op->slots_out, "reference.out", BM_REFERENCE, ELE_NEW);

	BM_mesh_select_mode_flush(bm); // Will update counters

}

