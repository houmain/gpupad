#version 450
#extension GL_ARB_separate_shader_objects : enable

vec3 vertices[6] = {
    vec3(-1.0, -1.0, 0.5),
    vec3(-1.0, 1.0, 0.5),
    vec3(1.0, 1.0, 0.5),
    vec3(1.0, 1.0, 0.5),
    vec3(1.0, -1.0, 0.5),
    vec3(-1.0, -1.0, 0.5),
};

void main()
{
    gl_Position = vec4(vertices[gl_VertexIndex], 1.0);
}
