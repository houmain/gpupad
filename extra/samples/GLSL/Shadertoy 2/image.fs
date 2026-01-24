/*--------------------------------------------------------------------------------------
License CC0 - http://creativecommons.org/publicdomain/zero/1.0/
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
----------------------------------------------------------------------------------------
^ This means do ANYTHING YOU WANT with this code. Because we are programmers, not lawyers.
-Otavio Good


This is a real-time ray traced tron style adventure game.

Use left/right arrow keys to steer.

When the movie Tron was made back in 1982, it took one of the best supercomputers in the world to
ray trace the scenes. Now that it's 2017, I decided I'd try to do a real-time ray traced Tron game.
This game uses ray tracing (not ray marching / sphere tracing). It was really problematic trying to
get boolean geometry working in shaders, so I can't recommend it to much, but you can see what I
got working. In some ways, it works much faster than ray marching, but some video cards run it slowly.
*/

// ---------------- Config ----------------
// This is an option that lets you render high quality frames for screenshots. It enables
// stochastic antialiasing and motion blur automatically for any shader.
//#define NON_REALTIME_HQ_RENDER
const float antialiasingSamples = 16.0; // 16x antialiasing - too much might make the shader compiler angry.

#define saturate(a) clamp(a, 0.0, 1.0)

struct Ray
{
    vec3 p0, dirNormalized;
    vec3 hitNormal;
    float t;
};
struct RayHit
{
    vec3 normMin, normMax;
    float tMin, tMax;
    vec3 hitMin, hitMax;
    float material;
};
const float bignum = 256.0*256.0;
RayHit NewRayHit()
{
    RayHit rh;
    rh.tMin = bignum;
    rh.tMax = bignum;
    rh.hitMin = vec3(0.0);
    rh.hitMax = vec3(0.0);
    rh.normMin = vec3(0.0);
    rh.normMax = vec3(0.0);
    rh.material = 0.0;
    return rh;
}

// --------------------------------------------------------
// These variables are for the non-realtime block renderer.
float localTime = 0.0;
float seed = 1.0;

// Animation variables
float fade = 1.0;
float exposure = 1.0;



// -------------------- Start of shared state / variables --------------------
#define PI 3.141592653
const ivec2 kTexState1 = ivec2(0, 160);
const float gameGridRadius = 16.0;
const vec2  gameGridCenter = vec2(80.0, 80.0);
const vec2 iotower = vec2(100.5, 6.5);
// Thumbnail is 288x162 res
const vec2 smallestResolution = vec2(288.0, 162.0);

vec3 bikeAPos = vec3(0.0);
float bikeAAngle = 0.0;
vec3 bikeBPos = vec3(0.0);
float bikeBAngle = 0.0;
vec4 keyStateLURD = vec4(0.0);
vec2 rotQueueA = vec2(0.0);
vec2 rotQueueB = vec2(0.0);
float bitsCollected = 0.0;
float framerate = 0.0;
float gameStage = 0.0;
float message = 0.0;
vec4 camFollowPos = vec4(0.0);
vec4 indicatorPulse = vec4(0.0);
vec4 animA = vec4(0);

vec4 LoadValue(ivec2 uv) { return texelFetch(iChannel0, uv, 0); }

void StoreValue(ivec2 st, vec4 value, inout vec4 fragColor, in vec2 fragCoord)
{ fragColor = (ivec2(fragCoord.xy) == st)? value : fragColor; }

void LoadState()
{
    vec4 state1 = LoadValue(kTexState1);
    bikeAPos = state1.xyz;
    bikeAAngle = state1.w;
    keyStateLURD = LoadValue(kTexState1 + ivec2(1, 0));
    vec4 tempRotQueue = LoadValue(kTexState1 + ivec2(2, 0));
    rotQueueA = tempRotQueue.xy;
    rotQueueB = tempRotQueue.zw;
    vec4 state2 = LoadValue(kTexState1 + ivec2(3, 0));
    bitsCollected = state2.x;
    framerate = state2.y;
    gameStage = state2.z;
    message = state2.w;
    camFollowPos = LoadValue(kTexState1 + ivec2(4, 0));
    indicatorPulse = LoadValue(kTexState1 + ivec2(5, 0));
    vec4 state3 = LoadValue(kTexState1 + ivec2(6, 0));
    bikeBPos = state3.xyz;
    bikeBAngle = state3.w;
    animA = LoadValue(kTexState1 + ivec2(7, 0));
}

void StoreState(inout vec4 fragColor, in vec2 fragCoord)
{
    vec4 state1 = vec4(bikeAPos, bikeAAngle);
    StoreValue(kTexState1, state1, fragColor, fragCoord);
    StoreValue(kTexState1 + ivec2(1, 0), keyStateLURD, fragColor, fragCoord);
    StoreValue(kTexState1 + ivec2(2, 0), vec4(rotQueueA.xy, rotQueueB), fragColor, fragCoord);
    vec4 state2 = vec4(bitsCollected, framerate, gameStage, message);
    StoreValue(kTexState1 + ivec2(3, 0), state2, fragColor, fragCoord);
    StoreValue(kTexState1 + ivec2(4, 0), camFollowPos, fragColor, fragCoord);
    StoreValue(kTexState1 + ivec2(5, 0), indicatorPulse, fragColor, fragCoord);
    vec4 state3 = vec4(bikeBPos, bikeBAngle);
    StoreValue(kTexState1 + ivec2(6, 0), state3, fragColor, fragCoord);
    StoreValue(kTexState1 + ivec2(7, 0), animA, fragColor, fragCoord);
}
// -------------------- End of shared state / variables --------------------




