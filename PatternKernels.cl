__kernel void patternFillCircle(__global float4   *buf,
                                         int       x,
                                         int       y,
                                         int       pattern_x,
                                         int       pattern_y,
                                         float     r,
                                         image2d_t pattern)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  if (length((float2)(x - gidx, y - gidy)) <= r)
    {
      const sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_REPEAT | CLK_FILTER_NEAREST;

      float2 coord = (float2)(pattern_x + gidx, pattern_y + gidy) / convert_float2(get_image_dim(pattern));

      buf[gidx + gidy * TILE_PIXEL_WIDTH] = read_imagef(pattern, sampler, coord);
    }
}
