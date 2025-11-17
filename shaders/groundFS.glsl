#version 330 core
in vec3 WorldPos;

out vec4 FragColor; //ground plane w checkered pattern

uniform vec3 colorA;
uniform vec3 colorB;
uniform float scale; //how large each tile is

void main()
{
    float x = WorldPos.x / scale;
    float z = WorldPos.z / scale;

    // floor and mod --> determine parity
    float fx = floor(x);
    float fz = floor(z);
    float parity = mod(fx + fz, 2.0);

    vec3 c = mix(colorA, colorB, parity);
    FragColor = vec4(c, 1.0);
}
