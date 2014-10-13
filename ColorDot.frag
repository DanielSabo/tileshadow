#version 150

in vec2 texCoord;
out vec4 fragColor;
uniform vec4 previewColor;
uniform float pixelRadius;

void main(void)
{
    float r = length(texCoord);

    if (r < pixelRadius)
    {
        fragColor = previewColor;
        fragColor.w *= clamp((pixelRadius - r), 0.0, 1.0);
    }
    else
        fragColor = vec4(0.0, 0.0, 0.0, 0.0);
}
