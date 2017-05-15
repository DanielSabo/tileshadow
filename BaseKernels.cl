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

  float4 outColor = src[idx];
  if (outColor.s3 > 0.0f)
    outColor.s012 = color.s012 * value + outColor.s012 * (1.0f - value);
  dst[idx] = outColor;
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

/* Color compositing operations from:
 * http://www.w3.org/TR/2015/CR-compositing-1-20150113/#blendingnonseparable */

float3 hsl_clip_color(float3 color);
float hsl_lum(float3 color);
float3 hsl_set_lum(float3 color, float lum);
float hsl_sat(float3 color);
float3 hsl_set_sat(float3 color, float s);

float3 hsl_clip_color(float3 color)
{
  float lum = hsl_lum(color);
  float3 lum_vec = (float3)(lum, lum, lum);
  float n = min(min(color.s0, color.s1), color.s2);
  float x = max(max(color.s0, color.s1), color.s2);
  if (n < 0.0f)
    color = lum_vec + (((color - lum_vec) * lum_vec) / (lum_vec - (float3)(n, n, n)));
  if (x > 1.0f)
    color = lum_vec + (((color - lum_vec) * ((float3)(1.0f, 1.0f, 1.0f) - lum_vec)) / ((float3)(x, x, x) - lum_vec));
  // Sanity clamp, the above code sometimes produces bad values when lum is zero
  color = clamp(color, (float3)(0.0f, 0.0f, 0.0f), (float3)(1.0f, 1.0f, 1.0f));
  return color;
}

float hsl_lum(float3 color)
{
  return 0.3f * color.s0 + 0.59f * color.s1 + 0.11f * color.s2;
}

float3 hsl_set_lum(float3 color, float lum)
{
  float d = lum - hsl_lum(color);
  color += (float3)(d, d, d);
  return hsl_clip_color(color);
}

float hsl_sat(float3 color)
{
  return max(max(color.s0, color.s1), color.s2) - min(min(color.s0, color.s1), color.s2);
}

float3 hsl_set_sat(float3 src_color, float s)
{
  float colors[3] = {src_color.s0, src_color.s1, src_color.s2};
  int Cmin = 0;
  int Cmid = 1;
  int Cmax = 2;

  // mini-bubble sort to find Cmin, Cmid, Cmax
  if (colors[Cmin] > colors[Cmid])
   { int tmp = Cmin; Cmin = Cmid; Cmid = tmp; }
  if (colors[Cmid] > colors[Cmax])
   { int tmp = Cmid; Cmid = Cmax; Cmax = tmp; }
  if (colors[Cmin] > colors[Cmid])
   { int tmp = Cmin; Cmin = Cmid; Cmid = tmp; }

  if (colors[Cmax] > colors[Cmin])
  {
    colors[Cmid] = ((colors[Cmid] - colors[Cmin]) * s) / (colors[Cmax] - colors[Cmin]);
    colors[Cmax] = s;
    colors[Cmin] = 0;
  }
  else
  {
    return (float3){0.0f, 0.0f, 0.0f};
  }
  return (float3){colors[0], colors[1], colors[2]};
}

__kernel void tileSVGHue(__global float4 *out,
                         __global float4 *in,
                         __global float4 *aux,
                                  float   opacity)
{
  float4 out_pixel;
  float4 in_pixel = in[get_global_id(0)];
  float4 aux_pixel = aux[get_global_id(0)];

  float alpha = aux_pixel.s3 * opacity;
  if (alpha > 0.0f)
  {
    float dst_alpha = in_pixel.s3;
    aux_pixel.s012 = hsl_set_lum(
                       hsl_set_sat(aux_pixel.s012, hsl_sat(in_pixel.s012)),
                       hsl_lum(in_pixel.s012)
                     );

    float a = alpha + dst_alpha * (1.0f - alpha);
    float src_term = alpha / a;
    float aux_term = 1.0f - src_term;
    out_pixel.s012 = aux_pixel.s012 * src_term + in_pixel.s012 * aux_term;
    out_pixel.s3   = a;
  }
  else
    out_pixel = in_pixel;

  out[get_global_id(0)] = out_pixel;
}

__kernel void tileSVGSaturation(__global float4 *out,
                                __global float4 *in,
                                __global float4 *aux,
                                         float   opacity)
{
  float4 out_pixel;
  float4 in_pixel = in[get_global_id(0)];
  float4 aux_pixel = aux[get_global_id(0)];

  float alpha = aux_pixel.s3 * opacity;
  if (alpha > 0.0f)
  {
    float dst_alpha = in_pixel.s3;
    aux_pixel.s012 = hsl_set_lum(
                       hsl_set_sat(in_pixel.s012, hsl_sat(aux_pixel.s012)),
                       hsl_lum(in_pixel.s012)
                     );

    float a = alpha + dst_alpha * (1.0f - alpha);
    float src_term = alpha / a;
    float aux_term = 1.0f - src_term;
    out_pixel.s012 = aux_pixel.s012 * src_term + in_pixel.s012 * aux_term;
    out_pixel.s3   = a;
  }
  else
    out_pixel = in_pixel;

  out[get_global_id(0)] = out_pixel;
}

