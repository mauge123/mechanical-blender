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
#include "DNA_mesh_types.h"
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

#include "mesh_dimensions.h"

static int mechanical_add_dimension_from_vertexs (const char* func_name, BMEditMesh *em, wmOperator *op) {

	BMOperator bmop;
	char op_str [255] = {0};

	sprintf(op_str,"%s %s", func_name, "verts=%hv");

	EDBM_op_init(em, &bmop, op, op_str, BM_ELEM_SELECT);

	/* deselect original verts */
	BMO_slot_buffer_hflag_disable(em->bm, bmop.slots_in, "verts", BM_VERT, BM_ELEM_SELECT, true);

	BMO_op_exec(em->bm, &bmop);

	//BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "verts.out", BM_VERT, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return false;
	}

	return true;
}



static int mechanical_add_dimension_diameter(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	if (em->bm->totvertsel >= 3) {
		return mechanical_add_dimension_from_vertexs("create_dimension_diameter", em, op);
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "invalid selection for dimension");
	}

	return OPERATOR_FINISHED;
}



static int mechanical_add_dimension_linear(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);


	if (em->bm->totvertsel >= 2) {
		mechanical_add_dimension_from_vertexs("create_dimension_linear",em, op);
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "invalid selection for dimension");
	}

	return OPERATOR_FINISHED;
}



void MESH_OT_mechanical_dimension_linear_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Linear Dimension";
	ot->description = "Adds a linear Dimension on Mesh";
	ot->idname = "MESH_OT_mechanical_dimension_linear_add";

	/* api callbacks */
	ot->exec = mechanical_add_dimension_linear;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

}


void MESH_OT_mechanical_dimension_diameter_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Diameter Dimension";
	ot->description = "Adds a Diameter Dimension on Mesh";
	ot->idname = "MESH_OT_mechanical_dimension_diameter_add";

	/* api callbacks */
	ot->exec = mechanical_add_dimension_diameter;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

}