// ---- noise functions ----
float v31(vec3 a)
{
    return a.x + a.y * 37.0 + a.z * 521.0;
}
float v21(vec2 a)
{
    return a.x + a.y * 37.0;
}
float Hash11(float a)
{
    return fract(sin(a)*10403.9);
}
float Hash21(vec2 uv)
{
    float f = uv.x + uv.y * 37.0;
    return fract(sin(f)*104003.9);
}
vec2 Hash22(vec2 uv)
{
    float f = uv.x + uv.y * 37.0;
    return fract(cos(f)*vec2(10003.579, 37049.7));
}
vec2 Hash12(float f)
{
    return fract(cos(f)*vec2(10003.579, 37049.7));
}

vec3 RotateX(vec3 v, float rad)
{
  float cos = cos(rad);
  float sin = sin(rad);
  return vec3(v.x, cos * v.y + sin * v.z, -sin * v.y + cos * v.z);
}
vec3 RotateY(vec3 v, float rad)
{
  float cos = cos(rad);
  float sin = sin(rad);
  return vec3(cos * v.x - sin * v.z, v.y, sin * v.x + cos * v.z);
}
vec3 RotateZ(vec3 v, float rad)
{
  float cos = cos(rad);
  float sin = sin(rad);
  return vec3(cos * v.x + sin * v.y, -sin * v.x + cos * v.y, v.z);
}

// min function that supports materials in the y component
vec2 matmin(vec2 a, vec2 b)
{
    if (a.x < b.x) return a;
    else return b;
}
vec2 matmax(vec2 a, vec2 b)
{
    if (a.x > b.x) return a;
    else return b;
}

// http://web.cse.ohio-state.edu/~parent/classes/681/Lectures/19.RayTracingCSGHandout.pdf
// These are hacks that don't do complete booleans of all the depth complexity. :(
RayHit Difference(RayHit a, RayHit b)
{
    RayHit rh = NewRayHit();
    //rh.tMin = bignum;
    //rh.tMax = bignum;
    if (a.tMax != bignum)
    {
        if (a.tMin < b.tMin) return a;
        else
        {
            if (b.tMax < a.tMin) return a;
            else if (b.tMax < a.tMax)
            {
                rh = b;
                rh.normMin = b.normMax;
                rh.tMin = b.tMax;
                rh.hitMin = b.hitMax;
                // ACCOUNT FOR THE Back SIDE OF THE SHAPE???!!!!
            }
        }
    }
    return rh;
}
RayHit Union(RayHit a, RayHit b)
{
    if (a.tMin < b.tMin)
    {
/*        if ((b.tMax > a.tMax) && (b.tMax != bignum))
        {
            a.tMax = b.tMax;
            a.normMax = b.normMax;
            a.hitMax = b.hitMax;
        }*/
        return a;
    }
    else {
        /*if ((a.tMax > b.tMax) && (a.tMax != bignum))
        {
            b.tMax = a.tMax;
            b.normMax = a.normMax;
            b.hitMax = a.hitMax;
        }*/
        return b;
    }
}
RayHit Intersection(RayHit a, RayHit b)
{
    if ((b.tMin < a.tMin) && (b.tMax > a.tMin)) {
        b.tMin = a.tMin;
        b.normMin = a.normMin;
        b.hitMin = a.hitMin;
        return b;
    }
    else if ((a.tMin < b.tMin) && (a.tMax > b.tMin)) {
        a.tMin = b.tMin;
        a.normMin = b.normMin;
        a.hitMin = b.hitMin;
        return a;
    }
    RayHit rh = NewRayHit();
    //rh.tMin = bignum;
    //rh.tMax = bignum;
    return rh;
}

// dirVec MUST BE NORMALIZED FIRST!!!!
// returns normal and t
/*vec4 SphereIntersect(vec3 pos, vec3 dirVecPLZNormalizeMeFirst, vec3 spherePos, float rad)
{
    vec3 radialVec = pos - spherePos;
    float b = dot(radialVec, dirVecPLZNormalizeMeFirst);
    float c = dot(radialVec, radialVec) - rad * rad;
    float h = b * b - c;
    if (h < 0.0) return vec4(bignum);
    float t = -b - sqrt(h);

    vec3 normal = normalize(radialVec + dirVecPLZNormalizeMeFirst * t);
    return vec4(normal, t);
}*/
// note: There are faster ways to intersect a sphere.
RayHit SphereIntersect2(vec3 pos, vec3 dirVecPLZNormalizeMeFirst, vec3 spherePos, float rad, float material)
{
    RayHit rh = NewRayHit();
    rh.material = material;
    vec3 delta = spherePos - pos;
    float projdist = dot(delta, dirVecPLZNormalizeMeFirst);
    vec3 proj = dirVecPLZNormalizeMeFirst * projdist;
    vec3 bv = proj - delta;
    float b = length(bv);
    if (b > rad) {
        //rh.tMin = bignum;
        //rh.tMax = bignum;
        return rh;
    }
    float x = sqrt(rad*rad - b*b);
    rh.tMin = projdist - x;
    if (rh.tMin < 0.0) {
        rh.tMin = bignum;
        return rh;
    }
    rh.tMax = projdist + x;
    rh.hitMin = pos + dirVecPLZNormalizeMeFirst * rh.tMin;
    rh.hitMax = pos + dirVecPLZNormalizeMeFirst * rh.tMax;
    rh.normMin = normalize(rh.hitMin - spherePos);
    rh.normMax = normalize(spherePos - rh.hitMax);
    return rh;
}