__kernel void tileSVGColor(__global float4 *out,
                           __global float4 *in,
                           __global float4 *aux,
                                    float   opacity)
{
  float4 out_pixel;
  float4 in_pixel = in[get_global_id(0)];
  float4 aux_pixel = aux[get_global_id(0)];

  float alpha = aux_pixel.s3 * opacity;
  if (alpha > 0.0f)
  {
    float dst_alpha = in_pixel.s3;
    aux_pixel.s012 = hsl_set_lum(aux_pixel.s012, hsl_lum(in_pixel.s012));

    float a = alpha + dst_alpha * (1.0f - alpha);
    float src_term = alpha / a;
    float aux_term = 1.0f - src_term;
    out_pixel.s012 = aux_pixel.s012 * src_term + in_pixel.s012 * aux_term;
    out_pixel.s3   = a;
  }
  else
    out_pixel = in_pixel;

  out[get_global_id(0)] = out_pixel;
}

__kernel void tileSVGLuminosity(__global float4 *out,
                                __global float4 *in,
                                __global float4 *aux,
                                         float   opacity)
{
  float4 out_pixel;
  float4 in_pixel = in[get_global_id(0)];
  float4 aux_pixel = aux[get_global_id(0)];

  float alpha = aux_pixel.s3 * opacity;
  if (alpha > 0.0f)
  {
    float dst_alpha = in_pixel.s3;
    aux_pixel.s012 = hsl_set_lum(in_pixel.s012, hsl_lum(aux_pixel.s012));

    float a = alpha + dst_alpha * (1.0f - alpha);
    float src_term = alpha / a;
    float aux_term = 1.0f - src_term;
    out_pixel.s012 = aux_pixel.s012 * src_term + in_pixel.s012 * aux_term;
    out_pixel.s3   = a;
  }
  else
    out_pixel = in_pixel;

  out[get_global_id(0)] = out_pixel;
}

/* Porter-Duff operations from:
 * http://www.w3.org/TR/2015/CR-compositing-1-20150113/ */

__kernel void tileSVGDstOut(__global float4 *out,
                            __global float4 *in,
                            __global float4 *aux,
                                     float   opacity)
{
  float4 in_pixel = in[get_global_id(0)];
  float4 aux_pixel = aux[get_global_id(0)];

  in_pixel.s3 *= 1.0f - (aux_pixel.s3 * opacity);

  out[get_global_id(0)] = in_pixel;
}

__kernel void tileSVGDstIn(__global float4 *out,
                           __global float4 *in,
                           __global float4 *aux,
                                    float   opacity)
{
  float4 in_pixel = in[get_global_id(0)];
  float4 aux_pixel = aux[get_global_id(0)];

  in_pixel.s3 *= aux_pixel.s3 * opacity;

  out[get_global_id(0)] = in_pixel;
}

__kernel void tileSVGSrcAtop(__global float4 *out,
                             __global float4 *in,
                             __global float4 *aux,
                                      float   opacity)
{
    float4 out_pixel;
    float4 in_pixel = in[get_global_id(0)];
    float4 aux_pixel = aux[get_global_id(0)];

    // SVG Src-Atop (Premultiplied):
    //(as x Cs x ab + ab x Cb x (1 – as))
    //(as x Cs + Cb x (1 – as)) x ab
    // Any common terms can be discarded because they don't impact the ratio of src to dst
    //(as x Cs + Cb x (1 – as))
    float alpha = aux_pixel.s3 * opacity;
    out_pixel.s012 = aux_pixel.s012 * alpha + in_pixel.s012 * (1.0f - alpha);
    out_pixel.s3 = in_pixel.s3;

    out[get_global_id(0)] = out_pixel;
}

__kernel void tileSVGDstAtop(__global float4 *out,
                             __global float4 *in,
                             __global float4 *aux,
                                      float   opacity)
{
    float4 out_pixel;
    float4 in_pixel = in[get_global_id(0)];
    float4 aux_pixel = aux[get_global_id(0)];

    float alpha = aux_pixel.s3 * opacity;
    out_pixel.s012 = aux_pixel.s012 * (1.0f - in_pixel.s3) + in_pixel.s012 * in_pixel.s3;
    out_pixel.s3 = alpha;

    out[get_global_id(0)] = out_pixel;

}

__kernel void tileColorMask(__global float4 *out,
                            __global float4 *in,
                            __global float4 *aux,
                                     float4  color)
{
    float4 out_pixel;
    float4 in_pixel = in[get_global_id(0)];
    float aux_alpha = aux[get_global_id(0)].s3;

    float alpha = aux_alpha * color.s3;
    float dst_alpha = in_pixel.s3;

    float a = alpha + dst_alpha * (1.0f - alpha);
    float src_term = (a > 0.0f) ? alpha / a : 0.0f;
    float aux_term = 1.0f - src_term;
    out_pixel.s012 = color.s012 * src_term + in_pixel.s012 * aux_term;
    out_pixel.s3   = a;

    out[get_global_id(0)] = out_pixel;
}
