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

/* FIXME: OpenCL may not support more than 8 args, need to combine some things */
__kernel void mypaint_dab(__global float4 *buf,
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
                                   float4  color)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  float yy = (gidy + y);
  float xx = (gidx + x);

  float segment1_offset = 1.0f;
  float segment1_slope  = slope1;
  float segment2_offset = -slope2;
  float segment2_slope  = slope2;

  float rr = calculate_rr (xx, yy, radius, aspect_ratio, sn, cs);
  float alpha = calculate_alpha(rr, hardness, segment1_offset, segment1_slope, segment2_offset, segment2_slope);

  if (alpha > 0.0f)
    {
      float4 pixel = buf[gidx + gidy * stride];
      
      alpha = alpha * color.s3;
      float dst_alpha = pixel.s3;

      float a = alpha + dst_alpha * (1.0f - alpha);
      float a_term = dst_alpha * (1.0f - alpha);

      pixel.s012 = (color.s012 * alpha + pixel.s012 * a_term) / a;
      pixel.s3   = a;

      buf[gidx + gidy * stride] = pixel;
    }
}