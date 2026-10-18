#version 450

layout(location = 0) out vec2 vTexCoord;

#if defined(VULKAN)
#define VERTEX_ID gl_VertexIndex
#else
#define VERTEX_ID gl_VertexID
#endif

mat4 perspective(float fieldOfView, float aspect, float nearPlane,
                 float farPlane)
{
    float f = 1.0 / tan(fieldOfView * 0.5);
    mat4 projection = mat4(0.0);
    projection[0][0] = f / aspect;
#if defined(VULKAN)
    projection[1][1] = -f;
    projection[2][2] = farPlane / (nearPlane - farPlane);
    projection[3][2] = nearPlane * farPlane / (nearPlane - farPlane);
#else
    projection[1][1] = f;
    projection[2][2] = (farPlane + nearPlane) / (nearPlane - farPlane);
    projection[3][2] = 2.0 * nearPlane * farPlane
                     / (nearPlane - farPlane);
#endif
    projection[2][3] = -1.0;
    return projection;
}

mat4 lookAt(vec3 eye, vec3 center, vec3 up)
{
    vec3 forward = normalize(center - eye);
    vec3 side = normalize(cross(forward, up));
    vec3 cameraUp = cross(side, forward);

    return mat4(
        side.x, cameraUp.x, -forward.x, 0.0,
        side.y, cameraUp.y, -forward.y, 0.0,
        side.z, cameraUp.z, -forward.z, 0.0,
        -dot(side, eye), -dot(cameraUp, eye), dot(forward, eye), 1.0);
}

void main()
{
    const vec3 positions[4] = vec3[](
        vec3(-400.0, 0.0,    4.0),
        vec3( 400.0, 0.0,    4.0),
        vec3(-400.0, 0.0, -900.0),
        vec3( 400.0, 0.0, -900.0));

    vec3 worldPosition = positions[VERTEX_ID];
    vTexCoord = worldPosition.xz * 0.01;

    mat4 view = lookAt(vec3(0.0, 5.0, 10.0), vec3(0.0, 0.0, -55.0),
                       vec3(0.0, 1.0, 0.0));
    mat4 projection = perspective(radians(60.0), 16.0 / 9.0, 0.1, 1200.0);
    gl_Position = projection * view * vec4(worldPosition, 1.0);
}
