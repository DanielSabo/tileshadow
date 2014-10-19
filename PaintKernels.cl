__kernel void fillFloats(__global float *buf,
                                  float  value)
{
  buf[get_global_id(0)] = value;
}

__kernel void maskCircle(__global float *buf,
                                  float   x,
                                  float   y,
                                  float   r,
                                  float   circle_alpha)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  float rr = r*r;
  float xx = (x - gidx) * (x - gidx);
  float yy = (y - gidy) * (y - gidy);

  if (xx + yy < rr)
    {
      float dist = sqrt(xx + yy);
      float mask = buf[gidx + gidy * TILE_PIXEL_WIDTH];

      if (dist < r - 1)
        {
          if (mask < circle_alpha)
            mask = circle_alpha;
        }
      else
        {
          float alpha = (r - dist) * circle_alpha;

          if (mask < alpha)
            mask = alpha;
        }

      buf[gidx + gidy * TILE_PIXEL_WIDTH] = mask;
    }
}

__kernel void applyMaskTile(__global float4 *out,
                            __global float4 *in,
                            __global float  *mask,
                                     float4  color)
{
  float4 out_pixel;
  float4 in_pixel = in[get_global_id(0)];
  float4 aux_pixel = color;
  aux_pixel.s3 *= mask[get_global_id(0)];

  float alpha = aux_pixel.s3;
  float dst_alpha = in_pixel.s3;

  float a = alpha + dst_alpha * (1.0f - alpha);
  float src_term = (a > 0.0f) ? alpha / a : 0.0f;
  float aux_term = 1.0f - src_term;
  out_pixel.s012 = aux_pixel.s012 * src_term + in_pixel.s012 * aux_term;
  out_pixel.s3   = a;

  out[get_global_id(0)] = out_pixel;
}
