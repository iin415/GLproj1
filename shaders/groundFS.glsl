#version 330 core
in vec2 TexCoord;
in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform sampler2D groundTexture;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;

uniform float shininess;

uniform bool flashlightOn;
uniform vec3 flashlightPos;
uniform vec3 flashlightDir;
uniform float innerCutoff; // inner "cone"
uniform float outerCutoff; // outer "cone"
uniform float strength;
uniform vec3 flashlightColor;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    vec3 albedo = texture(groundTexture, TexCoord).rgb;

    vec3 lightDir = normalize(lightPos - FragPos);
    float dist = length(lightPos - FragPos);
    float attenuation = 5.0 / (dist * dist);
    
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    float specStrength = 0.5f;

    vec3 ambient = 0.05 * albedo;
    vec3 diffuse = diff * albedo * lightColor * attenuation;
    vec3 specular = specStrength * spec * lightColor * attenuation;

    vec3 result = ambient + diffuse + specular;

    // FLASHLIGHT
    if(flashlightOn) {
        vec3 lightToFrag = normalize(FragPos - flashlightPos);
        float theta = dot(lightToFrag, normalize(flashlightDir));
        float epsilon = innerCutoff - outerCutoff;
        float intensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);

        vec3 color = mix(flashlightColor, vec3(1.0), intensity); //white at center, yellow around edges

        vec3 flashDir = normalize(flashlightPos - FragPos);
        float flashDiff = max(dot(norm, flashDir), 0.0);
        vec3 flashDiffuse = flashDiff * albedo;

        vec3 reflectDir = reflect(-flashDir, norm);
        vec3 viewDir = normalize(viewPos - FragPos);
        float flashSpec = pow(max(dot(reflectDir, viewDir), 0.0), shininess);
        vec3 flashSpecular = flashSpec * vec3(1.0);

        dist = length(flashlightPos - FragPos)/25;
        float atten = 1.0 / (1.0 + 0.1 * dist + 0.05 * dist * dist);
        atten = atten * atten * atten; // for sharper falloff

        result += (flashDiffuse + flashSpecular) * intensity * atten * strength * color; //add flashlight contribution to shading
    }

    FragColor = vec4(result, 1.0);
}
