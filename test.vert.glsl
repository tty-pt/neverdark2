#version 300 es
in vec3 coord3d;
in vec3 aNormal;
in vec2 aTexCoord;

out vec3 normal;
out vec3 fragPos;
out vec2 texCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
	fragPos = vec3(model * vec4(coord3d, 1.0));
	normal = mat3(transpose(inverse(model))) * aNormal;
	texCoord = aTexCoord;
	gl_Position = projection * view * vec4(fragPos, 1.0);
}
