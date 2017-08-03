#version 150

in vec2 vertex;
out vec2 texCoord;
uniform vec2 tileOrigin;
uniform vec2 tileScale;
uniform vec2 tilePixels;

void main( void )
{
    gl_Position = vec4((vertex.xy * tileScale.xy) + tileOrigin.xy, 1.0, 1.0);
    texCoord = vertex;
}
