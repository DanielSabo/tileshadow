#version 150

in vec2 texCoord;
uniform vec2 tilePixels;
uniform samplerBuffer tileImage;
out vec4 fragColor;

void main( void )
{
    int x = int(texCoord.x);
    int y = int(texCoord.y);
    int stride = int(tilePixels.x);
    fragColor = texelFetch(tileImage, x + y * stride);
}
