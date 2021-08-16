#version 450
#extension GL_ARB_separate_shader_objects : enable

#define FOG_MIN_DISTANCE 32.0
#define FOG_MAX_DISTANCE 32.0 * 3.0
#define FOG_COLOR vec3(54.0 / 255.0, 197.0 / 255.0, 244.0 / 255.0)

layout(location = 0) in vec2 frag_uv;
layout(location = 1) in vec3 frag_pos;
layout(location = 2) in vec3 frag_eye_pos;

layout(location = 0) out vec4 out_color;

layout(binding = 1) uniform sampler2D chunk_mesh_texture;

void main() {
  vec4 texture_color = texture(chunk_mesh_texture, frag_uv);

  // Fog calculation
  float frag_distance = length(vec2(frag_pos.x, frag_pos.z) -
                               vec2(frag_eye_pos.x, frag_eye_pos.z));
  float fog_amount = (frag_distance - FOG_MIN_DISTANCE) /
                     (FOG_MAX_DISTANCE - FOG_MIN_DISTANCE);
  fog_amount = clamp(fog_amount, 0.0, 1.0);

  out_color.rgb = mix(texture_color.rgb, FOG_COLOR, fog_amount);
  out_color.a = 1.0;
}
