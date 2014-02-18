/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Copyright 2008-2011 Martin Renold <martinxyz@gmx.ch>
 *                2014 Daniel Sabo
 */

/* FIXME: OpenCL may not support more than 8 args, need to combine some things */
__kernel void mypaint_dab(__global float4 *buf,
                                   int     stride,
                                   float   x,
                                   float   y,
                                   float   radius,
                                   float   hardness,
                                   float   aspect_ratio,
                                   float   angle,
                                   float   sn,
                                   float   cs,
                                   float4  segments,
                                   float4  color)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  float yy = (gidy + 0.5f - y);
  float xx = (gidx + 0.5f - x);

  float yyr=(yy*cs-xx*sn)*aspect_ratio;
  float xxr=yy*sn+xx*cs;
  float rr = (yyr*yyr + xxr*xxr) / (radius * radius);
  // rr is in range 0.0..1.0*sqrt(2)

  float   segment1_offset = segments.s0;
  float   segment1_slope  = segments.s1;
  float   segment2_offset = segments.s2;
  float   segment2_slope  = segments.s3;

  if (rr <= 1.0f)
    {
      float4 pixel = buf[gidx + gidy * stride];

      float alpha;

      if (rr <= hardness)
        {
          alpha = segment1_offset + rr * segment1_slope;
        }
      else
        {
          alpha = segment2_offset + rr * segment2_slope;
        }
      
      alpha = alpha * color.s3;
      float dst_alpha = pixel.s3;

      float a = alpha + dst_alpha * (1.0 - alpha);
      float a_term = dst_alpha * (1.0 - alpha);

      pixel.s012 = (color.s012 * alpha + pixel.s012 * a_term) / a;
      pixel.s3   = a;

      buf[gidx + gidy * stride] = pixel;
    }
}