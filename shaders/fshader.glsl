
#version 450 core
out vec4 FragColor;

//in vec3 ourColor;
in vec2 TexCoord;

uniform sampler2D wood;
uniform sampler2D face;

void main()
{
	//FragColor = vec4(ourColor, 1.0);
	//FragColor = texture(ourTexture, TexCoord) * vec4(ourColor, 1.0);
	//Now we are combining two textures: the 0.2 yields 80% of the first input and 20% of the second
	//FragColor = mix(texture(wood, TexCoord), texture(face, TexCoord), 0.2);
	FragColor = texture(wood, TexCoord);
}