#version 150

in vec2 texCoord;
uniform int binSize;
uniform vec2 tilePixels;
uniform samplerBuffer tileImage;
out vec4 fragColor;

void main( void )
{
    int stride = int(tilePixels.x);
    if (binSize > 1)
    {
        int x0 = (int(texCoord.x) / binSize) * binSize;
        int y0 = (int(texCoord.y) / binSize) * binSize;
        vec4 sum = vec4(0.0);

        for (int iy = 0; iy < binSize; ++iy)
        {
            int yOffset = (y0 + iy) * stride;
            for (int ix = 0; ix < binSize; ++ix)
            {
                int x = x0 + ix;
                sum += texelFetch(tileImage, x + yOffset);
            }
        }
        fragColor = sum / float(binSize * binSize);
    }
    else
    {
        int x = int(texCoord.x);
        int y = int(texCoord.y);
        fragColor = texelFetch(tileImage, x + y * stride);
    }
}
