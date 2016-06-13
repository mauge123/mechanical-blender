#ifndef MECHANICAL_UTILS_H
#define MECHANICAL_UTILS_H
/*
 *
 *
 *
*/

bool parallel_v3_v3(float *v1, float *v2);
bool parallel_v3u_v3u(float *v1, float *v2);
bool perpendicular_v3_v3(float *v1, float *v2);
bool point_on_axis(float *c, float*a, float *p);
bool point_on_plane(float *c, float *a, float *p);


bool parallel_v3u_v3u_prec(float *v1, float *v2);
bool point_on_axis_prec(float *c, float*a, float *p);

void v_perpendicular_to_axis(float *r, float *c, float *p, float *a);


void tag_vertexs_on_coplanar_faces(BMesh *bm, float* point, float *dir);
void tag_vertexs_affected_by_dimension (BMesh *bm, BMDim *edm);

float point_dist_to_plane (float *c, float *a, float *);
float point_dist_to_axis (float *c, float *a, float *p);

int center_of_3_points(float *center, float *p1, float *p2, float *p3);


#endif // MECHANICAL_UTILS_H
