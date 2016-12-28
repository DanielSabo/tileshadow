#pragma template DAB_ARGS dab
              float   hardness,
              float4  matrix,
              float   slope1,
              float   slope2,
#pragma template DAB_ARGS micro
              float   hardness,
              float4  matrix,
              float   slope1,
              float   slope2,
#pragma template DAB_ARGS mask
    read_only image2d_t stamp,
              float4    matrix,
#pragma template

#pragma template DAB dab
float alpha = alpha_from_dab(gidx - x, gidy - y, matrix, hardness, slope1, slope2);
#pragma template DAB micro
float alpha = alpha_from_subpixel_dab(gidx - x, gidy - y, matrix, hardness, slope1, slope2);
#pragma template DAB mask
float alpha = alpha_from_mask(gidx - x, gidy - y, matrix, stamp);
#pragma template

#pragma template APPLY_ALPHA
buf[idx] = apply_normal_mode(buf[idx], color, alpha, color_alpha);
#pragma template APPLY_ALPHA locked
buf[idx] = apply_locked_normal_mode(buf[idx], color, alpha);
#pragma template

#pragma template TEXTURE_ARGS
#pragma template TEXTURE_ARGS textured
              float   tex_offset_x,
              float   tex_offset_y,
    read_only image2d_t texture,
#pragma template

#pragma template TEXTURE
#pragma template TEXTURE textured
    alpha = apply_texture(alpha, gidx + tex_offset_x - x, gidy + tex_offset_y - y, texture);
#pragma template

#pragma template_body
__kernel void mypaintFUNCTION_SUFFIX(
    __global  float4 *buf,
              int     offset,
              float   x,
              float   y,
#pragma template DAB_ARGS
#pragma template TEXTURE_ARGS
              float   color_alpha,
              float4  color)
{
  int gidx = get_global_id(0);
  int gidy = get_global_id(1);

  #pragma template DAB

  if (alpha > 0.0f)
    {
      const int idx = gidx + gidy * TILE_PIXEL_WIDTH + offset;
      #pragma template TEXTURE
      #pragma template APPLY_ALPHA
    }
}
