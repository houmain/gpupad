/*--------------------------------------------------------------------------------------
License CC0 - http://creativecommons.org/publicdomain/zero/1.0/
To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring rights to this software to the public domain worldwide. This software is distributed without any warranty.
----------------------------------------------------------------------------------------
^ This means do ANYTHING YOU WANT with this code. Because we are programmers, not lawyers.
-Otavio Good
*/

// ---------------- Config ----------------
// This is an option that lets you render high quality frames for screenshots. It enables
// stochastic antialiasing and motion blur automatically for any shader.
//#define NON_REALTIME_HQ_RENDER
const float frameToRenderHQ = 22.0; // Time in seconds of frame to render
const float antialiasingSamples = 16.0; // 16x antialiasing - too much might make the shader compiler angry.

//#define MANUAL_CAMERA

// --------------------------------------------------------
// These variables are for the non-realtime block renderer.
float localTime = 0.0;
float seed = 1.0;

// Animation variables
float fade = 1.0;
float exposure = 1.0;
vec3 poofPos = vec3(0.0);
float poofCycle = 0.0;

// lighting vars
vec3 sunDir = normalize(vec3(0.93, 1.0, 1.0));
const vec3 sunCol = vec3(250.0, 220.0, 200.0) / 3555.0;
const vec3 horizonCol = vec3(0.95, 0.95, 0.95)*1.3;
const vec3 skyCol = vec3(0.13,0.28,0.95);
const vec3 groundCol = vec3(0.003,0.7,0.75);

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
// noise functions
float Hash2d(vec2 uv)
{
    float f = uv.x + uv.y * 37.0;
    return fract(sin(f)*104003.9);
}
float Hash3d(vec3 uv)
{
    float f = uv.x + uv.y * 37.0 + uv.z * 521.0;
    return fract(sin(f)*110003.9);
}
float mixP(float f0, float f1, float a)
{
    return mix(f0, f1, a*a*(3.0-2.0*a));
}

