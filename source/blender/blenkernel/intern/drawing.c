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
 * The Original Code is: all of this file.
 *
 * Contributor(s): jaume Bellet <mauge@bixo.or>.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/drawing.c
 *  \ingroup bke
 */

#include <stdlib.h> /* abort */
#include <string.h> /* strstr */
#include <sys/types.h>
#include <sys/stat.h>
#include <wchar.h>
#include <wctype.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_cursor_utf8.h"
#include "BLI_string_utf8.h"
#include "BLI_listbase.h"
#include "BLI_fileops.h"

#include "DNA_drawing_types.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_drawing.h"


#ifdef WITH_PYTHON
#include "BPY_extern.h"
#endif


Drawing *BKE_drawing_add(Main *bmain, const char *name)
{
	Drawing *dwg;

	dwg = BKE_libblock_alloc(bmain, ID_DW, name, 0);

	//BKE_text_init(ta);

	return dwg;
}
