/* 
 * GStreamer
 * Copyright (C) 2007 David Schleef <ds@schleef.org>
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

#define GST_CAT_DEFAULT gst_gl_filter_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);


static GstStaticPadTemplate gst_gl_filter_src_pad_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_GL_VIDEO_CAPS)
    );

static GstStaticPadTemplate gst_gl_filter_sink_pad_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_GL_VIDEO_CAPS)
    );

#define DEBUG_INIT(bla) \
  GST_DEBUG_CATEGORY_INIT (gst_gl_filter_debug, "glfilter", 0, "glfilter element");

GST_BOILERPLATE_FULL (GstGLFilter, gst_gl_filter, GstBaseTransform,
    GST_TYPE_BASE_TRANSFORM, DEBUG_INIT);

static void gst_gl_filter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_filter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_gl_filter_reset (GstGLFilter * filter);
static gboolean gst_gl_filter_start (GstBaseTransform * bt);
static gboolean gst_gl_filter_stop (GstBaseTransform * bt);
static gboolean gst_gl_filter_get_unit_size (GstBaseTransform * trans,
    GstCaps * caps, guint * size);
static GstFlowReturn gst_gl_filter_transform (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer * outbuf);
static GstFlowReturn gst_gl_filter_prepare_output_buffer (GstBaseTransform *
    trans, GstBuffer * input, gint size, GstCaps * caps, GstBuffer ** buf);
static gboolean gst_gl_filter_set_caps (GstBaseTransform * bt, GstCaps * incaps,
    GstCaps * outcaps);
static gboolean gst_gl_filter_do_transform (GstGLFilter * filter,
    GstGLBuffer * inbuf, GstGLBuffer * outbuf);


static void
gst_gl_filter_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_gl_filter_src_pad_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_gl_filter_sink_pad_template));
}

static void
gst_gl_filter_class_init (GstGLFilterClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;
  gobject_class->set_property = gst_gl_filter_set_property;
  gobject_class->get_property = gst_gl_filter_get_property;

  GST_BASE_TRANSFORM_CLASS (klass)->transform = gst_gl_filter_transform;
  GST_BASE_TRANSFORM_CLASS (klass)->start = gst_gl_filter_start;
  GST_BASE_TRANSFORM_CLASS (klass)->stop = gst_gl_filter_stop;
  GST_BASE_TRANSFORM_CLASS (klass)->set_caps = gst_gl_filter_set_caps;
  GST_BASE_TRANSFORM_CLASS (klass)->get_unit_size = gst_gl_filter_get_unit_size;
  GST_BASE_TRANSFORM_CLASS (klass)->prepare_output_buffer =
      gst_gl_filter_prepare_output_buffer;
}

static void
gst_gl_filter_init (GstGLFilter * filter, GstGLFilterClass * klass)
{
  //gst_element_create_all_pads (GST_ELEMENT (filter));

  filter->sinkpad = gst_element_get_static_pad (GST_ELEMENT (filter), "sink");
  filter->srcpad = gst_element_get_static_pad (GST_ELEMENT (filter), "src");

  gst_gl_filter_reset (filter);
}

static void
gst_gl_filter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  //GstGLFilter *filter = GST_GL_FILTER (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_filter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  //GstGLFilter *filter = GST_GL_FILTER (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_filter_reset (GstGLFilter * filter)
{
  if (filter->display) {
    g_object_unref (filter->display);
    filter->display = NULL;
  }
  filter->format = GST_GL_BUFFER_FORMAT_UNKNOWN;
  filter->width = 0;
  filter->height = 0;
}

static gboolean
gst_gl_filter_start (GstBaseTransform * bt)
{
  GstGLFilter *filter;
  GstGLFilterClass *filter_class;

  /* filter = GST_GL_FILTER (bt); */
  /* filter_class = GST_GL_FILTER_GET_CLASS (filter); */

  /* filter_class->start(filter); */

  return TRUE;
}

static gboolean
gst_gl_filter_stop (GstBaseTransform * bt)
{
  GstGLFilter *filter = GST_GL_FILTER (bt);

  gst_gl_filter_reset (filter);

  return TRUE;
}

