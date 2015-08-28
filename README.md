# Mechanical-blender
Mechanical Blender project is fork of blender BF-Project

## Purpose

The purpose of Mechanical Blender is the development of tools and fork blender source to allow blender
be a CAD software that could be used on industry. All changes will be performed in a way that
commitment in blender official trunk could be done easily if the community and the Blender foundation consider
it as convenient.

We wont a professional software to be used for companies and individuals that

* Want to be free and no to be imposed the OS, and upgrades (soft, and hardware)
* Don't want to be married with a company and a software.
* Give importance to code, not only the binaries.
* Cannot afford the expensive prices of licenses and they want to be legal.

## Builds

## Compile

To be compiled you must enable WITH_MECHANICAL flag. Once enable compile as usual done for bf-blender

## Branches

### master

This branch will be periodically updated with bf-blender trunk.

The mechanical branches will be also merged here periodically

### mechanical-dimensions

This branchs starts from bf-blender trunk.

currently on development

### mechanical-grab-w-basepoint

This branchs starts from bf-blender trunk.

The branch should be tested in other areas of Blender than 3D space editor.

* see https://vimeo.com/137361362
* see https://developer.blender.org/T45734

## mechanical-rotate-w-basepoint

This branchs starts from mechanical-grab-w-basepoint.

currenly on development.

## Issues

### Shortcut to enable/disable snapping does not work during transform operation

SHIFT+TAB does not work. Chosing the snap type with CTRL+SHIFT+TAB works.

In blender trunk, it works SHIFT+TAB, but not CTRL+SHIFT+TAB !

## Related

Related data and applications to allow blender be more CAD/CAE friendly.

### Adddons

#### Enhanced Custom Properties

* http://wiki.blender.org/index.php/Extensions:2.7/Py/Scripts/Object/Enhanced_Custom_Properties
* https://github.com/devynios/digidone

#### Tutorials and videos

## Wishes

### UCS

See https://developer.blender.org/T43964

### Drawing Space

Creation of a drawing space, to get 2D data for manufacturing or documentation.

### Snap to cursor

New snap mode allowing to set snap to cursor.

### Snap to gemoetry

New snap mode allowing to set snap to geometry, like centers, mid_points, tangents, perpendiculars and so on.

## Credits

* All the people that allowed blender be a reality
* Blender Foundation - www.blender.org
* Jaume Bellet - mauge@bixo.org
* Tecnologia Industrial i Fabricació de Maquinaria, SL - developer@tmaq.es