vec4 PlaneYIntersect(vec3 pos, vec3 dirVecPLZNormalizeMeFirst, vec3 planePos)
{
    // Ray trace a ground plane to infinity
    pos -= planePos;
    float t = -pos.y / dirVecPLZNormalizeMeFirst.y;
    if (t > 0.0) {
        vec3 normal = vec3(0.0, 1.0 * sign(-dirVecPLZNormalizeMeFirst.y), 0.0);
        return vec4(normal, t);
    } else return vec4(bignum);
}

vec3 ElementwiseEqual(in vec3 a, in float b) {
	return abs(sign(a - b));
}

// https://tavianator.com/fast-branchless-raybounding-box-intersections/
RayHit BoxIntersect(vec3 pos, vec3 dirVecPLZNormalizeMeFirst, vec3 boxPos, vec3 rad, float material)
{
    vec3 bmin = boxPos - rad;
    vec3 bmax = boxPos + rad;
    vec3 rayInv = 1.0 / dirVecPLZNormalizeMeFirst;

    vec3 t1 = (bmin - pos) * rayInv;
    vec3 t2 = (bmax - pos) * rayInv;

    vec3 vmin = min(t1, t2);
    vec3 vmax = max(t1, t2);

    float tmin = max(vmin.z, max(vmin.x, vmin.y));
    float tmax = min(vmax.z, min(vmax.x, vmax.y));

    RayHit rh = NewRayHit();
    //rh.tMin = bignum;
    //rh.tMax = bignum;
    if ((tmax <= tmin)) return rh;
    if ((tmin <= 0.0)) return rh;
    //if ((tmax <= tmin) || (tmin <= 0.0)) return rh;
    rh.tMin = tmin;
    rh.tMax = tmax;
    rh.hitMin = pos + dirVecPLZNormalizeMeFirst * rh.tMin;
    rh.hitMax = pos + dirVecPLZNormalizeMeFirst * rh.tMax;
    // optimize me!
    //if (t1.x == tmin) rh.normMin = vec3(-1.0, 0.0, 0.0);
    //if (t2.x == tmin) rh.normMin = vec3(1.0, 0.0, 0.0);
    //if (t1.y == tmin) rh.normMin = vec3(0.0, -1.0, 0.0);
    //if (t2.y == tmin) rh.normMin = vec3(0.0, 1.0, 0.0);
    //if (t1.z == tmin) rh.normMin = vec3(0.0, 0.0, -1.0);
    //if (t2.z == tmin) rh.normMin = vec3(0.0, 0.0, 1.0);
    rh.normMin = ElementwiseEqual(t1, tmin) - ElementwiseEqual(t2, tmin);
    // optimize me!
    //if (t1.x == tmax) rh.normMax = -vec3(-1.0, 0.0, 0.0);
    //if (t2.x == tmax) rh.normMax = -vec3(1.0, 0.0, 0.0);
    //if (t1.y == tmax) rh.normMax = -vec3(0.0, -1.0, 0.0);
    //if (t2.y == tmax) rh.normMax = -vec3(0.0, 1.0, 0.0);
    //if (t1.z == tmax) rh.normMax = -vec3(0.0, 0.0, -1.0);
    //if (t2.z == tmax) rh.normMax = -vec3(0.0, 0.0, 1.0);
    rh.normMax = ElementwiseEqual(t1, tmax) - ElementwiseEqual(t2, tmax);
    rh.material = material;
    return rh;
}

float waterLevel = 0.0;
vec3 CalcNormal(const in vec3 pos, const in vec3 rayVec)
{
    // find the axis that is closest to 1.0 or 0.0.
    vec3 normal = vec3(0.0);
    vec3 frpos = 0.5-abs(0.5-fract(pos));
    vec3 fr = frpos * 2.0;
    fr = pow(fr, vec3(0.25));
    float sum = fr.x + fr.y + fr.z;
    vec3 div = fr / sum;
    float minAll = min(frpos.x, min(frpos.y, frpos.z));
    if (frpos.x == minAll) normal = vec3(-1.0, 0.0, 0.0) * sign(rayVec.x);
    if (frpos.y == minAll) normal = vec3(0.0, -1.0, 0.0) * sign(rayVec.y);
    if (frpos.z == minAll) normal = vec3(0.0, 0.0, -1.0) * sign(rayVec.z);
    //normal = normalize(div + normal);

    // Water wave normals
    if (pos.y < waterLevel+0.01) {
        normal.x += texture(iChannel1, (pos.xz + localTime)*0.125).x*0.2-0.1;
        normal.y += texture(iChannel1, (pos.zx - localTime)*0.1).x*0.2-0.1;
        //normal = normalize(normal);
    }
    return normal;
}

//#define DISTANCE_FIELD
// This is the distance function that defines all the scene's geometry.
// The input is a position in space.
// The output is the distance to the nearest surface and a material index.
float DistanceToTerrain(in vec3 p)
{
#ifdef DISTANCE_FIELD
	return p.y - texelFetch(iChannel0, ivec2(p.xz+gameGridCenter),0).x;
#else
	return texelFetch(iChannel0, ivec2(p.xz+gameGridCenter),0).x;
#endif
}

