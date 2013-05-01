(* http://d.hatena.ne.jp/h013/20110625/1309114394 *)

(* Constant numbers *)
let width = 256
let height = 256
let nsubsamples = 2
let nao_samples = 8
let pi = 4.0 *. atan 1.0

(* Types *)
type vec = {
  x : float;
  y : float;
  z : float;
}

type hit_t =
  | Hit
  | NoHit

type isect_t = {
  t : float;
  p : vec;
  n : vec;
  hit : hit_t;
}

type sphere_t = {
  center : vec;
  radius : float;
}

type plane_t = {
  pos : vec;
  norm : vec;
}

type ray_t = {
  org : vec;
  dir : vec;
}

(* Global variables *)
let zero_vec = { x = 0.0; y = 0.0; z = 0.0 }
let spheres = Array.make 3 { center = zero_vec; radius = 0.0 }
let plane = ref { pos = zero_vec; norm = zero_vec }

(* Functions *)
let vdot v0 v1 =
  v0.x *. v1.x +. v0.y *. v1.y +. v0.z *. v1.z

let vcross v0 v1 =
  { x = v0.y *. v1.z -. v0.z *. v1.y;
    y = v0.z *. v1.x -. v0.x *. v1.z;
    z = v0.x *. v1.y -. v0.y *. v1.x }

let vnormalize c =
  let length = sqrt (vdot c c) in
    if (abs_float length) > 1.0e-17 then
      { x = c.x /. length; y = c.y /. length; z = c.z /. length }
    else
      c

let ray_sphere_intersect isect ray sphere =
  let rs = 
    { x = ray.org.x -. sphere.center.x;
      y = ray.org.y -. sphere.center.y;
      z = ray.org.z -. sphere.center.z } in
  let b = vdot rs ray.dir in
  let c = (vdot rs rs) -. sphere.radius *. sphere.radius in
  let d = b *. b -. c in
  let t = if d > 0.0 then -.b -. sqrt(d) else 0.0 in
    if (t > 0.0) && (t < isect.t) then
      let p = 
	{ x = ray.org.x +. ray.dir.x *. t;
	  y = ray.org.y +. ray.dir.y *. t;
	  z = ray.org.z +. ray.dir.z *. t } in
      let n = 
	{ x = p.x -. sphere.center.x;
	  y = p.y -. sphere.center.y;
	  z = p.z -. sphere.center.z } in
	{ t = t; p = p; n = vnormalize n; hit = Hit }
    else
      isect

let ray_plane_intersect isect ray plane =
  let d = -.vdot plane.pos plane.norm in
  let v = vdot ray.dir plane.norm in
  let t = if abs_float v > 1.0e-17 then 
    -.((vdot ray.org plane.norm) +. d) /. v
  else 0.0 in
    if (t > 0.0) && (t < isect.t) then
      { t = t;
	p = { x = ray.org.x +. ray.dir.x *. t;
	      y = ray.org.y +. ray.dir.y *. t;
	      z = ray.org.z +. ray.dir.z *. t};
	n = plane.norm; hit = Hit }
    else
      isect
    
let orthoBasis basis n =
  basis.(2) <- n;
  if (n.x < 0.6) && (n.x > -.0.6) then
    basis.(1) <- { x = 1.0; y = 0.0; z = 0.0 }
  else if (n.y < 0.6) && (n.y > -.0.6) then
    basis.(1) <- { x = 0.0; y = 1.0; z = 0.0 }
  else if (n.z < 0.6) && (n.z > -.0.6) then
    basis.(1) <- { x = 0.0; y = 0.0; z = 1.0 }
  else
    basis.(1) <- { x = 1.0; y = 0.0; z = 0.0 };
  
  let b0 = vcross basis.(1) basis.(2) in
    basis.(0) <- vnormalize b0;
    let b1 = vcross basis.(2) basis.(0) in
      basis.(1) <- vnormalize b1

let ambient_occlusion isect =
  let eps = 0.0001 in
  let ntheta = nao_samples in
  let nphi = nao_samples in
  let p = 
    { x = isect.p.x +. eps *. isect.n.x;
      y = isect.p.y +. eps *. isect.n.y;
      z = isect.p.z +. eps *. isect.n.z } in
  let basis = Array.make 3 zero_vec in
  let occlusion = ref 0.0 in
    orthoBasis basis isect.n;
    for loop = 0 to ntheta * nphi - 1 do
      let theta = sqrt (Random.float 1.0) in
      let phi = 2.0 *. pi *. (Random.float 1.0) in
      let x = (cos phi) *. theta in
      let y = (sin phi) *. theta in
      let z = sqrt (1.0 -. theta *. theta) in
      let rx = x *. basis.(0).x +. y *. basis.(1).x +. z *. basis.(2).x in
      let ry = x *. basis.(0).y +. y *. basis.(1).y +. z *. basis.(2).y in
      let rz = x *. basis.(0).z +. y *. basis.(1).z +. z *. basis.(2).z in
      let ray =
	{ org = p; dir = { x = rx; y = ry; z = rz } } in
      let occIsect =
	{ t = 1.0e+17; p = zero_vec; n = zero_vec; hit = NoHit } in
      let new_occIsect = ray_sphere_intersect occIsect ray spheres.(0) in
      let new_occIsect = ray_sphere_intersect new_occIsect ray spheres.(1) in
      let new_occIsect = ray_sphere_intersect new_occIsect ray spheres.(2) in
      let new_occIsect = ray_plane_intersect new_occIsect ray !plane in

	if new_occIsect.hit = Hit then
	  occlusion := !occlusion +. 1.0
    done;
    occlusion := ((float_of_int (ntheta * nphi)) -. !occlusion) /. float_of_int (ntheta * nphi);
    { x = !occlusion; y = !occlusion; z = !occlusion }

let clamp f =
  let i = int_of_float (f *. 255.5) in
    if i < 0 then 0 
    else if i > 255 then 255 
    else i

let render img w h nsubsamples = 
  let fw = float_of_int w in
  let fh = float_of_int h in
  let fimg = Array.make (w * h * 3) 0.0 in
  let fnsubsamples = float_of_int nsubsamples in
  let nnsamples = fnsubsamples *. fnsubsamples in
    for y = 0 to h-1 do
      let fy = float_of_int y in
	for x = 0 to w-1 do
	  let fx = float_of_int x in
	    for v = 0 to nsubsamples-1 do
	      let fv = float_of_int v in
		for u = 0 to nsubsamples-1 do
		  let fu = float_of_int u in
		  let px = (fx +. (fu /. fnsubsamples) -. (fw /. 2.0)) /. (fw /. 2.0) in
		  let py = -.(fy +. (fv /. fnsubsamples) -. (fh /. 2.0)) /. (fh /. 2.0) in
		  let ray = 
		    { org = zero_vec; 
		      dir = vnormalize { x = px; y = py; z = -.1.0 }} in
		  let isect = 
		    { t = 1.0e+17; p = zero_vec; n = zero_vec; hit = NoHit } in
		  let new_isect = ray_sphere_intersect isect ray spheres.(0) in
		  let new_isect = ray_sphere_intersect new_isect ray spheres.(1) in
		  let new_isect = ray_sphere_intersect new_isect ray spheres.(2) in
		  let new_isect = ray_plane_intersect new_isect ray !plane in
		    if new_isect.hit = Hit then
		      let col = ambient_occlusion new_isect in
			fimg.(3 * (y * w + x) + 0) <- fimg.(3 * (y * w + x) + 0) +. col.x;
			fimg.(3 * (y * w + x) + 1) <- fimg.(3 * (y * w + x) + 1) +. col.y;
			fimg.(3 * (y * w + x) + 2) <- fimg.(3 * (y * w + x) + 2) +. col.z
		done
	    done;
	    fimg.(3 * (y * w + x) + 0) <- fimg.(3 * (y * w + x) + 0) /. nnsamples;
	    fimg.(3 * (y * w + x) + 1) <- fimg.(3 * (y * w + x) + 1) /. nnsamples;
	    fimg.(3 * (y * w + x) + 2) <- fimg.(3 * (y * w + x) + 2) /. nnsamples;
	    
	    img.(3 * (y * w + x) + 0) <- clamp fimg.(3 * (y * w + x) + 0);
	    img.(3 * (y * w + x) + 1) <- clamp fimg.(3 * (y * w + x) + 1);
	    img.(3 * (y * w + x) + 2) <- clamp fimg.(3 * (y * w + x) + 2)
	done
    done
      
let init_scene () = 
  spheres.(0) <- 
    { center = { x = -.2.0; y = 0.0; z = -.3.5 };
      radius = 0.5 };
  spheres.(1) <- 
    { center = { x = -.0.5; y = 0.0; z = -.3.0 };
      radius = 0.5 };
  spheres.(2) <- 
    { center = { x = 1.0; y = 0.0; z = -.2.2 };
      radius = 0.5 };
  plane := { pos = { x = 0.0; y = -.0.5; z = 0.0 };
	     norm = { x = 0.0; y = 1.0; z = 0.0 } }

let saveppm fname w h img =
  let fout = open_out fname in
    Printf.fprintf fout "P6\n";
    Printf.fprintf fout "%d %d\n" w h;
    Printf.fprintf fout "255\n";
    for i = 0 to w * h * 3 - 1 do
      output_byte fout img.(i) 
    done;
    close_out fout

(* Entry point *)
let _ =
  let img = Array.make (width * height * 3) 0 in
    init_scene ();
    render img width height nsubsamples;
    saveppm "ao.ppm" width height img