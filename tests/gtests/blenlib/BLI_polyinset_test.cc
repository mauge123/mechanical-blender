/* Apache License, Version 2.0 */

#include "testing/testing.h"

extern "C" {
#include "MEM_guardedalloc.h"
#include "BLI_array_utils.h"
#include "BLI_polyinset.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
}

#define TEST_EPS 1e-5
#define EXPECT_NEAR_COORDS(a, b)  \
{ \
	EXPECT_NEAR((double)a[0], (double)b[0], TEST_EPS); \
	EXPECT_NEAR((double)a[1], (double)b[1], TEST_EPS); \
	EXPECT_NEAR((double)a[2], (double)b[2], TEST_EPS); \
} (void) 0

#define DEBUG_INSET 0

#if DEBUG_INSET
#define F3(a) a[0], a[1], a[2]
#endif

#define ECLAMP (INSET_EVEN_OFFSET|INSET_CLAMPED)

#if DEBUG_INSET
/* for debugging */
static void print_inset_result(InsetResult *r, float thickness) {
	int i, j;
	InsetFace *f;

	printf("polyinset result: thickness=%f\n", thickness);
	printf("vert_tot=%d  poly_tot=%d  inner_tot=%d\n", r->vert_tot, r->poly_tot, r->inner_tot);
	printf("coords:\n");
	for (i = 0; i < r->vert_tot; i++)
		printf("(%.7f, %.7f, %.7f) ", F3(r->vert_coords[i]));
	printf("\npolys:\n");
	for (i = 0; i < r->poly_tot; i++) {
		f = &r->polys[i];
		for (j = 0; j < f->vert_tot; j++)
			printf("%d ", f->vert_indices[j]);
		printf("\n");
	}
}
#endif

/* poly, poly_len, thickness, depth, flags : initial args for BLI_polyinset3d
 * expect_thickness : expected return value
 * expect_extra_coords: coords that will come after poly coords in answer's coords
 * expect_extra_coords_len: how many coords in expect_extra_coords
 * expect_poly_indices: returned polygons for inset, each as a sequence of indices
 * into concatenation of poly and expect_extra_coords, each poly preceded by the number of verts in it
 * expect_num_polys : number of polys encoded in previous
 */
static void polyinset_test(
        const float poly[][3], const unsigned int poly_len,
        const float thickness, const float depth, const unsigned int flags,
        const float expect_thickness,
        const float expect_extra_coords[][3], int expect_extra_coords_len,
        const int expect_poly_indices[], int expect_num_polys, int expect_inner_polys)
{
	int i, j, k, n;
	InsetResult r;
	InsetFace *iface;
	float th;
	MemArena *arena = BLI_memarena_new(BLI_POLYINSET_ARENA_SIZE, __func__);

	th = BLI_polyinset3d(poly, poly_len, thickness, depth, flags, &r, arena);
#if DEBUG_INSET
	print_inset_result(&r, th);
#endif
	EXPECT_NEAR((double)expect_thickness, (double)th, TEST_EPS);
	EXPECT_EQ(poly_len + expect_extra_coords_len, r.vert_tot);
	EXPECT_EQ(expect_num_polys, r.poly_tot);
	EXPECT_EQ(expect_inner_polys, r.inner_tot);
	if (expect_num_polys != r.poly_tot || poly_len + expect_extra_coords_len != r.vert_tot)
		return;
	for (i = 0; i < expect_extra_coords_len; i++) {
		EXPECT_NEAR_COORDS(expect_extra_coords[i], r.vert_coords[poly_len + i]);
	}
	j = 0;
	for (i = 0; i < r.poly_tot; i++) {
		if (i >= expect_num_polys)
			break;
		iface = &r.polys[i];
		n = iface->vert_tot;
		EXPECT_EQ(expect_poly_indices[j], n);
		if (expect_poly_indices[j] != n)
			break;
		j++;
		for (k = 0; k < n; k++) {
			EXPECT_EQ(expect_poly_indices[j], iface->vert_indices[k]);
			j++;
		}
	}
	BLI_memarena_free(arena);
}

