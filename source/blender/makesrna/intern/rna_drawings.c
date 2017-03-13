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
 * Contributor(s): Blender Foundation (2008)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/makesrna/intern/rna_drawings.c
 *  \ingroup RNA
 */


#include <stdlib.h>
#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "BKE_text.h"

#include "RNA_define.h"

#include "rna_internal.h"

#include "DNA_text_types.h"

#include "WM_types.h"

#ifdef RNA_RUNTIME

#else

static void rna_def_drawings(BlenderRNA *brna)
{
	StructRNA *srna;
	PropertyRNA *prop;

	srna = RNA_def_struct(brna, "Drawing", "ID");
	RNA_def_struct_ui_text(srna, "Drawing", "MEchanical Drawing Sheet");
	RNA_def_struct_ui_icon(srna, ICON_DRAWING);
	RNA_def_struct_clear_flag(srna, STRUCT_ID_REFCOUNT);

}


void RNA_def_drawings(BlenderRNA *brna)
{
	rna_def_drawings(brna);
}

#endif
