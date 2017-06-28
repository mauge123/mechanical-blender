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
#include "BKE_icons.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_drawings.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"


#include "RNA_access.h"

#include "UI_resources.h"
#include "UI_view2d.h"
#include "UI_interface.h"

#include "drawings_intern.h"  /* own include */

/* add handlers, stuff you only do once or on area/region changes */
static void drawings_header_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	//wmKeyMap *keymap = WM_keymap_find(wm->defaultconf, "3D View Generic", SPACE_VIEW3D, 0);
	//WM_event_add_keymap_handler(&ar->handlers, keymap);

	ED_region_header_init(ar);
}

static void drawings_header_region_draw(const bContext *C, ARegion *ar)
{
	ED_region_header(C, ar);
}

static void drawings_header_region_listener(bScreen *UNUSED(sc), ScrArea *UNUSED(sa), ARegion *UNUSED(ar), wmNotifier *UNUSED(wmn))
{
	/* context changes */

}


static float ED_drawing_grid_size(void)
{
	return U.widget_unit;
}

static void drawings_main_region_draw(const bContext *C, ARegion *ar)
{
	wmWindow *win = CTX_wm_window(C);
	View2DScrollers *scrollers;
	View2D *v2d = &ar->v2d;
	// Scene *scene = CTX_data_scene(C);
	SpaceDrawings *dwg = CTX_wm_space_drawings(C);


	UI_ThemeClearColor(TH_BACK);
	glClear(GL_COLOR_BUFFER_BIT);

	UI_view2d_view_ortho(v2d);

	/* XXX snode->cursor set in coordspace for placing new nodes, used for drawing noodles too */
	UI_view2d_region_to_view(&ar->v2d, win->eventstate->x - ar->winrct.xmin, win->eventstate->y - ar->winrct.ymin,
	                         &dwg->cursor[0], &dwg->cursor[1]);
	dwg->cursor[0] /= UI_DPI_FAC;
	dwg->cursor[1] /= UI_DPI_FAC;

	ED_region_draw_cb_draw(C, ar, REGION_DRAW_PRE_VIEW);

	/* only set once */
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_MAP1_VERTEX_3);


	/* default grid */
	UI_view2d_multi_grid_draw(v2d, TH_BACK, ED_drawing_grid_size(), DRAWING_GRID_STEPS, 2);

	{
		float w=210,h=297;
		glColor3f(0.9,0.9,0.9);
		glBegin(GL_QUADS);
			glVertex2f(0,0);
			glVertex2f(w,0);
			glVertex2f(w,h);
			glVertex2f(0,h);
		glEnd();
	}

	/* backdrop */
	//draw_nodespace_back_pix(C, ar, snode, NODE_INSTANCE_KEY_NONE);

	ED_region_draw_cb_draw(C, ar, REGION_DRAW_POST_VIEW);

	/* reset view matrix */
	UI_view2d_view_restore(C);

	/* scrollers */
	scrollers = UI_view2d_scrollers_calc(C, v2d, 10, V2D_GRID_CLAMP, V2D_ARG_DUMMY, V2D_ARG_DUMMY);
	UI_view2d_scrollers_draw(C, v2d, scrollers);
	UI_view2d_scrollers_free(scrollers);

	ED_region_pixelspace(ar);

}




/* ******************** default callbacks for view3d space ***************** */

static SpaceLink *drawings_new(const bContext *UNUSED(C))
{
	// Scene *scene = CTX_data_scene(C);
	ARegion *ar;

	SpaceDrawings *dwg;

	dwg = MEM_callocN(sizeof(SpaceDrawings), "init drawing");
	dwg->spacetype = SPACE_DRAWINGS;

	/* header */
	ar = MEM_callocN(sizeof(ARegion), "header for drawings");

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
	ar = MEM_callocN(sizeof(ARegion), "main region for drawings");

	BLI_addtail(&dwg->regionbase, ar);
	ar->regiontype = RGN_TYPE_WINDOW;

	ar->regiondata = MEM_callocN(sizeof(RegionView3D), "region drawing");

	ar->v2d.tot.xmin =  -12.8f * U.widget_unit;
	ar->v2d.tot.ymin =  -12.8f * U.widget_unit;
	ar->v2d.tot.xmax = 38.4f * U.widget_unit;
	ar->v2d.tot.ymax = 38.4f * U.widget_unit;

	ar->v2d.cur =  ar->v2d.tot;

	ar->v2d.min[0] = 1.0f;
	ar->v2d.min[1] = 1.0f;

	ar->v2d.max[0] = 32000.0f;
	ar->v2d.max[1] = 32000.0f;

	ar->v2d.minzoom = 0.09f;
	ar->v2d.maxzoom = 10.00f;

	ar->v2d.scroll = (V2D_SCROLL_RIGHT | V2D_SCROLL_BOTTOM);
	ar->v2d.keepzoom = V2D_LIMITZOOM | V2D_KEEPASPECT;
	ar->v2d.keeptot = 0;


	return (SpaceLink *)dwg;
}


const char *drawing_context_dir[] = {"edit_drawing", NULL};

static int drawings_context(const bContext *C, const char *member, bContextDataResult *result)
{
	SpaceDrawings *sdwg = CTX_wm_space_drawings(C);

	if (CTX_data_dir(member)) {
		CTX_data_dir_set(result, drawing_context_dir);
		return 1;
	}
	else if (CTX_data_equals(member, "edit_drawing")) {
		CTX_data_id_pointer_set(result, &sdwg->drawing->id);
		return 1;
	}

	return 0;
}


/* Initialize main region, setting handlers. */
static void drawings_main_region_init(wmWindowManager *UNUSED(wm), ARegion *ar)
{
	UI_view2d_region_reinit(&ar->v2d, V2D_COMMONVIEW_CUSTOM, ar->winx, ar->winy);
}


static void drawings_operatortypes(void)
{
	WM_operatortype_append(DRAWINGS_OT_new);
	WM_operatortype_append(DRAWINGS_OT_unlink);
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
	st->operatortypes = drawings_operatortypes;
	//st->keymap = view3d_keymap;
	//st->dropboxes = view3d_dropboxes;
	st->context = drawings_context;
	//st->id_remap = view3d_id_remap;


	/* regions: main window */
	art = MEM_callocN(sizeof(ARegionType), "spacetype drawings main region");
	art->regionid = RGN_TYPE_WINDOW;
	art->draw = drawings_main_region_draw;
	art->init = drawings_main_region_init;
	//art->exit = view3d_main_region_exit;
	//art->duplicate = view3d_main_region_duplicate;
	//art->listener = view3d_main_region_listener;
	//art->cursor = view3d_main_region_cursor;
	//art->lock = 1;   /* can become flag, see BKE_spacedata_draw_locks */
	art->keymapflag = ED_KEYMAP_UI | ED_KEYMAP_VIEW2D;

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
