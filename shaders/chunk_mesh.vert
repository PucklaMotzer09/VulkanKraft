#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in float in_light_value;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec3 frag_pos;
layout(location = 2) out vec3 frag_eye_pos;
layout(location = 3) out float frag_light_value;

layout(binding = 0) uniform Global {
  mat4 proj_view;
  vec3 eye_pos;
}
global;

void main() {
  gl_Position = global.proj_view * vec4(in_position, 1.0);
  frag_uv = in_uv;
  frag_pos = in_position;
  frag_eye_pos = global.eye_pos;
  frag_light_value = in_light_value;
}
