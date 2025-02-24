#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D textureTexture;

void main()
{
    vec4 texColor = texture(textureTexture, TexCoords);
    if (texColor.a < 1.0) {
        discard;
    }

    FragColor = texColor;
}
