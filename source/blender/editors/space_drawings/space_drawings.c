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

/** \file blender/editors/space_view3d/space_drawings.c
 *  \ingroup spdrawings
 */


#include <string.h>
#include <stdio.h>

#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_drawing_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_icons.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_screen.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"


#include "RNA_access.h"

#include "UI_resources.h"


/* add handlers, stuff you only do once or on area/region changes */
static void drawings_header_region_init(wmWindowManager *wm, ARegion *ar)
{
	//wmKeyMap *keymap = WM_keymap_find(wm->defaultconf, "3D View Generic", SPACE_VIEW3D, 0);
	//WM_event_add_keymap_handler(&ar->handlers, keymap);

	ED_region_header_init(ar);
}

static void drawings_header_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void drawings_header_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *ar, wmNotifier *wmn)
{
	/* context changes */
	switch (wmn->category) {
		case NC_SPACE:
			if (wmn->data == ND_SPACE_VIEW3D)
				ED_region_tag_redraw(ar);
			break;
	}
}


static void drawings_main_region_draw(const bContext *C, ARegion *ar)
{
	Scene *scene = CTX_data_scene(C);
	Drawing *dwg = CTX_wm_drawings(C);

	/* clear */
	UI_ThemeClearColorAlpha(TH_HIGH_GRAD, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	ED_region_pixelspace(ar);

}




/* ******************** default callbacks for view3d space ***************** */

static SpaceLink *drawings_new(const bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	ARegion *ar;

	Drawing *dwg;

	dwg = MEM_callocN(sizeof(Drawing), "initview3d");
	dwg->spacetype = SPACE_DRAWINGS;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for view3d");

	BLI_addtail(&dwg->regionbase, ar);
	ar->regiontype = RGN_TYPE_HEADER;
	ar->alignment = RGN_ALIGN_BOTTOM;

#if 0

	/* tool shelf */
	ar = MEM_callocN(sizeof(ARegion), "toolshelf for view3d");

	BLI_addtail(&dwg->regionbase, ar);
	ar->regiontype = RGN_TYPE_TOOLS;
	ar->alignment = RGN_ALIGN_LEFT;
	ar->flag = RGN_FLAG_HIDDEN;

	/* tool properties */
	ar = MEM_callocN(sizeof(ARegion), "tool properties for view3d");

	BLI_addtail(&dwg->regionbase, ar);
	ar->regiontype = RGN_TYPE_TOOL_PROPS;
	ar->alignment = RGN_ALIGN_BOTTOM | RGN_SPLIT_PREV;
	ar->flag = RGN_FLAG_HIDDEN;

	/* buttons/list view */
	ar = MEM_callocN(sizeof(ARegion), "buttons for view3d");

	BLI_addtail(&dwg->regionbase, ar);
	ar->regiontype = RGN_TYPE_UI;
	ar->alignment = RGN_ALIGN_RIGHT;
	ar->flag = RGN_FLAG_HIDDEN;
#endif

	/* main region */
	ar = MEM_callocN(sizeof(ARegion), "main region for view3d");

	BLI_addtail(&dwg->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;

	ar->regiondata = MEM_callocN(sizeof(RegionView3D), "region view3d");

	return (SpaceLink *)dwg;
}


/* only called once, from space/spacetypes.c */
void ED_spacetype_drawings(void)
{
	SpaceType *st = MEM_callocN(sizeof(SpaceType), "spacetype drawings");
	ARegionType *art;

	st->spaceid = SPACE_DRAWINGS;
	strncpy(st->name, "Drawings", BKE_ST_MAXNAME);


	st->new = drawings_new;
	//st->free = view3d_free;
	//st->init = view3d_init;
	//st->listener = space_view3d_listener;
	//st->duplicate = view3d_duplicate;
	//st->operatortypes = view3d_operatortypes;
	//st->keymap = view3d_keymap;
	//st->dropboxes = view3d_dropboxes;
	//st->context = view3d_context;
	//st->id_remap = view3d_id_remap;


	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype drawings main region");
	art->regionid = RGN_TYPE_WINDOW;
	//art->keymapflag = ED_KEYMAP_GPENCIL;
	art->draw = drawings_main_region_draw;
	//art->init = view3d_main_region_init;
	//art->exit = view3d_main_region_exit;
	//art->duplicate = view3d_main_region_duplicate;
	//art->listener = view3d_main_region_listener;
	//art->cursor = view3d_main_region_cursor;
	//art->lock = 1;   /* can become flag, see BKE_spacedata_draw_locks */
	BLI_addhead(&st->regiontypes, art);

#if 0
	/* regions: listview/buttons */
	art = MEM_callocN(sizeof(ARegionType), "spacetype drawings buttons region");
	art->regionid = RGN_TYPE_UI;
	art->prefsizex = 180; /* XXX */
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	art->listener = view3d_buttons_region_listener;
	art->init = view3d_buttons_region_init;
	art->draw = view3d_buttons_region_draw;
	BLI_addhead(&st->regiontypes, art);

	view3d_buttons_register(art);
#endif

#if 0
	/* regions: tool(bar) */
	art = MEM_callocN(sizeof(ARegionType), "spacetype view3d tools region");
	art->regionid = RGN_TYPE_TOOLS;
	art->prefsizex = 160; /* XXX */
	art->prefsizey = 50; /* XXX */
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	//art->listener = view3d_buttons_region_listener;
	//art->init = view3d_tools_region_init;
	//art->draw = view3d_tools_region_draw;
	BLI_addhead(&st->regiontypes, art);
#endif

#if 0
	/* regions: tool properties */
	art = MEM_callocN(sizeof(ARegionType), "spacetype view3d tool properties region");
	art->regionid = RGN_TYPE_TOOL_PROPS;
	art->prefsizex = 0;
	art->prefsizey = 120;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_FRAMES;
	art->listener = view3d_props_region_listener;
	art->init = view3d_tools_region_init;
	art->draw = view3d_tools_region_draw;
	BLI_addhead(&st->regiontypes, art);


	view3d_tool_props_register(art);
#endif

	/* regions: header */
	art = MEM_callocN(sizeof(ARegionType), "spacetype view3d header region");
	art->regionid = RGN_TYPE_HEADER;
	art->prefsizey = HEADERY;
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D | ED_KEYMAP_FRAMES | ED_KEYMAP_HEADER;
	art->listener = drawings_header_region_listener;
	art->init = drawings_header_region_init;
	art->draw = drawings_header_region_draw;
	BLI_addhead(&st->regiontypes, art);


	BKE_spacetype_register(st);
}
