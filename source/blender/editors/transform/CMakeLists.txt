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
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
	../include
	../../blenfont
	../../blenkernel
	../../blenlib
	../../blentranslation
	../../bmesh
	../../gpu
	../../ikplugin
	../../makesdna
	../../makesrna
	../../windowmanager
	../../depsgraph
	../../../../intern/guardedalloc
	../../../../intern/glew-mx
)

set(INC_SYS
	${GLEW_INCLUDE_PATH}
)

set(SRC
	transform.c
	transform_constraints.c
	transform_conversions.c
	transform_generics.c
	transform_input.c
	transform_manipulator.c
	transform_manipulator2d.c
	transform_ops.c
	transform_orientations.c
	transform_snap.c
	transform_snap_object.c

	transform.h
)

if(WITH_INTERNATIONAL)
	add_definitions(-DWITH_INTERNATIONAL)
endif()

if(WITH_MECHANICAL)
	list(APPEND INC
		../../mechanical
	)

	add_definitions(-DWITH_MECHANICAL)
	add_definitions(-DWITH_MECHANICAL_GRAB_W_BASE_POINT)
	add_definitions(-DWITH_MECHANICAL_ROTATE_W_BASE_POINT)
	add_definitions(-DWITH_MECHANICAL_RESIZE_W_BASE_POINT)
	add_definitions(-DWITH_MECHANICAL_SNAP_TO_CURSOR)
	add_definitions(-DWITH_MECHANICAL_TRANSFORM_MULTIPLE)
	add_definitions(-DWITH_MECHANICAL_SELECT_TRANSFORM_CENTER)
	add_definitions(-DWITH_MECHANICAL_EXIT_TRANSFORM_MODAL)
	add_definitions(-DWITH_MECHANICAL_MESH_DIMENSIONS)
	add_definitions(-DWITH_MECHANICAL_TRANSFORM_MATCH)
	add_definitions(-DWITH_MECHANICAL_MESH_REFERENCE_OBJECTS)
	add_definitions(-DWITH_MECHANICAL_SNAP_TO_PLANE)
	add_definitions(-DWITH_MECHANICAL_GEOMETRY)
	add_definitions(-DWITH_MECHANICAL_GRAB_FIX)
	add_definitions(-DWITH_MECHANICAL_UCS)
endif()

add_definitions(${GL_DEFINITIONS})

blender_add_lib(bf_editor_transform "${SRC}" "${INC}" "${INC_SYS}")
