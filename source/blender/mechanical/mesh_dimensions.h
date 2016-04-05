#ifndef MESH_DIMENSIONS_H
#define MESH_DIMENSIONS_H

#define DIM_CONSTRAINT_PRECISION 1.0e-3

#define DIM_PLANE_CONSTRAINT (1 << 0)
#define DIM_AXIS_CONSTRAINT (1 << 1)


void apply_dimension_value (BMesh *bm, BMDim *edm, float value, int constraints);
void apply_dimension_direction_value( BMVert *va, BMVert *vb, float value, float *res);

float get_dimension_value(BMDim *edm);

BMDim* get_selected_dimension(BMesh *bm);

int get_necessary_dimension_verts(int dim_type);

void get_dimension_mid(float mid[3],BMDim *edm);
void get_dimension_plane (float p[3], BMDim *edm);
void dimension_data_update (BMDim *edm);

void set_dimension_center(BMDim *edm);

void select_dimension_data (BMDim *edm, void *context);

/* Common use */
void mid_of_2_points(float *mid, float *p1, float *p2);
int center_of_3_points(float *center, float *p1, float *p2, float *p3);



#endif // MESH_DIMENSIONS_H
