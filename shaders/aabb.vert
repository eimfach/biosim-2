#version 450

// Vertex attributes input from buffer
layout (location=0) in vec3 vertex_position;
layout (location=1) in vec3 vertex_color;
layout (location=2) in vec3 vertex_normal;
layout (location=3) in vec2 vertex_uv;

// Output declaration from this shader for the next (fragment shader)
layout (location=0) out vec3 fragment_color;

// Uniform Buffer Sets (per Frame)
struct PointLight {
	vec4 position; // ignore w | in world space
	vec4 color; // w is intensity
};

layout(set = 0, binding = 0) uniform GlobalUBO {
	mat4 projectionMatrix;
	mat4 viewMatrix;
	mat4 inverseViewMatrix;
	vec3 directionalLightPosition;  // in world space
	vec4 directionalLightColor; // w is intensity
	vec4 ambientLightColor; // w is intensity
	PointLight pointLights[10];
	int numLights;
} ubo_0;

// ... more sets

// Push Constants (per Vertex) (128 Bytes guaranteed)
layout(push_constant) uniform Push {
  mat4 transformMatrix;
  vec3 color;
} push;

void main() {
	fragment_color = vertex_color;
	gl_Position = ubo_0.projectionMatrix * ubo_0.viewMatrix * push.transformMatrix * vec4(vertex_position, 1.0);
}