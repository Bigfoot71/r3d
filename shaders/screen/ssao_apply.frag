#version 330 core

/* === Varyings === */

noperspective in vec2 vTexCoord;

/* === Uniforms === */

uniform sampler2D uTexColor;
uniform sampler2D uTexAmbient;
uniform sampler2D uTexSSAO;

uniform float uSSAOPower;

/* === Fragments === */

out vec3 FragColor;

/* === Main program === */

void main()
{
    // Sampling scene color texture
    vec3 color = texture(uTexColor, vTexCoord).rgb;
	
	// Sampling scene ambient texture
    vec3 ambient = texture(uTexAmbient, vTexCoord).rgb;

	// Sampling SSAO texture
    float ssao = texture(uTexSSAO, vTexCoord).r;
	
	// Apply power factor to SSAO
    if (uSSAOPower != 1.0) {
        ssao = pow(ssao, uSSAOPower);
    }
	
	// Attenuate SSAO
	vec3 ssaoColor = vec3(1.0 - ssao) * ambient;
	
    // Final color output
	FragColor = color - ssaoColor;
}
