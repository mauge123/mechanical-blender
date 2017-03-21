# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>
import bpy
from bpy.types import Header, Menu, Panel
from bpy.app.translations import contexts as i18n_contexts


class DRAWINGS_HT_header(Header):
    bl_space_type = 'DRAWINGS'

    def draw(self, context):
        layout = self.layout

        st = context.space_data
        # dwg = st.drawing

        
        # mode_string = context.mode
        obj = context.active_object
        toolsettings = context.tool_settings

        row = layout.row(align=True)
        row.template_header()

        DRAWINGS_MT_editor_menus.draw_collapsible(context, layout)
        
        row = layout.row(align=True)
        row.template_ID(st, "drawing", new="drawings.new", unlink = "drawings.unlink")


class DRAWINGS_MT_editor_menus(Menu):
    bl_idname = "DRAWINGS_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        self.draw_menus(self.layout, context)

    @staticmethod
    def draw_menus(layout, context):
        st = context.space_data
        #text = st.text

        layout.menu("DRAWINGS_MT_view")
        
        layout.menu("DRAWINGS_MT_drawing")

        #if text:
        #    layout.menu("DRAWINGS_MT_edit")
        #    layout.menu("DRAWINGS_MT_format")

        #layout.menu("DRAWINGS_MT_templates")
        
class DRAWINGS_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        layout.operator("screen.area_dupli")
        layout.operator("screen.screen_full_area")
        layout.operator("screen.screen_full_area", text="Toggle Fullscreen Area").use_hide_panels = True
        ayout.separator()
        
class DRAWINGS_MT_drawing(Menu):
    bl_label = "Drawing"

    def draw(self, context):
        layout = self.layout

        #layout.operator("text.properties", icon='MENU_PANEL')
        layout.separator()
   

classes = (
    DRAWINGS_HT_header,
    DRAWINGS_MT_editor_menus,
    DRAWINGS_MT_view,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)