TEST(polyinset, BasicSquare)
{
	const float poly[][3] = {{0, 0, 0}, {10, 0, 0}, {10, 10, 0}, {0, 10, 0}};
	const float new_coords[][3] = {{2, 2, 0}, {8, 2, 0}, {8, 8, 0}, {2, 8, 0}};
	const int indices[] = {
		4,  0, 1, 5, 4,
		4,  1, 2, 6, 5,
		4,  2, 3, 7, 6,
		4,  3, 0, 4, 7,
		4,  4, 5, 6, 7};

	polyinset_test(poly, 4, 2.0f, 0.0f, INSET_EVEN_OFFSET, 2.0f, new_coords, 4, indices, 5, 1);
}

TEST(polyinset, Pgram)
{
	/* parallelogram with bottom and top having length 10, top shifted 5 to right from bottom */
	const float poly[][3] = {{0, 0, 1}, {10, 0, 1}, {15, 10, 1}, {5, 10, 1}};
	/* acute angles are atan(2), obtuse angles are pi - atan(2).
	 * let X = 3/tan(.5*atan(2)); W = 3/tan(.5*(pi - atan(2)) */
#define X 4.854101966249683f
#define W 1.854101966249685f
	const float new_coords[][3] = {{X, 3, 5}, {10-W , 3, 5}, {15-X, 7, 5}, {5+W, 7, 5}};
#undef X
#undef W
	const int indices[] = {
		4,  0, 1, 5, 4,
		4,  1, 2, 6, 5,
		4,  2, 3, 7, 6,
		4,  3, 0, 4, 7,
		4,  4, 5, 6, 7};

	polyinset_test(poly, 4, 3.0f, 4.0f, INSET_EVEN_OFFSET, 3.0f, new_coords, 4, indices, 5, 1);
}

TEST(polyinset, HalfhouseRelative)
{
	/* quad that is square but with another square cut on NE-SW diagonal on top of first */
	const float poly[][3] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 2, 0}};
	/* half angles at corners: 45, 45, 67.5, 22.5.
	 * let Y1 = y dip on top right: .1/tan(67.5)
	 * let Y2 = y dip on top left: .1/tan(22.5)
	 * average edge lengths at corners: 1.5, 1, (1+s2)/2 = ~ 1,207, (2+s2)/2 = ~1.707, where s2 = sqrt(2) */
#define Y1 0.041421356237309515f
#define Y2 0.24142135623730948f
#define F1 1.2071067811865475f
#define F2 1.7071067811865475f
	const float new_coords[][3] = {{1.5f * 0.1f, 1.5f * 0.1f, 0}, {0.9f, 0.1f, 0},
		{1.0f - F1 * 0.1f, 1.0f - F1 * Y1, 0}, {F2 * 0.1f, 2.0f - F2 * Y2, 0}};
#undef Y1
#undef Y2
#undef F1
#undef F2
	const int indices[] = {
		4,  0, 1, 5, 4,
		4,  1, 2, 6, 5,
		4,  2, 3, 7, 6,
		4,  3, 0, 4, 7,
		4,  4, 5, 6, 7};

	polyinset_test(poly, 4, 0.1f, 0.0f, INSET_EVEN_OFFSET|INSET_RELATIVE_OFFSET, 0.1f, new_coords, 4, indices, 5, 1);
}

