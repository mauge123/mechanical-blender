/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/polyinset.c
 *  \ingroup bli
 *
 * Polygon inset, with detection of self-intersections..
 *
 * No globals - keep threadsafe.
 */
 
#include "BLI_utildefines.h"
#include "BLI_math.h"

#include "BLI_memarena.h"
#include "BLI_alloca.h"
#include "BLI_listbase.h"

#include "BLI_polyinset.h"  /* own include */

#include "BLI_strict_flags.h"

#define INFINITE_THICKNESS 1e30f
#define INSET_EPSILON 1e-5f
#define INSET_EPSILON_SQ 1e-10f

#define DEBUG_INSET 0
#if DEBUG_INSET
#define F3(a) a[0], a[1], a[2]
#endif

typedef struct VertexEvent {
	float coords[3];
	unsigned int i;
	unsigned int i_other;
	float lambda;
} VertexEvent;

typedef struct VertexEventListElem {
	struct VertexEventListElem *next, *prev;
	VertexEvent *vev;
} VertexEventListElem;
typedef ListBase VertexEventList;

/* list of vertex indices (sometimes circular) */
typedef struct IndexListElem {
	struct IndexListElem *next, *prev;
	unsigned int index;
} IndexListElem;
typedef ListBase IndexList;

/* list of cycles, each given as a circular list of indices */
typedef struct CycleListElem {
	struct CycleListElem *next, *prev;
	IndexList *cycle_start;
} CycleListElem;
typedef ListBase CycleList;

/* extra per vertex data needed during inset calculation */
typedef struct Vdata {
	float inset_dir[3];  /* normalized inset growth direction from this vertex */
	float inset_fac;     /* factor to multiply thickness and inset direction by */
	float depth_fac;   /* factor to multiply depth and depth direction by */
	bool is_reflex;      /* is the angle reflex in its face? */
	unsigned int mergev;  /* used in inset collision step */
	unsigned int rayv;  /* ditto */
	float ray_end[3];  /* ditto */
	VertexEvent vevent;  /* ditto */
	VertexEventList splits; /* ditto */
} Vdata;


static inline unsigned int next(unsigned int i, unsigned int n)
{
	return (i + 1) % n;
}

static inline unsigned int prev(unsigned int i, unsigned int n)
{
	return (i + n - 1) % n;
}

static int cycle_length(IndexList *ilist)
{
	int n;
	IndexListElem *i_elem;

	n = 0;
	i_elem = ilist->first;
	if (i_elem == NULL)
		return 0;
	do {
		n++;
		i_elem = i_elem->next;
	} while (i_elem != ilist->first);
	return n;
}

#if DEBUG_INSET
/* for debugging */
static void print_cycle(IndexList *cycle)
{
	IndexListElem *i_elem;
	int i;

	i_elem = cycle->first;
	if (i_elem == NULL)
		return;
	i = 0;
	do {
		printf("%d ", i_elem->index);
		i_elem = i_elem->next;
	} while (i++ < 30 && i_elem && i_elem != cycle->first);
	if (i == 30)
		printf(" ...");
	printf("\n");
}

/* for debugging */
static void print_cycles(CycleList *cycles)
{
	CycleListElem *c;

	for (c = cycles->first; c; c = c->next) {
		print_cycle(c->cycle_start);
	}
}
#endif

static void insert_vertex_event(VertexEventList *velist, VertexEvent *ve, struct MemArena *arena)
{
	VertexEventListElem *velelem, *vel_iter;

	velelem = BLI_memarena_calloc(arena, sizeof(*velelem));
	velelem->vev = ve;
	if (BLI_listbase_is_empty(velist)) {
		BLI_addhead(velist, velelem);
	}
	else {
		/* insert so that list remains reverse sorted by lambda */
		for (vel_iter = velist->first; vel_iter != NULL; vel_iter = vel_iter->next) {
			if (velelem->vev->lambda >= vel_iter->vev->lambda) {
				BLI_insertlinkbefore(velist, vel_iter, velelem);
				return;
			}
		}
		/* if get here, lambda of velelem is smallest */
		BLI_addtail(velist, velelem);
	}
}


