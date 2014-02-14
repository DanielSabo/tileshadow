#version 150

in vec2 vertex;
out vec2 texCoord;
uniform vec2 tileOrigin;
uniform vec2 tileSize;

void main( void )
{
    gl_Position = vec4((vertex.xy * tileSize.xy) + tileOrigin.xy, 1.0, 1.0);
    texCoord = vec2(vertex.x, 1.0 - vertex.y);
}
