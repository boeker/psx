#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D textureTexture;

void main()
{
    FragColor = texture(textureTexture, TexCoords).rgba;
    //FragColor = vec4(texture(textureTexture, TexCoords).rg, 1.0, 1.0);
    //FragColor = vec4(1.0, 0.5, 0.5, 1.0);
}
