import Data.Maybe (catMaybes, listToMaybe)
import Data.List (sort)
import Control.Monad (forM_)
import Data.Bits (shift, xor, (.&.))
import Control.Monad.State

data Vec = Vec { x :: Double,
                 y :: Double,
                 z :: Double
               } deriving (Show, Eq)

data Isect = Isect { isect_t :: Double,
                     isect_p :: Vec,
                     isect_n :: Vec
                   } deriving (Show)

instance Eq Isect where
  i1 == i2 = isect_t i1 == isect_t i2

instance Ord Isect where
  i1 <= i2 = isect_t i1 <= isect_t i2


data Scene = Scene { scene_objs :: [Object]
                   } deriving Show

data Object = ObjSphere Sphere 
            | ObjPlane Plane
            deriving Show

data Sphere = Sphere { sphere_center :: Vec,
                       sphere_radius :: Double
                     } deriving Show

data Plane = Plane { plane_p :: Vec,
                     plane_n :: Vec
                   } deriving Show

data Ray = Ray { ray_org :: Vec,
                 ray_dir :: Vec
               } deriving Show

type StateForRand a = State (Int, Int, Int, Int) a

rand_xor :: StateForRand Double
rand_xor = do (x, y, z, w) <- get
              let t = x `xor` ((x .&. 0xfffff) `shift` 11)
                  u = (w `xor` (w `shift` (-19)) `xor` (t `xor` (t `shift` (-8))))
              modify (\(x, y, z, w) -> (y, z, w, u))
              return $ (fromIntegral (u `mod` bnum)) / bnumf
  where bnum  = 1 `shift` 29 :: Int
        bnumf = fromIntegral bnum :: Double 


-- 画像情報
init_scene :: Scene
init_scene = Scene { scene_objs = [
                        ObjSphere (Sphere { sphere_center = Vec { x = -2.0, y = 0.0, z = -3.5 }, sphere_radius = 0.5 }),
                        ObjSphere (Sphere { sphere_center = Vec { x = -0.5, y = 0.0, z = -3.0 }, sphere_radius = 0.5 }),
                        ObjSphere (Sphere { sphere_center = Vec { x =  1.0, y = 0.0, z = -2.2 }, sphere_radius = 0.5 }),
                        ObjPlane  (Plane  { plane_p = Vec { x = 0.0, y = -0.5, z = 0.0 }, plane_n = Vec { x = 0.0, y = 1.0, z = 0.0 } })
                        ]
                   }
             
-- レンダリング情報
width = 256
height = 256
nsubsamples = 2
nao_samples = 8

-- main 
main = do putStrLn "P3"                                                 -- ppm-header
          putStrLn $ show (floor width) ++ " " ++ show (floor height)   -- ppm-header
          putStrLn "255"                                                -- ppm-header
          let img = evalState (render init_scene width height nsubsamples nao_samples) 
                    (123456789, 362436069, 521288629, 88675123) -- rendering-image
          forM_ img (\(r,g,b) -> putStrLn $ show r ++ " " ++ show g ++ " " ++ show b) -- output-image
     

render :: Scene -> Double -> Double -> Double -> Double -> StateForRand [(Int, Int, Int)]
render scene w h nsubsamples nao_samples = do
	fimg <- mapM (uncurry $ calcPixelColor scene w h nsubsamples nao_samples) screenPixels
	let img = map vclamp fimg
	return img
  where screenPixels = [(x, y) | y <- [0 .. h-1], x <- [0 .. w-1]]
        vclamp vec = (clamp (x vec), clamp (y vec), clamp (z vec))

calcPixelColor :: Scene -> Double -> Double -> Double -> Double -> Double -> Double -> StateForRand Vec
calcPixelColor scene w h nsubsamples nao_samples x y = do
	subcols <- mapM calc subpixels
	return $ flip vscale (1.0 / sq nsubsamples) $ foldr vadd zerovec $ catMaybes subcols
	where
          subpixels = [(u, v) | v <- [0 .. nsubsamples-1], u <- [0 .. nsubsamples-1]]
          calc (u, v) = do let maybeIsect = emit u v
                           case maybeIsect of
                             Nothing    -> return Nothing
                             Just isect -> calcColor isect >>= return . Just
          emit u v = findNearestIsect (scene_objs scene) $ subray u v
          subray u v = screenRay nsubsamples w h x y u v
          calcColor = ambient_occlusion (scene_objs scene) nao_samples

clamp :: Double -> Int
clamp = min 255 . max 0 . floor . (* 255.99)

-- 汎用関数
sq x = x * x

justIf :: Bool -> a -> Maybe a
justIf cond val
	| cond		= Just val
	| otherwise	= Nothing


-- vector演算
zerovec = Vec { x = 0, y = 0, z = 0 }

vadd v0 v1 = Vec (x v0 + x v1) (y v0 + y v1) (z v0 + z v1)
vsub v0 v1 = Vec (x v0 - x v1) (y v0 - y v1) (z v0 - z v1)
vscale v0 s = Vec (x v0 * s) (y v0 * s) (z v0 * s)