// Noise generator from https://otaviogood.github.io/noisegen/
// Params: 3D, Seed 1, Waves 128, Octaves 7, Smooth 1
float NoiseGen(vec3 p) {
    // This is a bit faster if we use 2 accumulators instead of 1.
    // Timed on Linux/Chrome/TitanX Pascal
    float wave0 = 0.0;
    float wave1 = 0.0;
    wave0 += sin(dot(p, vec3(-1.316, 0.918, 1.398))) * 0.0783275458;
    wave1 += sin(dot(p, vec3(0.295, -0.176, 2.167))) * 0.0739931495;
    wave0 += sin(dot(p, vec3(-0.926, 1.445, 1.429))) * 0.0716716966;
    wave1 += sin(dot(p, vec3(-1.878, -0.174, 1.258))) * 0.0697839187;
    wave0 += sin(dot(p, vec3(-1.995, 0.661, -0.908))) * 0.0685409863;
    wave1 += sin(dot(p, vec3(-1.770, 1.350, -0.905))) * 0.0630152419;
    wave0 += sin(dot(p, vec3(2.116, -0.021, 1.161))) * 0.0625361712;
    wave1 += sin(dot(p, vec3(0.405, -1.712, -1.855))) * 0.0567751048;
    wave0 += sin(dot(p, vec3(1.346, 0.945, 1.999))) * 0.0556465603;
    wave1 += sin(dot(p, vec3(-0.397, -0.573, 2.495))) * 0.0555747667;
    wave0 += sin(dot(p, vec3(0.103, -2.457, -1.144))) * 0.0516322279;
    wave1 += sin(dot(p, vec3(-0.483, -1.323, 2.330))) * 0.0513093320;
    wave0 += sin(dot(p, vec3(-1.715, -1.810, -1.164))) * 0.0504567036;
    wave1 += sin(dot(p, vec3(2.529, 0.479, 1.011))) * 0.0500811899;
    wave0 += sin(dot(p, vec3(-1.643, -1.814, -1.437))) * 0.0480875812;
    wave1 += sin(dot(p, vec3(1.495, -1.905, -1.648))) * 0.0458268348;
    wave0 += sin(dot(p, vec3(-1.874, 1.559, 1.762))) * 0.0440084357;
    wave1 += sin(dot(p, vec3(1.068, -2.090, 2.081))) * 0.0413624154;
    wave0 += sin(dot(p, vec3(-0.647, -2.197, -2.237))) * 0.0401592830;
    wave1 += sin(dot(p, vec3(-2.146, -2.171, -1.135))) * 0.0391682940;
    wave0 += sin(dot(p, vec3(2.538, -1.854, -1.604))) * 0.0349588163;
    wave1 += sin(dot(p, vec3(1.687, 2.191, -2.270))) * 0.0342888847;
    wave0 += sin(dot(p, vec3(0.205, 2.617, -2.481))) * 0.0338465332;
    wave1 += sin(dot(p, vec3(3.297, -0.440, -2.317))) * 0.0289423448;
    wave0 += sin(dot(p, vec3(1.068, -1.944, 3.432))) * 0.0286404261;
    wave1 += sin(dot(p, vec3(-3.681, 1.068, 1.789))) * 0.0273625684;
    wave0 += sin(dot(p, vec3(3.116, 2.631, -1.658))) * 0.0259772492;
    wave1 += sin(dot(p, vec3(-1.992, -2.902, -2.954))) * 0.0245830241;
    wave0 += sin(dot(p, vec3(-2.409, -2.374, 3.116))) * 0.0245592756;
    wave1 += sin(dot(p, vec3(0.790, 1.768, 4.196))) * 0.0244078334;
    wave0 += sin(dot(p, vec3(-3.289, 1.007, 3.148))) * 0.0241328015;
    wave1 += sin(dot(p, vec3(3.421, -2.663, 3.262))) * 0.0199736126;
    wave0 += sin(dot(p, vec3(3.062, 2.621, 3.649))) * 0.0199230290;
    wave1 += sin(dot(p, vec3(4.422, -2.206, 2.621))) * 0.0192399437;
    wave0 += sin(dot(p, vec3(2.714, 3.022, 4.200))) * 0.0182510631;
    wave1 += sin(dot(p, vec3(-0.451, 4.143, -4.142))) * 0.0181293526;
    wave0 += sin(dot(p, vec3(-5.838, -0.360, -1.536))) * 0.0175114826;
    wave1 += sin(dot(p, vec3(-0.278, -4.565, 4.149))) * 0.0170799341;
    wave0 += sin(dot(p, vec3(-5.893, -0.163, -2.141))) * 0.0167655258;
    wave1 += sin(dot(p, vec3(4.855, -4.153, 0.606))) * 0.0163155335;
    wave0 += sin(dot(p, vec3(4.498, 0.987, -4.488))) * 0.0162770287;
    wave1 += sin(dot(p, vec3(-1.463, 5.321, -3.315))) * 0.0162569125;
    wave0 += sin(dot(p, vec3(-1.862, 4.386, 4.749))) * 0.0154338176;
    wave1 += sin(dot(p, vec3(0.563, 3.616, -5.751))) * 0.0151952226;
    wave0 += sin(dot(p, vec3(-0.126, 2.569, -6.349))) * 0.0151089405;
    wave1 += sin(dot(p, vec3(-5.094, 4.759, 0.186))) * 0.0147947096;
    wave0 += sin(dot(p, vec3(1.319, 5.713, 3.845))) * 0.0147035221;
    wave1 += sin(dot(p, vec3(7.141, -0.327, 1.420))) * 0.0140573910;
    wave0 += sin(dot(p, vec3(3.888, 6.543, 0.547))) * 0.0133309850;
    wave1 += sin(dot(p, vec3(-1.898, -3.563, -6.483))) * 0.0133171360;
    wave0 += sin(dot(p, vec3(1.719, 7.769, 0.340))) * 0.0126913718;
    wave1 += sin(dot(p, vec3(-2.210, -7.836, 0.102))) * 0.0123746071;
    wave0 += sin(dot(p, vec3(6.248, -5.451, 1.866))) * 0.0117861898;
    wave1 += sin(dot(p, vec3(1.627, -7.066, -4.732))) * 0.0115417453;
    wave0 += sin(dot(p, vec3(4.099, -7.704, 1.474))) * 0.0112591564;
    wave1 += sin(dot(p, vec3(7.357, 3.788, 3.204))) * 0.0112252325;
    wave0 += sin(dot(p, vec3(-2.797, 6.208, 6.253))) * 0.0107206906;
    wave1 += sin(dot(p, vec3(6.130, -5.335, -4.650))) * 0.0105693992;
    wave0 += sin(dot(p, vec3(5.276, -5.576, -5.438))) * 0.0105139072;
    wave1 += sin(dot(p, vec3(9.148, 2.530, -0.383))) * 0.0103996383;
    wave0 += sin(dot(p, vec3(3.894, 2.559, 8.357))) * 0.0103161113;
    wave1 += sin(dot(p, vec3(-6.604, 8.024, -0.289))) * 0.0094066875;
    wave0 += sin(dot(p, vec3(-5.925, 6.505, -6.403))) * 0.0089444733;
    wave1 += sin(dot(p, vec3(9.085, 10.331, -0.451))) * 0.0069245599;
    wave0 += sin(dot(p, vec3(-8.228, 6.323, -9.900))) * 0.0066251015;
    wave1 += sin(dot(p, vec3(10.029, -3.802, 12.151))) * 0.0058122824;
    wave0 += sin(dot(p, vec3(-10.151, -6.513, -11.063))) * 0.0057522358;
    wave1 += sin(dot(p, vec3(-1.773, -16.284, 2.828))) * 0.0056578101;
    wave0 += sin(dot(p, vec3(11.081, 8.687, -9.852))) * 0.0054614334;
    wave1 += sin(dot(p, vec3(-3.941, -4.386, 16.191))) * 0.0054454253;
    wave0 += sin(dot(p, vec3(-6.742, 2.133, -17.268))) * 0.0050050132;
    wave1 += sin(dot(p, vec3(-10.743, 5.698, 14.975))) * 0.0048323955;
    wave0 += sin(dot(p, vec3(-9.603, 12.472, 14.542))) * 0.0043264378;
    wave1 += sin(dot(p, vec3(13.515, 14.345, 8.481))) * 0.0043208884;
    wave0 += sin(dot(p, vec3(-10.330, 16.209, -9.742))) * 0.0043013736;
    wave1 += sin(dot(p, vec3(-8.580, -6.628, 19.191))) * 0.0042005922;
    wave0 += sin(dot(p, vec3(-17.154, 10.620, 11.828))) * 0.0039482427;
    wave1 += sin(dot(p, vec3(16.330, 14.123, -10.420))) * 0.0038474789;
    wave0 += sin(dot(p, vec3(-21.275, 10.768, -3.252))) * 0.0038320501;
    wave1 += sin(dot(p, vec3(1.744, 7.922, 23.152))) * 0.0037560829;
    wave0 += sin(dot(p, vec3(-3.895, 21.321, 12.006))) * 0.0037173885;
    wave1 += sin(dot(p, vec3(-22.705, 2.543, 10.695))) * 0.0036484394;
    wave0 += sin(dot(p, vec3(-13.053, -16.634, -13.993))) * 0.0036291121;
    wave1 += sin(dot(p, vec3(22.697, -11.230, 1.417))) * 0.0036280459;
    wave0 += sin(dot(p, vec3(20.646, 14.602, 3.400))) * 0.0036055008;
    wave1 += sin(dot(p, vec3(5.824, -8.717, -23.680))) * 0.0035501527;
    wave0 += sin(dot(p, vec3(6.691, 15.499, 20.079))) * 0.0035029508;
    wave1 += sin(dot(p, vec3(9.926, -22.778, 9.144))) * 0.0034694278;
    wave0 += sin(dot(p, vec3(-9.552, -27.491, 2.197))) * 0.0031359281;
    wave1 += sin(dot(p, vec3(21.071, -17.991, -11.566))) * 0.0030453280;
    wave0 += sin(dot(p, vec3(9.780, 1.783, 28.536))) * 0.0030251754;
    wave1 += sin(dot(p, vec3(8.738, -18.373, 22.725))) * 0.0029960272;
    wave0 += sin(dot(p, vec3(14.105, 25.703, -8.834))) * 0.0029840058;
    wave1 += sin(dot(p, vec3(-24.926, -17.766, -4.740))) * 0.0029487709;
    wave0 += sin(dot(p, vec3(1.060, -1.570, 32.535))) * 0.0027980099;
    wave1 += sin(dot(p, vec3(-24.532, -19.629, -16.759))) * 0.0025538949;
    wave0 += sin(dot(p, vec3(28.772, -21.183, -9.935))) * 0.0024494819;
    wave1 += sin(dot(p, vec3(-28.413, 22.959, 8.338))) * 0.0024236674;
    wave0 += sin(dot(p, vec3(-27.664, 22.197, 13.301))) * 0.0023965996;
    wave1 += sin(dot(p, vec3(-27.421, 20.643, 18.713))) * 0.0023203498;
    wave0 += sin(dot(p, vec3(18.961, -7.189, 35.907))) * 0.0021967023;
    wave1 += sin(dot(p, vec3(-23.949, 4.885, 33.762))) * 0.0021727461;
    wave0 += sin(dot(p, vec3(35.305, 8.594, 20.564))) * 0.0021689816;
    wave1 += sin(dot(p, vec3(30.364, -11.608, -27.199))) * 0.0021357139;
    wave0 += sin(dot(p, vec3(34.268, 26.742, 0.958))) * 0.0020807976;
    wave1 += sin(dot(p, vec3(-26.376, -17.313, -32.023))) * 0.0020108850;
    wave0 += sin(dot(p, vec3(31.860, -32.181, -2.834))) * 0.0019919601;
    wave1 += sin(dot(p, vec3(25.590, 32.340, 21.381))) * 0.0019446179;
    wave0 += sin(dot(p, vec3(-17.771, -23.941, 37.324))) * 0.0018898258;
    wave1 += sin(dot(p, vec3(-38.699, 19.953, -22.675))) * 0.0018379538;
    wave0 += sin(dot(p, vec3(-46.284, 11.672, -15.411))) * 0.0017980056;
    wave1 += sin(dot(p, vec3(-32.023, -43.976, -7.378))) * 0.0016399251;
    wave0 += sin(dot(p, vec3(-42.390, -21.165, -31.889))) * 0.0015752176;
    wave1 += sin(dot(p, vec3(-18.949, -40.461, 39.107))) * 0.0015141244;
    wave0 += sin(dot(p, vec3(-21.507, -5.939, -58.531))) * 0.0014339601;
    wave1 += sin(dot(p, vec3(-51.745, -43.821, 9.651))) * 0.0013096306;
    wave0 += sin(dot(p, vec3(39.239, 25.971, -52.615))) * 0.0012701774;
    wave1 += sin(dot(p, vec3(-49.669, -35.051, -36.306))) * 0.0012661695;
    wave0 += sin(dot(p, vec3(-49.996, 35.309, 38.460))) * 0.0012398870;
    wave1 += sin(dot(p, vec3(27.000, -65.904, -36.267))) * 0.0011199347;
    wave0 += sin(dot(p, vec3(-52.523, -26.557, 57.693))) * 0.0010856391;
    wave1 += sin(dot(p, vec3(-42.670, 0.269, -71.125))) * 0.0010786551;
    wave0 += sin(dot(p, vec3(-9.377, 64.575, -68.151))) * 0.0009468199;
    wave1 += sin(dot(p, vec3(14.571, -29.160, 106.329))) * 0.0008019719;
    wave0 += sin(dot(p, vec3(-21.549, 103.887, 36.882))) * 0.0007939609;
    wave1 += sin(dot(p, vec3(-42.781, 110.966, -9.070))) * 0.0007473261;
    wave0 += sin(dot(p, vec3(-112.686, 18.296, -37.920))) * 0.0007409259;
    wave1 += sin(dot(p, vec3(71.493, 33.838, -96.931))) * 0.0007121903;
    return wave0+wave1;
}

