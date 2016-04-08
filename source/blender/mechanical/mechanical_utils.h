#ifndef MECHANICAL_UTILS_H
#define MECHANICAL_UTILS_H
/*
 *
 *
 *
*/


void tag_vertexs_on_coplanar_faces(BMesh *bm, float* point, float *dir);
void tag_vertexs_affected_by_dimension (BMesh *bm, BMDim *edm);


#endif // MECHANICAL_UTILS_H