vdot v0 v1 = (x v0 * x v1) + (y v0 * y v1) + (z v0 * z v1)

vcross v0 v1 = Vec cx cy cz
  where cx = y v0 * z v1 - z v0 * y v1
        cy = z v0 * x v1 - x v0 * z v1
        cz = x v0 * y v1 - y v0 * x v1

vlength :: Vec -> Double
vlength c = sqrt $ c `vdot` c

vnormalize :: Vec -> Vec
vnormalize c
  | l > 1.0e-17         = c `vscale` (1.0 / l)
  | otherwise           = c
  where l = vlength c


-- 視線と球の交差
ray_sphere_intersect :: Ray -> Sphere -> Maybe Isect
ray_sphere_intersect ray sphere =
	justIf (d > 0.0 && t > 0.0) isect
	where
		rs = ray_org ray `vsub` sphere_center sphere
		b = rs `vdot` ray_dir ray
		c = rs `vdot` rs - sq (sphere_radius sphere)
		d = b * b - c
		t = -b - sqrt d
		hitpos = ray_org ray `vadd` (ray_dir ray `vscale` t)
		isect = Isect {
			isect_t = t,
			isect_p = hitpos,
			isect_n = vnormalize $ hitpos `vsub` sphere_center sphere
			}

-- 視線と平面の交差
ray_plane_intersect :: Ray -> Plane -> Maybe Isect
ray_plane_intersect ray plane =
	justIf (abs v >= 1.0e-17 && t > 0) isect
	where
		d = -(plane_p plane `vdot` plane_n plane)
		v = ray_dir ray `vdot` plane_n plane
		t = -(ray_org ray `vdot` plane_n plane + d) / v
		isect = Isect {
			isect_t = t,
			isect_p = ray_org ray `vadd` (ray_dir ray `vscale` t),
			isect_n = plane_n plane
			}

orthoBasis :: Vec -> (Vec, Vec, Vec)
orthoBasis n = (basis0, basis1, basis2)
	where
		basis2 = n
		basis1'
			| x n < 0.6 && x n > -0.6	= Vec { x = 1, y = 0, z = 0 }
			| y n < 0.6 && y n > -0.6	= Vec { x = 0, y = 1, z = 0 }
			| z n < 0.6 && z n > -0.6	= Vec { x = 0, y = 0, z = 1 }
			| otherwise					= Vec { x = 1, y = 0, z = 0 }
		basis0 = vnormalize $ basis1' `vcross` basis2
		basis1 = vnormalize $ basis2 `vcross` basis0

findRayObjIntersect :: Ray -> Object -> Maybe Isect
findRayObjIntersect ray (ObjSphere sphere) = ray_sphere_intersect ray sphere
findRayObjIntersect ray (ObjPlane  plane)  = ray_plane_intersect  ray plane

hemisphereOccluders :: Vec -> (Vec, Vec, Vec) -> [Object] -> Double -> Double -> [Isect]
hemisphereOccluders p (basis0, basis1, basis2) objs r1 r2 = isects
	where
		theta = sqrt r1
		phi = 2.0 * pi * r2
		v = Vec {
			x = cos phi * theta,
			y = sin phi * theta,
			z = sqrt $ 1.0 - theta * theta
			}
		-- local -> global
		r = Vec {
			x = x v * x basis0 + y v * x basis1 + z v * x basis2,
			y = x v * y basis0 + y v * y basis1 + z v * y basis2,
			z = x v * z basis0 + y v * z basis1 + z v * z basis2
			}
		ray = Ray { ray_org = p, ray_dir = r }
		isects = catMaybes $ map (findRayObjIntersect ray) objs

ambient_occlusion :: [Object] -> Double -> Isect -> StateForRand Vec
ambient_occlusion objs nao_samples isect = do
	occlusions <- mapM sampleOcclusion samples
	let occlusion = (ntheta * nphi - sum occlusions) / (ntheta * nphi)
	return $ Vec { x = occlusion, y = occlusion, z = occlusion }
	where
		ntheta = nao_samples
		nphi = nao_samples
		eps = 0.0001
		p = isect_p isect `vadd` (isect_n isect `vscale` eps)
		basis = orthoBasis $ isect_n isect
		samples = [(j, i) | j <- [0 .. ntheta-1], i <- [0 .. nphi-1]]
		sampleOcclusion _ = do
			r1 <- rand_xor
			r2 <- rand_xor
			if null $ hemisphereOccluders p basis objs r1 r2
				then return 0
				else return 1
screenRay :: Double -> Double -> Double -> Double -> Double -> Double -> Double -> Ray
screenRay nsubsamples w h x y u v =
	Ray { ray_org = zerovec, ray_dir = vnormalize (Vec { x = px, y = py, z = -1 }) }
	where
		px = (x + (u / nsubsamples) - (w / 2.0)) / (w / 2.0)
		py = -(y + (v / nsubsamples) - (h / 2.0)) / (h / 2.0)

findNearestIsect :: [Object] -> Ray -> Maybe Isect
findNearestIsect objs ray =
	listToMaybe $ sort isects
	where
		isects = catMaybes $ map (findRayObjIntersect ray) $ objs