static gboolean
gst_gl_filter_get_unit_size (GstBaseTransform * trans, GstCaps * caps,
    guint * size)
{
  gboolean ret;
  GstGLBufferFormat format;
  int width;
  int height;

  ret = gst_gl_buffer_format_parse_caps (caps, &format, &width, &height);
  if (ret) {
    *size = gst_gl_buffer_format_get_size (format, width, height);
  }

  return TRUE;
}

static GstFlowReturn
gst_gl_filter_prepare_output_buffer (GstBaseTransform * trans,
    GstBuffer * inbuf, gint size, GstCaps * caps, GstBuffer ** buf)
{
  GstGLFilter *filter;
  GstGLBuffer *gl_inbuf = GST_GL_BUFFER (inbuf);
  GstGLBuffer *gl_outbuf;

  filter = GST_GL_FILTER (trans);

  if (filter->display == NULL) {
    filter->display = gl_inbuf->display;
  }

  gl_outbuf = gst_gl_buffer_new_with_format (filter->display,
      filter->format, filter->width, filter->height);

  *buf = GST_BUFFER (gl_outbuf);
  gst_buffer_set_caps (*buf, caps);

  return GST_FLOW_OK;
}

static gboolean
gst_gl_filter_set_caps (GstBaseTransform * bt, GstCaps * incaps,
    GstCaps * outcaps)
{
  GstGLFilter *filter;
  gboolean ret;

  filter = GST_GL_FILTER (bt);

  ret = gst_gl_buffer_format_parse_caps (incaps, &filter->format,
      &filter->width, &filter->height);

  if (!ret) {
    GST_DEBUG ("bad caps");
    return FALSE;
  }

  GST_ERROR ("set_caps %d %d", filter->width, filter->height);

  return ret;
}

static GstFlowReturn
gst_gl_filter_transform (GstBaseTransform * bt, GstBuffer * inbuf,
    GstBuffer * outbuf)
{
  GstGLFilter *filter;
  GstGLBuffer *gl_inbuf = GST_GL_BUFFER (inbuf);
  GstGLBuffer *gl_outbuf = GST_GL_BUFFER (outbuf);

  filter = GST_GL_FILTER (bt);

  gst_gl_filter_do_transform (filter, gl_inbuf, gl_outbuf);

  return GST_FLOW_OK;
}

static gboolean
gst_gl_filter_do_transform (GstGLFilter * filter,
    GstGLBuffer * inbuf, GstGLBuffer * outbuf)
{
  GstGLDisplay *display = inbuf->display;
  GstGLFilterClass *filter_class;
  unsigned int fbo;

  filter_class = GST_GL_FILTER_GET_CLASS (filter);

  gst_gl_display_lock (display);

  glGenFramebuffersEXT (1, &fbo);
  glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, fbo);

  glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
      GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, outbuf->texture, 0);

  glDrawBuffer (GL_COLOR_ATTACHMENT0_EXT);
  glReadBuffer (GL_COLOR_ATTACHMENT0_EXT);

  g_assert (glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT) ==
      GL_FRAMEBUFFER_COMPLETE_EXT);

  glViewport (0, 0, outbuf->width, outbuf->height);

  glClearColor (0.3, 0.3, 0.3, 1.0);
  glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  glDisable (GL_CULL_FACE);
  glEnableClientState (GL_TEXTURE_COORD_ARRAY);

  glColor4f (1, 1, 1, 1);

  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glActiveTexture (GL_TEXTURE0);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, inbuf->texture);

  filter_class->filter (filter, inbuf, outbuf);

#if 0
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  glColor4f (1, 0, 1, 1);
  glBegin (GL_QUADS);

  glNormal3f (0, 0, -1);

  glTexCoord2f (inbuf->width, 0);
  glVertex3f (0.9, -0.9, 0);
  glTexCoord2f (0, 0);
  glVertex3f (-1.0, -1.0, 0);
  glTexCoord2f (0, inbuf->height);
  glVertex3f (-1.0, 1.0, 0);
  glTexCoord2f (inbuf->width, inbuf->height);
  glVertex3f (1.0, 1.0, 0);
  glEnd ();
#endif

  glFlush ();

  glDeleteFramebuffersEXT (1, &fbo);

  gst_gl_display_unlock (display);

  return TRUE;
}
