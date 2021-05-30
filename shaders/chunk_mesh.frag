#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 frag_uv;

layout(location = 0) out vec4 out_color;

layout(binding = 1) uniform sampler2D chunk_mesh_texture;

void main() { out_color = texture(chunk_mesh_texture, frag_uv); }
