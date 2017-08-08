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

/** \file blender/editors/mesh/editmesh_dimension.c
 *  \ingroup edmesh
 */

#include "DNA_object_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_library.h"
#include "BKE_editmesh.h"
#include "BKE_main.h"

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


static int mechanical_dimension_data_action (BMEditMesh *em, int action, wmOperator *op)
{

	BMOperator bmop;
	char op_str [255] = {0};

	sprintf(op_str,"dimension_action %s %s %s", "verts=%hv", "dims=%hd", "action=%i");

	if (!EDBM_op_init(em, &bmop, op, op_str, BM_ELEM_SELECT, BM_ELEM_SELECT, action)) {
		return false;
	}
	BMO_op_exec(em->bm, &bmop);

	switch (action) {
		case DIM_DATA_ACTION_SELECT:
		case DIM_DATA_ACTION_SET:
			BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "verts.out", BM_VERT, BM_ELEM_SELECT, true);
			break;
		case DIM_DATA_ACTION_RESET:
			BMO_slot_buffer_hflag_disable(em->bm, bmop.slots_out, "verts.out", BM_VERT, BM_ELEM_SELECT, true);
			break;
	}


	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return false;
	}

	return true;
}


static int mechanical_dimension_data_action_exec(bContext *C, wmOperator *op)
{
	int ret = OPERATOR_FINISHED;

	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	int action = RNA_enum_get(op->ptr, "action");

	if (em->bm->totdim) {
		switch (action) {
			case DIM_DATA_ACTION_SELECT:
			case DIM_DATA_ACTION_RESET:
			case DIM_DATA_ACTION_SET:
				if (!mechanical_dimension_data_action(em, action, op)) {
					ret = OPERATOR_CANCELLED;
				}
				break;
			default:
				BKE_report(op->reports, RPT_ERROR, "Invalid action");
				ret = OPERATOR_CANCELLED;
		}
	} else {
		BKE_report(op->reports, RPT_ERROR, "No dimension selected");
		ret = OPERATOR_CANCELLED;
	}

	/* to push this up to edges & faces. */
	EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX);

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);

	return ret;
}

static int mechanical_add_dimension_from_vertexs (int dim_type, MDim *mdm, BMEditMesh *em, wmOperator *op)
{

	BMOperator bmop;
	char op_str [255] = {0};

	sprintf(op_str,"create_dimension %s %s %s %s", "verts=%hv", "geom=%hg", "dim_type=%i", "dimension=%p");

	EDBM_op_init(em, &bmop, op, op_str, BM_ELEM_SELECT, BM_ELEM_SELECT, dim_type, mdm);

	/* deselect original verts */
	BMO_slot_buffer_hflag_disable(em->bm, bmop.slots_in, "verts", BM_VERT, BM_ELEM_SELECT, true);

	BMO_op_exec(em->bm, &bmop);

	//BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "verts.out", BM_VERT, BM_ELEM_SELECT, true);

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return false;
	}

	return true;
}

static int mechanical_add_dimension_exec(bContext *C, wmOperator *op)
{
	int ret = OPERATOR_FINISHED;

	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	int dim_type = RNA_enum_get(op->ptr, "dim_type");

	// Create Dimension Object
	Main *bmain = CTX_data_main(C);


	if (em->bm->totvertsel >= get_necessary_dimension_verts(dim_type)) {

		MDim *mdm = BKE_libblock_alloc(bmain, ID_DM, "Dimension", 0);
		mdm->ob = obedit;

		if (!mechanical_add_dimension_from_vertexs(dim_type, mdm, em, op)) {
			ret = OPERATOR_CANCELLED;
		}
	}
	else {
		BKE_report(op->reports, RPT_ERROR, "invalid selection for dimension");
		ret = OPERATOR_CANCELLED;
	}

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);

	return ret;
}