TEST(polyinset, BumpRectVertexClamp)
{
	/* rectangle with small bump at bottom having 135 degree bottom corners that will meet first */
	const float poly[][3] = {{-0.1f, 0, 0}, {0.1f, 0, 0}, {0.2f, 0.1f, 0}, {1, 0.1f, 0},
		{1, 1, 0}, {-1, 1, 0}, {-1, 0.1f, 0}, {-0.2f, 0.1f, 0}};
	/* first, second vertex spokes meet first; at y = Y1 = .1 tan (67.5). That is also the thickness of the inset then.
	 * the 3rd vert has x moved left by X1 = Y1 tan(22.5) = .1 */
#define Y1 0.2414213562373095f
#define X1 0.1f
	const float new_coords[][3] = {{0, Y1, 0}, {0.2f - X1, 0.1f + Y1}, {1.0f - Y1, 0.1f + Y1, 0},
		{1.0f - Y1, 1.0f - Y1, 0}, {-1.0f + Y1, 1.0f - Y1, 0}, {-1.0f + Y1, 0.1f + Y1, 0}, {-0.2f + X1, 0.1f + Y1}};
#undef X1
	const int indices[] = {
		3,  0, 1, 8,
		4,  1, 2, 9, 8,
		4,  2, 3, 10, 9,
		4,  3, 4, 11, 10,
		4,  4, 5, 12, 11,
		4,  5, 6, 13, 12,
		4,  6, 7, 14, 13,
		4,  7, 0, 8, 14,
		7,  8, 9, 10, 11, 12, 13, 14};

	polyinset_test(poly, 8, 0.3f, 0.0f, ECLAMP, Y1, new_coords, 7, indices, 9, 1);
#undef Y1
}

TEST(polyinset, BumpOct3VertexClamp)
{
	/* rectangle with half octagon bump at bottom having three 135 corners that will meet first */
	/* L1 is 1/sqrt(2) */
#define L1 0.7071067811865475f
	const float poly[][3] = {{0, -1, 0}, {L1, -L1, 0}, {1, 0, 0}, {3, 0, 0}, {3, 2, 0},
		 {-3, 2, 0}, {-3, 0, 0}, {-1, 0, 0}, {-L1, -L1, 0}};
	/* intersection of lines through (0,0) slope 1+sqrt(2); and through (1,0) slope -cot(3pi/16) */
#define X1 0.3826834324f
#define Y1 0.9238795325f
	const float new_coords[][3] = {{0, 0, 0}, {X1, Y1, 0}, {3 - Y1, Y1, 0},  {3 - Y1, 2 - Y1, 0},
		{Y1 - 3, 2 - Y1, 0}, {Y1 - 3, Y1, 0}, {-X1, Y1, 0}};
	const int indices[] = {
		3,  0, 1, 9,
		4,  1, 2, 10, 9,
		4,  2, 3, 11, 10,
		4,  3, 4, 12, 11,
		4,  4, 5, 13, 12,
		4,  5, 6, 14, 13,
		4,  6, 7, 15, 14,
		4,  7, 8, 9, 15,
		3,  8, 0, 9,
		7,  9, 10, 11, 12, 13, 14, 15};

	polyinset_test(poly, 9, 1.0f, 0.0f, ECLAMP, Y1, new_coords, 7, indices, 10, 1);
#undef L1
#undef X1
#undef Y1
}

TEST(polyinset, BumpOct3VertexInner)
{
	/* BumpOct3VertexClamp, but continuing after first clamp */
#define L1 0.7071067811865475f
#define X1 0.3826834324f
#define Y1 0.9238795325f
	const float poly[][3] = {{0, 0, 0}, {X1, Y1, 0}, {3 - Y1, Y1, 0},  {3 - Y1, 2 - Y1, 0},
		{Y1 - 3, 2 - Y1, 0}, {Y1 - 3, Y1, 0}, {-X1, Y1, 0}
	};
#define X2 0.3318214f
#define Y2 0.1989123f
	const float new_coords[][3] = {{0, Y2, 0}, {X2, 1, 0}, {2, 1, 0}, {-2, 1, 0}, {-X2, 1, 0}};
	const int indices[] = {
		4,  0, 1, 8, 7,
		4,  1, 2, 9, 8,
		3,  2, 3, 9,
		6,  3, 4, 10, 11, 8, 9,
		3,  4, 5, 10,
		4,  5, 6, 11, 10,
		4,  6, 0, 7, 11,
		3,  7, 8, 11};
	
	polyinset_test(poly, 7, 1.0f, 0.0f, ECLAMP, 1.0f - Y1, new_coords, 5, indices, 8, 1);
#undef L1
#undef X1
#undef Y1
#undef X2
#undef Y2
}

