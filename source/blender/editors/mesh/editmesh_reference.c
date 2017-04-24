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
 * WITH_MECHANICAL_MESH_REFERENCE_OBJECTS
 *
 * Contributor(s): Jaume Bellet
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_reference.c
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

#include "../editors/transform/transform.h"

#include "mesh_references.h"


static EnumPropertyItem reference_type_items[] = {
	{BM_REFERENCE_TYPE_PLANE, "plane", 0, "Plane", "Plane Reference"},
	{BM_REFERENCE_TYPE_AXIS, "axis", 0, "Axis", "Axis Reference"},
    {0, NULL, 0, NULL, NULL}
};

static int mechanical_add_reference_exec(bContext *C, wmOperator *op)
{

	float loc[3], rot[3], mat[4][4], dia;
	bool enter_editmode;
	unsigned int layer;

	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	BMReference *erf = NULL;

	int reference_type = RNA_enum_get(op->ptr, "reference_type");

	BMOperator bmop;
	char op_str [255] = {0};

	WM_operator_view3d_unit_defaults(C, op);
	ED_object_add_generic_get_opts(C, op, 'Z', loc, rot, &enter_editmode, &layer, NULL);
	dia = ED_object_new_primitive_matrix(C, obedit, loc, rot, mat);
	bool create_ts = RNA_boolean_get(op->ptr,"create_ts");

	sprintf(op_str,"create_reference_plane %s %s %s %s", "verts=%hv", "matrix=%m4", "dia=%f", "ref_type=%i");

	EDBM_op_init(em, &bmop, op, op_str, BM_ELEM_SELECT, mat, dia, reference_type);

	BMO_op_exec(em->bm, &bmop);

	erf = BMO_slot_buffer_get_first(bmop.slots_out, "reference.out");

	if (erf && create_ts) {
		float pmat[3][3];
		float origin[3];
		reference_plane_matrix(erf,pmat);
		reference_plane_origin(erf, origin);
		addMatrixSpace(C, pmat, origin, erf->name, true);
	}

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return OPERATOR_CANCELLED;
	}

	WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);

	return OPERATOR_FINISHED;

}


void MESH_OT_mechanical_reference_plane_add(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Add Reference Plane";
	ot->description = "Adds a Reference plane to be used on Mesh";
	ot->idname = "MESH_OT_mechanical_reference_plane_add";

	/* api callbacks */
	ot->exec = mechanical_add_reference_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_enum(ot->srna,"reference_type",reference_type_items,BM_REFERENCE_TYPE_PLANE,"reference type","reference type");

	prop = RNA_def_boolean(ot->srna,"create_ts",false,"Create transform orientation","creates a tranform orientation");

	ED_object_add_generic_props(ot,false);
}


void MESH_OT_mechanical_reference_axis_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Reference Axis";
	ot->description = "Adds a Reference Axis to be used on Mesh";
	ot->idname = "MESH_OT_mechanical_reference_axis_add";

	/* api callbacks */
	ot->exec = mechanical_add_reference_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	ot->prop = RNA_def_boolean(ot->srna,"create_ts",false,"Create transform orientation","creates a tranform orientation");

	ED_object_add_generic_props(ot,false);
}
