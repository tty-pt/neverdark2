#version 300 es
precision mediump float;
out vec4 fragColor;

in vec3 normal;
in vec3 fragPos;
in vec2 texCoord;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;

uniform sampler2D objectTexture;

void main() {
	// ambient
	float ambientStrength = 0.1;
	vec3 ambient = ambientStrength * lightColor;

	// diffuse
	vec3 norm = normalize(normal);
	vec3 lightDir = normalize(lightPos - fragPos);
	float diff = max(dot(norm, lightDir), 0.0);
	vec3 diffuse = diff * lightColor;

	// specular
	float specularStrength = 0.5;
	vec3 viewDir = normalize(viewPos - fragPos);
	float spec = 0.0f;
	/* vec3 reflectDir = reflect(-lightDir, norm); */
	// blinn-phong
	vec3 halfwayDir = normalize(lightDir + viewDir);
	spec = pow(max(dot(normal, halfwayDir), 0.0), 32.0f);
	// phong
	/* vec3 reflectDir = reflect(-lightDir, norm); */
	/* spec = pow(max(dot(viewDir, reflectDir), 0.0), 8.0f); */

	vec3 specular = specularStrength * spec * lightColor;

	vec4 result = vec4((ambient + diffuse + specular) * objectColor, 1.0) \
		      * texture(objectTexture, texCoord);
	fragColor = result;
}
