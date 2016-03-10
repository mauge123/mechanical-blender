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
 * The Original Code is Copyright (C) 2004 by Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_dimension.c
 *  \ingroup edmesh
 */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_editmesh.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_object.h"
#include "BKE_report.h"

#include "mesh_intern.h"  /* own include */

#include "BLI_string.h"

#include "ED_dimensions.h"

static int mechanical_add_dimension_between_two_vertex (BMEditMesh *em, wmOperator *op) {

	BMOperator bmop;

	EDBM_op_init(em, &bmop, op,"create_dimension verts=%hv", BM_ELEM_SELECT);

	/* deselect original verts */
	BMO_slot_buffer_hflag_disable(em->bm, bmop.slots_in, "verts", BM_VERT, BM_ELEM_SELECT, true);

	BMO_op_exec(em->bm, &bmop);

	//BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "verts.out", BM_VERT, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return false;
	}

	return true;
}


static int mechanical_add_dimension(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	if (em->bm->totedge == 1) {

	}
	else if (em->bm->totvertsel == 2) {
		mechanical_add_dimension_between_two_vertex (em, op);
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "invalid selection for dimension");
	}

	return OPERATOR_FINISHED;
}



void MESH_OT_mechanical_dimension_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Dimension";
	ot->description = "Adds a Dimension on Mesh";
	ot->idname = "MESH_OT_mechanical_dimension_add";

	/* api callbacks */
	ot->exec = mechanical_add_dimension;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

}

//Return dimension between two vertex
float get_dimension_value(BMDim *edm){
	return(len_v3v3(edm->v1->co, edm->v2->co));
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

void apply_dimension_value (BMesh *bm, BMDim *edm, float value, int constraints) {
	float v[3];
	BMIter iter;
	BMVert *eve;

	if (edm->dir == 0){
		// Both directions
		float len=get_dimension_value(edm);
		float len2=(value-len)/2;
		edm->dir = 1;
		apply_dimension_value (bm, edm,(len+len2), constraints);
		edm->dir = -1;
		apply_dimension_value (bm, edm,value,constraints);
		edm->dir = 0;
		return;
	}

	// Tag all elements to be affected by change
	BM_ITER_MESH (eve, &iter, bm, BM_VERTS_OF_MESH) {
		if (BM_elem_flag_test(eve, BM_ELEM_SELECT)) {
			BM_elem_flag_enable(eve, BM_ELEM_TAG);
		} else {
			// Disable as default
			BM_elem_flag_disable(eve, BM_ELEM_TAG);
		}
	}


	if (constraints & DIM_PLANE_CONSTRAINT) {
		BMFace *f;
		BMIter viter;
		float vec[3];
		float d_dir[3], p[3];

		sub_v3_v3v3(d_dir,edm->v2->co,edm->v1->co);
		normalize_v3(d_dir);
		BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
			float d = dot_v3v3(d_dir,f->no);
			if (fabs(d*d -1) < DIM_CONSTRAINT_PRECISION) {
				// Coplanar?
				BM_ITER_ELEM (eve, &viter, f, BM_VERTS_OF_FACE) {
					if (edm->v1 ==  eve || edm->v2 == eve) {
						continue;
					}
					if (edm->dir == -1) {
						sub_v3_v3v3(vec, edm->v1->co, eve->co);
					} else if (edm->dir == 1) {
						sub_v3_v3v3(vec, edm->v2->co, eve->co);
					}
					normalize_v3(vec);
					break;
				}
				project_v3_v3v3(p,vec,d_dir);
				if (len_squared_v3(p) < DIM_CONSTRAINT_PRECISION) {
					BM_ITER_ELEM (eve, &viter, f, BM_VERTS_OF_FACE) {
						BM_elem_flag_enable(eve, BM_ELEM_TAG);
					}
				}
			}
		}

	}
	// Untag dimensions vertex
	BM_elem_flag_disable(edm->v1, BM_ELEM_TAG);
	BM_elem_flag_disable(edm->v2, BM_ELEM_TAG);


	// Update Dimension Verts
	if(edm->dir==1){
		apply_dimension_direction_value(edm->v2,edm->v1, value, v);
	}else if(edm->dir==-1){
		apply_dimension_direction_value(edm->v1,edm->v2, value,v);
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

	sub_v3_v3v3(mid, edm->v2->co, edm->v1->co);
	mul_v3_fl(mid,0.5);
	add_v3_v3(mid, edm->v1->co);


}



