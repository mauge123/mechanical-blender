#ifndef MESH_DIMENSIONS_H
#define MESH_DIMENSIONS_H

#define DIM_CONSTRAINT_PRECISION 0.01

#define DIM_PLANE_CONSTRAINT (1 << 0)
#define DIM_AXIS_CONSTRAINT (1 << 1)
#define DIM_CONSTRAINT_OVERRIDE (1<<2)
#define DIM_ALLOW_SLIDE_CONSTRAINT (1<<3)

#define DIM_DIR_LEFT    1
#define DIM_DIR_RIGHT   2
#define DIM_DIR_BOTH    3
#define DIM_DIR_AUTO	4

#include "DNA_scene_types.h"


void apply_dimension_value(BMesh *bm, BMDim *edm, float value, ToolSettings *ts);
void apply_dimension_direction_value(BMVert *va, BMVert *vb, float value, float *res);

float get_dimension_value(BMDim *edm);

BMDim* get_selected_dimension(BMesh *bm);

int get_necessary_dimension_verts(int dim_type);

void get_dimension_mid(float mid[3],BMDim *edm);
void get_dimension_plane(float v[3], float p[3], BMDim *edm);
void get_dimension_transform_orientation_plane (float normal[3], BMDim *edm, Scene *scene);
void get_dimension_transform_orientation_matrix (float mat[][3], BMDim *edm, Scene *scene);

void dimension_data_update(BMesh *bm, BMDim *edm, Scene *scene);

void set_dimension_center(BMDim *edm);

void set_dimension_direction(BMDim *edm, void *context);

/* Common use */
void mid_of_2_points(float *mid, float *p1, float *p2);

bool valid_constraint_setting(BMDim *edm, int constraint);


#endif // MESH_DIMENSIONS_H