const float PI=3.14159265;

vec3 saturate(vec3 a) { return clamp(a, 0.0, 1.0); }
vec2 saturate(vec2 a) { return clamp(a, 0.0, 1.0); }
float saturate(float a) { return clamp(a, 0.0, 1.0); }

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

// This function basically is a procedural environment map that makes the sun
vec3 GetSunColorSmall(vec3 rayDir, vec3 sunDir, float sunFilter)
{
	vec3 localRay = normalize(rayDir);
	float dist = 1.0 - (dot(localRay, sunDir) * 0.5 + 0.5);
	float sunIntensity = 0.05 / dist;
    sunIntensity += exp(-dist*(40.0 + 110.0 * sunFilter))*7000.0;
	sunIntensity = min(sunIntensity, 8000.0);
	return sunCol * sunIntensity*0.2;
}

vec3 GetEnvMap(vec3 rayDir, vec3 sunDir, float sunFilter)
{
    // fade the sky color, multiply sunset dimming
    vec3 finalColor = mix(horizonCol, skyCol, pow(saturate(rayDir.y), 0.47))*0.95;

    // add the sun
    finalColor += GetSunColorSmall(rayDir, sunDir, sunFilter);
    return finalColor;
}

// min function that supports materials in the y component
vec2 matmin(vec2 a, vec2 b)
{
    if (a.x < b.x) return a;
    else return b;
}