TEST(polyinset, TriCollapseToPoint)
{
	/* triangle inscribed in radius 1 circle at (0,0,0); X1 = sqrt(3)/2 */
#define X1 0.8660254037844386f
	const float poly[][3] = {{-X1, -0.5f, 0}, {X1, -0.5f, 0}, {0, 1, 0}};
#undef X1
	const float new_coords[][3] = {{0, 0, 0}};
	const int indices[] = {
		3,  0, 1, 3,
		3,  1, 2, 3,
		3,  2, 0, 3};

	polyinset_test(poly, 3, 1.0f, 0.0f, ECLAMP, 0.5f, new_coords, 1, indices, 3, 0);
}

TEST(polyinset, RectCollapseToLine)
{
	/* 4 x 2 rectangle on YZ plane with lower left at (0,0,0) */
	const float poly[][3] = {{0, 0, 0}, {0, 4, 0}, {0, 4, 2}, {0, 0, 2}};
	const float new_coords[][3] = {{0, 1, 1}, {0, 3, 1}};
	const int indices[] = {
		4,  0, 1, 5, 4,
		3,  1, 2, 5,
		4,  2, 3, 4, 5,
		3,  3, 0, 4};

	polyinset_test(poly, 4, 1.0f, 0.0f, ECLAMP, 1.0f, new_coords, 2, indices, 4, 0);
}

TEST(polyinset, RectPlusCollapseToLine)
{
	/* 4 x 2 rectangle on XY plane with lower left at (0,0,0) and extra point at midpoint on top */
	/* The center poly appears to be a triangle, but collapses to a line */
	const float poly[][3] = {{0, 0, 0}, {4, 0, 0}, {4, 2, 0}, {2, 2, 0}, {0, 2, 0}};
	const float new_coords[][3] = {{1, 1, 0}, {3, 1, 0}, {2, 1, 0}};
	const int indices[] = {
		5,  0, 1, 6, 7, 5,
		3,  1, 2, 6,
		4,  2, 3, 7, 6,
		4,  3, 4, 5, 7,
		3,  4, 0, 5};

	polyinset_test(poly, 5, 1.1f, 0.0f, ECLAMP, 1.0f, new_coords, 3, indices, 5, 0);
}

TEST(polyinset, RectReflex)
{
	/* rectangle with +-1 y extents, top edge split in middle at middle point lowered to (0,0),
	 * and x coords at +-sqrt(3) to make top lines make 30 degree angles with horizontal */
	/* X1 = sqrt(3). TH = 2*X1 - 3.  Y1 = TH - 1.  X2 = X1 - TH.   Y2 = 1 - X1*TH */
#define X1 1.7320508075688772f
#define TH 0.4641016151377546f
#define Y1 -0.5358983848622454f
#define X2 1.2679491924311226f
#define Y2 0.1961524227066319
	const float poly[][3] = {{4, -X1, -1}, {4, X1, -1}, {4, X1, 1}, {4, 0, 0}, {4, -X1, 1}};
	const float new_coords[][3] = {{4, -X2, Y1}, {4, X2, Y1}, {4, X2, Y2}, {4, 0, Y1}, {4, -X2, Y2}};
	const int indices[] = {
		5,  0, 1, 6, 8, 5,
		4,  1, 2, 7, 6,
		4,  2, 3, 8, 7,
		4,  3, 4, 9, 8,
		4,  4, 0, 5, 9,
		3,  5, 8, 9,
		3,  6, 7, 8,
	};

	polyinset_test(poly, 5, .5f, 0.0f, ECLAMP, TH, new_coords, 5, indices, 7, 2);
#undef X1
#undef TH
#undef Y1
#undef X2
#undef Y2
}

