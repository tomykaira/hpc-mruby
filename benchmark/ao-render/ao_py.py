"""
AO render benchmark
Original code Processing language in by Syoyo Fujita
  http://lucille.atso-net.jp/aobench/

ao_py.py, a porting project "aobench" by syoyo to Python,
  modified by leonardo maffi, V.1.0, Mar 25 2008.

This version is designed to be fast with Psyco.
"""

from math import sqrt, sin, cos, fabs
from array import array

try:
    import psyco
    psyco.full()
except:
    pass


WIDTH = 256
HEIGHT = WIDTH
NSUBSAMPLES = 2
NAO_SAMPLES = 8

def rand():
    t=rand.x ^ ((rand.x & 0xfffff) << 11);
    rand.x = rand.y
    rand.y = rand.z
    rand.z = rand.w
    rand.w = (rand.w ^ (rand.w >> 19) ^ (t ^ (t >> 8)))
    BNUM = 1<<29
    BNUMF = float(BNUM)
    return (rand.w % BNUM) / BNUMF

rand.x = 123456789
rand.y = 362436069
rand.z = 521288629
rand.w = 88675123


def vcross(v0, v1):
    c = [0] * 3
    c[0] = v0[1] * v1[2] - v0[2] * v1[1]
    c[1] = v0[2] * v1[0] - v0[0] * v1[2]
    c[2] = v0[0] * v1[1] - v0[1] * v1[0]
    return c


def vnormalize(c):
    length = sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2])
    if fabs(length) > 1.0e-17:
        c[0] /= length
        c[1] /= length
        c[2] /= length
    return c


def ray_sphere_intersect(isect, ray, sphere):
    ro = ray[0]
    rd = ray[1]
    rs0 = ro[0] - sphere[0]
    rs1 = ro[1] - sphere[1]
    rs2 = ro[2] - sphere[2]

    B = rs0 * rd[0] + rs1 * rd[1] + rs2 * rd[2]
    C = (rs0 * rs0 + rs1 * rs1 + rs2 * rs2) - (sphere[3] * sphere[3])
    D = B * B - C

    if D > 0.0:
        t = B * -1 - sqrt(D)
        if t > 0.0 and t < isect[0]:
            isect[0] = t
            isect[7] = 1
            isect[1] = ro[0] + rd[0] * t
            isect[2] = ro[1] + rd[1] * t
            isect[3] = ro[2] + rd[2] * t

            n = vnormalize([isect[1] - sphere[0], isect[2] - sphere[1], isect[3] - sphere[2]])
            isect[4] = n[0]
            isect[5] = n[1]
            isect[6] = n[2]
    return isect


def ray_plane_intersect(isect, ray, plane):
    ro = ray[0]
    rd = ray[1]
    d = -1 * (plane[0] * plane[3] + plane[1] * plane[4] + plane[2] * plane[5])
    v = rd[0] * plane[3] + rd[1] * plane[4] + rd[2] * plane[5]

    if abs(v) < 1.0e-17:
        return isect
    t = -1 * ((ro[0] * plane[3] + ro[1] * plane[4] + ro[2] * plane[5]) + d) / v

    if t > 0.0 and t < isect[0]:
        isect[0] = t
        isect[7] = 1
        isect[1] = ro[0] + rd[0] * t
        isect[2] = ro[1] + rd[1] * t
        isect[3] = ro[2] + rd[2] * t
        isect[4] = plane[3]
        isect[5] = plane[4]
        isect[6] = plane[5]
    return isect


def ortho_basis(basis, n):
    b2 = n
    b0 = basis[0]

    v = [0.0, 0.0, 0.0]
    if n[0] < 0.6 and n[0] > -0.6:
        v[0] = 1.0
    elif n[1] < 0.6 and n[1] > -0.6:
        v[1] = 1.0
    elif n[2] < 0.6 and n[2] > -0.6:
        v[2] = 1.0
    else:
        v[0] = 1.0

    b1 = [v[0], v[1], v[2]]
    b0 = vnormalize(vcross(b1, b2))
    basis[1] = vnormalize(vcross(b2, b0))
    basis[0] = b0
    basis[2] = b2


