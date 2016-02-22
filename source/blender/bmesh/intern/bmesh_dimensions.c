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
#include "bmesh.h"

#include "mesh_intern.h"  /* own include */




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
