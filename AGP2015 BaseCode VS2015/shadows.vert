#version 330 core
// Author - Matteo Marcuzzo

// Based on and adapted from the shadow mapping tutorial by http://learnopengl.com/
// Available: http://learnopengl.com/#!Advanced-Lighting/Shadows/Shadow-Mapping
// [Accessed: November 2016]

layout (location = 0) in vec3 position;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec2 texCoords;

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;
uniform mat4 lightSpaceMatrix;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0f);
	// pass the fragment position in normal world-space coordinates
    vs_out.FragPos = vec3(model * vec4(position, 1.0));
    vs_out.Normal = transpose(inverse(mat3(model))) * normal;
    vs_out.TexCoords = texCoords;
	// pass the fragment transformed in light-space coordinates
    vs_out.FragPosLightSpace = lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);
}