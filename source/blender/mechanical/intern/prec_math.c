/*
 *
 *
 */


#define MECHANICAL_DEC_PRECISION 100
#define MECHANICAL_DEC_PRECISION_SQ 10000


#include "prec_math.h"
#include "math.h"


static void v_pre(int *r, const float *v) {

	r[0]= (int) roundf((v[0]*MECHANICAL_DEC_PRECISION));
	r[1]= (int) roundf((v[1]*MECHANICAL_DEC_PRECISION));
	r[2]= (int) roundf((v[2]*MECHANICAL_DEC_PRECISION));
}

static void v_to_pre(float *r, const int *v) {
	r[0]= ((float) v[0]/(float) MECHANICAL_DEC_PRECISION);
	r[1]= ((float) v[1]/(float) MECHANICAL_DEC_PRECISION);
	r[2]= ((float) v[2]/(float) MECHANICAL_DEC_PRECISION);
}

static void reduce_v3_precision (int *v) {
	v[0] = v[0]/10;
	v[1] = v[1]/10;
	v[2] = v[2]/10;
}

static int reduce_f_precision (int v) {
	return v/10;
}


static int f_pre (float f) {
	return (int) (f*MECHANICAL_DEC_PRECISION);
}


static float f_to_pre (int f) {
	return ((float) f/(float) MECHANICAL_DEC_PRECISION);
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


float normalize_v3_prec(float fa[3])
{
	float d = dot_v3v3_prec(fa, fa);
	d = sqrtf(d);
	fa[0] = fa[0] * (1.0f / d);
	fa[1] = fa[1] * (1.0f / d);
	fa[2] = fa[2] * (1.0f / d);
	return d;
}

float ensure_f_prec (float f) {
	return f_to_pre(f_pre(f));
}

void ensure_v3_prec (float f[3]) {
	f[0] = ensure_f_prec(f[0]);
	f[1] = ensure_f_prec(f[1]);
	f[2] = ensure_f_prec(f[2]);
}

int eq_v3v3_prec(const float fa[3], const float fb[3])
{
	int a[3],b[3];

	v_pre(a,fa);
	v_pre(b,fb);

	reduce_v3_precision(a);
	reduce_v3_precision(b);

	return (a[0] ==  b[0]) && (a[1] == b[1]) && (a[2] == b[2]);
}

int eq_ff_prec(const float fa, const float fb)
{
	int a,b;

	a = f_pre(fa);
	b = f_pre(fb);
	a = reduce_f_precision(a);
	b = reduce_f_precision(b);

	return (a == b);
}