const float smallVal = 0.000625;
RayHit MarchOneRay(const in vec3 start, const in vec3 rayVec,
                 out vec3 posI, const in int maxIter)
{
    RayHit rh = NewRayHit();
    rh.material = 256.0;
	float dist = 1000000.0;
	float t = 0.0;
	vec3 pos = vec3(0.0);
    vec3 signRay = max(vec3(0.0), sign(rayVec));
	// ray marching time
    pos = start;
    posI = floor(start);
    vec3 delta = signRay - fract(pos);
    vec3 hit = (delta/rayVec);
    vec3 signRayVec = sign(rayVec);
    vec3 invAbsRayVec = abs(1.0 / rayVec);
    // This is the highest we can ray march before early exit.
    float topBounds = max(10.0, start.y);
    for (int i = 0; i < maxIter; i++)	// This is the count of the max times the ray actually marches.
    {
#ifdef DISTANCE_FIELD
        //dist = DistanceToTerrain(posI);
		dist = posI.y - texelFetch(iChannel0, ivec2(posI.xz+gameGridCenter),0).x;
#else
		vec4 terrainTex = texelFetch(iChannel0, ivec2(posI.xz+gameGridCenter),0);
        dist = terrainTex.x;
#endif
        if ((terrainTex.y != 0.0) && (dist >= posI.y)) {
            if (round(posI.y) == 1.0)
            {
                float inGameGrid = max(abs(posI.x), abs(posI.z));
                float tex = mod(terrainTex.y, 16.0);
                // Only draw yellow trail walls if we're inside the game grid.
                if (inGameGrid <= gameGridRadius) {
                    vec3 boxSizeX = vec3(0.02, 0.25, 0.02);
                    vec3 boxSizeZ = vec3(0.02, 0.25, 0.02);
                    boxSizeX.x += sign(float(int(tex)&1))*0.25;
                    boxSizeX.x += sign(float(int(tex)&2))*0.25;
                    boxSizeZ.z += sign(float(int(tex)&4))*0.25;
                    boxSizeZ.z += sign(float(int(tex)&8))*0.25;
                    vec3 boxOffsetX = posI + 0.5 - vec3(0,0.25,0);
                    vec3 boxOffsetZ = posI + 0.5 - vec3(0,0.25,0);
                    // Shrink the blue trail when bad guy dies
                    if (terrainTex.y >= 16.0) {
                        float anim = saturate(animA.x);
                        boxSizeX.y *= anim;
                        boxSizeZ.y *= anim;
                        boxOffsetX.y -= (1.0 - anim) * 0.25;
                        boxOffsetZ.y -= (1.0 - anim) * 0.25;
                    }
                    boxOffsetX.x -= sign(float(int(tex)&1))*0.25;
                    boxOffsetX.x += sign(float(int(tex)&2))*0.25;
                    boxOffsetZ.z -= sign(float(int(tex)&4))*0.25;
                    boxOffsetZ.z += sign(float(int(tex)&8))*0.25;
                    RayHit rh = BoxIntersect(pos, rayVec, boxOffsetX, boxSizeX, 1.0);
                    rh = Union(rh, BoxIntersect(pos, rayVec, boxOffsetZ, boxSizeZ, 1.0));
                    if (rh.tMin == bignum) dist = smallVal;
                    else {
                        rh.material = 128.0;
                        if (terrainTex.y >= 16.0) rh.material = 129.0;
                        return rh;
                    }
                }
            }
            if (terrainTex.y == 256.0) {
                vec3 bpos = pos - (posI + 0.5);
                bpos = RotateY(bpos, localTime*8.0);
                vec3 rayVecR = RotateY(rayVec, localTime*8.0);
                bpos += (posI + 0.5);
                RayHit rh = BoxIntersect(bpos, rayVecR, posI + 0.5, vec3(0.28), 8.0);
                if (rh.tMin == bignum) dist = smallVal;  // Missed powerup. Keep ray marching.
                else return rh;  // Hit powerup
            }
            //vec4 hitS = SphereIntersect(pos, rayVec, posI + 0.5, 0.5);
            //if (hitS.w == bignum) dist = smallVal;
        }
        // || (t > maxDepth)
#ifdef DISTANCE_FIELD
        if ((dist < smallVal) || (posI.y > topBounds)) break;
#else
        if ((dist >= posI.y) || (posI.y > topBounds)) break;
#endif

        vec3 absHit = abs(hit);
        t = min(absHit.x, min(absHit.y, absHit.z));
        vec3 walk = step(absHit, vec3(t));
        hit += walk * invAbsRayVec;
        posI += walk * signRayVec;
        /*if (t == absHit.x) {
            hit.x += invAbsRayVec.x;
            posI.x += signRayVec.x;
        }
        if (t == absHit.y) {
            hit.y += invAbsRayVec.y;
            posI.y += signRayVec.y;
        }
        if (t == absHit.z) {
            hit.z += invAbsRayVec.z;
            posI.z += signRayVec.z;
        }*/
    }
#ifdef DISTANCE_FIELD
    if (dist >= smallVal) return rh;
#else
    if (dist < posI.y) return rh;
#endif
    // Hit the voxel terrain
    pos = t * rayVec + start;
	//vec4 tex = texelFetch(iChannel0, ivec2(pos.xz+gameGridCenter),0);
    //if (abs(pos.y - tex.x) > 2.5) rh.material = 1.0;
    rh.tMin = t;
    rh.hitMin = pos;
    rh.normMin = CalcNormal(pos, rayVec);
    rh.tMax = rh.tMin + 1.0;
    return rh;
}