/* Calculate per vertex Vdata in perparation for inset. */
static void calc_inset_vdata(
	const float (*coords)[3],
	const unsigned int coords_tot,
	const unsigned int flags,
	const float *face_no,
	Vdata *r_vdata)
{
	bool use_relative_offset = flags & INSET_RELATIVE_OFFSET;
	bool clamped = flags & INSET_CLAMPED;
	float tvec[3], edge_no[3], prev_edge_no[3];
	float edge_len, prev_edge_len, relative_fac;
	unsigned int i;

	BLI_assert(coords_tot >= 2);

	if (use_relative_offset)
		prev_edge_len = len_v3v3(coords[coords_tot - 1], coords[0]);
	sub_v3_v3v3(tvec, coords[coords_tot - 1], coords[0]);
	cross_v3_v3v3(prev_edge_no, tvec, face_no);
	normalize_v3(prev_edge_no);

	for (i = 0; i < coords_tot; i++) {
		/* 'edge normal' is perp to edge leaving i and pointing into face */
		sub_v3_v3v3(tvec, coords[i], coords[next(i, coords_tot)]);
		cross_v3_v3v3(edge_no, tvec, face_no);
		normalize_v3(edge_no);

		add_v3_v3v3(r_vdata[i].inset_dir, prev_edge_no, edge_no);
		normalize_v3(r_vdata[i].inset_dir);

		r_vdata[i].inset_fac = 1.0f;
		r_vdata[i].depth_fac = 1.0f;
		r_vdata[i].is_reflex = false;
		if (flags & INSET_EVEN_OFFSET)
			r_vdata[i].inset_fac *= shell_v3v3_mid_normalized_to_dist(prev_edge_no,  edge_no);

		if (use_relative_offset) {
			edge_len = len_v3v3(coords[i], coords[next(i, coords_tot)]);
			relative_fac = 0.5f * (prev_edge_len + edge_len);
			r_vdata[i].depth_fac = relative_fac;
			r_vdata[i].inset_fac *= relative_fac;
			prev_edge_len = edge_len;
		}

		if (clamped) {
			/* some data only needed when doing a clamped calculation */
			cross_v3_v3v3(tvec, edge_no, prev_edge_no);
			r_vdata[i].is_reflex = (dot_v3v3(tvec, face_no) >= 0.0f);
		}
#if DEBUG_INSET
		printf("vdata[%d]: fac=%f reflex=%d dir=(%f,%f,%f) coords=(%f,%f,%f)\n",
			i, r_vdata[i].inset_fac, r_vdata[i].is_reflex, F3(r_vdata[i].inset_dir), F3(coords[i]));
#endif

		copy_v3_v3(prev_edge_no, edge_no);
	}
}

/* Assume p is origin + lambda * dir; recover lambda and return it.
 * TODO: make a version of isect_line_line_v3 that returns lambdas as a byproduct of calculation. */
static float inset_get_lambda(const float p[3], const float origin[3], const float dir[3])
{
	float delta[3];
	int maxi;

	sub_v3_v3v3(delta, p, origin);
	maxi = 0;
	if (fabsf(delta[maxi]) < fabsf(delta[1]))
		maxi = 1;
	if (fabsf(delta[maxi]) < fabsf(delta[2]))
		maxi = 2;
	if (dir[maxi] == 0.0f)
		return INFINITE_THICKNESS;
	return delta[maxi] / dir[maxi];
}

/* Try to intersect rays for given coords and directions.
 * If there is an intersection (any near crossing in the positive directions),
 * return the thickness needed to cause an intersection, 
 * and put the intersection point in *r_intersect.
 * If there is no intersection, return INFINITE_THICKNESS. */
