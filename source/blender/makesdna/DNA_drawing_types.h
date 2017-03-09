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
 * Contributor(s): jaume bellet <developer@tmaq.es>.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file DNA_drawing_types.h
 *  \ingroup DNA
 */

#ifndef __DNA_DRAWING_TYPES_H__
#define __DNA_DRAWING_TYPES_H__

struct SpaceLink;


#include "DNA_defs.h"
#include "DNA_listBase.h"


/* Drawing ViewPort Struct */
typedef struct Drawing {
	struct SpaceLink *next, *prev;
	ListBase regionbase;		/* storage of regions for inactive spaces */
	int spacetype;
	float blockscale;
	short blockhandler[8];

} Drawing;

#endif //__DNA_DRAWING_TYPES_H__
