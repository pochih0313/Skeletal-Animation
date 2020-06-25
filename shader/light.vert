#version 330 core

uniform mat4 model;
uniform mat4 projection;
uniform mat4 view;
const int MAX_BONES = 240;
uniform mat4 bones[MAX_BONES];

layout(location=0) in vec3 position;
layout(location=1) in vec3 normal;
layout(location=2) in vec2 texcoord;
layout (location = 3) in ivec4 bone_ids;
layout (location = 4) in vec4 weights;

// out vec2 fTexcoord;
out vec3 fNormal;
out vec3 fPosition;

void main(void)
{
  	mat4 bone_transform = bones[bone_ids[0]] * weights[0];
	bone_transform += bones[bone_ids[1]] * weights[1];
	bone_transform += bones[bone_ids[2]] * weights[2];
	bone_transform += bones[bone_ids[3]] * weights[3];

  	//fTexcoord = texcoord;
  	vec4 pos = (projection * view * model) * bone_transform * vec4(position, 1.0);
	fPosition = vec3(model * bone_transform * vec4(position, 1.0f));
	fNormal = mat3(transpose(inverse(model * bone_transform))) * normal;
	gl_Position = pos;
}