TEST(polyinset, RectReflexNonPlanar)
{
	/* like RectReflex but with non-planar vert that makes intersection non-exact*/
#define X1 1.7320508075688772f
#define TH 0.46563223004341125f
	const float poly[][3] = {{4, -X1, -1}, {4, X1, -1}, {4, X1, 1}, {4.1f, 0, 0}, {4, -X1, 1}};
	const float new_coords[][3] = {{4.0155125f, -1.2664186f, -0.5346262f},
		{4.0155125f, 1.2664186f, -0.5346262f}, {3.9731839f, 1.2664186f, 0.1955146f},
		{4.0821052f, 0.0000000f, -0.5368460f}, {3.9731839f, -1.2664185f, 0.1955146f}};
	const int indices[] = {
		5,  0, 1, 6, 8, 5,
		4,  1, 2, 7, 6,
		4,  2, 3, 8, 7,
		4,  3, 4, 9, 8,
		4,  4, 0, 5, 9,
		3,  5, 8, 9,
		3,  6, 7, 8,
	};

	polyinset_test(poly, 5, .5f, 0.0f, ECLAMP, TH, new_coords, 5, indices, 7, 2);
#undef X1
#undef TH
#undef Y1
#undef X2
#undef Y2
}

TEST(polyinset, RectReflex2)
{
	/* 11 x 3 rectangle with two notches along top (each 45 degrees down to y=2, then up)  */
	/* TH = 2/(1+sqrt(2)). W = TH/tan(72.5) = 2/(3+2sqrt(2)) */
#define TH 0.8284271247461902f
#define W 0.3431457505076198f
#define Y1 (3.0f - TH)
	const float poly[][3] = {{0, 0, 0}, {11, 0, 0}, {11, 3, 0}, {9, 3, 0}, {8, 2, 0}, {7, 3, 0},
		{4, 3, 0}, {3, 2, 0}, {2, 3, 0}, {0, 3, 0}};
	const float new_coords[][3] = {{TH, TH, 0}, {11 - TH, TH, 0}, {11 - TH, Y1, 0},
		{9 + W, Y1, 0}, {8, TH, 0}, {7 - W, Y1, 0}, {4 + W, Y1, 0}, {3, TH, 0}, {2 - W, Y1, 0}, {TH, Y1, 0}};
	const int indices[] = {
		6,  0, 1, 11, 14, 17, 10,
		4,  1, 2, 12, 11,
		4,  2, 3, 13, 12,
		4,  3, 4, 14, 13,
		4,  4, 5, 15, 14,
		4,  5, 6, 16, 15,
		4,  6, 7, 17, 16,
		4,  7, 8, 18, 17,
		4,  8, 9, 19, 18,
		4,  9, 0, 10, 19,
		4,  10, 17, 18, 19,
		4,  14, 15, 16, 17,
		4,  11, 12, 13, 14,
	};
	
	polyinset_test(poly, 10, 1.0f, 0.0f, ECLAMP, TH, new_coords, 10, indices, 13, 3);
#undef TH
#undef W
#undef Y1
}

TEST(polyinset, Dumbbell)
{
	/* 2x2 squares separated by 1 on x, with 1x1 square bridging them */
	const float poly[][3] = {{0, 0, 0}, {2, 0, 0}, {2, 0.5f, 0}, {3, 0.5f, 0}, {3, 0, 0}, {5, 0, 0},
		{5, 2, 0}, {3, 2, 0}, {3, 1.5, 0}, {2, 1.5, 0}, {2, 2, 0}, {0, 2, 0}};
	const float new_coords[][3] = {{0.5f, 0.5f, 0}, {1.5f, 0.5f, 0}, {1.5f, 1, 0}, {3.5, 1, 0},
		{3.5f, 0.5f, 0}, {4.5f, 0.5f, 0}, {4.5f, 1.5f, 0}, {3.5f, 1.5f, 0}, {1.5f, 1.5f, 0}, {0.5f, 1.5f, 0}};
	const int indices[] = {
		4,  0, 1, 13, 12,
		4,  1, 2, 14, 13,
		4,  2, 3, 15, 14,
		4,  3, 4, 16, 15,
		4,  4, 5, 17, 16,
		4,  5, 6, 18, 17,
		4,  6, 7, 19, 18,
		4,  7, 8, 15, 19,
		4,  8, 9, 14, 15,
		4,  9, 10, 20, 14,
		4,  10, 11, 21, 20,
		4,  11, 0, 12, 21,
		5,  12, 13, 14, 20, 21,
		5,  16, 17, 18, 19, 15,
	};

	polyinset_test(poly, 12, 0.6f, 0.0f, ECLAMP, 0.5f, new_coords, 10, indices, 14, 2);
}