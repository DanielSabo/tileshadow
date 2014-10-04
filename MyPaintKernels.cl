/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Copyright 2008-2011 Martin Renold <martinxyz@gmx.ch>
 *                2014 Daniel Sabo
 */

float calculate_rr (float xx, float yy, float radius, float aspect_ratio, float sn, float cs);
float calculate_alpha (float rr, float hardness, float segment1_offset, float segment1_slope, float segment2_offset, float segment2_slope);
float color_query_weight (float xx, float yy, float radius);

float calculate_rr (float xx, float yy, float radius, float aspect_ratio, float sn, float cs)
{
  float yyr = (yy * cs - xx * sn) * aspect_ratio;
  float xxr =  yy * sn + xx * cs;
  float rr  = (yyr * yyr + xxr * xxr) / (radius * radius);
  // rr is in range 0.0..1.0*sqrt(2)

  return rr;
}

float calculate_alpha (float rr, float hardness, float segment1_offset, float segment1_slope, float segment2_offset, float segment2_slope)
{
  if (rr <= 1.0f)
    {
      if (rr <= hardness)
        return segment1_offset + rr * segment1_slope;
      else
        return segment2_offset + rr * segment2_slope;
    }

  return 0.0f;
}


float color_query_weight (float xx, float yy, float radius)
{

/*
  float rr = calculate_rr (xx, yy, radius, 1.0f, 0.0f, 1.0f);
  float pixel_weight = calculate_alpha(rr, 0.5f, 1.0f, -1.0f, 1.0f, -1.0f);
*/

  float rr  = (yy * yy + xx * xx) / (radius * radius);

  if (rr <= 1.0f)
    return 1.0f - rr;

  return 0.0f;
}

__kernel void mypaint_color_query_part1(__global float4 *buf,
                                                 float   x,
                                                 float   y,
                                                 int     offset,
                                                 int     width,
                                                 int     accum_offset,
                                        __global float4 *accum,
                                                 int     stride,
                                                 float   radius)
{
  /* The real mypaint function renders a dab with these constants
    const float hardness = 0.5f;
    const float aspect_ratio = 1.0f;
    const float angle = 0.0f;

    Therefor:
    float slope1 = -(1.0f / 0.5f - 1.0f);
    float slope2 = -(0.5f / (1.0f - 0.5f));

    float slope1 = -1.0f;
    float slope2 = -1.0f);
  */
  int gidy = get_global_id(0);

  float4 total_accum  = (float4)(0.0f, 0.0f, 0.0f, 0.0f);
  float  total_weight = 0.0f;

  for (int ix = 0; ix < width; ++ix)
    {
      float xx = (ix - x);
      float yy = (gidy - y);
      float pixel_weight = color_query_weight (xx, yy, radius);
      float4 pixel = buf[ix + gidy * stride + offset];

      total_accum  += pixel * (float4)(pixel.s333, 1.0f) * pixel_weight;
      total_weight += pixel_weight;
    }

  accum[(gidy + accum_offset) * 2] = total_accum;
  accum[(gidy + accum_offset) * 2 + 1] = (float4)(total_weight);
}

__kernel void mypaint_color_query_part2(__global float4 *accum,
                                                 int     count)
{
  float4 total_accum  = (float4)(0.0f, 0.0f, 0.0f, 0.0f);
  float  total_weight = 0.0f;
  size_t end_i = count * 2;
  size_t i = 0;

  while (i < end_i)
    {
      total_accum  += accum[i++];
      total_weight += accum[i++].s0;
    }

  accum[0] = total_accum;
  accum[1] = (float4)(total_weight);
}

/* FIXME: OpenCL may not support more than 8 args, need to combine some things */
__kernel void mypaint_dab(__global float4 *buf,
                                   int     offset,
                                   int     stride,
                                   float   x,
                                   float   y,
                                   float   radius,
                                   float   hardness,
                                   float   aspect_ratio,
                                   float   sn,
                                   float   cs,
                                   float   slope1,
                                   float   slope2,
                                   float   color_alpha, /* Max alpha value */
                                   float4  color)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  float yy = (gidy - y);
  float xx = (gidx - x);

  float segment1_offset = 1.0f;
  float segment1_slope  = slope1;
  float segment2_offset = -slope2;
  float segment2_slope  = slope2;

  float rr = calculate_rr (xx, yy, radius, aspect_ratio, sn, cs);
  float alpha = calculate_alpha(rr, hardness, segment1_offset, segment1_slope, segment2_offset, segment2_slope);

  if (alpha > 0.0f)
    {
      const int idx = gidx + gidy * stride + offset;
      float4 pixel = buf[idx];
      
      alpha = alpha * color.s3;
      float dst_alpha = pixel.s3;

      float a = alpha * color_alpha + dst_alpha * (1.0f - alpha);
      float a_term = dst_alpha * (1.0f - alpha);

      if (a > 0.0f) /* Needed because color_alpha can make a = zero */
        pixel.s012 = (color.s012 * alpha * color_alpha + pixel.s012 * a_term) / a;
      pixel.s3   = a;

      buf[idx] = pixel;
    }
}
