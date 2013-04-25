/*
AO render benchmark
Original code Processing language in by Syoyo Fujita
  http://lucille.atso-net.jp/aobench/

ao4_c.c, a porting project "aobench" by syoyo to C,
modified by leonardo maffi, V.1.1, Apr 20 2011.

You can use doubles instead of floats changing a #define.
I have removed the dynamic allocations of the arrays because they are useless.
Now I think the program uses no dynamic heap allocations.
A small gray-scale-only PPM too can be saved.

Best compilation, GCC 4.6:

gcc -Wall -Wextra -Ofast -s -fomit-frame-pointer -msse3 -march=core2 -ffast-math -fwhole-program -ftrapv -mfpmath=sse -std=c99 ao_c_double.c -o ao_c_double
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#define IMAGE_WIDTH 256
#define IMAGE_HEIGHT 256
#define NSUBSAMPLES 2
#define NAO_SAMPLES 8


#define PI_FP 3.14159265358979323846264338327950288

// you can change this to double if you want
#define FP double


#ifndef drand48
static inline FP drand48() { // if not available
    return rand() / (FP)RAND_MAX;
}
#endif

typedef struct vec {
    FP x, y, z;
} vec;


typedef struct Isect {
    FP t;
    vec p, n;
    int hit;
} Isect;


typedef struct Sphere {
    vec center;
    FP radius;
} Sphere;


typedef struct Plane {
    vec p, n;
} Plane;


typedef struct Ray {
    vec org, dir;
} Ray;


// Scene data
// Sphere                  {{center.x, center.y, center.z}, radius}
const Sphere spheres[3] = {{{-2.0, 0.0, -3.5}, 0.5},
                           {{-0.5, 0.0, -3.0}, 0.5},
                           {{1.0, 0.0, -2.2},  0.5}};

//Plane             {{p.x, p.y, p.z},  {n.x, n.y, n.z}}
const Plane plane = {{0.0, -0.5, 0.0}, {0.0, 1.0, 0.0}};


static inline FP vdot(vec v0, vec v1) {
    return v0.x * v1.x + v0.y * v1.y + v0.z * v1.z;
}

static inline void vcross(vec *c, vec v0, vec v1) {
    c->x = v0.y * v1.z - v0.z * v1.y;
    c->y = v0.z * v1.x - v0.x * v1.z;
    c->z = v0.x * v1.y - v0.y * v1.x;
}

static inline void vnormalize(vec *c) {
    FP length = sqrt(vdot((*c), (*c)));

    if (fabs(length) > 1.0e-17) {
        c->x /= length;
        c->y /= length;
        c->z /= length;
    }
}


void ray_sphere_intersect(Isect *restrict isect,
                          const Ray *restrict ray,
                          const Sphere *restrict sphere) {
    vec rs;
    rs.x = ray->org.x - sphere->center.x;
    rs.y = ray->org.y - sphere->center.y;
    rs.z = ray->org.z - sphere->center.z;

    FP B = vdot(rs, ray->dir);
    FP C = vdot(rs, rs) - sphere->radius * sphere->radius;
    FP D = B * B - C;

    if (D > 0.0) {
        FP t = -B - sqrt(D);

        if (t > 0.0 && t < isect->t) {
            isect->t = t;
            isect->hit = 1;

            isect->p.x = ray->org.x + ray->dir.x * t;
            isect->p.y = ray->org.y + ray->dir.y * t;
            isect->p.z = ray->org.z + ray->dir.z * t;

            isect->n.x = isect->p.x - sphere->center.x;
            isect->n.y = isect->p.y - sphere->center.y;
            isect->n.z = isect->p.z - sphere->center.z;

            vnormalize(&(isect->n));
        }
    }
}


void ray_plane_intersect(Isect *restrict isect, const Ray *ray, const Plane *restrict mplane) {
    FP d = -vdot(mplane->p, mplane->n);
    FP v = vdot(ray->dir, mplane->n);

    if (fabs(v) < 1.0e-17)
        return;

    FP t = -(vdot(ray->org, mplane->n) + d) / v;

    if (t > 0.0 && (t < isect->t)) {
        isect->t = t;
        isect->hit = 1;

        isect->p.x = ray->org.x + ray->dir.x * t;
        isect->p.y = ray->org.y + ray->dir.y * t;
        isect->p.z = ray->org.z + ray->dir.z * t;

        isect->n = mplane->n;
    }
}


void ortho_basis(vec *basis, vec n) {
    basis[2] = n;
    basis[1].x = 0.0; basis[1].y = 0.0; basis[1].z = 0.0;

    if (n.x < 0.6 && n.x > -0.6)
        basis[1].x = 1.0;
    else if ((n.y < 0.6) && (n.y > -0.6))
        basis[1].y = 1.0;
    else if ((n.z < 0.6) && (n.z > -0.6))
        basis[1].z = 1.0;
    else
        basis[1].x = 1.0;

    vcross(&basis[0], basis[1], basis[2]);
    vnormalize(&basis[0]);

    vcross(&basis[1], basis[2], basis[0]);
    vnormalize(&basis[1]);
}


void ambient_occlusion(vec *restrict col, const Isect *restrict isect) {
    int i, j;
    int ntheta = NAO_SAMPLES;
    int nphi   = NAO_SAMPLES;
    FP eps = 0.0001;
    vec p;

    p.x = isect->p.x + eps * isect->n.x;
    p.y = isect->p.y + eps * isect->n.y;
    p.z = isect->p.z + eps * isect->n.z;

    vec basis[3];
    ortho_basis(basis, isect->n);

    FP occlusion = 0.0;

    for (j = 0; j < ntheta; j++) {
        for (i = 0; i < nphi; i++) {
            FP theta = sqrt(drand48());
            FP phi   = 2.0 * PI_FP * drand48();

            FP x = cos(phi) * theta;
            FP y = sin(phi) * theta;
            FP z = sqrt(1.0 - theta * theta);

            // local -> global
            FP rx = x * basis[0].x + y * basis[1].x + z * basis[2].x;
            FP ry = x * basis[0].y + y * basis[1].y + z * basis[2].y;
            FP rz = x * basis[0].z + y * basis[1].z + z * basis[2].z;

            Ray ray;

            ray.org = p;
            ray.dir.x = rx;
            ray.dir.y = ry;
            ray.dir.z = rz;

            Isect occIsect;
            occIsect.t   = 1.0e+17;
            occIsect.hit = 0;

            ray_sphere_intersect(&occIsect, &ray, &spheres[0]);
            ray_sphere_intersect(&occIsect, &ray, &spheres[1]);
            ray_sphere_intersect(&occIsect, &ray, &spheres[2]);
            ray_plane_intersect (&occIsect, &ray, &plane);

            if (occIsect.hit)
                occlusion += 1.0;
        }
    }

    occlusion = (ntheta * nphi - occlusion) / (FP)(ntheta * nphi);
    col->x = occlusion;
    col->y = occlusion;
    col->z = occlusion;
}

__attribute__((const))
inline static unsigned char clamp(FP f) {
    int i = (int)(f * 255.5);
    if (i < 0)
        i = 0;
    if (i > 255)
        i = 255;
    return (unsigned char)i;
}


void render(FILE *fout) {
    int x, y, u, v;
    FP nsubs = (FP)(NSUBSAMPLES * NSUBSAMPLES);

    for (y = 0; y < IMAGE_HEIGHT; y++) {
        for (x = 0; x < IMAGE_WIDTH; x++) {
            FP rfp = 0.0;
            FP gfp = 0.0;
            FP bfp = 0.0;

            for (v = 0; v < NSUBSAMPLES; v++) {
                for (u = 0; u < NSUBSAMPLES; u++) {
                    FP px = (x + (u / (FP)NSUBSAMPLES) - (IMAGE_WIDTH / 2.0)) / (IMAGE_WIDTH / 2.0);
                    FP py = -(y + (v / (FP)NSUBSAMPLES) - (IMAGE_HEIGHT / 2.0)) / (IMAGE_HEIGHT / 2.0);

                    Ray ray;

                    ray.org.x = 0.0;
                    ray.org.y = 0.0;
                    ray.org.z = 0.0;

                    ray.dir.x = px;
                    ray.dir.y = py;
                    ray.dir.z = -1.0;
                    vnormalize(&(ray.dir));

                    Isect isect;
                    isect.t   = 1.0e+17;
                    isect.hit = 0;

                    ray_sphere_intersect(&isect, &ray, &spheres[0]);
                    ray_sphere_intersect(&isect, &ray, &spheres[1]);
                    ray_sphere_intersect(&isect, &ray, &spheres[2]);
                    ray_plane_intersect (&isect, &ray, &plane);

                    if (isect.hit) {
                        vec col;
                        ambient_occlusion(&col, &isect);
                        rfp += col.x;
                        gfp += col.y;
                        bfp += col.z;
                    }
                }
            }

            putc(clamp(rfp / nsubs), fout);
            putc(clamp(gfp / nsubs), fout);
            putc(clamp(bfp / nsubs), fout);
        }
    }
}

int main() {
    FILE *fout = fopen("ao_c_double.ppm", "wb");
    if (fout == NULL)
        exit(EXIT_FAILURE);

    fprintf(fout, "P6\n");
    fprintf(fout, "%d %d\n", IMAGE_WIDTH, IMAGE_HEIGHT);
    fprintf(fout, "255\n");

    render(fout);
    fclose(fout);

    exit(EXIT_SUCCESS);
}