def ambient_occlusion(col, isect):
    global NAO_SAMPLES, sphere1, sphere2, sphere3, plane

    i = j = 0
    ntheta = NAO_SAMPLES
    nphi = NAO_SAMPLES
    eps = 0.0001

    p = [isect[1] + eps * isect[4], isect[2] + eps * isect[5], isect[3] + eps * isect[6]]

    basis = [[0, 0, 0], [0, 0, 0], [0, 0, 0]]
    ortho_basis(basis, [isect[4], isect[5], isect[6]])

    occlusion = 0.0

    b0 = basis[0]
    b1 = basis[1]
    b2 = basis[2]

    for j in xrange(ntheta):
        for i in xrange(nphi):
            r = rand()
            phi = 2.0 * 3.14159265 * rand()

            x = cos(phi) * sqrt(1.0 - r)
            y = sin(phi) * sqrt(1.0 - r)
            z = sqrt(r)

            # local -> global
            rx = x * b0[0] + y * b1[0] + z * b2[0]
            ry = x * b0[1] + y * b1[1] + z * b2[1]
            rz = x * b0[2] + y * b1[2] + z * b2[2]

            ray = [p, [rx, ry, rz]]
            occ_isect = [1.0e+17, 0, 0, 0, 0, 0, 0, 0]

            occ_isect = ray_sphere_intersect(occ_isect, ray, sphere1)
            occ_isect = ray_sphere_intersect(occ_isect, ray, sphere2)
            occ_isect = ray_sphere_intersect(occ_isect, ray, sphere3)
            occ_isect = ray_plane_intersect(occ_isect, ray, plane)

            if occ_isect[7] == 1:
                occlusion += 1.0

    occlusion = (ntheta * nphi - occlusion) / float(ntheta * nphi)
    col[0] = occlusion
    col[1] = occlusion
    col[2] = occlusion
    return col


def clamp(f):
    i = int(f * 255.5)
    if i < 0:
        i = 0
    if i > 255:
        i = 255
    return i


def render(w, h, nsubsamples):
    global sphere1, sphere2, sphere3, plane
    img = [0] * (WIDTH * HEIGHT * 3)

    nsubs = float(nsubsamples)
    nsubs_nsubs = nsubs * nsubs
    w2 = w / 2.0
    h2 = h / 2.0

    for y in xrange(h):
        for x in xrange(w):
            fr = 0.0
            fg = 0.0
            fb = 0.0

            for v in xrange(nsubsamples):
                for u in xrange(nsubsamples):
                    px = (x + (u / nsubs) - w2) / w2
                    py = -1 * (y + (v / nsubs) - h2) / h2
                    ray0 = [0.0, 0.0, 0.0]
                    ray1 = vnormalize([px, py, -1.0])
                    ray = [ray0, ray1]

                    isect = [1.0e+17, 0, 0, 0, 0, 0, 0, 0]

                    isect = ray_sphere_intersect(isect, ray, sphere1)
                    isect = ray_sphere_intersect(isect, ray, sphere2)
                    isect = ray_sphere_intersect(isect, ray, sphere3)
                    isect = ray_plane_intersect(isect, ray, plane)

                    if isect[7] == 1:
                        col = [0, 0, 0]
                        col = ambient_occlusion(col, isect)
                        fr += col[0]
                        fg += col[1]
                        fb += col[2]

            img[3 * (y * w + x) + 0] = clamp(fr / nsubs_nsubs)
            img[3 * (y * w + x) + 1] = clamp(fg / nsubs_nsubs)
            img[3 * (y * w + x) + 2] = clamp(fb / nsubs_nsubs)
    return img


def init_scene():
    global sphere1, sphere2, sphere3, plane
    sphere1 = [-2.0, 0.0, -3.5, 0.5]
    sphere2 = [-0.5, 0.0, -3.0, 0.5]
    sphere3 = [1.0, 0.0, -2.2, 0.5]
    plane = [0.0, -0.5, 0.0, 0.0, 1.0, 0.0]


def save_ppm(img, w, h, fname):
    fout = open(fname, "wb")
    print >>fout, "P6"
    print >>fout, "%i %i" % (w, h)
    print >>fout, "255"
    array("B", img).tofile(fout)
    fout.close()


init_scene()
img = render(WIDTH, HEIGHT, NSUBSAMPLES)
save_ppm(img, WIDTH, HEIGHT, "ao_py.ppm")
