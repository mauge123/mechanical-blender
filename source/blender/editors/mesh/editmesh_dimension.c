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

void apply_dimension_direction_value( BMVert *va, BMVert *vb, float value){
	float vect [3];
	sub_v3_v3v3(vect,va->co,vb->co);
	normalize_v3(vect);
	mul_v3_fl(vect,value);
	add_v3_v3v3(va->co,vb->co, vect);

}
void apply_dimension_value (BMDim *edm, float value) {
	float len=get_dimension_value(edm);
	float len2=(value-len)/2;

	if(edm->dir==1){
		apply_dimension_direction_value(edm->v2,edm->v1, value);
	}else if(edm->dir==-1){
		apply_dimension_direction_value(edm->v1,edm->v2, value);
	}else{
		apply_dimension_direction_value(edm->v2,edm->v1, (len+len2));

		apply_dimension_direction_value(edm->v1,edm->v2,value);
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



