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

#define GST_CAT_DEFAULT gst_gl_filtersobel_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define GST_TYPE_GL_FILTERSOBEL            (gst_gl_filtersobel_get_type())
#define GST_GL_FILTERSOBEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_FILTERSOBEL,GstGLFilterSobel))
#define GST_IS_GL_FILTERSOBEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_FILTERSOBEL))
#define GST_GL_FILTERSOBEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_FILTERSOBEL,GstGLFilterSobelClass))
#define GST_IS_GL_FILTERSOBEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_FILTERSOBEL))
#define GST_GL_FILTERSOBEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_FILTERSOBEL,GstGLFilterSobelClass))
typedef struct _GstGLFilterSobel GstGLFilterSobel;
typedef struct _GstGLFilterSobelClass GstGLFilterSobelClass;

struct _GstGLFilterSobel
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

static const gchar *sobel_fragment_source = 
  "#extension GL_ARB_texture_rectangle : enable\n"
  "uniform sampler2DRect tex;"
  "uniform float hkern[9];"
  "uniform float vkern[9];"
  "void main () {"
  "  vec2 offset[9] = vec2[9] ( vec2(-1.0,-1.0), vec2( 0.0,-1.0), vec2( 1.0,-1.0),"
  "                             vec2(-1.0, 0.0), vec2( 0.0, 0.0), vec2( 1.0, 0.0),"
  "                             vec2(-1.0, 1.0), vec2( 0.0, 1.0), vec2( 1.0, 1.0) );"
  "  vec2 texturecoord = gl_TexCoord[0].st;"
  "  int i;"
  "  float luma;"
  "  float gx = 0.0;"
  "  float gy = 0.0 ;"
  "  for (i = 0; i < 9; i++) { "
  "    if(hkern[i] != 0.0 || vkern[i] != 0.0) {"
  "      vec4 neighbor = texture2DRect(tex, texturecoord + vec2(offset[i])); "
  "      luma = dot(neighbor, vec4(0.2125, 0.7154, 0.0721, neighbor.a));"
  "      gx += luma * hkern[i]; "
  "      gy += luma * vkern[i]; "
  "    }"
  "  }"
  "  float g = sqrt(gx*gx + gy*gy);"
  "  gl_FragColor = vec4(g);"
  "}";

struct _GstGLFilterSobelClass
{
  GstGLFilterClass filter_class;
};

static const GstElementDetails element_details =
GST_ELEMENT_DETAILS ("Gstreamer OpenGL Filtersobel",
    "Filter/Effect",
    "Sobel Convolution Effect",
    "Filippo Argiolas <filippo.argiolas@gmail.com>");

#define DEBUG_INIT(bla)							\
  GST_DEBUG_CATEGORY_INIT (gst_gl_filtersobel_debug, "glfiltersobel", 0, "glfiltersobel element");

GST_BOILERPLATE_FULL (GstGLFilterSobel, gst_gl_filtersobel, GstGLFilter,
		      GST_TYPE_GL_FILTER, DEBUG_INIT);

static void gst_gl_filtersobel_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_filtersobel_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_gl_filtersobel_set_target (GstGLFilterSobel * filtersobel, GLuint tex);
static void gst_gl_filtersobel_draw_texture (GstGLFilterSobel * filtersobel, GLuint tex);

static GLuint gst_gl_filtersobel_prepare_texture (GLint width, GLint height);
static GstFlowReturn gst_gl_filtersobel_transform (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer * outbuf);

static void
gst_gl_filtersobel_reset_resources (GstGLFilterSobel * filtersobel)
{
  GLenum err;

  g_message ("deleting textures");
  glDeleteTextures (1, &filtersobel->midtexture);

  g_message ("deleting fbo");
  glDeleteFramebuffersEXT (1, &filtersobel->fbo);
  err = glGetError ();
  if (err)
    g_warning (G_STRLOC "error: 0x%x", err);

  filtersobel->fbo = 0;
}

static gboolean
gst_gl_filtersobel_stop (GstGLFilter * filter)
{
  GstGLFilterSobel *filtersobel;

  filtersobel = GST_GL_FILTERSOBEL (filter);

  g_message ("stop!");

  if (filter->display) {
    gst_gl_display_lock (filter->display);
    gst_gl_filtersobel_reset_resources (filtersobel);
    gst_gl_display_unlock (filter->display);
  }

  return TRUE;
}

