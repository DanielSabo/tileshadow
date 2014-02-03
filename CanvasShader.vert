#version 150

in vec3 vertex;
out vec2 vertCoord;

void main( void )
{
    gl_Position = vec4(vertex, 1.0);
    vertCoord = vec2(vertex.xy);
}

/*
attribute vec4 qt_Vertex;
attribute vec4 qt_MultiTexCoord0;
uniform mat4 qt_ModelViewProjectionMatrix;
varying vec4 qt_TexCoord0;

void main(void)
{
    gl_Position = qt_ModelViewProjectionMatrix * qt_Vertex;
    qt_TexCoord0 = qt_MultiTexCoord0;
}
*/
