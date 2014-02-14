#version 150

in vec2 texCoord;
uniform sampler2D tileImage;
out vec4 fragColor;

void main( void )
{
//    float grey = (texCoord.x + texCoord.y) / 2.0f;
//    fragColor = vec4(grey, grey, grey, 1.0);
    fragColor = texture(tileImage, texCoord);
}
