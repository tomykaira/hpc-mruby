/*
AO render benchmark
Original code Processing language in by Syoyo Fujita
  http://lucille.atso-net.jp/aobench/

AO.java, a porting to Java from the Processing version, V.1.1, Mar 27 2008.

I have removed the dynamic allocations of the arrays because they are useless.
A small gray-scale-only PPM too can be saved.

This Java version is very naive, it's probably easy to speed it up.
*/

import java.io.*;
import java.math.*;

final class Vec {
    float x, y, z;

    Vec(float x, float y, float z) {
        this.x = x;
        this.y = y;
        this.z = z;
    }

    Vec(Vec v) {
        this.x = v.x;
        this.y = v.y;
        this.z = v.z;
    }

    Vec add(Vec b) {
        return new Vec(this.x + b.x, this.y + b.y, this.z + b.z);
    }

    Vec sub(Vec b) {
        return new Vec(this.x - b.x, this.y - b.y, this.z - b.z);
    }

    Vec cross(Vec b) {
        float u = this.y * b.z - b.y * this.z;
        float v = this.z * b.x - b.z * this.x;
        float w = this.x * b.y - b.x * this.y;
        return new Vec(u, v, w);
    }

    void normalize() {
        float d = this.len();

        if (Math.abs(d) > 1.0e-6) {
            float invlen = 1.0f / d;
            this.x *= invlen;
            this.y *= invlen;
            this.z *= invlen;
        }
    }

    float len() {
        float d = this.x * this.x + this.y * this.y + this.z * this.z;
        return (float)Math.sqrt(d);
    }

    float dot(Vec b) {
        return this.x * b.x + this.y * b.y + this.z * b.z;
    }

    Vec neg() {
        return new Vec(-this.x, -this.y, -this.z);
    }
}


final class Ray {
    Vec org, dir;

    Ray(Vec o, Vec d) {
        org = o;
        dir = d;
    }
}


final class Intersection {
    float t;
    Vec p; // hit point
    Vec n; // normal
    boolean hit;

    Intersection() {
        hit = false;
        t = 1.0e+30f;
        n = new Vec(0.0f, 0.0f, 0.0f);
    }
}


final class Sphere {
    Vec center;
    float radius;

    Sphere(Vec center, float radius) {
        this.center = center;
        this.radius = radius;
    }

    void intersect(Intersection isect, Ray ray) {
        Vec rs = ray.org.sub(this.center);
        float B = rs.dot(ray.dir);
        float C = rs.dot(rs) - (this.radius * this.radius);
        float D = B * B - C;

        if (D > 0.0) {
            float t = -B - (float)Math.sqrt(D);
            if ((t > 0.0) && (t < isect.t)) {
                isect.t = t;
                isect.hit = true;

                // calculate normal.
                Vec p = new Vec(ray.org.x + ray.dir.x * t,
                                ray.org.y + ray.dir.y * t,
                                ray.org.z + ray.dir.z * t);
                Vec n = p.sub(center);
                n.normalize();
                isect.n = n;
                isect.p = p;
            }
        }
    }
}


final class Plane {
    Vec p; // point on the plane
    Vec n; // normal to the plane

    Plane(Vec p, Vec n) {
        this.p = p;
        this.n = n;
    }

    void intersect(Intersection isect, Ray ray) {
        float d = -p.dot(n);
        float v = ray.dir.dot(n);

        if (Math.abs(v) < 1.0e-6)
            return; // the plane is parallel to the ray.

        float t = -(ray.org.dot(n) + d) / v;

        if ((t > 0) && (t < isect.t)) {
            isect.hit = true;
            isect.t = t;
            isect.n = n;

            Vec p = new Vec(ray.org.x + t * ray.dir.x,
                            ray.org.y + t * ray.dir.y,
                            ray.org.z + t * ray.dir.z);
            isect.p = p;
        }
    }
}


final public class AO {
    // render settings
    final int IMAGE_WIDTH = 256;
    final int IMAGE_HEIGHT = 256;
    final int NSUBSAMPLES = 2;
    final int NAO_SAMPLES = 8; // # of sample rays for ambient occlusion

    // scene data
    Sphere spheres[];
    Plane plane;

    void setup() {
        spheres = new Sphere[3];
        spheres[0] = new Sphere(new Vec(-2.0f, 0.0f, -3.5f), 0.5f);
        spheres[1] = new Sphere(new Vec(-0.5f, 0.0f, -3.0f), 0.5f);
        spheres[2] = new Sphere(new Vec(1.0f, 0.0f, -2.2f), 0.5f);
        plane = new Plane(new Vec(0.0f, -0.5f, 0.0f), new Vec(0.0f, 1.0f, 0.0f));
    }

    char clamp(float f) {
        int i = (int)(f * 255.5);
        if (i < 0)
            i = 0;
        if (i > 255)
            i = 255;
        return (char)i;

    }

