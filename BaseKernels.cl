__kernel void circle(__global float4 *buf,
                              int     stride,
                              int     x,
                              int     y,
                              float   r,
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

__kernel void floatToU8(__global float4 *in,
                        __global uchar4 *out)
{
  out[get_global_id(0)] = convert_uchar4_sat_rte(in[get_global_id(0)] * 255.0f);
}

__kernel void tileSVGOver(__global float4 *out,
                          __global float4 *in,
                          __global float4 *aux,
                                   float   opacity)
{
    float4 out_pixel;
    float4 in_pixel = in[get_global_id(0)];
    float4 aux_pixel = aux[get_global_id(0)];
    aux_pixel.s3 *= opacity;

    float alpha = aux_pixel.s3;
    float dst_alpha = in_pixel.s3;

    float a = alpha + dst_alpha * (1.0f - alpha);
    float src_term = (a > 0.0f) ? alpha / a : 0.0f;
    float aux_term = 1.0f - src_term;
    out_pixel.s012 = aux_pixel.s012 * src_term + in_pixel.s012 * aux_term;
    out_pixel.s3   = a;

    out[get_global_id(0)] = out_pixel;
}

/* Composite operations:
 * http://www.w3.org/TR/2004/WD-SVG12-20041027/rendering.html#comp-op-prop
 */

__kernel void tileSVGMultipy(__global float4 *out,
                             __global float4 *in,
                             __global float4 *aux,
                                      float   opacity)
{
  float4 out_pixel;
  float4 in_pixel = in[get_global_id(0)];
  float4 aux_pixel = aux[get_global_id(0)];
  aux_pixel.s3 *= opacity;

  /* Pre-multiply */
  in_pixel.s012 *= in_pixel.s333;
  aux_pixel.s012 *= aux_pixel.s333;

  float aA = in_pixel.s3;
  float aB = aux_pixel.s3;
  float aD = aA + aB - aA * aB;
  float3 cA = in_pixel.s012;
  float3 cB = aux_pixel.s012;

  out_pixel.s012 = cA * cB + cA * (1.0f - aB) + cB * (1.0f - aA);
  out_pixel.s3 = aD;

  /* Un-pre-multiply */
  if (out_pixel.s3 > 0.0f)
    out_pixel.s012 /= (float3)(aD, aD, aD);

  out[get_global_id(0)] = out_pixel;
}

__kernel void tileSVGMColorDodge(__global float4 *out,
                                 __global float4 *in,
                                 __global float4 *aux,
                                          float   opacity)
{
  float4 out_pixel;
  float4 in_pixel = in[get_global_id(0)];
  float4 aux_pixel = aux[get_global_id(0)];
  aux_pixel.s3 *= opacity;

  /* Pre-multiply */
  in_pixel.s012 *= in_pixel.s333;
  aux_pixel.s012 *= aux_pixel.s333;

  float aA = in_pixel.s3;
  float aB = aux_pixel.s3;
  float aD = aA + aB - aA * aB;
  float3 cA = in_pixel.s012;
  float3 cB = aux_pixel.s012;

  float3 common_path = cB * (1.0f - aA) + cA * (1.0f - aB);
  float3 out_path_if = aB * aA + common_path;
  float3 out_path_else = cA * aB / (1.0f - cB / aB) + common_path;

  out_pixel.s012 = (cB * aA + cA * aB >= aB * aA) ? out_path_if : out_path_else;
  out_pixel.s3 = aD;

  /* Un-pre-multiply */
  if (out_pixel.s3 > 0.0f)
    out_pixel.s012 /= out_pixel.s333;

  out[get_global_id(0)] = out_pixel;
}

__kernel void tileSVGColorBurn(__global float4 *out,
                               __global float4 *in,
                               __global float4 *aux,
                                        float   opacity)
{
  float4 out_pixel;
  float4 in_pixel = in[get_global_id(0)];
  float4 aux_pixel = aux[get_global_id(0)];
  aux_pixel.s3 *= opacity;

  /* Pre-multiply */
  in_pixel.s012 *= in_pixel.s333;
  aux_pixel.s012 *= aux_pixel.s333;

  float aA = in_pixel.s3;
  float aB = aux_pixel.s3;
  float aD = aA + aB - aA * aB;
  float3 cA = in_pixel.s012;
  float3 cB = aux_pixel.s012;

  float3 common_path = cB * (1.0f - aA) + cA * (1.0f - aB);
  float3 other_common_path = cB * aA + cA * aB;
  float3 out_path_if = common_path;
  float3 out_path_else = aB * (other_common_path - aB * aA) / cB + common_path;

  out_pixel.s012 = (other_common_path <= aB * aA) ? out_path_if : out_path_else;
  out_pixel.s3 = aD;

  /* Un-pre-multiply */
  if (out_pixel.s3 > 0.0f)
    out_pixel.s012 /= out_pixel.s333;

  out[get_global_id(0)] = out_pixel;
}

__kernel void tileSVGScreen(__global float4 *out,
                            __global float4 *in,
                            __global float4 *aux,
                                     float   opacity)
{
  float4 out_pixel;
  float4 in_pixel = in[get_global_id(0)];
  float4 aux_pixel = aux[get_global_id(0)];
  aux_pixel.s3 *= opacity;

  /* Pre-multiply */
  in_pixel.s012 *= in_pixel.s333;
  aux_pixel.s012 *= aux_pixel.s333;

  out_pixel = aux_pixel + in_pixel - aux_pixel * in_pixel;

  /* Un-pre-multiply */
  if (out_pixel.s3 > 0.0f)
    out_pixel.s012 /= out_pixel.s333;

  out[get_global_id(0)] = out_pixel;
}
