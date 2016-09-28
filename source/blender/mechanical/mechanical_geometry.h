#ifndef MECHANICAL_GEOMETRY_H
#define MECHANICAL_GEOMETRY_H
/*
 *
 *
 *
*/

#define MAX_ANGLE_EDGE_FROM_CENTER_ON_ARC 3.14/10
// WITH_MECHANICAL_GEOMETRY
/* Number of Geometries Points */
#define GEO_SNAP_POINTS_PER_LINE 4
#define GEO_SNAP_POINTS_PER_ARC	7
// Center, Perp to point, Tangent1 to Point, Tangent2 to point
#define GEO_SNAP_POINTS_PER_CIRCLE 4


typedef struct test_circle_data {
	float center[3];
	float angle;
}test_circle_data;

void mechanical_update_mesh_geometry(BMesh *bm);
void set_geometry_center (BMesh *em, float center[3]);

void mechanical_clean_geometry (BMesh *bm);


 int get_max_geom_points(BMesh *em);
 void arc_mid_point(BMGeom *egm);


#endif //MECHANICAL_GEOMETRY_H
