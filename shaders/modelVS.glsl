#version 450 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

layout (location = 3) in mat4 instanceModel; //for per-instance. occupies locations 3, 4, 5, 6

out vec2 TexCoords;
out vec3 FragPos;
out vec3 Normal;

uniform mat4 model; //ignore for instanced draws, keep for non-instanced
uniform mat4 view;
uniform mat4 projection;

uniform bool useInstancing;

void main()
{

    mat4 M = useInstancing ? instanceModel : model;

    TexCoords = aTexCoords; 

    vec4 worldPos = M * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;

    Normal = mat3(transpose(inverse(M))) * aNormal;

    gl_Position = projection * view * worldPos;
}