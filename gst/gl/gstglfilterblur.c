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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gstglbuffer.h>
#include <gstglfilter.h>
#include "glextensions.h"
#include <gstglshader.h>
#include <string.h>
#include <math.h>

#define GST_CAT_DEFAULT gst_gl_filterblur_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define GST_TYPE_GL_FILTERBLUR            (gst_gl_filterblur_get_type())
#define GST_GL_FILTERBLUR(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_FILTERBLUR,GstGLFilterBlur))
#define GST_IS_GL_FILTERBLUR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_FILTERBLUR))
#define GST_GL_FILTERBLUR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_FILTERBLUR,GstGLFilterBlurClass))
#define GST_IS_GL_FILTERBLUR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_FILTERBLUR))
#define GST_GL_FILTERBLUR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_FILTERBLUR,GstGLFilterBlurClass))
typedef struct _GstGLFilterBlur GstGLFilterBlur;
typedef struct _GstGLFilterBlurClass GstGLFilterBlurClass;

struct _GstGLFilterBlur
{
  GstGLFilter filter;

  /* < private? > */
  GLuint fbo;

  GLuint intexture;
  GLuint midtexture;
  GLuint outtexture;
  GstGLShader *shader0;
  GstGLShader *shader1;
};

/* horizontal convolution */
static const gchar *hconv9_fragment_source = 
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect tex;"
"uniform float norm_const;"
"uniform float norm_offset;"
"uniform float kernel[9];"
"void main () {"
"  float offset[9] = float[9] (-4.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0);"
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

/* vertical convolution */
static const gchar *vconv9_fragment_source = 
"#extension GL_ARB_texture_rectangle : enable\n"
"uniform sampler2DRect tex;"
"uniform float norm_const;"
"uniform float norm_offset;"
"uniform float kernel[9];"
"void main () {"
"  float offset[9] = float[9] (-4.0, -3.0, -2.0, -1.0, 0.0, 1.0, 2.0, 3.0, 4.0);"
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

struct _GstGLFilterBlurClass
{
  GstGLFilterClass filter_class;
};

static const GstElementDetails element_details =
GST_ELEMENT_DETAILS ("Gstreamer OpenGL Blur",
    "Filter/Effect",
    "Blur with 9x9 separable convolution",
    "Filippo Argiolas <filippo.argiolas@gmail.com>");

#define DEBUG_INIT(bla)							\
  GST_DEBUG_CATEGORY_INIT (gst_gl_filterblur_debug, "glfilterblur", 0, "glfilterblur element");

GST_BOILERPLATE_FULL (GstGLFilterBlur, gst_gl_filterblur, GstGLFilter,
		      GST_TYPE_GL_FILTER, DEBUG_INIT);

static void gst_gl_filterblur_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_filterblur_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_gl_filterblur_set_target (GstGLFilterBlur * filterblur, GLuint tex);
static void gst_gl_filterblur_draw_texture (GstGLFilterBlur * filterblur, GLuint tex);

static GLuint gst_gl_filterblur_prepare_texture (GLint width, GLint height);
static GstFlowReturn gst_gl_filterblur_transform (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer * outbuf);

static void
gst_gl_filterblur_reset_resources (GstGLFilterBlur * filterblur)
{
  GLenum err;

  g_message ("deleting textures");
  glDeleteTextures (1, &filterblur->midtexture);

  g_message ("deleting fbo");
  glDeleteFramebuffersEXT (1, &filterblur->fbo);
  err = glGetError ();
  if (err)
    g_warning (G_STRLOC "error: 0x%x", err);

  filterblur->fbo = 0;
}

static gboolean
gst_gl_filterblur_stop (GstGLFilter * filter)
{
  GstGLFilterBlur *filterblur;

  filterblur = GST_GL_FILTERBLUR (filter);

  g_message ("stop!");

  if (filter->display) {
    gst_gl_display_lock (filter->display);
    gst_gl_filterblur_reset_resources (filterblur);
    gst_gl_display_unlock (filter->display);
  }

  return TRUE;
}

static void
gst_gl_filterblur_init_resources (GstGLFilterBlur * filterblur)
{
  GstGLFilter *filter = GST_GL_FILTER (filterblur);

  if (filterblur->fbo) {
    return;
  }

  g_message ("creating fbo");
  glGenFramebuffersEXT (1, &filterblur->fbo);
  g_message ("generating midtexture");
  filterblur->midtexture = gst_gl_filterblur_prepare_texture (filter->width, filter->height);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

static gboolean
gst_gl_filterblur_start (GstGLFilter * filter)
{
  g_message ("start!");
  return TRUE;
}

static void
gst_gl_filterblur_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details (element_class, &element_details);
}

static void
gst_gl_filterblur_class_init (GstGLFilterBlurClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;
  gobject_class->set_property = gst_gl_filterblur_set_property;
  gobject_class->get_property = gst_gl_filterblur_get_property;

  GST_BASE_TRANSFORM_CLASS (klass)->transform = gst_gl_filterblur_transform;
  GST_GL_FILTER_CLASS (klass)->start = gst_gl_filterblur_start;
  GST_GL_FILTER_CLASS (klass)->stop = gst_gl_filterblur_stop;
}

static void
gst_gl_filterblur_init (GstGLFilterBlur * filterblur, GstGLFilterBlurClass * klass)
{
  filterblur->shader0 = NULL;
  filterblur->shader1 = NULL;
}