static void
gst_gl_filtersobel_init_resources (GstGLFilterSobel * filtersobel)
{
  GstGLFilter *filter = GST_GL_FILTER (filtersobel);

  if (filtersobel->fbo) {
    return;
  }

  g_message ("creating fbo");
  glGenFramebuffersEXT (1, &filtersobel->fbo);
  g_message ("generating midtexture");
  filtersobel->midtexture = gst_gl_filtersobel_prepare_texture (filter->width, filter->height);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}

static gboolean
gst_gl_filtersobel_start (GstGLFilter * filter)
{
  g_message ("start!");
  return TRUE;
}

static void
gst_gl_filtersobel_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details (element_class, &element_details);
}

static void
gst_gl_filtersobel_class_init (GstGLFilterSobelClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;
  gobject_class->set_property = gst_gl_filtersobel_set_property;
  gobject_class->get_property = gst_gl_filtersobel_get_property;

  GST_BASE_TRANSFORM_CLASS (klass)->transform = gst_gl_filtersobel_transform;
  GST_GL_FILTER_CLASS (klass)->start = gst_gl_filtersobel_start;
  GST_GL_FILTER_CLASS (klass)->stop = gst_gl_filtersobel_stop;
}

static void
gst_gl_filtersobel_init (GstGLFilterSobel * filtersobel, GstGLFilterSobelClass * klass)
{
  filtersobel->shader0 = NULL;
  filtersobel->shader1 = NULL;
}

static void
gst_gl_filtersobel_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  /* GstGLFilterSobel *filtersobel = GST_GL_FILTERSOBEL (object); */

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_filtersobel_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  /* GstGLFilterSobel *filtersobel = GST_GL_FILTERSOBEL (object); */

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_filtersobel_draw_texture (GstGLFilterSobel * filtersobel, GLuint tex)
{
  GstGLFilter *filter = GST_GL_FILTER (filtersobel);

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
gst_gl_filtersobel_set_target (GstGLFilterSobel * filtersobel, GLuint tex)
{
  glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, filtersobel->fbo);

  glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
      GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, tex, 0);

  gst_gl_shader_use (NULL);

  g_assert (glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT) ==
      GL_FRAMEBUFFER_COMPLETE_EXT);

  glDrawBuffer (GL_COLOR_ATTACHMENT0_EXT);
  glReadBuffer (GL_COLOR_ATTACHMENT0_EXT);
}

static GLuint
gst_gl_filtersobel_prepare_texture (GLint width, GLint height)
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
gst_gl_filtersobel_transform (GstBaseTransform * bt, GstBuffer * bt_inbuf,
    GstBuffer * bt_outbuf)
{
  GstGLFilter *filter;
  GstGLFilterSobel *filtersobel;
  GstGLBuffer *inbuf = GST_GL_BUFFER (bt_inbuf);
  GstGLBuffer *outbuf = GST_GL_BUFFER (bt_outbuf);
  GstGLDisplay *display = inbuf->display;

  gfloat hkern[9] = { 
    1.0, 0.0, -1.0,
    2.0, 0.0, -2.0,
    1.0, 0.0, -1.0
  };
  
  gfloat vkern[9] = {
     1.0,  2.0,  1.0,
     0.0,  0.0,  0.0,
    -1.0, -2.0, -1.0
    };

  filter = GST_GL_FILTER (bt);
  filtersobel = GST_GL_FILTERSOBEL (filter);

  filtersobel->intexture = inbuf->texture;
  filtersobel->outtexture = outbuf->texture;

  gst_gl_display_lock (display);

  gst_gl_filtersobel_init_resources (filtersobel);

  glViewport (0, 0, filter->width, filter->height);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  if (!filtersobel->shader0) 
    filtersobel->shader0 = gst_gl_shader_new ();
  if (!filtersobel->shader1) 
    filtersobel->shader1 = gst_gl_shader_new ();
  
  gst_gl_filtersobel_set_target (filtersobel, filtersobel->outtexture);

  /* temp assert just for debug */
  g_assert (gst_gl_shader_compile_and_check (
	     filtersobel->shader0,
	     sobel_fragment_source, 
	     GST_GL_SHADER_FRAGMENT_SOURCE));
  gst_gl_shader_use (filtersobel->shader0);

  glActiveTexture (GL_TEXTURE1);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, filtersobel->intexture);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (filtersobel->shader0, "tex", 1);

  gst_gl_shader_set_uniform_1fv (filtersobel->shader0, "hkern", 9, hkern);
  gst_gl_shader_set_uniform_1fv (filtersobel->shader0, "vkern", 9, vkern);

  gst_gl_filtersobel_draw_texture (filtersobel, filtersobel->intexture);

  gst_gl_display_unlock (display);

  return GST_FLOW_OK;
}