// ---- shapes defined by distance fields ----
// See this site for a reference to more distance functions...
// http://iquilezles.org/www/articles/distfunctions/distfunctions.htm

// signed box distance field
float sdBox(vec3 p, vec3 radius)
{
  vec3 dist = abs(p) - radius;
  return min(max(dist.x, max(dist.y, dist.z)), 0.0) + length(max(dist, 0.0));
}

float cyl(vec3 p, float rad)
{
    return length(p.xy) - rad;
}

float cylcap(vec3 p, float rad, float lenrad)
{
    return max(abs(p.z) - lenrad, length(p.xy) - rad);
}

float sSphere(vec3 p, float rad)
{
    return length(p) - rad;
}

vec3 RepeatOffset(vec3 a, float len)
{
    return fract(a+vec3(-0.5, 0.0, 0.0)) - 0.5 * len;
}

// This is the distance function that defines all the scene's geometry.
// The input is a position in space.
// The output is the distance to the nearest surface, a material index,
// and the difference between the iceberg distance and the water distance.
vec2 DistanceToObject(vec3 p)
{
    float dist;

    vec3 lp = p + vec3(0.0, 1.5, 0.0);
    dist = sdBox(lp, vec3(1.75, 0.2, 0.5));
    dist = max(dist, (lp.x + lp.y)*0.72 - 1.1);
    dist = min(dist, cylcap(p.zyx + vec3(0.0, 0.85, -0.2), 0.45, 1.15));
    dist = min(dist, sdBox(p + vec3(1.5, 0.5, 0.0), vec3(0.5, 0.75, 0.5)));
    dist = max(dist, -cyl(p + vec3(1.6, 0.25, 0.0), 0.25));
    dist = min(dist, cylcap(p.zxy + vec3(0.0, -0.6, 0.0), 0.2, 0.5));
    dist = min(dist, cylcap(p.zxy + vec3(0.0, 0.25, 0.3), 0.15, 0.35));
    dist = min(dist, sdBox(p + vec3(1.4, -0.3, 0.0), vec3(0.8, 0.1, 0.6)));

    float table = sdBox(p + vec3(0.0, 2.125, 0.0), vec3(46.0, 0.1, 6.0));
    vec2 distAndMat = matmin(vec2(table, 1.0), vec2(dist, 0.0));

    vec3 mirror = abs(p + vec3(0.4, 1.5, 0.0)) + vec3(-1.0, 0.0, -0.7);
    dist = cylcap(mirror, 0.5, 0.1);
    dist = min(dist, cylcap(mirror, 0.125, 0.15));
    vec2 wheels = vec2(dist, 2.0);
    distAndMat = matmin(distAndMat, wheels);

    dist = length(p + poofPos) - (0.2 + poofCycle*0.5);
    distAndMat = matmin(distAndMat, vec2(dist, 3.0));

    return distAndMat;
}

