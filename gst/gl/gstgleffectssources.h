/* 
 * GStreamer
 * Copyright (C) 2008 Filippo Argiolas <filippo.argiolas@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_GL_EFFECTS_SOURCES__
#define __GST_GL_EFFECTS_SOURCES__

/* Mirror effect */
static const gchar *mirror_fragment_source =
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect tex;"
  "uniform float width, height;"
  "void main () {"
  "vec2 tex_size = vec2 (width, height);"
  "vec2 texturecoord = gl_TexCoord[0].xy;"
  "vec2 normcoord;"
  "normcoord = texturecoord / tex_size - 1.0;"
  "normcoord.x *= sign (normcoord.x);"
  "texturecoord = (normcoord + 1.0) * tex_size;"
  "vec4 color = texture2DRect (tex, texturecoord); "
  "gl_FragColor = color * gl_Color;"
  "}";

/* Light Tunnel effect */
static const gchar *tunnel_fragment_source =
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect tex;"
  "uniform float width, height;"
  "void main () {"
  "vec2 tex_size = vec2 (width, height);"
  "vec2 texturecoord = gl_TexCoord[0].xy;"
  "vec2 normcoord;"
  /* little trick with normalized coords to obtain a circle */
  "normcoord = texturecoord / tex_size.x - tex_size / tex_size.x;"
  "float r =  length(normcoord);"
  "float phi = atan(normcoord.y, normcoord.x);"
  "r = clamp (r, 0.0, 0.5);" /* is there a way to do this without polars? */
  "normcoord.x = r * cos(phi);"
  "normcoord.y = r * sin(phi);" 
  "texturecoord = (normcoord + tex_size/tex_size.x) * tex_size.x;"
  "vec4 color = texture2DRect (tex, texturecoord); "
  "gl_FragColor = color;"
  "}";

/* FishEye effect */
static const gchar *fisheye_fragment_source =
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect tex;"
  "uniform float width, height;"
  "void main () {"
  "vec2 tex_size = vec2 (width, height);"
  "vec2 texturecoord = gl_TexCoord[0].xy;"
  "vec2 normcoord;"
  "normcoord = texturecoord / tex_size - 1.0;"
  "float r =  length (normcoord);"
  "normcoord *= r/sqrt(2.0);"
  "texturecoord = (normcoord + 1.0) * tex_size;"
  "vec4 color = texture2DRect (tex, texturecoord); "
  "gl_FragColor = color;"
  "}";

/* Bulge effect */
static const gchar *bulge_fragment_source =
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect tex;"
  "uniform float width, height;"
  "void main () {"
  "vec2 tex_size = vec2 (width, height);"
  "vec2 texturecoord = gl_TexCoord[0].xy;"
  "vec2 normcoord;"
  "normcoord = texturecoord / tex_size - 1.0;"
  "float r =  length (normcoord);"
  "normcoord *= smoothstep (-0.1, 0.5, r);"
  "texturecoord = (normcoord + 1.0) * tex_size;"
  "vec4 color = texture2DRect (tex, texturecoord); "
  "gl_FragColor = color;"
  "}";

/* Twirl effect */
static const gchar *twirl_fragment_source =
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect tex;"
  "uniform float width, height;"
  "void main () {"
  "vec2 tex_size = vec2 (width, height);"
  "vec2 texturecoord = gl_TexCoord[0].xy;"
  "vec2 normcoord;"
  "normcoord = texturecoord / tex_size - 1.0;"
  "float r =  length (normcoord);"
  "float phi = atan (normcoord.y, normcoord.x);"
  "phi += (1.0 - smoothstep (-0.6, 0.6, r)) * 4.8;"
  "normcoord.x = r * cos(phi);"
  "normcoord.y = r * sin(phi);"
  "texturecoord = (normcoord + 1.0) * tex_size;"
  "vec4 color = texture2DRect (tex, texturecoord); "
  "gl_FragColor = color;"
  "}";  

/* Squeeze effect */
static const gchar *squeeze_fragment_source =
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect tex;"
  "uniform float width, height;"
  "void main () {"
  "vec2 tex_size = vec2 (width, height);"
  "vec2 texturecoord = gl_TexCoord[0].xy;"
  "vec2 normcoord;"
  "normcoord = texturecoord / tex_size - 1.0; "
  "float r = length (normcoord); "
  "r = pow(r, 0.40)*1.3;"
  "normcoord = normcoord / r;"
  "texturecoord = (normcoord + 1.0) * tex_size;"
  "vec4 color = texture2DRect (tex, texturecoord); "
  "gl_FragColor = color * gl_Color;"
  "}";

