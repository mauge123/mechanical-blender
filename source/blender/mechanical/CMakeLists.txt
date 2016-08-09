# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(SRC
	intern/prec_math.c
	intern/mesh_dimensions.c
	intern/mechanical_utils.c
	intern/mesh_references.c
	intern/mechanical_geometry.c
	intern/mechanical_geometry_utils.c
)

set(INC
	.
	../bmesh
	../blenkernel
	../blenlib
	../blentranslation
	../imbuf
	../makesdna
	../makesrna
	../editors/include
	../../../intern/guardedalloc
)


add_definitions(-DWITH_MECHANICAL)
add_definitions(-DWITH_MECHANICAL_MESH_DIMENSIONS)
add_definitions(-DWITH_MECHANICAL_GEOMETRY)

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_PLAYER)
	add_definitions(-DWITH_PLAYER)
endif()

blender_add_lib(bf_mechanical "${SRC}" "${INC}" "${INC_SYS}")
