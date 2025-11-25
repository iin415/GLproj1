#version 330 core
layout (location = 0) in vec3 aPos; //reads from vertex buffer
layout (location = 1) in vec2 aTexCoord; //read texture cords (U, V)
layout (location = 2) in vec3 aNormal;

out vec2 TexCoord; //pas coords to frag shader
out vec3 FragPos; //worldspace pos
out vec3 Normal; //worldspace normal

uniform mat4 model;
uniform mat4 view; //world -> camera
uniform mat4 projection; //camera -> clip

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;

    TexCoord = aTexCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