static const gchar *convolution_fragment_source = 
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect tex;"
  "uniform float norm_const;"
  "uniform float norm_offset;"
  "uniform float kernel[9];"
  "void main () {"
  "  vec2 offset[9];"
  "  offset = vec2[] ( vec2(-1.0,-1.0), vec2( 0.0,-1.0), vec2( 1.0,-1.0),"
  "                    vec2(-1.0, 0.0), vec2( 0.0, 0.0), vec2( 1.0, 0.0),"
  "                    vec2(-1.0, 1.0), vec2( 0.0, 1.0), vec2( 1.0, 1.0) );"
  "  vec2 texturecoord = gl_TexCoord[0].st;"
  "  int i;"
  "  vec4 sum = vec4 (0.0);"
  "  for (i = 0; i < 9; i++) { "
  "    if (kernel[i] != 0.0) {"
  "      vec4 neighbor = texture2DRect(tex, texturecoord + vec2(offset[i])); "
  "      sum += neighbor * kernel[i]/norm_const; "
  "    }"
  "  }"
  "  gl_FragColor = sum + norm_offset;"
  "}";

#if 0
/* FIXME: too much texture accesses */
static const gchar *large_convolution_fragment_source = 
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect tex;"
"uniform float norm_const;"
"uniform float norm_offset;"
"uniform float kernel[25];"
"void main () {"
"  vec2 offset[25];"
"  offset = vec2[] ( vec2(-2.0,-2.0), vec2(-1.0,-2.0), vec2( 0.0,-2.0), vec2( 1.0,-2.0), vec2( 2.0,-2.0),"
"                    vec2(-2.0,-1.0), vec2(-1.0,-1.0), vec2( 0.0,-1.0), vec2( 1.0,-1.0), vec2( 2.0,-1.0),"
"                    vec2(-2.0, 0.0), vec2(-1.0, 0.0), vec2( 0.0, 0.0), vec2( 1.0, 0.0), vec2( 2.0, 0.0),"
"                    vec2(-2.0, 1.0), vec2(-1.0, 1.0), vec2( 0.0, 1.0), vec2( 1.0, 1.0), vec2( 2.0, 1.0),"
"                    vec2(-2.0, 2.0), vec2(-1.0, 2.0), vec2( 0.0, 2.0), vec2( 1.0, 2.0), vec2( 2.0, 2.0) );"
"  vec2 texturecoord = gl_TexCoord[0].st;"
"  int i;"
"  vec4 sum = vec4 (0.0);"
"  for (i = 0; i < 25; i++) { "
"    if (kernel[i] != 0) {"
"      vec4 neighbor = texture2DRect(tex, texturecoord + vec2(offset[i])); "
"        sum += neighbor * kernel[i]/norm_const; "
"      }"
"  }"
"  gl_FragColor = sum + norm_offset;"
"}";
#endif

/* Junk/Test Effect */
static const gchar *test_fragment_source =
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect tex;"
  "uniform sampler2DRect base;"
  "void main () {"
#if 0
  "float kern_gauss[9], kern_laplace[9], kern_emboss[9], kern_mean[9];"
  "kern_gauss = float[9](1.0, 2.0, 1.0, 2.0, 4.0, 2.0, 1.0, 2.0, 1.0);"
  "kern_laplace = float[9](0.0, -1.0, 0.0, -1.0, 4.0, -1.0, 0.0, -1.0, 0.0);"
  "kern_emboss = float[9](2.0, 0.0, 0.0, 0.0, -1.0, 0.0, 0.0, .0, -1.0);"
  "kern_mean = float[9](1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0);"
  "vec2 texturecoord = gl_TexCoord[0].st;"
  "int i;"
  "vec4 color = texture2DRect (tex, texturecoord);"