RayHit Wheel(vec3 camPos, vec3 rayVec, float offset, float squeeze, float material)
{
    RayHit ray;
    ray = SphereIntersect2(camPos, rayVec, vec3(offset, 0.25, 0.0), 0.25, material);
    ray = Intersection(ray, BoxIntersect(camPos, rayVec, vec3(offset, 0.25, 0.0), vec3(0.5, 0.25, squeeze), material));
    ray = Difference(ray, SphereIntersect2(camPos, rayVec, vec3(offset, 0.25, 0.3), 0.25, 3.0));
    ray = Difference(ray, SphereIntersect2(camPos, rayVec, vec3(offset, 0.25, -0.3), 0.25, 3.0));
    ray = Union(ray, SphereIntersect2(camPos, rayVec, vec3(offset, 0.25, 0.0), 0.10, 0.0));
    return ray;
}

RayHit Bike(vec3 camPos, vec3 rayVec, vec3 bpos, float bangle, float material)
{
    mat3 m = mat3(
        1.0, 0.0, 0.0, // first column (not row!)
        0.0, 1.0, 0.0, // second column
        0.0, 0.0, 1.0  // third column
    );
//    float cos = cos(rad);
  //  float sin = sin(rad);
    //return vec3(cos * v.x - sin * v.z, v.y, sin * v.x + cos * v.z);

    // could optimize by not recalcing the sin/cos every time. or matrixing?
    bpos -= camPos;
    bpos = RotateY(bpos, bangle);
    rayVec = RotateY(rayVec, bangle);
    bpos += camPos;

    camPos -= bpos;
    RayHit bi = BoxIntersect(camPos, rayVec, vec3(0.0, 0.33, 0.0), vec3(1.0, 0.33, 0.15), 0.0);
    //bi.normMin = RotateY(bi.normMin, -bangle);
    //return bi;
    //vec4 sp = SphereIntersect(camPos, rayVec, vec3(0.0), 1.0);
    if (bi.tMin == bignum)
    {
        RayHit rh = NewRayHit();
        //rh.tMin = bignum;
        return rh;
    }
    RayHit ray = Wheel(camPos, rayVec, 0.5, 0.145, material);
    RayHit r2 = Wheel(camPos, rayVec, -0.5, 0.12, material);
    ray = Union(ray, r2);
    RayHit body = SphereIntersect2(camPos, rayVec, vec3(0, -0.63, 0.0), 1.25, material);
    body = Intersection( body, BoxIntersect(camPos, rayVec, vec3(0.0, 0.4, 0.0), vec3(0.5, 0.25, 0.06), material));
    if ((body.hitMin.y > 0.5) && (body.hitMin.x > 0.1)) body.material = 3.0;
    RayHit body2 = SphereIntersect2(camPos, rayVec, vec3(0, -0.75, 0.0), 1.25, 2.0);
    body2 = Intersection(body2, BoxIntersect(camPos, rayVec, vec3(0.0, 0.4, 0.0), vec3(0.35, 0.2, 0.09), 2.0));
    ray = Union(ray, body);
    ray = Union(ray, body2);

    //ray = BoxIntersect(camPos, rayVec, vec3(0.0, 0.33, 0.0), vec3(1.0, 0.33, 0.01)*0.9, 0.0);
    ray.normMin = RotateY(ray.normMin, -bangle);

    ray.hitMin = RotateY(ray.hitMin, -bangle)+bpos;

    return ray;
}