static EnumPropertyItem dim_type_items[] = {
	{DIM_TYPE_LINEAR, "linear", 0, "Linear", "Linear Dimension"},
	{DIM_TYPE_DIAMETER, "diameter", 0, "Diameter", "Diameter Dimension"},
	{DIM_TYPE_RADIUS, "radius", 0, "Radius", "Radius Dimension"},
    {DIM_TYPE_ANGLE_3P, "angle3p", 0, "Angle3P", "3 Points Angle dimension"},
    {DIM_TYPE_ANGLE_4P, "angle4p", 0, "Angle4P", "4 Points Angle dimension"},
    {0, NULL, 0, NULL, NULL}
};

static EnumPropertyItem dim_data_actions[] = {
	{DIM_DATA_ACTION_SELECT, "select", 0, "Select", "Selects dimension data"},
	{DIM_DATA_ACTION_RESET, "reset", 0, "Reset", "Resets dimension extra data"},
	{DIM_DATA_ACTION_SET, "set", 0, "Set", "Sets dimension extra data"},
    {0, NULL, 0, NULL, NULL}
};

void MESH_OT_mechanical_dimension_linear_add(wmOperatorType *ot)
{		
	/* identifiers */
	ot->name = "Add Linear Dimension";
	ot->description = "Adds a linear Dimension on Mesh";
	ot->idname = "MESH_OT_mechanical_dimension_linear_add";

	/* api callbacks */
	ot->exec = mechanical_add_dimension_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna,"dim_type",dim_type_items,DIM_TYPE_LINEAR,"dimension type","dimension type");

}


void MESH_OT_mechanical_dimension_diameter_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Diameter Dimension";
	ot->description = "Adds a Diameter Dimension on Mesh";
	ot->idname = "MESH_OT_mechanical_dimension_diameter_add";

	/* api callbacks */
	ot->exec = mechanical_add_dimension_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna,"dim_type",dim_type_items,DIM_TYPE_DIAMETER,"dimension type","dimension type");


}


void MESH_OT_mechanical_dimension_radius_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Radius Dimension";
	ot->description = "Adds a Radius Dimension on Mesh";
	ot->idname = "MESH_OT_mechanical_dimension_radius_add";

	/* api callbacks */
	ot->exec = mechanical_add_dimension_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna,"dim_type",dim_type_items,DIM_TYPE_RADIUS,"dimension type","dimension type");

}

void MESH_OT_mechanical_dimension_angle_3p_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add 3 Points Angle Dimension";
	ot->description = "Adds an angle dimension on mesh from 3 points";
	ot->idname = "MESH_OT_mechanical_dimension_angle_3p_add";

	/* api callbacks */
	ot->exec = mechanical_add_dimension_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna,"dim_type",dim_type_items,DIM_TYPE_ANGLE_3P,"dimension type","dimension type");
}


void MESH_OT_mechanical_dimension_angle_4p_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add 4 Points Angle Dimension";
	ot->description = "Adds an angle dimension on mesh from 4 points";
	ot->idname = "MESH_OT_mechanical_dimension_angle_4p_add";

	/* api callbacks */
	ot->exec = mechanical_add_dimension_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna,"dim_type",dim_type_items,DIM_TYPE_ANGLE_4P,"dimension type","dimension type");
}

void MESH_OT_mechanical_dimension_data_select(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select dimension related data";
	ot->description = "Selects dimension related data, (vertexs)";
	ot->idname = "MESH_OT_mechanical_dimension_data_select";

	/* api callbacks */
	ot->exec = mechanical_dimension_data_action_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna,"action",dim_data_actions,DIM_DATA_ACTION_SELECT,"dimension data action","dimension data action");
}


void MESH_OT_mechanical_dimension_data_reset(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reset dimension related data";
	ot->description = "Remove extra vertices set on dimension";
	ot->idname = "MESH_OT_mechanical_dimension_data_reset";

	/* api callbacks */
	ot->exec = mechanical_dimension_data_action_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna,"action",dim_data_actions,DIM_DATA_ACTION_RESET,"dimension data action","dimension data action");
}


void MESH_OT_mechanical_dimension_data_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Set dimension related data";
	ot->description = "Replace dimension related data, (vertexs)";
	ot->idname = "MESH_OT_mechanical_dimension_data_set";

	/* api callbacks */
	ot->exec = mechanical_dimension_data_action_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna,"action",dim_data_actions,DIM_DATA_ACTION_SET,"dimension data action","dimension data action");
}