    void orthoBasis(Vec basis[], Vec n) {
        int i;
        basis[2] = new Vec(n);
        basis[1] = new Vec(0.0f, 0.0f, 0.0f);

        if ((n.x < 0.6) && (n.x > -0.6)) {
            basis[1].x = 1.0f;
        } else if ((n.y < 0.6) && (n.y > -0.6)) {
            basis[1].y = 1.0f;
        } else if ((n.z < 0.6) && (n.z > -0.6)) {
            basis[1].z = 1.0f;
        } else {
            basis[1].x = 1.0f;
        }

        basis[0] = basis[1].cross(basis[2]);
        basis[0].normalize();

        basis[1] = basis[2].cross(basis[0]);
        basis[1].normalize();
    }

    Vec ambientOcclusion(Intersection isect) {
        int i, j;
        int ntheta = NAO_SAMPLES;
        int nphi = NAO_SAMPLES;
        float eps = 0.0001f;

        // Slightly move ray org towards ray dir to avoid numerical probrem.
        Vec p = new Vec(isect.p.x + eps * isect.n.x,
                        isect.p.y + eps * isect.n.y,
                        isect.p.z + eps * isect.n.z);

        // Calculate orthogonal basis.
        Vec basis[]; basis = new Vec[3];
        orthoBasis(basis, isect.n);

        float occlusion = 0.0f;

        for (j = 0; j < ntheta; j++) {
            for (i = 0; i < nphi; i++) {
                // Pick a random ray direction with importance sampling.
                float r = (float)Math.random();
                float phi = 2.0f * (float)Math.PI * (float)Math.random();

                float sq = (float)Math.sqrt(1.0 - r);
                float x = (float)Math.cos(phi) * sq;
                float y = (float)Math.sin(phi) * sq;
                float z = (float)Math.sqrt(r);

                // local -> global
                float rx = x * basis[0].x + y * basis[1].x + z * basis[2].x;
                float ry = x * basis[0].y + y * basis[1].y + z * basis[2].y;
                float rz = x * basis[0].z + y * basis[1].z + z * basis[2].z;

                Vec raydir = new Vec(rx, ry, rz);
                Ray ray = new Ray(p, raydir);

                Intersection occIsect = new Intersection();
                spheres[0].intersect(occIsect, ray);
                spheres[1].intersect(occIsect, ray);
                spheres[2].intersect(occIsect, ray);
                plane.intersect(occIsect, ray);

                if (occIsect.hit)
                    occlusion += 1.0;
            }
        }

        // [0.0, 1.0]
        occlusion = (ntheta * nphi - occlusion) / (float)(ntheta * nphi);
        return new Vec(occlusion, occlusion, occlusion);
    }

    void rowRender(int[] row, int width, int height, int y, int nsubsamples) {
        for (int x = 0; x < width; x++) {
            float r = 0.0f;
            float g = 0.0f;
            float b = 0.0f;

            // subsampling
            for (int v = 0; v < nsubsamples; v++) {
                for (int u = 0; u < nsubsamples; u++) {
                    float px = (x + (u / (float)nsubsamples) - (width / 2.0f))/(width / 2.0f);
                    float py = (y + (v / (float)nsubsamples) - (height / 2.0f))/(height / 2.0f);
                    py = -py;     // flip Y

                    float t = 10000.0f;
                    Vec eye = new Vec(px, py, -1.0f);
                    eye.normalize();

                    Ray ray = new Ray(new Vec(0.0f, 0.0f, 0.0f), new Vec(eye));

                    Intersection isect = new Intersection();
                    spheres[0].intersect(isect, ray);
                    spheres[1].intersect(isect, ray);
                    spheres[2].intersect(isect, ray);
                    plane.intersect(isect, ray);

                    if (isect.hit) {
                        t = isect.t;
                        Vec col = ambientOcclusion(isect);
                        r += col.x;
                        g += col.y;
                        b += col.z;
                    }
                }
            }

            row[3 * x + 0] = clamp(r / (nsubsamples * nsubsamples));
            row[3 * x + 1] = clamp(g / (nsubsamples * nsubsamples));
            row[3 * x + 2] = clamp(b / (nsubsamples * nsubsamples));
        }
    }

    void generate(String fileName) throws IOException {
        int[] renderLine = new int[IMAGE_WIDTH * 3];

        PrintWriter fout = new PrintWriter(new BufferedWriter(new FileWriter(fileName)));
        fout.println("P3");
        fout.println(IMAGE_WIDTH + " " + IMAGE_HEIGHT);
        fout.println("255");

        for (int y = 0; y < IMAGE_HEIGHT; y++) {
            rowRender(renderLine, IMAGE_WIDTH, IMAGE_HEIGHT, y, NSUBSAMPLES);

            for (int x = 0; x < (IMAGE_WIDTH * 3); x += 3) {
                fout.print(renderLine[x + 0] + " ");
                fout.print(renderLine[x + 1] + " ");
                fout.println(renderLine[x + 2]);
            }
        }

        fout.close();
    }

    public static void main(String[] args) {
        AO ao = new AO();

        ao.setup();
        try {
            ao.generate("ao_java.ppm");
        } catch (IOException e) {}
    }
}