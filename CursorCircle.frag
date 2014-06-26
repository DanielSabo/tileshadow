#version 150

in vec2 texCoord;
out vec4 fragColor;
uniform float pixelRadius;

void main( void )
{
    float r = length(texCoord);

    if (r < pixelRadius - 1 && r > pixelRadius - 3)
      {
        if (r > pixelRadius - 2)
          fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        else
          fragColor = vec4(1.0, 1.0, 1.0, 1.0);
      }
    else
      {
        fragColor = vec4(0.0, 0.0, 0.0, 0.0);
      }
}
