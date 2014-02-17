__kernel void circle(__global float4 *buf,
                              int     stride,
                              int     x,
                              int     y,
                              int     r,
                              float4  color)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  float dist = sqrt(pow(x-gidx, 2.0f) + pow(y-gidy, 2.0f));

  if (dist < r)
    {
      float4 pixel = buf[gidx + gidy * stride];

      if (dist < r - 1)
        {
          pixel.s012 = color.s012;

          if (pixel.s3 < color.s3)
            pixel.s3 = color.s3;
        }
      else
        {
          float alpha = (r - dist) * color.s3;
          float dst_alpha = pixel.s3;

          float a = alpha + dst_alpha * (1.0 - alpha);
          float a_term = dst_alpha * (1.0 - alpha);

          pixel.s012 = (color.s012 * alpha + pixel.s012 * a_term);
          pixel.s012 = pixel.s012 / a;

          if (pixel.s3 < alpha)
            pixel.s3 = alpha;
        }

      buf[gidx + gidy * stride] = pixel;
    }
}

__kernel void fill(__global float4 *buf,
                            float4  color)
{
  int gidx = get_global_id(0);

  buf[get_global_id(0)] = color;
}
