#version 460 core
#extension GL_EXT_ray_tracing : enable

layout(location = 0) rayPayloadInEXT vec4 payload;

struct SphereData {
    vec3 center;
    float radius;
    vec4 color;
};

layout(std430, set = 2, binding = 0) readonly buffer Spheres
{
    SphereData data[];
}
spheres;

void main()
{
    // Compute some lighting because we can
    vec3 lightDir = normalize(vec3(1.0));

    SphereData sphereData = spheres.data[gl_PrimitiveID];
    // Intersection point on sphere surface
    vec3 worldHit = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
    // Normal from Sphere
    vec3 normalAtHit = normalize(worldHit - sphereData.center);

    // Diffuse Factor
    float diffuse = max(dot(lightDir, normalAtHit), 0.0);

    payload = vec4(sphereData.color.rgb * diffuse, 1);
}