float repramp(float x) {
	return pow(sin(x)*0.5+0.5, 8.0) + cos(x)*0.7 + 0.7;
}

vec3 TraceOneRay(vec3 camPos, vec3 rayVec, out vec3 normal, out vec2 distAndMat, out float t) {
    normal = vec3(0.0);
    distAndMat = vec2(0.0, -1.0);  // Distance and material
    vec3 finalColor = vec3(0.0);
	// ----------------------------- Ray march the scene ------------------------------
	t = 0.0;
	const float maxDepth = 32.0; // farthest distance rays will travel
	vec3 pos = vec3(0.0);
    const float smallVal = 0.000625;
	// ray marching time
    for (int i = 250; i >= 0; i--)	// This is the count of the max times the ray actually marches.
    {
        // Step along the ray.
        pos = (camPos + rayVec * t);
        // This is _the_ function that defines the "distance field".
        // It's really what makes the scene geometry. The idea is that the
        // distance field returns the distance to the closest object, and then
        // we know we are safe to "march" along the ray by that much distance
        // without hitting anything. We repeat this until we get really close
        // and then break because we have effectively hit the object.
        distAndMat = DistanceToObject(pos);

        // move down the ray a safe amount
        t += distAndMat.x;
        // If we are very close to the object, let's call it a hit and exit this loop.
        if ((t > maxDepth) || (abs(distAndMat.x) < smallVal)) break;
    }

	// --------------------------------------------------------------------------------
	// Now that we have done our ray marching, let's put some color on this geometry.
	// If a ray actually hit the object, let's light it.
    if (t <= maxDepth)
	{
        float dist = distAndMat.x;
        // calculate the normal from the distance field. The distance field is a volume, so if you
        // sample the current point and neighboring points, you can use the difference to get
        // the normal.
        vec3 smallVec = vec3(smallVal, 0, 0);
        vec3 normalU = vec3(dist - DistanceToObject(pos - smallVec.xyy).x,
                           dist - DistanceToObject(pos - smallVec.yxy).x,
                           dist - DistanceToObject(pos - smallVec.yyx).x);
        normal = normalize(normalU);

        // calculate ambient occlusion values.
        float ambientS = 1.0;
        vec2 distAndMatA = DistanceToObject(pos + normal * 0.4);
        ambientS *= saturate(distAndMatA.x*2.5);
        distAndMatA = DistanceToObject(pos + normal * 0.8);
        ambientS *= saturate(distAndMatA.x*1.25);
        ambientS = max(0.05, ambientS);

        // Trace a ray toward the sun for sun shadows
        float sunShadow = 1.0;
        float iter = 0.1;
		for (int i = 0; i < 20; i++)
        {
            float tempDist = DistanceToObject(pos + sunDir * iter).x;
	        sunShadow *= saturate(tempDist*20.0);
            if (tempDist <= 0.0) break;
            iter += max(0.05, tempDist);
        }
        sunShadow = saturate(sunShadow);

        // calculate the reflection vector for highlights
        vec3 ref = reflect(rayVec, normal);

        // ------ Calculate texture color  ------
        vec3 texColor = vec3(0.0);
        if (distAndMat.y == 2.0) {
		    vec3 mirror = pos + vec3(0.4, 1.5, 0.0) + vec3(-1.0, 0.0, -0.7);
            mirror.xz = RepeatOffset((pos + vec3(0.4, 1.5, 0.0)) + vec3(-1.0, 0.0, -0.7), 1.0).xz;
            pos = RotateZ(mirror, -localTime/(PI/4.0));
        }
        if (distAndMat.y == 1.0) pos = pos.yxz + vec3(20.0, 2.0+localTime, 0.0);
        if (distAndMat.y == 3.0) {
            pos += poofPos;
	        pos.xzy = pos;
        }

        // Wood shader
        float rings = repramp(length(pos.xz + vec2(NoiseGen(pos*vec3(8.0, 1.5, 8.0)), NoiseGen(-pos*vec3(8.0, 1.5, 8.0)+4.5678))*0.05)*64.0) / 1.8;
        rings -= NoiseGen(pos *1.0)*0.75;
        texColor = mix(vec3(0.3, 0.19, 0.075)*0.95, vec3(1.0, 0.73, 0.326)*0.4, rings)*1.5;
        texColor = max(vec3(0.0), texColor);
        float rough = (NoiseGen(pos*64.0*vec3(1.0, 0.2, 1.0))*0.1+0.9);
        texColor *= rough;
        texColor = saturate(texColor);

        // ------ Calculate lighting color ------
        // Start with sun color, standard lighting equation, and shadow
        vec3 lightColor = vec3(14.0)*sunCol * saturate(dot(sunDir, normal)) * sunShadow;
        lightColor += skyCol * ((normal.y) * 0.5 + 0.5) * 0.5 * ambientS;

        // apply the light to the texture.
        finalColor = texColor * lightColor;
        finalColor *= 2.0;

        // specular highlight
        finalColor += GetEnvMap(ref + rough*0.5, sunDir, 0.0)*0.005 * (0.5 - rings*0.5) * sunShadow * rough;
        // visualize length of gradient of distance field to check distance field correctness
        //finalColor = vec3(0.5) * (length(normalU) / smallVec.x);
        //finalColor = vec3(1.0)*ambientS;
	}
    else
    {
        // Our ray trace hit nothing, so draw background.
        finalColor =vec3(0.9);
    }
    return finalColor;
}

