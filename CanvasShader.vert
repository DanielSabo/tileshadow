#version 150

in vec2 vertex;
out vec2 texCoord;
uniform vec2 tileOrigin;
uniform vec4 tileMatrix;
uniform vec2 tilePixels;

void main( void )
{
    gl_Position = vec4((vertex.xx * tileMatrix.xy + vertex.yy * tileMatrix.zw) + tileOrigin.xy, 1.0, 1.0);
    texCoord = vertex;
}
