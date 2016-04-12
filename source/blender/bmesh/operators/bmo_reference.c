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

#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_math.h"
#include "BLI_buffer.h"

#include "BKE_customdata.h"

#include "bmesh.h"

#include "intern/bmesh_operators_private.h" /* own include */

void bmo_create_reference_plane_exec(BMesh *bm, BMOperator *op)
{
	//BMReference *erf;
	float mat[4][4];
	float v1[3],v2[3],v3[3],v4[3];
	float dia;

	BMO_slot_mat4_get(op->slots_in,"matrix",mat);
	dia = BMO_slot_float_get(op->slots_in,"dia");

	v1[0]=dia; v1[1]=dia; v1[2]=0;
	v2[0]=dia;v2[1]=-dia;v2[2]=0;
	v3[0]=-dia; v3[1]=-dia;v3[2]=0;
	v4[0]=-dia;v4[1]=dia; v4[2]=0;

	mul_m4_v3(mat, v1);
	mul_m4_v3(mat, v2);
	mul_m4_v3(mat, v3);
	mul_m4_v3(mat, v4);

	BM_reference_plane_create(bm, v1, v2, v3, v4, NULL, 0);
}

