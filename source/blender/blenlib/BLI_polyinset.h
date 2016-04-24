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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_POLYINSET_H__
#define __BLI_POLYINSET_H__

/* bits for flags argument */
#define INSET_CLAMPED 1
#define INSET_EVEN_OFFSET (1<<1)
#define INSET_RELATIVE_OFFSET (1<<2)
#define INSET_OUTSET (1<<3)

struct MemArena;

/* A face in the result. The face has vert_tot vertices, specified by indices into a coordinates array */
typedef struct InsetFace {
	unsigned int vert_tot;
	unsigned int *vert_indices;
} InsetFace;

/* An inset result.
 * The vert_coords start with those passed in as argument, and are followed by any new ones needed,
 * giving a total of vert_tot vertices.
 * The polys array has poly_tot InsetFaces, with vertex indices referencing vert_coords.
 * inner_tot gives the number of polys at the end of the polys array that are innermost after the inset.
 */
typedef struct InsetResult {
	unsigned int vert_tot;
	float (*vert_coords)[3];
	unsigned int poly_tot;
	unsigned int inner_tot;
	InsetFace *polys;
} InsetResult;

/* Calculate the coordinates of the inset polygon with given coords.
 * If INSET_CLAMPED is on flags, stop when inset reaches a self-intersection point,
 * and return the amount of thickness when that happens.
 * The answer is return in *r_result, which will have data allocated in the arena argument.
 * Note: the first coords_tot coordinates of the returned r_result->vert_coords will not be
 * populated, since they will be the same as the input coords array.
 * (So the caller is reponsible for freeing the space by freeing the arena.)
 * r_coords (and r_collapse_split, if INSET_CLAMPED) need to be supplied by
 */
float BLI_polyinset3d(
	const float (*coords)[3],
	const unsigned int coords_tot,
	const float thickness,
	const float depth,
	const unsigned int flags,
	InsetResult *r_result,

	struct MemArena *arena);

/* default size of polyinset arena */
#define BLI_POLYINSET_ARENA_SIZE MEM_SIZE_OPTIMAL(1<<14)

#endif  /* __BLI_POLYINSET_H__ */