// Input is UV coordinate of pixel to render.
// Output is RGB color.
vec3 RayTrace(in vec2 fragCoord )
{
    fade = 1.0;

	vec3 camPos, camUp, camLookat;
	// ------------------- Set up the camera rays for ray marching --------------------
    // Map uv to [-1.0..1.0]
	vec2 uv = fragCoord.xy/iResolution.xy * 2.0 - 1.0;
    uv /= 3.0;  // zoom in

#ifdef MANUAL_CAMERA
    // Camera up vector.
	camUp=vec3(0,1,0);

	// Camera lookat.
	camLookat=vec3(0,0,0);

    // debugging camera
    float mx=-iMouse.x/iResolution.x*PI*2.0;
	float my=iMouse.y/iResolution.y*3.14*0.95 + PI/2.0;
	camPos = vec3(cos(my)*cos(mx),sin(my),cos(my)*sin(mx))*9.0;
#else
    // Repeat the animation after time t3
    float time = localTime *0.25;
    camPos = vec3(0.0, 0.4, 8.0);
    camPos.x = sin(time)*7.0;
    camPos.y += (-cos(time*2.0)+1.0)*4.0;
    camPos.z = cos(time)*7.0;
    camUp=vec3(0,1,0);
    camLookat=vec3(0,-0.5,0.5);
#endif

    // ---- animation ----
    poofCycle = fract(localTime*0.25);
    poofPos = -vec3(0.6 - poofCycle*poofCycle*12.0, poofCycle * 4.0 + 0.5, 0.0);

	// Camera setup for ray tracing / marching
	vec3 camVec=normalize(camLookat - camPos);
	vec3 sideNorm=normalize(cross(camUp, camVec));
	vec3 upNorm=cross(camVec, sideNorm);
	vec3 worldFacing=(camPos + camVec);
	vec3 worldPix = worldFacing + uv.x * sideNorm * (iResolution.x/iResolution.y) + uv.y * upNorm;
	vec3 rayVec = normalize(worldPix - camPos);

    vec3 normal;
    vec2 distAndMat;
    float t;
    vec3 finalColor = TraceOneRay(camPos, rayVec, normal, distAndMat, t);

    // vignette?
    finalColor *= vec3(1.0) * saturate(1.0 - length(uv/2.5));
    finalColor *= exposure;

	// output the final color without gamma correction - will do gamma later.
	return vec3(clamp(finalColor, 0.0, 1.0));
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
    float frame = floor(iGlobalTime * blockRate);
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
        discard;
    }
}
#endif

void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
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
	    localTime = frameToRenderHQ;
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
    localTime = iGlobalTime;
    finalColor = RayTrace(fragCoord);
#endif

    fragColor = vec4(sqrt(clamp(finalColor, 0.0, 1.0)),1.0);
}


