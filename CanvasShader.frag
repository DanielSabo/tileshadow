#version 150

in vec2 vertCoord;
out vec4 fragColor;

void main( void )
{
    float grey = (vertCoord.x + vertCoord.y) / 2.0f;
    fragColor = vec4(grey, grey, grey, 1.0);
}
