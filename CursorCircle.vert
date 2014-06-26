#version 150

in vec2 vertex;
out vec2 texCoord;
uniform vec4 dimensions;
uniform float pixelRadius;

void main( void )
{
    gl_Position = vec4((vertex.xy * dimensions.zw) + dimensions.xy, 1.0, 1.0);
    texCoord = vertex.xy * pixelRadius;
}
