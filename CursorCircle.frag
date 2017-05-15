#version 150

in vec2 texCoord;
out vec4 fragColor;
uniform vec4 innerColor;
uniform vec4 outerColor;
uniform float pixelRadius;

void main( void )
{
    float r = length(texCoord);

    if (r < pixelRadius - 1 && r > pixelRadius - 3)
      {
        if (r > pixelRadius - 2)
          fragColor = outerColor;
        else
          fragColor = innerColor;
      }
    else
      {
        fragColor = vec4(0.0, 0.0, 0.0, 0.0);
      }
}
