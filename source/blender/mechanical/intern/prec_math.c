/*
 *
 *
 */


#define MECHANICAL_DEC_PRECISION 100
#define MECHANICAL_DEC_PRECISION_SQ 10000


#include "prec_math.h"
#include "math.h"


static void v_pre(int *r, const float *v) {
	r[0]= (int) (v[0]*MECHANICAL_DEC_PRECISION);
	r[1]= (int) (v[1]*MECHANICAL_DEC_PRECISION);
	r[2]= (int) (v[2]*MECHANICAL_DEC_PRECISION);
}

static void v_to_pre(float *r, const int *v) {
	r[0]= (v[0]/MECHANICAL_DEC_PRECISION);
	r[1]= (v[1]/MECHANICAL_DEC_PRECISION);
	r[2]= (v[2]/MECHANICAL_DEC_PRECISION);
}


float dot_v3v3_prec(const float fa[3], const float fb[3])
{
	int a[3],b[3];
	float r;
	v_pre(a,fa);
	v_pre(b,fb);

	r = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
	return r /MECHANICAL_DEC_PRECISION_SQ;
}



void sub_v3_v3v3_prec(float fr[3], const float fa[3], const float fb[3])
{
	int a[3],b[3],r[3];
	v_pre(a,fa);
	v_pre(b,fb);

	r[0] = a[0] - b[0];
	r[1] = a[1] - b[1];
	r[2] = a[2] - b[2];
	v_to_pre(fr,r);
}


float normalize_v3_prec(float fa[3]) {
	float d = dot_v3v3_prec(fa, fa);
	d = sqrtf(d);
	fa[0] = fa[0] * (1.0f / d);
	fa[1] = fa[1] * (1.0f / d);
	fa[2] = fa[2] * (1.0f / d);
	return d;
}
