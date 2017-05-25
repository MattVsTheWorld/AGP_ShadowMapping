#version 330 core

// Based on and adapted from the shadow mapping tutorial by http://learnopengl.com/
// Available: http://learnopengl.com/#!Advanced-Lighting/Shadows/Shadow-Mapping
// [Accessed: November 2016]
// This fragment shader uses the Blinn-Phong lighting model

out vec4 FragColor;

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
    vec4 FragPosLightSpace;
} fs_in;

uniform sampler2D diffuseTexture;
uniform sampler2D shadowMap;

uniform vec3 lightPos;
uniform vec3 viewPos;

float ShadowCalculation(vec4 fragPosLightSpace)
{

    // perform perspective divide (unnecessary for orthographic projection but still good use)
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Compress to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    // Get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(shadowMap, projCoords.xy).r; 

    // Get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;

	// Shadow bias; we offset the depth of the surface (or the shadow map) by a small bias amount 
	// such that fragments are not incorrectly considered below the surface.
	vec3 normal = normalize(fs_in.Normal);
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);
    float bias = max(0.0005 * (1.0 - dot(normal, lightDir)), 0.0005);

	// Check whether current frag pos is in shadow
	// float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;  
	float shadow = 0.0;

	// PCF - combine and average neighbouring texels to make shadows softer
	vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
		for(int x = -1; x <= 1; ++x)
		{
			for(int y = -1; y <= 1; ++y)
			{
				float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
				shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
			}    
		}
	shadow /= 9.0;

	// Force the shadow value to 0.0 for coordinates outside the far plane of the light's orthographic frustum
	if(projCoords.z > 1.0)
        shadow = 0.0;

    return shadow;
}

void main()
{           
    vec3 color = texture(diffuseTexture, fs_in.TexCoords).rgb;
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightColor = vec3(1.0);
    // Ambient
    vec3 ambient = 0.15 * color;
    // Diffuse
    vec3 lightDir = normalize(lightPos - fs_in.FragPos);
    float diff = max(dot(lightDir, normal), 0.0);
    vec3 diffuse = diff * lightColor;
    // Specular
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);
    float spec = 0.0;
    vec3 halfwayDir = normalize(lightDir + viewDir);  
    spec = pow(max(dot(normal, halfwayDir), 0.0), 64.0);
    vec3 specular = spec * lightColor;    
    // Calculate shadow
    float shadow = ShadowCalculation(fs_in.FragPosLightSpace);       
	// Ambient is untouched to simulate light scattering
	// diffuse and specular depend on whether the fragment is shadowed or not
    vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular)) * color;    


    
    FragColor = vec4(lighting, 1.0f);
}