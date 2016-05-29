#ifndef MECHANICAL_GEOMETRY_H
#define MECHANICAL_GEOMETRY_H
/*
 *
 *
 *
*/

#define MAX_ANGLE_EDGE_FROM_CENTER_ON_ARC 3.14/10

#define DIM_GEOMETRY_PRECISION 0.01


void mechanical_update_mesh_geometry(BMEditMesh *em);

#endif // MECHANICAL_GEOMETRY_H
