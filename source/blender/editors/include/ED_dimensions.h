#ifndef ED_DIMENSIONS_H
#define ED_DIMENSIONS_H


void apply_dimension_value (BMDim *edm, float value);
float get_dimension_value(BMDim *edm);

//float get_txt_position(BMDim *edm);
void apply_txt_dimension_value (BMDim *edm, float value);

BMDim* get_selected_dimension(BMEditMesh *em);

void get_dimension_mid(float mid[3],BMDim *edm);
#endif // ED_DIMENSIONS_H
