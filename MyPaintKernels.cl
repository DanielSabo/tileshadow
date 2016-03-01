/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * Copyright 2008-2011 Martin Renold <martinxyz@gmx.ch>
 *                2014 Daniel Sabo
 */

float calculate_alpha (float rr, float hardness, float slope1, float slope2);
float color_query_weight (float xx, float yy, float radius);

float alpha_from_dab(float x, float y, float4 matrix, float hardness, float slope1, float slope2);
float alpha_from_subpixel_dab(float x, float y, float4 matrix, float hardness, float slope1, float slope2);
float alpha_from_mask(float x, float y, float4 matrix, image2d_t stamp);

inline float4 apply_normal_mode (float4 pixel, float4 color, float alpha, float color_alpha);
inline float4 apply_locked_normal_mode (float4 pixel, float4 color, float alpha);

float calculate_alpha (float rr, float hardness, float slope1, float slope2)
{
  float segment1_offset = 1.0f;
  float segment1_slope  = slope1;
  float segment2_offset = -slope2;
  float segment2_slope  = slope2;

  if (rr <= 1.0f)
    {
      if (rr <= hardness)
        return segment1_offset + rr * segment1_slope;
      else
        return segment2_offset + rr * segment2_slope;
    }

  return 0.0f;
}

float alpha_from_dab(float x, float y, float4 matrix, float hardness, float slope1, float slope2)
{
  float xr = x * matrix.s0 + y * matrix.s1;
  float yr = x * matrix.s2 + y * matrix.s3;
  float rr  = (yr * yr + xr * xr);
  return calculate_alpha(rr, hardness, slope1, slope2);
}

float alpha_from_subpixel_dab(float x, float y, float4 matrix, float hardness, float slope1, float slope2)
{
  // The corners of the pixel, in the dab's coordiate space
  float xr0 = (x - 0.5f) * matrix.s0 + (y - 0.5f) * matrix.s1;
  float yr0 = (x - 0.5f) * matrix.s2 + (y - 0.5f) * matrix.s3;
  float xr1 = (x + 0.5f) * matrix.s0 + (y + 0.5f) * matrix.s1;
  float yr1 = (x + 0.5f) * matrix.s2 + (y + 0.5f) * matrix.s3;

  float x_near, y_near;

  // If the signs differ this pixel contains the center of the dab
  if (signbit(xr0) != signbit(xr1))
    x_near = 0;
  else
    x_near = min(fabs(xr0), fabs(xr1));

  if (signbit(yr0) != signbit(yr1))
    y_near = 0;
  else
    y_near = min(fabs(yr0), fabs(yr1));

  // distance^2 for the point closes to the dab's center
  float rr_near = x_near * x_near + y_near * y_near;

  if (rr_near > 1.0f)
  {
    // This pixel is entirely outside the dab
    return 0.0f;
  }
  else
  {
    // distance^2 for a distant point
    // This point is somewhat arbitrary, it just needs to be reasonably far away from the near point.
    float x_far = max(fabs(xr0), fabs(xr1));
    float y_far = max(fabs(yr0), fabs(yr1));
    float rr_far = x_far * x_far + y_far * y_far;

    // MyPaint's AA blend
    float visibilityNear = 1.0f - rr_near;
    float delta = rr_far - rr_near;
    float delta2 = 1.0f + delta;
    visibilityNear /= delta2;
    float rr = 1.0f - visibilityNear;

    return calculate_alpha(rr, hardness, slope1, slope2);
  }
}

float alpha_from_mask(float x, float y, float4 matrix, image2d_t stamp)
{
  const sampler_t sampler = CLK_NORMALIZED_COORDS_TRUE | CLK_ADDRESS_CLAMP | CLK_FILTER_LINEAR;

  // (x, y) is centered on the range (-radius, radius)
  float2 coord = (float2)(x, y);

  // Apply rotation and shift to (0.0, 1.0)
  coord = (float2)(dot(coord, matrix.s01) + 0.5f,
                   dot(coord, matrix.s23) + 0.5f);

  return read_imagef(stamp, sampler, coord).s0;
}

float color_query_weight (float xx, float yy, float radius)
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
                                                 float   radius)
{
  int gidy = get_global_id(0);

  float4 total_accum  = (float4)(0.0f, 0.0f, 0.0f, 0.0f);
  float  total_weight = 0.0f;

  for (int ix = 0; ix < width; ++ix)
    {
      float xx = (ix - x);
      float yy = (gidy - y);
      float pixel_weight = color_query_weight (xx, yy, radius);
      float4 pixel = buf[ix + gidy * TILE_PIXEL_WIDTH + offset];

      total_accum  += pixel * (float4)(pixel.s333, 1.0f) * pixel_weight;
      total_weight += pixel_weight;
    }

  accum[(gidy + accum_offset) * 2] = total_accum;
  accum[(gidy + accum_offset) * 2 + 1] = (float4)(total_weight);
}

