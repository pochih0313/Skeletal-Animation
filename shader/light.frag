#version 330 core

layout(location=0) out vec4 color;

// in vec2 fTexcoord;
in vec3 fPosition;
in vec3 fNormal;

uniform vec3 light_color = vec3(1.0f, 1.0f, 1.0f);
uniform vec3 object_color = vec3(0.54f, 0.21f, 0.06f);
uniform float shininess = 8.0f;

uniform vec3 lightPos;
uniform vec3 viewPos;

uniform bool blinnFlag;
//uniform sampler2D uSampler;

//glBindTexture in main set value to sampler2D
void main()
{
	vec3 lightDir = normalize(lightPos - fPosition);
	vec3 normal = normalize(fNormal);
	float diffuse = max(0.0f, dot(normal, lightDir));

	vec3 viewDir = normalize(lightPos - fPosition);
    float spec;
	
	if (blinnFlag) {
        vec3 halfwayDir = normalize(lightDir + viewDir);
        spec = pow(max(0.0f, dot(normal, halfwayDir)), shininess * 4);
    } else {
        vec3 r = reflect(-lightDir, normal);
        spec = pow(max(0.0f, dot(viewDir, r)), shininess);
    }
	
	vec3 color_ambient = 0.05 * light_color * object_color;
	vec3 color_diffuse = diffuse * light_color * object_color;
	vec3 color_specular = 0.3 * spec * light_color * object_color;

	vec3 result = color_ambient + color_diffuse + color_specular;
	color = vec4(result, 1.0f);
}