float effect = 1.0;
vec3 CalcTexColor(RayHit ray, vec3 camPos)
{
    vec3 texColor = vec3(0.3);
    if (ray.material == 1.0) {
        // Bike's yellow
        texColor = vec3(1.0, 0.4, 0.1);
    }
    else if (ray.material == 2.0) texColor *= vec3(0.25);
    else if (ray.material == 3.0) texColor *= vec3(0.0);
    //else if (ray.material == 5.0) texColor *= vec3(1.0, 0.1, 0.06);
    else if (ray.material == 5.0) texColor *= vec3(0.05, 0.4, 1.0);
    else if (ray.material == 8.0) texColor = vec3(0.0, 1.5, 0.0);  // bit collecting powerups
    else if (ray.material == 9.0) {
        // I/O tower
        texColor = texture(iChannel1, ray.hitMin.xy*0.01 - localTime * 0.4).xxx;
        // IO tower changes to blue once you can transmit information.
        if ((gameStage >= 300.0) && (gameStage < 1000.0)) texColor *= vec3(2.0, 4.0, 8.0);
        else texColor *= vec3(8.0, 1.0, 1.5);
    }
    else if (ray.material == 128.0) {
        texColor *= vec3(1.0, 0.6, 0.1)*6.0;
        texColor += pow(abs(1.0-fract(ray.hitMin.y)), 4.0);
        texColor.g -= pow(abs(fract(ray.hitMin.y)), 4.0)*0.5;
    }
    else if (ray.material == 129.0) {
        texColor *= vec3(0.05, 0.3, 0.8)*6.0;
        texColor += pow(abs(1.0-fract(ray.hitMin.y)), 4.0);
        texColor.g -= pow(abs(fract(ray.hitMin.y)), 4.0)*0.5;
    }
    else if (ray.material == 256.0) {
        vec3 gridXYZ = abs(fract(ray.hitMin+0.5)-0.5)*2.0;
        float grid = max(gridXYZ.y, max(gridXYZ.x, gridXYZ.z));
        texColor = vec3(0.05, 0.2, 0.9) * 4.0 * saturate(grid - 0.8);// * ray.normMin.y;
        // Make the grid fade toward the back for antialiasing / mip map style
        float dist = 1.0/distance(ray.hitMin, camPos);
        // Also make the grid lines wider toward the back for mip mapped look
        float wide = 1.0 + dist*28.0;
        texColor = vec3(0.05, 0.2, 0.9) * saturate(pow(abs(grid),wide));
        texColor += vec3(1.0, 0.8, 0.3) * 1.0 * saturate(pow(abs(grid), 8.0)*dist);
        //if (ray.normMin.y < 0.5) texColor += vec3(0.5,0.1,0.1);
        //vec3 maxd = max(dFdx(ray.hitMin), dFdy(ray.hitMin));
        //float delta = max(max(maxd.x, maxd.y), maxd.z);
        //texColor *= 1.0-saturate(delta);
        // hack to fix grid aliasing in the distance
        texColor *= saturate(vec3(6.0) * dist);

        // highlight the game grid
        float inGameGrid = max(abs(ray.hitMin.x-0.5), abs(ray.hitMin.z-0.5));
        if (inGameGrid < gameGridRadius) {
            texColor += vec3(0.0, 0.05, 1.05)*0.1;
        }

        grid = fract(ray.hitMin.y);
        if (ray.normMin.y == 0.0) texColor += vec3(3.0,0.0,0.0) * pow(abs(grid-0.5)*2.0,12.0);
        // Water color
        if (ray.hitMin.y < waterLevel+0.01) texColor = vec3(0, 0.07, .38);
    }
    // indicator pulse
    float pulseRad = length(ray.hitMin - indicatorPulse.xyz);
    texColor += saturate(sin((pulseRad - localTime*2.0)*8.0) - pulseRad*0.03) * saturate(indicatorPulse.w);

    // teleport in effect
    float noise = texture(iChannel1, ray.hitMin.xz*0.5 + iTime*7.0).x;
    texColor += vec3(0.01, 0.2, 1.0) * noise*28.0 * effect;

    return texColor;
}

