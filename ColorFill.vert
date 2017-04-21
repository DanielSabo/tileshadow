#version 150

in vec2 vertex;

void main( void )
{
    gl_Position = vec4(vertex.xy, 1.0, 1.0);
}
