#ifndef ED_DIMENSIONS_H
#define ED_DIMENSIONS_H


void apply_dimension_value (BMDim *edm, float value);
void apply_dimension_direction_value( BMVert *va, BMVert *vb, float value);
float get_dimension_value(BMDim *edm);

//float get_txt_position(BMDim *edm);
void apply_txt_dimension_value (BMDim *edm, float value);

BMDim* get_selected_dimension(BMEditMesh *em);

void get_dimension_mid(float mid[3],BMDim *edm);

void select_dimension_data (BMDim *edm, void *context);
#endif // ED_DIMENSIONS_H