/* //	  "vec4 color = vec4 (gray);" */
/* //	  "vec4 basecolor = texture2DRect(tex, texturecoord);" */
/* 	  "gl_FragColor =  sum;" */
/* //	  "gl_FragColor = 0.5 * sum + basecolor;" */
    //from gpu gems 
  "float dx = 2.0;"
  "float dy = 2.0;"
  "float trs = 0.4;"
  "vec2 ox = vec2 (dx, 0.0);"
  "vec2 oy = vec2 (0.0, dy);"
  "float g00 = texture2DRect(tex, texturecoord - oy - ox).x;"
  "float g01 = texture2DRect(tex, texturecoord - oy).x;"
  "float g02 = texture2DRect(tex, texturecoord - oy + ox).x;"
  "float g10 = texture2DRect(tex, texturecoord - ox).x;"
  "float g11 = texture2DRect(tex, texturecoord).x;"
  "float g12 = texture2DRect(tex, texturecoord + ox).x;"
  "float g20 = texture2DRect(tex, texturecoord + oy - ox).x;"
  "float g21 = texture2DRect(tex, texturecoord + oy).x;"
  "float g22 = texture2DRect(tex, texturecoord + oy + ox).x;"
  "float sx = g20 + g22 - g00 - g02 + 2 * (g21 - g01);"
  "float sy = g22 + g02 - g00 - g20 + 2 * (g12 - g10);"
  "float dist = (sx * sx + sy * sy);"
  "float tsq = trs*trs;"
  "vec4 basecolor = texture2DRect(base, texturecoord);"
  "if (dist > tsq) { gl_FragColor = vec4(0.0); }"
  "else gl_FragColor = vec4(1.0);"
//  "else gl_FragColor = basecolor;"
  "gl_FragColor.a = 1.0;"
//  "gl_FragColor = basecolor;"
//  "gl_FragColor = basecolor;"
  //"vec4 blend = floor (color * 5.0)/5.0;" //posterize
  //"vec4 white = vec4 (1.0);"
  //"gl_FragColor = white - ((white - blend) * (white - color));"
  //"gl_FragColor = blend;"
  //"gl_FragColor = blend + color;"
  //"gl_FragColor = sum * vec4(smoothstep(1.0, 0.0, dist));"
  //       "gl_FragColor = sum;"
#endif
  "  vec2 offset[9];"
  "  offset = vec2[] ( vec2(-1.0,-1.0), vec2( 0.0,-1.0), vec2( 1.0,-1.0),"
  "                    vec2(-1.0, 0.0), vec2( 0.0, 0.0), vec2( 1.0, 0.0),"
  "                    vec2(-1.0, 1.0), vec2( 0.0, 1.0), vec2( 1.0, 1.0) );"
  "vec2 texturecoord = gl_TexCoord[0].st;"
  "int i;"
  "vec4 color = texture2DRect(tex, texturecoord); "
  "float luma = dot(color, vec3(0.2125, 0.7154, 0.0721));"
  "float avg = 0.0;"
  "  for (i = 0; i < 9; i++) { "
  "      vec4 neighbor = texture2DRect(tex, texturecoord + vec2(offset[i])); "
  "      avg +=  (neighbor.r + neighbor.g + neighbor.b) / (3.0*9.0);"
  "  }"
//  "gl_FragColor = smoothstep (0.23, 0.25, avg) *  floor(color * 5.0)/5.0;"
  "gl_FragColor = smoothstep (0.39, 0.40, avg);"
  "}";

static const gchar *blend_fragment_source = 
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect base;"
  "uniform sampler2DRect blend;"
  "void main () {"
  "vec4 basecolor = texture2DRect (base, gl_TexCoord[0].st);"
  "vec4 blendcolor = texture2DRect (blend, gl_TexCoord[0].st);"
  "vec4 white = vec4(1.0);"
  "gl_FragColor = basecolor + blendcolor / 3.5;"
  "}";


/* Stretch Effect */
static const gchar *stretch_fragment_source =
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect tex;"
  "uniform float width, height;"
  "void main () {"
  "vec2 tex_size = vec2 (width, height);"
  "vec2 texturecoord = gl_TexCoord[0].xy;"
  "vec2 normcoord;"
  "normcoord = texturecoord / tex_size - 1.0;"
  "float r = length (normcoord); "
  "normcoord *= 2.0 - smoothstep(0.0, 0.7, r);"
  "texturecoord = (normcoord + 1.0) * tex_size;"
  "vec4 color = texture2DRect (tex, texturecoord); "
  "gl_FragColor = color * gl_Color;" 
  "}";

