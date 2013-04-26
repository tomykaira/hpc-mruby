#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#define IMAGE_WIDTH 256
#define IMAGE_HEIGHT 256
#define NSUBSAMPLES 2
#define NAO_SAMPLES 8


// you can change this to double if you want
#define FP double

FP xorrand(){
  static int x=123456789, y=362436069, z = 521288629, w = 88675123;
  int BNUM = 1 << 29;
  FP BNUMF = BNUM;
  int t = x ^ ((x & 0xfffff) << 11);
  x=y;
  y=z;
  z=w;
  w=(w ^ (w >> 19) ^ (t ^ (t >> 8)));
  return (w % BNUM) / BNUMF;
}

typedef struct{
  FP x,y,z;
}Vec;


Vec vadd(Vec a, Vec b){
  return (Vec){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec vsub(Vec a, Vec b){
  return (Vec){a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec vcross(Vec a, Vec b){
  return
    (Vec){a.y*b.z - a.z*b.y,
      a.z*b.x - a.x*b.z,
      a.x*b.y - a.y*b.x};
}

FP vdot(Vec a, Vec b){
  return
    a.x * b.x + a.y * b.y + a.z * b.z;
}

FP vlength(Vec a){
  return
    sqrt(a.x*a.x + a.y*a.y + a.z*a.z);
}

Vec vnormalize(Vec a){
  FP len = vlength(a);
  Vec v = a;
  if(len > 1.0e-17) {
    v.x = v.x / len;
    v.y = v.y / len;
    v.z = v.z / len;
  }
  return v;
}


typedef struct{
  Vec org, dir;
}Ray;

typedef struct{
  FP t;
  int hit;
  Vec pl, n;
}Isect;

Isect isect_new(){
  return
    (Isect)
    {10000000.0, 0,
        (Vec){0.0, 0.0, 0.0},
        (Vec){0.0, 0.0, 0.0}};
}


typedef struct{
  Vec center;
  FP radius;
}Sphere;


void sphere_intersect(Sphere *this, Ray ray, Isect *isect){
  
  Vec rs = vsub(ray.org, this->center);
  FP b = vdot(rs, ray.dir);
  FP c = vdot(rs, rs) - (this->radius * this->radius);
  FP d = b * b -c;
  if(d > 0.0) {
    FP t = - b - sqrt(d);

    if( t > 0.0 && t < isect->t) {
      isect->t = t;
      isect->hit = 1;
      isect->pl = (Vec){ray.org.x + ray.dir.x * t,
                        ray.org.y + ray.dir.y * t,
                        ray.org.z + ray.dir.z * t};
      Vec n = vsub(isect->pl, this->center);
      isect->n = vnormalize(n);
    }
  }
}

typedef struct {
  Vec p, n;
}Plane;

void plane_intersect(Plane *this, Ray ray, Isect *isect){
  FP d = -vdot(this->p,this->n);
  FP v = vdot(ray.dir, this->n);
  FP v0 = v;

  if(v < 0.0) {
    v0 = -v;
  }
  if(v0 < 1.0e-17) {
    return;
  }

  FP t = -(vdot(ray.org, this->n) + d) / v;

  if(t > 0.0 && t < isect->t) {
    isect->hit = 1;
    isect->t = t;
    isect->n = this->n;
    isect->pl = (Vec){ray.org.x + t * ray.dir.x,
                       ray.org.y + t * ray.dir.y,
                       ray.org.z + t * ray.dir.z};
  }
}


int clamp(FP f){
  FP i = f * 255.5;
  if(i > 255.0) {
    i = 255.0;
  }
  if(i < 0.0) {
    i = 0.0;
  }
  return i;
}

void otherBasis(Vec *basis,Vec n){
  basis[2] = n;
  basis[1] = (Vec){0.0, 0.0, 0.0};

  if(n.x < 0.6 && n.x > -0.6) {
    basis[1].x = 1.0;
  }else if(n.y < 0.6 && n.y > -0.6) {
    basis[1].y = 1.0;
  }else if(n.z < 0.6 && n.z > -0.6) {
    basis[1].z = 1.0;
  }else {
    basis[1].x = 1.0;
  }

  basis[0] = vcross(basis[1], basis[2]);
  basis[0] = vnormalize(basis[0]);

  basis[1] = vcross(basis[2], basis[0]);
  basis[1] = vnormalize(basis[1]);
}


typedef struct{
  Sphere spheres[3];
  Plane plane;
}Scene;

Scene scene_new(){
  Scene ret;
  ret.spheres[0]=
    (Sphere){(Vec){-2.0, 0.0, -3.5}, 0.5};
  ret.spheres[1]=
    (Sphere){(Vec){-0.5, 0.0, -3.0}, 0.5};
  ret.spheres[2]=
    (Sphere){(Vec){1.0, 0.0, -2.2}, 0.5};
  ret.plane = (Plane){(Vec){0.0, -0.5, 0.0}, (Vec){0.0, 1.0, 0.0}};
  return ret;
}

Vec ambient_occlusion(Scene *this, Isect isect){
  Vec basis[3];
  otherBasis(basis, isect.n);

  int ntheta = NAO_SAMPLES;
  int nphi = NAO_SAMPLES;
  FP eps = 0.0001;
  FP occlusion = 0.0;

  Vec p0 = (Vec){isect.pl.x + eps * isect.n.x,
                 isect.pl.y + eps * isect.n.y,
                 isect.pl.z + eps * isect.n.z};
  int j;
  for(j=0; j < nphi; ++j) {
    int i;
    for(i=0; i < ntheta; ++i) {
      FP r = xorrand();
      FP phi = 2.0 * 3.14159265 * xorrand();
      FP x = cos(phi) * sqrt(1.0 - r);
      FP y = sin(phi) * sqrt(1.0 - r);
      FP z = sqrt(r);

      FP rx = x * basis[0].x + y * basis[1].x + z * basis[2].x;
      FP ry = x * basis[0].y + y * basis[1].y + z * basis[2].y;
      FP rz = x * basis[0].z + y * basis[1].z + z * basis[2].z;

      Vec raydir = (Vec){rx, ry, rz};
      Ray ray = (Ray){p0, raydir};

      Isect occisect = isect_new();
      sphere_intersect(&(this->spheres[0]), ray, &occisect);
      sphere_intersect(&(this->spheres[1]), ray, &occisect);
      sphere_intersect(&(this->spheres[2]), ray, &occisect);
      plane_intersect(&(this->plane), ray, &occisect);

      if(occisect.hit) {
        occlusion = occlusion + 1.0;
      }else {
        //0.0;
      }
    }
  }

  occlusion = ((FP)(ntheta) * (FP)(nphi) - occlusion) / ((FP)(ntheta) * (FP)(nphi));
  return (Vec){occlusion, occlusion, occlusion};
}

void render(Scene *this, int w,int h, int nsubsamples, FILE* fout){
  int cnt = 0;
  FP nsf = nsubsamples;
  int y;
  for(y=0; y < h; ++y){
    int x;
    for(x=0; x < w; ++x){
      Vec rad = (Vec){0.0, 0.0, 0.0};
      int v;
      for(v = 0; v < nsubsamples; ++v){
        int u;
        for(u = 0; u <  nsubsamples; ++u){
          cnt = cnt + 1;
          FP wf = w;
          FP hf = h;
          FP xf = x;
          FP yf = y;
          FP uf = u;
          FP vf = v;

          FP px = (xf + (uf / nsf) - (wf / 2.0)) / (wf / 2.0);
          FP py = -(yf + (vf / nsf) - (hf / 2.0)) / (hf / 2.0);

          Vec eye = vnormalize((Vec){px, py, -1.0});

          Ray ray = (Ray){(Vec){0.0, 0.0, 0.0}, eye};

          Isect isect = isect_new();
          sphere_intersect(&(this->spheres[0]), ray, &isect);
          sphere_intersect(&(this->spheres[1]), ray, &isect);
          sphere_intersect(&(this->spheres[2]), ray, &isect);
          plane_intersect(&(this->plane), ray, &isect);
          if(isect.hit) {
            Vec col = ambient_occlusion(this, isect);
            rad.x = rad.x + col.x;
            rad.y = rad.y + col.y;
            rad.z = rad.z + col.z;
          }else{
            //0.0;
          }
        }
      }

      FP r = rad.x / (nsf * nsf);
      FP g = rad.y / (nsf * nsf);
      FP b = rad.z / (nsf * nsf);
      fprintf(fout, "%c", clamp(r));
      fprintf(fout, "%c", clamp(g));
      fprintf(fout, "%c", clamp(b));
    }
  }
}


int main(){
  FILE *fout = fopen("ao-render-c.ppm", "wb");
  if (fout == NULL)
    exit(EXIT_FAILURE);
  
  fprintf(fout, "P6\n");
  fprintf(fout, "%d %d\n", IMAGE_WIDTH, IMAGE_HEIGHT);
  fprintf(fout, "255\n");// IMAGE_WIDTH, IMAGE_HEIGHT);
  Scene scene=scene_new();
  render(&scene, IMAGE_WIDTH, IMAGE_HEIGHT, NSUBSAMPLES, fout);
  return 0;
}
