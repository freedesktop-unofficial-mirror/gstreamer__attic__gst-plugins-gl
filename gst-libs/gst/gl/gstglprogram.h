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

#ifndef __GST_GL_PROGRAM_H__
#define __GST_GL_PROGRAM_H__

#include <GL/glx.h>
#include <GL/gl.h>
#include <gst/gst.h>
#include "gstglshader.h"

G_BEGIN_DECLS

#define GST_GL_TYPE_PROGRAM         (gst_gl_program_get_type())
#define GST_GL_PROGRAM(o)           (G_TYPE_CHECK_INSTANCE_CAST((o), GST_GL_TYPE_PROGRAM, GstGLProgram))
#define GST_GL_PROGRAM_CLASS(k)     (G_TYPE_CHECK_CLASS((k), GST_GL_TYPE_PROGRAM, GstGLProgramClass))
#define GST_GL_IS_PROGRAM(o)        (G_TYPE_CHECK_INSTANCE_TYPE((o), GST_GL_TYPE_PROGRAM))
#define GST_GL_IS_PROGRAM_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE((k), GST_GL_TYPE_PROGRAM))
#define GST_GL_PROGRAM_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS((o), GST_GL_TYPE_PROGRAM, GstGLProgramClass))

typedef struct _GstGLProgram        GstGLProgram;
typedef struct _GstGLProgramPrivate GstGLProgramPrivate;
typedef struct _GstGLProgramClass   GstGLProgramClass;

struct _GstGLProgram {
     /*< private >*/
     GObject parent;
     GstGLProgramPrivate *priv;
};

struct _GstGLProgramClass {
     /*< private >*/
     GObjectClass parent_class;
};

GType gst_gl_program_get_type (void);

GstGLProgram *gst_gl_program_new (void);

gboolean gst_gl_program_set_shader (GstGLProgram *program, GstGLShader *shader, GError **error);
gboolean gst_gl_program_detach_shader (GstGLProgram *program, GstGLShader *shader);
gboolean gst_gl_program_use (GstGLProgram *program);

G_END_DECLS

#endif /* __GST_GL_PROGRAM_H__ */
