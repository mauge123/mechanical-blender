#ifndef MECHANICAL_GEOMETRY_H
#define MECHANICAL_GEOMETRY_H
/*
 *
 *
 *
*/

#define MAX_ANGLE_EDGE_FROM_CENTER_ON_ARC 3.14/10

typedef struct test_circle_data {
	float center[3];
	float angle;
}test_circle_data;

void mechanical_update_mesh_geometry(BMEditMesh *em);
void set_geometry_center (BMEditMesh *em, float center[3]);

#endif // MECHANICAL_GEOMETRY_H
