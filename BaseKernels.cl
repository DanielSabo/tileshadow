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

__kernel void gradientApply(__global const float4 *src,
                            __global float4 *dst,
                                     int2    offset,
                                     float2  vec,
                                     float4  color)
{
  const float dither[] = {
    -0.000894779815076f, 0.002526183202987f, -0.002977620989162f, -0.000961841665921f, -0.002219203533179f, 0.001836599187491f,
    0.000179014870731f, 0.003835451823258f, -0.002844696376256f, -0.002614203357553f, -0.000898684569121f, -0.003781822497427f,
    -0.002539903013743f, -0.000730459373852f, -0.000492445411811f, -0.000019733080357f, 0.000133445502853f, -0.002035096966073f,
    -0.001587624212566f, 0.001823724900110f, 0.003437126611359f, -0.002672838034602f, 0.000914430428299f, 0.001704298686780f,
    0.003892962149703f, -0.001302441040775f, -0.000471331115497f, 0.001042514261109f, -0.003063514405616f, 0.000820492568349f,
    0.001999632846291f, -0.001792838602531f, 0.002961764819442f, 0.001970719332613f, -0.001919485561856f, -0.000647735315795f,
    0.002299355285000f, -0.000581302872970f, -0.002460143133076f, 0.003562597378860f, -0.000396333070068f, -0.003393733448024f,
    0.002455015627391f, -0.002245091262948f, 0.001779916879067f, 0.002850201434824f, 0.003134923628933f
  };
  const int dither_size = sizeof(dither) / sizeof(dither[0]);

  int gidx = get_global_id(0);
  int gidy = get_global_id(1);
  int idx = gidx + gidy * TILE_PIXEL_WIDTH;

  int2 coord = (int2)(gidx, gidy) + offset;
  float value = vec.x * coord.x + vec.y * coord.y + dither[idx % dither_size];
  value = clamp(value, 0.0f, 1.0f);

  float3 outColor = color.s012 * value + src[idx].s012 * (1.0f - value);
  dst[idx] = (float4)(outColor, src[idx].s3);
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