static float isect_inset_rays(
	const float coord1[3],
	const Vdata *v1,
	const float coord2[3],
	const Vdata *v2,
	float r_intersect[3])
{
	float other1[3], other2[3], i1[3], i2[3];
	float t1, t2;
	int isect_kind;

	add_v3_v3v3(other1, coord1, v1->inset_dir);
	add_v3_v3v3(other2, coord2, v2->inset_dir);
	isect_kind = isect_line_line_v3(coord1, other1, coord2, other2, i1, i2);
	if (isect_kind == 0) /* collinear */
		return INFINITE_THICKNESS;

	/* with even offsets and not relative, angle bisector dirs, t1 and t2 should always be about equal */
	t1 = inset_get_lambda(i1, coord1, v1->inset_dir);
	t2 = inset_get_lambda(i2, coord2, v2->inset_dir);
	if (t1 < 0.0f || t2 < 0.0f)
		return INFINITE_THICKNESS;  /* lines diverge in direction of motion */
	if (v1->inset_fac != 0.0f)
		t1 /= v1->inset_fac;
	if (v2->inset_fac != 0.0f)
		t2 /= v2->inset_fac;
	if (t1 >= t2) {
		copy_v3_v3(r_intersect, i1);
		return t1;
	}
	else {
		copy_v3_v3(r_intersect, i2);
		return t2;
	}
}

/* Return thickness at which the 'edge event' happens for edge from vertex i.
 * An edge event is when the advancing rays from adjacent vertices intersect,
 * so that the edge between them shrinks to a point.
 * Since these are 3D coords, we'll count an intersection as the point of closest approach.
 * Allow for the possibility of no intersection (possible for an 'outset') by returning INFINITE_THICKNESS in that case.
 * If there is an event, return the coordinates of the intersection point in r_isect */
static inline float edge_event_time(
	const float (*coords)[3],
	const unsigned int coords_tot,
	const Vdata *vdata,
	const unsigned int i,
	float r_isect[3])
{
	unsigned int j = next(i, coords_tot);
	float t =  isect_inset_rays(coords[i], &vdata[i], coords[j], &vdata[j], r_isect);
	return t;
}

/* Return minimum time at which next edge event happens; INFINITE_THICKNESS if none. */
static float next_edge_event(const float (*coords)[3], const unsigned int coords_tot, const Vdata *vdata)
{
	unsigned int i;
	float ipoint[3];
	float t, min_t;

	min_t = INFINITE_THICKNESS;
	for (i = 0; i < coords_tot; i++) {
		t = edge_event_time(coords, coords_tot, vdata, i, ipoint);
		if (t < min_t)
			min_t = t;
	}
	return min_t;
}

/* Return the thickness at which the 'vertex event' happens for the
 * edge from reflex vertex i, as it intersects the advancing edge
 * between vertex iother and (iother+1)%coords_tot.
 * Allow fo rhte possibility of no intersection by returning INFINITE_THICKNESS
 * in that case.
 * If there is an event, return the coords of the intersection point in r_isect
 *
 * At thickness t, the end of the ray from vertex i is
 *    o + d*s*t
 * where o is coords[i], d is vdata[i].inset_dir, s is vdata[i].inset_fac
 * The advancing edge line has this equation:
 *    oo + od*os*t + p*w
 * where oo, od, os are o, d, s for the ray out of vertex iother,
 * and p is the direction vector parallel to the advancing edge,
 * and w is the parameter that varies to draw the line.
 * Since this is in 3d, they may not intersect but instead be skew.
 * But we want the point of closest approach.
 * At that point, the vector between the point on the advancing ray
 * and the point on the advancing edge should be perpendicular to both lines.
 * Setting the dot product of that difference and each of the direction vectors
 * to zero, we get two equations in two variables (t and w) to solve.
 * | m11 m12 | |t|  =  |b1|
 * | m21 m22 | |w|     |b2|
 */
