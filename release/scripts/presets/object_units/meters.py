import bpy
scene = bpy.context.scene
object = bpy.context.object

object.unit_settings.system = 'METRIC'
object.unit_settings.scale_length = 1.0
