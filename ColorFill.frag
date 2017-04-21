#version 150

out vec4 fragColor;
uniform vec4 fillColor;

void main( void )
{
  fragColor = fillColor;
}