/* Square Effect */
static const gchar *square_fragment_source =
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect tex;"
  "uniform float width;"
  "uniform float height;"
  "void main () {"
  "vec2 tex_size = vec2 (width, height);"
  "vec2 texturecoord = gl_TexCoord[0].xy;"
  "vec2 normcoord;"
  "normcoord = texturecoord / tex_size - 1.0;"
  "float r = length (normcoord); "
  "normcoord *= 1.0 + smoothstep(0.25, 0.5, abs(normcoord));"
  "normcoord /= 2.0;" /* zoom amount */
  "texturecoord = (normcoord + 1.0) * tex_size;"
  " vec4 color = texture2DRect (tex, texturecoord); "
  " gl_FragColor = color * gl_Color;" 
  "}";

static const gchar *luma_to_curve_fragment_source =
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect tex;"
  "uniform sampler1D curve;"
  "void main () {"
  "vec2 texturecoord = gl_TexCoord[0].st;"
  "vec4 color = texture2DRect (tex, texturecoord); "
  "float luma = dot(color.rgb, vec3(0.2125, 0.7154, 0.0721));"
  "vec4 outcolor;"
  "color = texture1D(curve, luma);"
  "gl_FragColor = color;" 
  "}";


static const gchar *luma_threshold_fragment_source =
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect tex;"
  "void main () {"
  "vec2 texturecoord = gl_TexCoord[0].st;"
  "int i;"
  "vec4 color = texture2DRect(tex, texturecoord); "
  "float luma = dot(color, vec3(0.2125, 0.7154, 0.0721));"
// "gl_FragColor = step (0.4, luma);"
  "gl_FragColor = smoothstep (0.30, 0.50, luma);"
  "}";

static const gchar *hconv9_fragment_source = 
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect tex;"
"uniform float norm_const;"
"uniform float norm_offset;"
"uniform float kernel[9];"
"void main () {"
"  float offset[9];"
"  offset = float[] (-4.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0);"
"  vec2 texturecoord = gl_TexCoord[0].st;"
"  int i;"
"  vec4 sum = vec4 (0.0);"
"  for (i = 0; i < 9; i++) { "
"    if (kernel[i] != 0) {"
"        vec4 neighbor = texture2DRect(tex, vec2(texturecoord.s+offset[i], texturecoord.t)); "
"        sum += neighbor * kernel[i]/norm_const; "
"      }"
"  }"
"  gl_FragColor = sum + norm_offset;"
"}";

static const gchar *vconv9_fragment_source = 
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect tex;"
"uniform float norm_const;"
"uniform float norm_offset;"
"uniform float kernel[9];"
"void main () {"
"  float offset[9];"
"  offset = float[] (-4.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0);"
"  vec2 texturecoord = gl_TexCoord[0].st;"
"  int i;"
"  vec4 sum = vec4 (0.0);"
"  for (i = 0; i < 9; i++) { "
"    if (kernel[i] != 0) {"
"        vec4 neighbor = texture2DRect(tex, vec2(texturecoord.s, texturecoord.t+offset[i])); "
"        sum += neighbor * kernel[i]/norm_const; "
"      }"
"  }"
"  gl_FragColor = sum + norm_offset;"
"}";

#if 0
static const gchar *hconv5_fragment_source = 
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect tex;"
"uniform float norm_const;"
"uniform float norm_offset;"
"uniform float kernel[5];"
"void main () {"
"  float offset[5];"
"  offset = float[] (-2.0, -1.0, 0.0, 1.0, 2.0);"
"  vec2 texturecoord = gl_TexCoord[0].st;"
"  int i;"
"  vec4 sum = vec4 (0.0);"
"  for (i = 0; i < 5; i++) { "
"    if (kernel[i] != 0) {"
"        vec4 neighbor = texture2DRect(tex, vec2(texturecoord.s+offset[i], texturecoord.t)); "
"        sum += neighbor * kernel[i]/norm_const; "
"      }"
"  }"
"  gl_FragColor = sum + norm_offset;"
"}";

static const gchar *vconv5_fragment_source = 
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect tex;"
"uniform float norm_const;"
"uniform float norm_offset;"
"uniform float kernel[5];"
"void main () {"
"  float offset[5];"
"  offset = float[] (-2.0, -1.0, 0.0, 1.0, 2.0);"
"  vec2 texturecoord = gl_TexCoord[0].st;"
"  int i;"
"  vec4 sum = vec4 (0.0);"
"  for (i = 0; i < 5; i++) { "
"    if (kernel[i] != 0) {"
"        vec4 neighbor = texture2DRect(tex, vec2(texturecoord.s, texturecoord.t+offset[i])); "
"        sum += neighbor * kernel[i]/norm_const; "
"      }"
"  }"
"  gl_FragColor = sum + norm_offset;"
"}";