static float vertex_event_time(
	const float (*coords)[3],
	const unsigned int coords_tot,
	const Vdata *vdata,
	const unsigned int i,
	const unsigned int iother,
	float r_isect[3])
{
	float m11, m12, m21, m22, b1, b2, t, w, den;
	const float *o, *oo, *oonext;
	float p[3];
	unsigned int ionext;
	const Vdata *v, *vother;

	ionext = next(iother, coords_tot);
	o = coords[i];
	oo = coords[iother];
	oonext = coords[ionext];
	v = &vdata[i];
	vother = &vdata[iother];
	sub_v3_v3v3(p, oonext, oo);
	normalize_v3(p);
	m11 = vother->inset_fac * dot_v3v3(vother->inset_dir, v->inset_dir) -
		v->inset_fac * dot_v3v3(v->inset_dir, v->inset_dir);
	m12 = dot_v3v3(p, v->inset_dir);
	m21 = vother->inset_fac * dot_v3v3(vother->inset_dir, p) -
		v->inset_fac * dot_v3v3(v->inset_dir, p);
	m22 = dot_v3v3(p, p);
	b1 = dot_v3v3(o, v->inset_dir) - dot_v3v3(oo, v->inset_dir);
	b2 = dot_v3v3(o, p) - dot_v3v3(oo, p);
	den = m11 * m22 - m12 * m21;
	t = w = -1.0f;
	if (fabsf(den) > INSET_EPSILON) {
		t = (m22 * b1 - m12 * b2) / den;
		w = (m11 * b2 - m21 * b1) / den;
	}
	/* if intersection is in backwards direction, or division would have blown up, no intersection */
	if (t < 0.0f || w < 0.0f)
		return INFINITE_THICKNESS;
	/* see if intersection on edge is beyond oonext */
	if (w > len_v3v3(oo, oonext))
		return INFINITE_THICKNESS;
	madd_v3_v3v3fl(r_isect, o, v->inset_dir, v->inset_fac * t);
	return t;
}

/* Return minimum time at which next vertex event happens; INFINITE_THICKNESS if none */
static float next_vertex_event(const float (*coords)[3], const unsigned int coords_tot, const Vdata *vdata)
{
	unsigned int i, iother;
	float ipoint[3];
	float t, min_t;
	
	min_t = INFINITE_THICKNESS;
	for (i = 0; i < coords_tot; i++) {
		if (!vdata[i].is_reflex)
			continue;
		for (iother = 0; iother < coords_tot; iother++) {
			if (iother == i || iother == prev(i, coords_tot))
				continue;
			t = vertex_event_time(coords, coords_tot, vdata, i, iother, ipoint);
			if (t < min_t)
				min_t = t;
		}
	}
	return min_t;

}

/* Given that the first collision event happens at given thickness,
 * gather together all events that happen at that thickness, and
 * fill in *r_result appropriately.
 * See literature on Straight Skeletons (Aichholzer et al.; Eppstein and Erickson; etc.
 * for the ideas and terminology around how to calculate collisions and modify the topology accordingly. */
static void inset_collision_step(
	const float (*coords)[3],
	const unsigned int coords_tot,
	const float thickness,
	const float depth,
	const float *fno,
	Vdata *vdata,
	InsetResult *r_result,
	struct MemArena *arena)
{
	unsigned int i, j, k, n, inext, jnext, rayvi, prevrayvi;
	float t, ipoint[3], dir[3], lambda;
	Vdata *vd, *vdj;
	IndexList *cycle, *new_cycle;
	IndexListElem *i_elem, *cycle_elem, *p, *q;
	VertexEventListElem *ve_elem;
	CycleList cycles;
	CycleListElem *cycles_elem, *new_cycles_elem;
	InsetFace *iface;
	bool is_tri, changed;
	int num_splits;
	
	BLI_assert(coords_tot > 2);
	
