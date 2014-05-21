__kernel void circle(__global float4 *buf,
                              int     stride,
                              int     x,
                              int     y,
                              int     r,
                              float4  color)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  float rr = r*r;
  float xx = (x - gidx) * (x - gidx);
  float yy = (y - gidy) * (y - gidy);

  if (xx + yy < rr)
    {
      float dist = sqrt(xx + yy);
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

          float a = alpha + dst_alpha * (1.0f - alpha);
          float a_term = dst_alpha * (1.0f - alpha);

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
  buf[get_global_id(0)] = color;
}

__kernel void tileSVGOver(__global float4 *out,
                          __global float4 *in,
                          __global float4 *aux)
{
    float4 out_pixel;
    float4 in_pixel = in[get_global_id(0)];
    float4 aux_pixel = aux[get_global_id(0)];

    float alpha = aux_pixel.s3;
    float dst_alpha = in_pixel.s3;

    float a = alpha + dst_alpha * (1.0f - alpha);
    float src_term = alpha / a;
    float aux_term = 1.0f - src_term;
    out_pixel.s012 = aux_pixel.s012 * src_term + in_pixel.s012 * aux_term;
    out_pixel.s3   = a;

    out[get_global_id(0)] = out_pixel;
}
