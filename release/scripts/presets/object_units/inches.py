import bpy
scene = bpy.context.scene
object = bpy.context.object

object.unit_settings.system = 'IMPERIAL'
object.unit_settings.scale_length = 0.0254