	/* The following fields of vdata[i] are used by this function:
	 * mergev will be lowest-numbered vert index for all whose ray ends will be
	 * at the same coordinate.
	 * splits will be a list of VertEvent pointers for verts whose rays are strictly within
	 * the segment between (end of ray i, end of ray next(i)), ordered backwards
	 * along the segment.
	 * ray_end is coords of ray at time t, sometimes modified slightly if at an event.
	 * vevent will have lambda != -1.0f if there is a vertex event caused by ray i.
	 * rayv will be the index of the new coord that will be at the end of the ray
	 */
	for (i = 0; i < coords_tot; i++) {
		vd = &vdata[i];
		vd->mergev = i;
		madd_v3_v3v3fl(vd->ray_end, coords[i], vd->inset_dir, thickness * vd->inset_fac);
		if (depth != 0.0f)
			madd_v3_v3fl(vd->ray_end, fno, depth * vd->depth_fac);
		vd->vevent.lambda = -1.0f;
		BLI_listbase_clear(&vd->splits);
	}
	/* do merges due to edge events */
	for (i = 0; i < coords_tot; i++) {
		vd = &vdata[i];
		t = edge_event_time(coords, coords_tot, vdata, i, ipoint);
		if (fabsf(t - thickness) < INSET_EPSILON) {
			/* inside edge from mergev[i] to mergev[(i+1) % coords_tot] collapes */
			vdata[next(i, coords_tot)].mergev = vd->mergev;
			copy_v3_v3(vd->ray_end, ipoint);
			if (depth != 0.0f)
				madd_v3_v3fl(vd->ray_end, fno, depth * vd->depth_fac);
		}
	}
	/* fix possible wraparound to 0 */
	if (vdata[0].mergev != 0) {
		for (i = coords_tot - 1; i > 0; i--) {
			if (vdata[i].mergev == vdata[0].mergev)
				vdata[i].mergev = 0;
			else
				break;
		}
		vdata[0].mergev = 0;
	}
	/* now vertex events */
	for (i = 0; i < coords_tot; i++) {
		vd = &vdata[i];
		if (!vd->is_reflex)
			continue;
		for (j = 0; j < coords_tot; j++) {
			if (j == i || j == prev(i, coords_tot))
				continue;
			t = vertex_event_time(coords, coords_tot, vdata, i, j, ipoint);
			if (fabsf(t - thickness) < INSET_EPSILON) {
				jnext = next(j, coords_tot);
				sub_v3_v3v3(dir, coords[jnext], coords[j]);
				lambda = inset_get_lambda(ipoint, coords[j], dir);
				if (fabsf(lambda) < INSET_EPSILON)
					lambda = 0.0f;
				else if (fabsf(1.0f - lambda) < INSET_EPSILON)
					lambda = 1.0f;
				if (lambda >= 0.0f && lambda <= 1.0f) {
					copy_v3_v3(vd->vevent.coords, ipoint);
					vd->vevent.i = i;
					vd->vevent.i_other = j;
					vd->vevent.lambda = lambda;
					if (lambda == 0.0f) {
						/* treat as merge */
						if (i < j)
							vdata[j].mergev = vd->mergev;
						else
							vd->mergev = vdata[j].mergev;
					}
					else if (lambda == 1.0f) {
						if (i < jnext)
							vdata[jnext].mergev = vd->mergev;
						else
							vd->mergev = vdata[jnext].mergev;
					}
					else {
						insert_vertex_event(&vdata[j].splits, &vd->vevent, arena);
					}
				}
			}
		}
	}
	/* now find any cases where non-contiguous rays need to merge */
	for (i = 0; i < coords_tot; i++) {
		vd = &vdata[i];
		if (vd->mergev != i)
			continue;  /* only need consider canonical i's */
		for (j = i + 2; j < coords_tot; j++) {
			vdj = &vdata[j];
			if (j == prev(i, coords_tot) || j == next(i, coords_tot) || vdj->mergev == i || vdj->mergev != j)
				continue;
			if (len_squared_v3v3(vd->ray_end, vdata[j].ray_end) < INSET_EPSILON_SQ) {
				/* merge j (and anything merged with it) into i */
				for (k = 0; k < coords_tot; k++) {
					if (vdata[k].mergev == vdj->mergev)
						vdata[k].mergev = vd->mergev;
				}
			}
		}
	}
#if DEBUG_INSET
	printf("final merge values:\n");
	for (i = 0; i < coords_tot; i++) {
		printf("%d -> %d\n", i, vdata[i].mergev);
	}
#endif
	/* now can assign indices for new inner vertices. first assign indices for canonical rays */
	j = coords_tot;
	for (i = 0; i < coords_tot; i++) {
		if (vdata[i].mergev == i) {
			vdata[i].rayv = j;
			copy_v3_v3(r_result->vert_coords[j], vdata[i].ray_end);
			j++;
		}
	}
	r_result->vert_tot = j;
	/* now assign vertices for non-canonical rays */
	for (i = 0; i < coords_tot; i++) {
		k = vdata[i].mergev;
		if (k != i) {
			BLI_assert(vdata[k].mergev == k);
			vdata[i].rayv = vdata[k].rayv;
		}
	}
	/* now make initial inner-poly cycle */
	cycle = BLI_memarena_calloc(arena, sizeof(*cycle));
	BLI_listbase_clear(cycle);
	prevrayvi = r_result->vert_tot;  /* won't match anything */
	for (i = 0; i < coords_tot; i++) {
		vd = &vdata[i];
		rayvi = vd->rayv;
		num_splits = BLI_listbase_count(&vd->splits);
		if (rayvi != prevrayvi && (i == 0 || rayvi != vdata[0].rayv)) {
			i_elem = BLI_memarena_calloc(arena, sizeof(*i_elem));
			i_elem->index = rayvi;
			BLI_addtail(cycle, i_elem);
		}
		if (num_splits > 0) {
			for (j = 0, ve_elem = vd->splits.last; j < (unsigned int)num_splits; j++, ve_elem = ve_elem->prev) {
				i_elem = BLI_memarena_calloc(arena, sizeof(*i_elem));
				i_elem->index = vdata[ve_elem->vev->i].rayv;
				BLI_addtail(cycle, i_elem);
			}
		}
		prevrayvi = rayvi;
	}
	/* make it circular and perhaps add it to cycles */
	((IndexListElem *)cycle->last)->next = cycle->first;
	((IndexListElem *)cycle->first)->prev = cycle->last;
	BLI_listbase_clear(&cycles);
	if (cycle_length(cycle) >= 3) {
		cycles_elem = BLI_memarena_alloc(arena, sizeof(*cycles_elem));
		cycles_elem->cycle_start = cycle;
		BLI_addtail(&cycles, cycles_elem);
	}

#if DEBUG_INSET
	printf("initial cycles\n"); print_cycles(&cycles);
#endif