static const gchar *vmedian3_fragment_source = 
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect tex;"
"float find_median (float a, float b, float c)"
"{"
" float median;"
"  if( a < b ) {"
"    if( b < c) median = b;"
"    else median = max(a,c);"
"  }"
"  else {"
"    if( a < c) median = a;"
"    else median = max(b,c);"
"  }"
"  return median;"
"}"
"void main () {"
"  float offset[3];"
"  offset = float[] (-1.0, 0.0, 1.0);"
"  vec2 texturecoord = gl_TexCoord[0].st;"
"  int i;"
"  vec4 neighbor[3];"
"  for (i = 0; i < 3; i++) { "
"    neighbor[i] = texture2DRect(tex, vec2(texturecoord.s, texturecoord.t+offset[i])); "
"  }"
"  gl_FragColor.r = find_median (neighbor[0].r, neighbor[1].r, neighbor[2].r);" 
"  gl_FragColor.g = find_median (neighbor[0].g, neighbor[1].g, neighbor[2].g);" 
"  gl_FragColor.b = find_median (neighbor[0].b, neighbor[1].b, neighbor[2].b);" 
"  gl_FragColor.a = neighbor[1].a;"
"}";

static const gchar *hmedian3_fragment_source = 
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect tex;"
"float find_median (float a, float b, float c)"
"{"
"float median;"
"  if( a < b ) {"
"    if( b < c) median = b;"
"    else median = max(a,c);"
"  }"
"  else {"
"    if( a < c) median = a;"
"    else median = max(b,c);"
"  }"
"  return median;"
"}"
"void main () {"
"  float offset[3];"
"  offset = float[] (-1.0, 0.0, 1.0);"
"  vec2 texturecoord = gl_TexCoord[0].st;"
"  int i;"
"  vec4 neighbor[3];"
"  for (i = 0; i < 3; i++) { "
"    neighbor[i] = texture2DRect(tex, vec2(texturecoord.s+offset[i], texturecoord.t)); "
"  }"
"  gl_FragColor.r = find_median (neighbor[0].r, neighbor[1].r, neighbor[2].r);" 
"  gl_FragColor.g = find_median (neighbor[0].g, neighbor[1].g, neighbor[2].g);" 
"  gl_FragColor.b = find_median (neighbor[0].b, neighbor[1].b, neighbor[2].b);" 
"  gl_FragColor.a = neighbor[1].a;"
"}";
#endif


static const gchar *difference_matte_fragment_source =
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect front;"
  "uniform sampler2DRect back;"
  "void main () {"
  "vec4 frontcolor = texture2DRect (front, gl_TexCoord[0].st);"
  "vec4 backcolor = texture2DRect (back, gl_TexCoord[0].st);"
//  "vec4 newcolor = texture2DRect (new, gl_TexCoord[0].st);"
  "vec4 white = vec4(1.0);"
//  "gl_FragColor = (length(frontcolor - backcolor) > 0.2) ? frontcolor : white;"
//  "float alpha = smoothstep (0.09, 0.12, length (frontcolor - backcolor));"
//  "gl_FragColor = (1 - alpha) * newcolor + alpha * frontcolor;"
  "gl_FragColor = step (0.12, length (frontcolor - backcolor));"
//  "gl_FragColor = smoothstep (0.09, 0.12, length (frontcolor - backcolor));"
  "}";

static const gchar *interpolate_fragment_source = 
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect front;"
  "uniform sampler2DRect back;"
  "uniform sampler2DRect alpha;"
  "void main () {"
  "vec4 frontcolor = texture2DRect (front, gl_TexCoord[0].st);"
  "vec4 backcolor = texture2DRect (back, gl_TexCoord[0].st);"
  "vec4 alphacolor = texture2DRect (alpha, gl_TexCoord[0].st);"
  "vec4 white = vec4(1.0);"
//  "gl_FragColor = alphacolor;" //debug
//  "gl_FragColor = (1 - alphacolor) * backcolor + alphacolor * frontcolor;" 
  "gl_FragColor = backcolor + (1 - backcolor.a) * frontcolor;"
  "}";

#endif
