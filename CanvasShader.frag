#version 150

in vec2 vertCoord;
out vec4 fragColor;

void main( void )
{
    float grey = (vertCoord.x + vertCoord.y) / 2.0f;
    fragColor = vec4(grey, grey, grey, 1.0);
}
/*
uniform sampler2D qt_Texture0;
varying vec4 qt_TexCoord0;

void main(void)
{
    gl_FragColor = texture2D(qt_Texture0, qt_TexCoord0.st);
}
*/