	/* Simplify and split cycles: repeat until no change:
	 *   Find two elements p and q in cycle such that p->index = q->index.
	 *   Split the cycle there by swapping next and prev pointers among p and q.
	 *   Only keep cycles whose length is >= 3.
	 */
	changed = true;
	while (changed) {
		changed = false;
		for (cycles_elem = cycles.first; cycles_elem; cycles_elem = cycles_elem->next) {
			cycle = cycles_elem->cycle_start;
			n = (unsigned int) cycle_length(cycle);
			for (i = 0, p = cycle->first; i < n; i++, p = p->next) {
				for (j = i + 1, q = p->next; j < n; j++, q = q->next) {
					if (p->index == q->index) {
						if (p->next == q || q->next == p) {
							/* just remove q */
							BLI_remlink(cycle, q);
						}
						else {
							/* swap next pointers, fix prev ones, and new cycle at old p->next == new q->next */
							cycle_elem = p->next;
							p->next = q->next;
							q->next = cycle_elem;
							q->next->prev = q;
							p->next->prev = p;
							new_cycle = BLI_memarena_alloc(arena, sizeof(*new_cycle));
							BLI_listbase_clear(new_cycle);
							new_cycle->first = q->next;
							new_cycle->last = q;
							if (cycle_length(new_cycle) >= 3) {
								new_cycles_elem = BLI_memarena_alloc(arena, sizeof(*cycles_elem));
								new_cycles_elem->cycle_start = new_cycle;
								BLI_addtail(&cycles, new_cycles_elem);
							}
							/* remove original cycle if it is now too small */
							if (cycle_length(cycle) < 3) {
								BLI_remlink(&cycles, cycles_elem);
							}
						}
						changed = true;
						break;
					}
				}
				if (changed)
					break;
			}
		}
	}
#if DEBUG_INSET
	printf("final cycles\n"); print_cycles(&cycles);
#endif