__kernel void mypaint_color_query_empty_part1(         float   x,
                                                       float   y,
                                                       int     width,
                                                       int     accum_offset,
                                              __global float4 *accum,
                                                       float   radius)
{
  int gidy = get_global_id(0);

  float4 total_accum  = (float4)(0.0f, 0.0f, 0.0f, 0.0f);
  float  total_weight = 0.0f;

  for (int ix = 0; ix < width; ++ix)
    {
      float xx = (ix - x);
      float yy = (gidy - y);

      total_weight += color_query_weight(xx, yy, radius);
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

inline float4 apply_normal_mode(float4 pixel, float4 color, float alpha, float color_alpha)
{
  alpha = alpha * color.s3;
  float dst_alpha = pixel.s3;

  float a = alpha * color_alpha + dst_alpha * (1.0f - alpha);
  float a_term = dst_alpha * (1.0f - alpha);

  if (a > 0.0f) /* Needed because color_alpha can make a = zero */
    pixel.s012 = (color.s012 * alpha * color_alpha + pixel.s012 * a_term) / a;
  pixel.s3 = a;

  return pixel;
}

inline float4 apply_locked_normal_mode(float4 pixel, float4 color, float alpha)
{
  if (pixel.s3)
    {
      alpha = alpha * color.s3;
      float a_term = 1.0f - alpha;

      pixel.s012 = color.s012 * alpha + pixel.s012 * a_term;
    }

  return pixel;
}

__kernel void mypaint_dab(__global float4 *buf,
                                   int     offset,
                                   float   x,
                                   float   y,
                                   float   hardness,
                                   float4  matrix,
                                   float   slope1,
                                   float   slope2,
                                   float   color_alpha, /* Max alpha value */
                                   float4  color)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  float alpha = alpha_from_dab(gidx - x, gidy - y, matrix, hardness, slope1, slope2);

  if (alpha > 0.0f)
    {
      const int idx = gidx + gidy * TILE_PIXEL_WIDTH + offset;
      buf[idx] = apply_normal_mode(buf[idx], color, alpha, color_alpha);
    }
}

__kernel void mypaint_dab_locked(__global float4 *buf,
                                          int     offset,
                                          float   x,
                                          float   y,
                                          float   hardness,
                                          float4  matrix,
                                          float   slope1,
                                          float   slope2,
                                          float   color_alpha, /* Max alpha value (ignored) */
                                          float4  color)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  float alpha = alpha_from_dab(gidx - x, gidy - y, matrix, hardness, slope1, slope2);

  if (alpha > 0.0f)
    {
      const int idx = gidx + gidy * TILE_PIXEL_WIDTH + offset;
      buf[idx] = apply_locked_normal_mode(buf[idx], color, alpha);
    }
}

__kernel void mypaint_micro_dab(__global float4 *buf,
                                         int     offset,
                                         float   x,
                                         float   y,
                                         float   hardness,
                                         float4  matrix,
                                         float   slope1,
                                         float   slope2,
                                         float   color_alpha, /* Max alpha value */
                                         float4  color)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  float alpha = alpha_from_subpixel_dab(gidx - x, gidy - y, matrix, hardness, slope1, slope2);

  if (alpha > 0.0f)
    {
      const int idx = gidx + gidy * TILE_PIXEL_WIDTH + offset;
      buf[idx] = apply_normal_mode(buf[idx], color, alpha, color_alpha);
    }
}

__kernel void mypaint_micro_dab_locked(__global float4 *buf,
                                                int     offset,
                                                float   x,
                                                float   y,
                                                float   hardness,
                                                float4  matrix,
                                                float   slope1,
                                                float   slope2,
                                                float   color_alpha, /* Max alpha value (ignored) */
                                                float4  color)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  float alpha = alpha_from_subpixel_dab(gidx - x, gidy - y, matrix, hardness, slope1, slope2);

  if (alpha > 0.0f)
    {
      const int idx = gidx + gidy * TILE_PIXEL_WIDTH + offset;
      buf[idx] = apply_locked_normal_mode(buf[idx], color, alpha);
    }
}

__kernel void mypaint_mask_dab(__global  float4   *buf,
                                         int       offset,
                                         float     x,
                                         float     y,
                               read_only image2d_t stamp,
                                         float4    matrix,
                                         float     color_alpha, /* Max alpha value */
                                         float4    color)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  float alpha = alpha_from_mask(gidx - x, gidy - y, matrix, stamp);

  if (alpha > 0.0f)
    {
      const int idx = gidx + gidy * TILE_PIXEL_WIDTH + offset;
      buf[idx] = apply_normal_mode(buf[idx], color, alpha, color_alpha);
    }
}

__kernel void mypaint_mask_dab_locked(__global  float4   *buf,
                                                int       offset,
                                                float     x,
                                                float     y,
                                      read_only image2d_t stamp,
                                                float4    matrix,
                                                float     color_alpha, /* Max alpha value (ignored) */
                                                float4    color)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  float alpha = alpha_from_mask(gidx - x, gidy - y, matrix, stamp);

  if (alpha > 0.0f)
    {
      const int idx = gidx + gidy * TILE_PIXEL_WIDTH + offset;
      buf[idx] = apply_locked_normal_mode(buf[idx], color, alpha);
    }
}
