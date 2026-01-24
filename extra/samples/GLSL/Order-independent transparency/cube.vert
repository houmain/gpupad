#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;

layout(location = 0) out vec4 color;
layout(location = 1) out vec3 worldNormal;
layout(location = 2) out vec3 worldPosition;
layout(location = 3) out vec3 worldView;

layout(set = 1, binding = 0) uniform Camera
{
    mat4 viewMatrix;
    mat4 projectionMatrix;
}
camera;

void main()
{
    color = vec4(1.0, 1.0, 1.0, 0.05);
    worldNormal = vertexNormal;
    worldPosition = vertexPosition * 32.0;

    // Extract world eye pos from viewMatrix
    vec3 worldEyePos = inverse(camera.viewMatrix)[3].xyz;
    worldView = normalize(worldPosition - worldEyePos);

    gl_Position = camera.projectionMatrix * camera.viewMatrix * vec4(worldPosition, 1.0);
}
