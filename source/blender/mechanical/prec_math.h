/**
 *
 *
 *
 */


#ifndef PREC_MATH_H
#define PREC_MATH_H


float dot_v3v3_prec(const float fa[3], const float fb[3]);

void sub_v3_v3v3_prec(float fr[3], const float fa[3], const float fb[3]);

float normalize_v3_prec (float fa[3]);

int eq_ff_prec(const float fa, const float fb);
int eq_v3v3_prec(const float fa[3], const float fb[3]);


float ensure_f_prec (float f);
void ensure_v3_prec (float f[3]);

#endif //PREC_MATH_H