	/* now make polys */
	r_result->inner_tot = (unsigned int) BLI_listbase_count(&cycles);
	r_result->poly_tot = coords_tot + r_result->inner_tot;
	r_result->polys = BLI_memarena_alloc(arena, sizeof(*r_result->polys) * r_result->poly_tot);

	for (i = 0; i < coords_tot; i++) {
		vd = &vdata[i];
		inext = (i + 1) % coords_tot;
		iface = &r_result->polys[i];
		is_tri = vd->mergev == vdata[inext].mergev;
		num_splits = BLI_listbase_count(&vd->splits);
		iface->vert_tot = (unsigned int) ((is_tri ? 3 : 4) + num_splits);
		/* TODO: maybe have one big index buffer to avoid multiple allocs here */
		iface->vert_indices = BLI_memarena_alloc(arena, sizeof(*iface->vert_indices) * iface->vert_tot );
		iface->vert_indices[0] = i;
		iface->vert_indices[1] = inext;
		if (num_splits > 0) {
			BLI_assert(!is_tri);
			iface->vert_indices[2] = vdata[inext].rayv;
			for (j = 0, ve_elem = vd->splits.first; j < (unsigned int)num_splits; j++, ve_elem = ve_elem->next) {
				iface->vert_indices[3 + j] = vdata[ve_elem->vev->i].rayv;
			}
			iface->vert_indices[3 + num_splits] = vd->rayv;
		}
		else if (is_tri) {
			iface->vert_indices[2] = vd->rayv;
		}
		else {
			iface->vert_indices[2] = vdata[inext].rayv;
			iface->vert_indices[3] = vd->rayv;
		}
	}

	for (cycles_elem = cycles.first; cycles_elem; i++, cycles_elem = cycles_elem->next) {
		cycle = cycles_elem->cycle_start;
		iface = &r_result->polys[i];
		iface->vert_tot = (unsigned int) cycle_length(cycle);
		iface->vert_indices = BLI_memarena_alloc(arena, sizeof(*iface->vert_indices) * iface->vert_tot);
		for (j = 0, cycle_elem = cycle->first; j < iface->vert_tot; j++, cycle_elem = cycle_elem->next) {
			iface->vert_indices[j] = cycle_elem->index;
		}
	}
}

/* The simple case where the inset polygon has the same number of verts as the original one */
static void no_collision_result(
	const float (*coords)[3],
	const unsigned int coords_tot,
	const float thickness,
	const float depth,
	const float *fno,
	Vdata *vdata,
	InsetResult *r_result,

	struct MemArena *arena)
{
	unsigned int i, j, inext;
	InsetFace *iface;

	r_result->vert_tot = 2 * coords_tot;
	r_result->poly_tot = coords_tot + 1;
	r_result->inner_tot = 1;
	r_result->polys = BLI_memarena_alloc(arena, sizeof(r_result->polys[0]) * r_result->poly_tot);

	for (i = 0; i < coords_tot; i++) {
		j = i + coords_tot;
		inext = next(i, coords_tot);
		madd_v3_v3v3fl(r_result->vert_coords[j], coords[i], vdata[i].inset_dir, thickness * vdata[i].inset_fac);
		if (depth != 0.0f)
			madd_v3_v3fl(r_result->vert_coords[j], fno, depth * vdata[i].depth_fac);

		iface = &r_result->polys[i];
		iface->vert_tot = 4;
		/* TODO: maybe have one big index buffer here to avoid multiple allocs */
		iface->vert_indices = BLI_memarena_alloc(arena, sizeof(iface->vert_indices[0]) * iface->vert_tot );
		iface->vert_indices[0] = i;
		iface->vert_indices[1] = inext;
		iface->vert_indices[2] = coords_tot + inext;
		iface->vert_indices[3] = coords_tot + i;
	}