static void
gst_gl_filterblur_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  /* GstGLFilterBlur *filterblur = GST_GL_FILTERBLUR (object); */

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_filterblur_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  /* GstGLFilterBlur *filterblur = GST_GL_FILTERBLUR (object); */

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_filterblur_draw_texture (GstGLFilterBlur * filterblur, GLuint tex)
{
  GstGLFilter *filter = GST_GL_FILTER (filterblur);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, tex);

  glBegin (GL_QUADS);

  glTexCoord2f (0.0, 0.0);
  glVertex2f (-1.0, -1.0);
  glTexCoord2f (filter->width, 0.0);
  glVertex2f (1.0, -1.0);
  glTexCoord2f (filter->width, filter->height);
  glVertex2f (1.0, 1.0);
  glTexCoord2f (0.0, filter->height);
  glVertex2f (-1.0, 1.0);

  glEnd ();
}

static void
gst_gl_filterblur_set_target (GstGLFilterBlur * filterblur, GLuint tex)
{
  glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, filterblur->fbo);

  glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
      GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, tex, 0);

  gst_gl_shader_use (NULL);

  g_assert (glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT) ==
      GL_FRAMEBUFFER_COMPLETE_EXT);

  glDrawBuffer (GL_COLOR_ATTACHMENT0_EXT);
  glReadBuffer (GL_COLOR_ATTACHMENT0_EXT);
}

static GLuint
gst_gl_filterblur_prepare_texture (GLint width, GLint height)
{
  GLuint tex;

  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glGenTextures (1, &tex);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, tex);
  glTexImage2D (GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA,
      width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  return tex;
}

/* override GstGLFilter Transform function */
static GstFlowReturn
gst_gl_filterblur_transform (GstBaseTransform * bt, GstBuffer * bt_inbuf,
    GstBuffer * bt_outbuf)
{
  GstGLFilter *filter;
  GstGLFilterBlur *filterblur;
  GstGLBuffer *inbuf = GST_GL_BUFFER (bt_inbuf);
  GstGLBuffer *outbuf = GST_GL_BUFFER (bt_outbuf);
  GstGLDisplay *display = inbuf->display;

  /* hard coded kernel, it could be easily generated at runtime with a
   * property to change standard deviation */
  gfloat gauss_kernel[9] = { 
    0.026995, 0.064759, 0.120985,
    0.176033, 0.199471, 0.176033,
    0.120985, 0.064759, 0.026995 };
  /* std = 2.0, sum = 0.977016 */

  filter = GST_GL_FILTER (bt);
  filterblur = GST_GL_FILTERBLUR (filter);

  filterblur->intexture = inbuf->texture;
  filterblur->outtexture = outbuf->texture;

  gst_gl_display_lock (display);

  gst_gl_filterblur_init_resources (filterblur);

  glViewport (0, 0, filter->width, filter->height);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  if (!filterblur->shader0) 
    filterblur->shader0 = gst_gl_shader_new ();
  if (!filterblur->shader1) 
    filterblur->shader1 = gst_gl_shader_new ();

  /* here is the key thing about multi-step rendering: 
     1. set_target: binds a texture to the fbo, all rendering
     operations would output in that texture
     2. compile and use shader
     3. draw the texture (modified by the shader)
  */

  /* target: midtexture, input: intexture */
  gst_gl_filterblur_set_target (filterblur, filterblur->midtexture);

  /* temp assert just for debug */
  g_assert (gst_gl_shader_compile_and_check (
	     filterblur->shader0,
	     hconv9_fragment_source, 
	     GST_GL_SHADER_FRAGMENT_SOURCE));
  gst_gl_shader_use (filterblur->shader0);

  glActiveTexture (GL_TEXTURE1);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, filterblur->intexture);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (filterblur->shader0, "tex", 1);

  gst_gl_shader_set_uniform_1fv (filterblur->shader0, "kernel", 9, gauss_kernel);
  gst_gl_shader_set_uniform_1f (filterblur->shader0, "norm_const", 0.977016);
  gst_gl_shader_set_uniform_1f (filterblur->shader0, "norm_offset", 0.0);

  gst_gl_filterblur_draw_texture (filterblur, filterblur->intexture);

  /* note: the last attached texture has to be outtexture */
  /* target: outtexture, input: midtexture */
  gst_gl_filterblur_set_target (filterblur, filterblur->outtexture);

  /* temp assert just for debug */
  g_assert (gst_gl_shader_compile_and_check (
	     filterblur->shader1,
	     vconv9_fragment_source, 
	     GST_GL_SHADER_FRAGMENT_SOURCE));
  gst_gl_shader_use (filterblur->shader1);

  glActiveTexture (GL_TEXTURE1);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, filterblur->midtexture);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (filterblur->shader1, "tex", 1);

  gst_gl_shader_set_uniform_1fv (filterblur->shader1, "kernel", 9, gauss_kernel);
  gst_gl_shader_set_uniform_1f (filterblur->shader1, "norm_const", 0.977016);
  gst_gl_shader_set_uniform_1f (filterblur->shader1, "norm_offset", 0.0);

  gst_gl_filterblur_draw_texture (filterblur, filterblur->midtexture);

  gst_gl_display_unlock (display);

  return GST_FLOW_OK;
}