// Make a procedural environment map with a giant softbox light.
vec3 GetEnvColor2(vec3 camPos, vec3 rayDir, vec4 bike, bool cinematicHack)
{
    if (cinematicHack) {
        vec3 col = texture(iChannel3, RotateZ(rayDir, 0.0)).xyz;
        vec4 normalT = PlaneYIntersect(camPos, rayDir, bike.xyz);
        vec3 hit = camPos + rayDir * normalT.w;
        float shadowSize = 1.73;
        float shadow = saturate(shadowSize*length(hit-bike.xyz - vec3(0.5, 0, 0)));
        shadow *= saturate(shadowSize*length(hit-bike.xyz + vec3(0.5, 0, 0)));
        return col * shadow;
    }
    // fade bottom to top so it looks like the softbox is casting light on a floor
    // and it's bouncing back
    vec3 final = vec3(1.0) * dot(rayDir, /*sunDir*/ vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5;
    final *= 0.125;
    // overhead softbox, stretched to a rectangle
    if ((rayDir.y > abs(rayDir.x)*1.0) && (rayDir.y > abs(rayDir.z*0.25))) final = vec3(2.0)*rayDir.y;
    // fade the softbox at the edges with a rounded rectangle.
    float roundBox = length(max(abs(rayDir.xz/max(0.0,rayDir.y))-vec2(0.9, 4.0),0.0))-0.1;
    final += vec3(0.8)* pow(saturate(1.0 - roundBox*0.5), 6.0);
    // purple lights from side
    //final += vec3(8.0,6.0,7.0) * saturate(0.001/(1.0 - abs(rayDir.x)));
    // yellow lights from side
    //final += vec3(8.0,7.0,6.0) * saturate(0.001/(1.0 - abs(rayDir.z)));
    return vec3(final);
}

RayHit RayTraceScene(vec3 camPos, vec3 rayVec, const in int maxIter, bool cinematicHack)
{
    RayHit ray = Bike(camPos, rayVec, bikeAPos, bikeAAngle, 1.0);
    if (!cinematicHack) {
        ray = Union(ray, Bike(camPos, rayVec, bikeBPos, bikeBAngle, 5.0));
        //ray = Union(ray, Bike(camPos, rayVec, bikeAPos + vec3(0.0, 0.0, -1.0), 0.0));
        //ray = Union(ray, Bike(camPos, rayVec, bikeAPos + vec3(0.0, 0.0, -2.0), 0.0));
        //ray = Union(ray, Bike(camPos, rayVec, bikeAPos + vec3(0.0, 0.0, -3.0), 0.0));
        //ray = Union(ray, Bike(camPos, rayVec, bikeAPos + vec3(0.0, 0.0, -4.0), 0.0));

        vec3 posI;
        RayHit voxels = MarchOneRay(camPos, rayVec, posI, maxIter);
        ray = Union(ray,voxels);

        // I/O Tower
        ray = Union(ray, BoxIntersect(camPos, rayVec, vec3(iotower.x, 0.25, iotower.y), vec3(0.5, 256.0, 0.5), 9.0));
    }
    return ray;
}

void RenderScene(vec3 camPos, vec3 rayVec, inout vec3 finalColor, bool cinematicHack)
{
	RayHit ray = RayTraceScene(camPos, rayVec, 176, cinematicHack);

    if (ray.tMin != bignum)
    {
        // calculate the reflection vector for highlights
        vec3 ref = normalize(reflect(rayVec, ray.normMin));
        vec3 refColor = GetEnvColor2(camPos, ref, vec4(bikeAPos, bikeAAngle), cinematicHack) * 1.5;

        RayHit refRay = RayTraceScene(ray.hitMin + ref*0.01, ref, 16, cinematicHack);
        
        vec3 texColor = CalcTexColor(ray, camPos);
        vec3 lightColor = vec3(1.0) * (saturate(ray.normMin.y *0.5+0.5))*1.25;
        finalColor = texColor * lightColor;
        float fresnel = saturate(1.0 - dot(-rayVec, ray.normMin));
        fresnel = mix(0.5, 1.0, fresnel);
        if ((ray.material == 256.0) && (ray.normMin.y == 1.0)) refColor *= vec3(1.0, 0.5, 0.2)*3.0*dot(ray.normMin, -rayVec);
        finalColor += refColor * fresnel * 0.4;
        // fog
		finalColor = mix(vec3(1.0, 0.41, 0.41) + vec3(1.0), finalColor, exp(-ray.tMin*0.0007));
        if (cinematicHack) {
            // fog the bike as it drives into the distance
            vec3 backCol = texture(iChannel3, RotateZ(rayVec, 0.0)).xyz;
            finalColor *= (rayVec.y * 0.5 + 0.5);
            finalColor = mix(finalColor, backCol, saturate(-bikeAPos.x*0.01));
            // Fade towards bottom so shadow matches photo better
        }

        if (refRay.tMin != bignum) {
	        vec3 refTexColor = CalcTexColor(refRay, camPos);
            finalColor = mix(finalColor, refTexColor, 0.1);
        }
    } else {
        finalColor = GetEnvColor2(camPos, rayVec, vec4(bikeAPos, bikeAAngle), cinematicHack);
    }
    //finalColor = vec3(0.03)*ray.tMin;

/*    if (ray.tMin != bignum)
    {
        finalColor = ray.normMin * 0.5 + 0.5;
	    finalColor.r += pow(sin(ray.hitMin.x*32.0)*0.5+0.5, 64.0);
	    finalColor.g += pow(sin(ray.hitMin.y*32.0)*0.5+0.5, 64.0);
	    finalColor.b += pow(sin(ray.hitMin.z*32.0)*0.5+0.5, 64.0);
        finalColor *= 0.01;
        finalColor.b += ray.hitMin.z*ray.hitMin.z*0.05;
    }*/

}


// Input is UV coordinate of pixel to render.
// Output is RGB color.
vec3 RayTrace(in vec2 fragCoord )
{
    fade = 1.0;
    effect = 0.0;

	vec3 camPos, camUp, camLookat;
	// ------------------- Set up the camera rays for ray marching --------------------
    // Map uv to [-1.0..1.0]
	vec2 uv = fragCoord.xy/iResolution.xy * 2.0 - 1.0;
    uv /= 2.0;  // zoom in

    bool cinematicHack = false;
    // End game cinematic
    if ((gameStage >= 600.0) && (gameStage < 700.0)) {
        cinematicHack = true;
        bikeAAngle = PI;
        bikeAPos = vec3(0);
    }

    // Camera up vector.
	camUp=vec3(0,1,0);

	// Camera lookat.
	camLookat=vec3(0,0.5,0) + vec3(bikeAPos.x, max(1.0, bikeAPos.y), bikeAPos.z);

    // debugging camera
    float mx=-iMouse.x/iResolution.x*PI*2.0;
	float my=iMouse.y/iResolution.y*3.14*0.95 + PI/2.0;
    vec3 bikeDir = round(vec3(cos(bikeAAngle), 0.0, -sin(bikeAAngle)));
    if (cinematicHack) {
        camPos = vec3(2,1.0,0.95);
        float uplook = smoothstep(0.0, 1.0, saturate((gameStage - 605.0)*0.0625))*10.0;
        //uplook = saturate((gameStage - 604.0)*0.25);
        camLookat=vec3(0,1.0 + uplook,0.95);
    } else {
    	camPos = camFollowPos.xyz;
        if ((gameStage >= 100.0) && (gameStage < 102.0)) {
            camLookat = mix(camLookat, vec3(bikeBPos.x, 1.0, bikeBPos.z), saturate((gameStage-100.0)*0.5));
        }
        if ((gameStage >= 102.0) && (gameStage < 104.0)) {
            camLookat = mix(vec3(bikeBPos.x, 1.0, bikeBPos.z), camLookat, saturate((gameStage-102.0)*0.5));
        }
    }
    // debug camera
	//camPos = vec3(cos(my)*cos(mx),sin(my),cos(my)*sin(mx))*82.0 + bikeAPos;

	// Camera setup for ray tracing / marching
	vec3 camVec=normalize(camLookat - camPos);
	vec3 sideNorm=normalize(cross(camUp, camVec));
	vec3 upNorm=cross(camVec, sideNorm);
	vec3 worldFacing=(camPos + camVec);
	vec3 worldPix = worldFacing + uv.x * sideNorm * (iResolution.x/iResolution.y) + uv.y * upNorm;
	vec3 rayVec = normalize(worldPix - camPos);

    // End game cinematic
    if (cinematicHack) {
        cinematicHack = true;
        bikeAAngle = PI;
        float accel = pow(3.0, max(0.0, gameStage - 604.0));
        bikeAPos = vec3(-accel+1.0, 0.3, 0.125);
        effect = 1.0-saturate((gameStage - 600.0) * 0.5);
    }

	// ----------------------------- Ray trace the scene ------------------------------

    vec3 finalColor = vec3(0.0);
	RenderScene(camPos, rayVec, finalColor, cinematicHack);

    // vignette?
    finalColor *= saturate(1.0 - length(uv/2.5));
    finalColor *= exposure;
    if ((gameStage >= 500.0) && (gameStage < 600.0)) finalColor = mix(finalColor, vec3(1), saturate((gameStage - 500.0) * 0.5));
    if ((gameStage >= 600.0) && (gameStage < 700.0)) {
        finalColor = mix(vec3(1), finalColor, saturate((gameStage - 600.0) * 0.5));
        finalColor = mix(finalColor, vec3(1.0), saturate((gameStage - 607.0) * 0.333));
    }

	// output the final color without gamma correction - will do gamma later.
	return vec3(saturate(finalColor)*saturate(fade));
}

void HUD(inout vec3 finalColor, vec2 fragCoord) {
	vec2 uv = fragCoord.xy/iResolution.xy;// * 2.0 - 1.0;
    uv.y = 1.0 - uv.y;
    uv *= 8.0;
    uv.x *= 2.0;
    vec2 uvf = fract(uv);
    vec2 uvi = floor(uv);
    float map = texelFetch(iChannel0, ivec2(uvi), 0).w;
    vec2 charPos = vec2(mod(map, 16.0), floor(map / 16.0));
    vec4 tex = texture(iChannel2, (uvf + charPos) / 16.0f, -100.0);
	finalColor = finalColor * pow(saturate(tex.w+0.4), 6.0);
	finalColor = mix(finalColor, vec3(1.0), tex.x);
	//finalColor = vec3(uvi/16.0,0);
    //if (fragCoord.x < iTimeDelta*1000.0) finalColor = vec3(1);
}


#ifdef NON_REALTIME_HQ_RENDER
// This function breaks the image down into blocks and scans
// through them, rendering 1 block at a time. It's for non-
// realtime things that take a long time to render.

// This is the frame rate to render at. Too fast and you will
// miss some blocks.
const float blockRate = 20.0;
void BlockRender(in vec2 fragCoord)
{
    // blockSize is how much it will try to render in 1 frame.
    // adjust this smaller for more complex scenes, bigger for
    // faster render times.
    const float blockSize = 64.0;
    // Make the block repeatedly scan across the image based on time.
    float frame = floor(iTime * blockRate);
    vec2 blockRes = floor(iResolution.xy / blockSize) + vec2(1.0);
    // ugly bug with mod.
    //float blockX = mod(frame, blockRes.x);
    float blockX = fract(frame / blockRes.x) * blockRes.x;
    //float blockY = mod(floor(frame / blockRes.x), blockRes.y);
    float blockY = fract(floor(frame / blockRes.x) / blockRes.y) * blockRes.y;
    // Don't draw anything outside the current block.
    if ((fragCoord.x - blockX * blockSize >= blockSize) ||
    	(fragCoord.x - (blockX - 1.0) * blockSize < blockSize) ||
    	(fragCoord.y - blockY * blockSize >= blockSize) ||
    	(fragCoord.y - (blockY - 1.0) * blockSize < blockSize))
    {
//        discard;
    }
}
#endif

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Load state, unless we're in small thumbnail mode, then there is no state.
    if (iResolution.y > 160.0) LoadState();
#ifdef NON_REALTIME_HQ_RENDER
    // Optionally render a non-realtime scene with high quality
    BlockRender(fragCoord);
#endif

    // Do a multi-pass render
    vec3 finalColor = vec3(0.0);
#ifdef NON_REALTIME_HQ_RENDER
    for (float i = 0.0; i < antialiasingSamples; i++)
    {
        const float motionBlurLengthInSeconds = 1.0 / 60.0;
        // Set this to the time in seconds of the frame to render.
    localTime = iTime;
        // This line will motion-blur the renders
        localTime += Hash11(v21(fragCoord + seed)) * motionBlurLengthInSeconds;
        // Jitter the pixel position so we get antialiasing when we do multiple passes.
        vec2 jittered = fragCoord.xy + vec2(
            Hash21(fragCoord + seed),
            Hash21(fragCoord*7.234567 + seed)
            );
        // don't antialias if only 1 sample.
        if (antialiasingSamples == 1.0) jittered = fragCoord;
        // Accumulate one pass of raytracing into our pixel value
	    finalColor += RayTrace(jittered);
        // Change the random seed for each pass.
	    seed *= 1.01234567;
    }
    // Average all accumulated pixel intensities
    finalColor /= antialiasingSamples;
#else
    // Regular real-time rendering
    localTime = iTime;
    finalColor = RayTrace(fragCoord);
    HUD(finalColor, fragCoord);
#endif

    //finalColor += texelFetch(iChannel0, ivec2(fragCoord.x, iResolution.y-fragCoord.y), 0).xyz*256.0;
    fragColor = vec4(sqrt(clamp(finalColor, 0.0, 1.0)),1.0);
}