	iface = &r_result->polys[r_result->poly_tot - 1];
	iface->vert_tot = coords_tot;
	iface->vert_indices = BLI_memarena_alloc(arena, sizeof(*iface->vert_indices) * iface->vert_tot );
	for (i = 0; i < coords_tot; i++) {
		iface->vert_indices[i] = coords_tot + i;
	}
}

/* Calculate the coordinates of the inset polygon with given coords.
 * The thickness argument specifies how much to move in the (average) plane of the face:
 *      With no flags, it is amount to move vertices along the angle bisectors.
 *      With INSET_EVEN_OFFSET on the flags, the amount is modified for each corner
 *          so that the perpendicular distance from inset edges to their originals is thickness.
 *      With INSET_RELATIVE_OFFSET, the amount is further multiplied by the average of the
 *          two edges adjacent to the vertex.
 * The depth parameter gives additional vertical movement, in the direction of the average face normal
 * (also scaled by the average adjacent edge length, if INSET_RELATIVE_OFFSET).
 *
 * If INSET_CLAMPED is on flags, stop when inset reaches a self-intersection point,
 * and return the amount of thickness when that happens.
 *
 * The answer is return in *r_result, which will have data allocated in the arena argument.
 * (So the caller is reponsible for freeing the space by freeing the arena.)
 */
float BLI_polyinset3d(
	const float (*coords)[3],
	const unsigned int coords_tot,
	const float thickness,
	const float depth,
	const unsigned int flags,
	InsetResult *r_result,

	struct MemArena *arena)
{
	float fno[3];
	float t, te, tv, th, dp;
	unsigned int i;
	Vdata *vdata = BLI_memarena_alloc(arena, sizeof(*vdata) * coords_tot);

	/* single polygon inset requires at most coords_tot extra vertices if unclamped or stops at first clamp */
	r_result->vert_coords = BLI_memarena_alloc(arena, sizeof(r_result->vert_coords[0]) * 2 * coords_tot);
	for (i = 0; i < coords_tot; i++)
		copy_v3_v3(r_result->vert_coords[i], coords[i]);

	if (coords_tot < 3) {
		/* no non-degenerate polygon to inset */
		r_result->vert_tot = coords_tot;
		r_result->poly_tot = 0;
		r_result->polys = NULL;
		return 0.0f;
	}

	/* Calculate the face normal.
	 * Using the face normal to use for edge tangents is maybe not the best
	 * if the face is very non-planar; yet doing corner-local tangents also has its problems. */
	normal_poly_v3(fno, coords, coords_tot);

	calc_inset_vdata(coords, coords_tot, flags, fno, vdata);

	th = thickness;
	/* when not even or when relative, the intersection events get a lot more complicated,
	 * so don't handle clamp for those */
	if ((flags & INSET_CLAMPED) && (flags & INSET_EVEN_OFFSET) && !(flags & INSET_RELATIVE_OFFSET)) {
		te = next_edge_event(coords, coords_tot, vdata);
		tv = next_vertex_event(coords, coords_tot, vdata);
		t = min_ff(te, tv);
		if (t <= thickness) {
			th = t;
			/* if not going all the way, need to pro-rate depth for distance we are going */
			dp = depth * (t / thickness);
			inset_collision_step(coords, coords_tot, th, dp, fno, vdata, r_result, arena);
		}
		else {
			no_collision_result(coords, coords_tot, th, depth, fno, vdata, r_result, arena);
		}
	}
	else {
		no_collision_result(coords, coords_tot, th, depth, fno, vdata, r_result, arena);
	}

	return th;
}
