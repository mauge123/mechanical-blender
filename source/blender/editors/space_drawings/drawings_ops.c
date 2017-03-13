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
 *
 * Contributor(s): jaume bellet <developer@tmaq.es>
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_drawings/space_drawings.c
 *  \ingroup spdrawings
 */




#include <string.h>
#include <errno.h>

#include "MEM_guardedalloc.h"

#include "DNA_drawing_types.h"

#include "BLI_blenlib.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_report.h"
#include "BKE_drawing.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "UI_interface.h"
#include "UI_resources.h"

#include "RNA_access.h"
#include "RNA_define.h"

#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif


#include "drawings_intern.h"  /* own include */


/******************* new operator *********************/

static int drawing_new_exec(bContext *C, wmOperator *UNUSED(op))
{

	SpaceDrawings *sdwg = CTX_wm_space_drawings(C);
	Main *bmain = CTX_data_main(C);
	Drawing *dwg;
	PointerRNA ptr, idptr;
	PropertyRNA *prop;

	dwg = BKE_drawing_add(bmain, "Drawing");

	/* hook into UI */
	UI_context_active_but_prop_get_templateID(C, &ptr, &prop);

	if (prop) {
		RNA_id_pointer_create(&dwg->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}
	else if (sdwg) {
		sdwg->drawing = dwg;
		//text_drawcache_tag_update(st, 1);
	}

	//WM_event_add_notifier(C, NC_TEXT | NA_ADDED, text);

	return OPERATOR_FINISHED;
}

void DRAWINGS_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Create Drawing";
	ot->idname = "DRAWINGS_OT_new";
	ot->description = "Create a new drawing sheet";

	/* api callbacks */
	ot->exec = drawing_new_exec;
	//ot->poll = text_new_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;
}


static int drawing_unlink_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	SpaceDrawings *sdwg = CTX_wm_space_drawings(C);
	Drawing *dwg = CTX_data_edit_drawing(C);

	/* make the previous text active, if its not there make the next text active */
	if (sdwg) {
		if (dwg->id.prev) {
			sdwg->drawing = dwg->id.prev;
			//text_update_cursor_moved(C);
			//WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);
		}
		else if (dwg->id.next) {
			sdwg->drawing = dwg->id.next;
			//text_update_cursor_moved(C);
			//WM_event_add_notifier(C, NC_TEXT | ND_CURSOR, st->text);
		} else {
			sdwg->drawing = NULL;
		}
	}

	BKE_libblock_delete(bmain, dwg);

	//text_drawcache_tag_update(st, 1);
	//WM_event_add_notifier(C, NC_TEXT | NA_REMOVED, NULL);

	return OPERATOR_FINISHED;
}


void DRAWINGS_OT_unlink(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Unlink";
	ot->idname = "DRAWINGS_OT_unlink";
	ot->description = "Unlink active drawing data-block";

	/* api callbacks */
	ot->exec = drawing_unlink_exec;
	ot->invoke = WM_operator_confirm;
	//ot->poll = text_unlink_poll;

	/* flags */
	ot->flag = OPTYPE_UNDO;
}
