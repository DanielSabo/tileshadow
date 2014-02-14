#version 150

in vec3 vertex;
out vec2 vertCoord;
uniform vec2 tileOrigin;
uniform vec2 tileSize;

void main( void )
{
    gl_Position = vec4(vertex.xy + tileOrigin, 1.0, 1.0);
    vertCoord = vec2(vertex.xy / tileSize.xy);
}