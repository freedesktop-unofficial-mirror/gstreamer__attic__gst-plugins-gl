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

#include "gstglprogram.h"
#include "gstglshader.h"
#include "glextensions.h"

#define GST_GL_PROGRAM_GET_PRIVATE(o) \
     (G_TYPE_INSTANCE_GET_PRIVATE((o), GST_GL_TYPE_PROGRAM, GstGLProgramPrivate))

struct _GstGLProgramPrivate
{
  GLhandleARB handle;
};

G_DEFINE_TYPE (GstGLProgram, gst_gl_program, G_TYPE_OBJECT);

#define GST_GL_SHADER_GET_PRIVATE(o) \
     (G_TYPE_INSTANCE_GET_PRIVATE((o), GST_GL_TYPE_SHADER, GstGLShaderPrivate))

enum
{
  PROP_0,
  PROP_VERTEX_SRC,
  PROP_FRAGMENT_SRC,
  PROP_COMPILED,
  PROP_ACTIVE
};

struct _GstGLShaderPrivate
{
  gchar *vertex_src;
  gchar *fragment_src;

  GLhandleARB vertex_handle;
  GLhandleARB fragment_handle;

  gboolean compiled;
  gboolean active;
};

G_DEFINE_TYPE (GstGLShader, gst_gl_shader, G_TYPE_OBJECT);

static void
gst_gl_program_finalize (GObject * object)
{
  GstGLProgram *program;
  GstGLProgramPrivate *priv;

  program = GST_GL_PROGRAM (object);
  priv = program->priv;

  /* Q.what if handle is 0? A.from spec: 
     glDeleteObject() will silently ignore deleting the value 0. */

  glDeleteObjectARB (priv->handle);

  G_OBJECT_CLASS (gst_gl_program_parent_class)->finalize (object);
}

static void
gst_gl_program_class_init (GstGLProgramClass * klass)
{
  /* bind class methods .. */
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GstGLProgramPrivate));

  obj_class->finalize = gst_gl_program_finalize;
}

static void
gst_gl_program_init (GstGLProgram * self)
{
  /* initialize sources and program to null */
  GstGLProgramPrivate *priv;

  priv = self->priv = GST_GL_PROGRAM_GET_PRIVATE (self);

  priv->handle = glCreateProgramObjectARB ();
}

GstGLProgram *
gst_gl_program_new (void)
{
  return g_object_new (GST_GL_TYPE_PROGRAM, NULL);
}

static void
gst_gl_shader_finalize (GObject * object)
{
  GstGLShader *shader;
  GstGLShaderPrivate *priv;

  shader = GST_GL_SHADER (object);
  priv = shader->priv;

  g_free (priv->vertex_src);
  g_free (priv->fragment_src);

  /* FIXME: release all the shaders after use */

  G_OBJECT_CLASS (gst_gl_shader_parent_class)->finalize (object);
}

static void
gst_gl_shader_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstGLShader *shader = GST_GL_SHADER (object);

  switch (prop_id) {
    case PROP_VERTEX_SRC:
      gst_gl_shader_set_vertex_source (shader, g_value_get_string (value));
      break;
    case PROP_FRAGMENT_SRC:
      gst_gl_shader_set_fragment_source (shader, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_shader_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstGLShader *shader = GST_GL_SHADER (object);
  GstGLShaderPrivate *priv = shader->priv;

  switch (prop_id) {
    case PROP_VERTEX_SRC:
      g_value_set_string (value, priv->vertex_src);
      break;
    case PROP_FRAGMENT_SRC:
      g_value_set_string (value, priv->fragment_src);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

}

static void
gst_gl_shader_class_init (GstGLShaderClass * klass)
{
  /* bind class methods .. */
  GObjectClass *obj_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (GstGLShaderPrivate));

  obj_class->finalize = gst_gl_shader_finalize;
  obj_class->set_property = gst_gl_shader_set_property;
  obj_class->get_property = gst_gl_shader_get_property;

  /* .. and install properties */

  g_object_class_install_property (obj_class,
      PROP_VERTEX_SRC,
      g_param_spec_string ("vertex-src",
          "Vertex Source",
          "GLSL Vertex Shader source code", NULL, G_PARAM_READWRITE));
  g_object_class_install_property (obj_class,
      PROP_FRAGMENT_SRC,
      g_param_spec_string ("fragmen-src",
          "Fragment Source",
          "GLSL Fragment Shader source code", NULL, G_PARAM_READWRITE));
  g_object_class_install_property (obj_class,
      PROP_ACTIVE,
      g_param_spec_string ("active",
          "Active", "Enable/Disable the shader", NULL, G_PARAM_READWRITE));


}

void
gst_gl_shader_set_vertex_source (GstGLShader * shader, const gchar * src)
{
  GstGLShaderPrivate *priv;

  g_return_if_fail (GST_GL_IS_SHADER (shader));
  g_return_if_fail (src != NULL);

  priv = shader->priv;

  g_free (priv->vertex_src);

  priv->vertex_src = g_strdup (src);
}

void
gst_gl_shader_set_fragment_source (GstGLShader * shader, const gchar * src)
{
  GstGLShaderPrivate *priv;

  g_return_if_fail (GST_GL_IS_SHADER (shader));
  g_return_if_fail (src != NULL);

  priv = shader->priv;

  g_free (priv->fragment_src);

  priv->fragment_src = g_strdup (src);
}

G_CONST_RETURN gchar *
gst_gl_shader_get_vertex_source (GstGLShader * shader)
{
  g_return_val_if_fail (GST_GL_IS_SHADER (shader), NULL);
  return shader->priv->vertex_src;
}

G_CONST_RETURN gchar *
gst_gl_shader_get_fragment_source (GstGLShader * shader)
{
  g_return_val_if_fail (GST_GL_IS_SHADER (shader), NULL);
  return shader->priv->fragment_src;
}

static void
gst_gl_shader_init (GstGLShader * self)
{
  /* initialize sources and program to null */
  GstGLShaderPrivate *priv;

  priv = self->priv = GST_GL_SHADER_GET_PRIVATE (self);

  priv->vertex_src = NULL;
  priv->fragment_src = NULL;

  priv->fragment_handle = 0;
  priv->vertex_handle = 0;

  priv->compiled = FALSE;
  priv->active = FALSE;
}

GstGLShader *
gst_gl_shader_new (void)
{
  return g_object_new (GST_GL_TYPE_SHADER, NULL);
}


static gboolean
gst_gl_program_attach_shader (GstGLShader * shader, GstGLProgram * program)
{
  GstGLShaderPrivate *shader_priv;
  GstGLProgramPrivate *program_priv;

  g_return_val_if_fail (GST_GL_IS_SHADER (shader), FALSE);
  g_return_val_if_fail (GST_GL_IS_PROGRAM (program), FALSE);

  shader_priv = shader->priv;
  program_priv = program->priv;

  g_return_val_if_fail (program_priv->handle, FALSE);

  if (shader_priv->vertex_handle) {
    glAttachObjectARB (program_priv->handle, shader_priv->vertex_handle);

    glDeleteObjectARB (shader_priv->vertex_handle);
  }
  if (shader_priv->fragment_handle) {
    glAttachObjectARB (program_priv->handle, shader_priv->fragment_handle);
    glDeleteObjectARB (shader_priv->fragment_handle);
  }

  return TRUE;
}

static gboolean
gst_gl_program_link (GstGLProgram * program, GError ** error)
{
  GstGLProgramPrivate *priv;

  gchar info_buffer[2048];
  GLsizei len = 0;
  GLint status = GL_FALSE;

  g_return_val_if_fail (GST_GL_IS_PROGRAM (program), FALSE);

  priv = program->priv;

  g_return_val_if_fail (priv->handle, FALSE);

  glLinkProgramARB (priv->handle);

  glGetObjectParameterivARB (priv->handle, GL_LINK_STATUS, &status);

  glGetInfoLogARB (priv->handle, sizeof (info_buffer) - 1, &len, info_buffer);
  info_buffer[len] = '\0';

  if (status != GL_TRUE) {
    g_set_error (error, GST_GL_SHADER_ERROR,
        GST_GL_SHADER_ERROR_LINK, "Shader Linking failed:\n%s", info_buffer);

    return FALSE;
  } else if (len > 1) {
    g_message ("%s\n", info_buffer);
  }

  glUseProgramObjectARB (priv->handle);

  return TRUE;
}

gboolean
gst_gl_shader_compile (GstGLShader * shader, GError ** error)
{
  GstGLShaderPrivate *priv;

  gchar info_buffer[2048];
  GLsizei len = 0;
  GLint status = GL_FALSE;

  g_return_val_if_fail (GST_GL_IS_SHADER (shader), FALSE);

  priv = shader->priv;

  if (priv->vertex_src) {
    const gchar *vertex_source = priv->vertex_src;

    priv->vertex_handle = glCreateShaderObjectARB (GL_VERTEX_SHADER);
    glShaderSourceARB (priv->vertex_handle, 1, &vertex_source, NULL);

    glCompileShaderARB (priv->vertex_handle);

    glGetInfoLogARB (priv->vertex_handle, sizeof (info_buffer) - 1, &len,
        info_buffer);
    info_buffer[len] = '\0';

    glGetObjectParameterivARB (priv->vertex_handle, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
      g_set_error (error, GST_GL_SHADER_ERROR,
          GST_GL_SHADER_ERROR_COMPILE,
          "Vertex Shader compilation failed:\n%s", info_buffer);

      return FALSE;
    } else if (len > 1) {
      g_message ("%s\n", info_buffer);
    }
  }

  if (priv->fragment_src) {
    const gchar *fragment_source = priv->fragment_src;

    priv->fragment_handle = glCreateShaderObjectARB (GL_FRAGMENT_SHADER_ARB);
    glShaderSourceARB (priv->fragment_handle, 1, &fragment_source, NULL);

    glCompileShaderARB (priv->fragment_handle);

    glGetObjectParameterivARB (priv->fragment_handle,
        GL_OBJECT_COMPILE_STATUS_ARB, &status);

    glGetInfoLogARB (priv->fragment_handle,
        sizeof (info_buffer) - 1, &len, info_buffer);
    info_buffer[len] = '\0';

    if (status != GL_TRUE) {
      g_set_error (error, GST_GL_SHADER_ERROR,
          GST_GL_SHADER_ERROR_COMPILE,
          "Fragment Shader compilation failed:\n%s", info_buffer);

      return FALSE;
    } else if (len > 1) {
      g_message ("%s\n", info_buffer);
    }
  }
  return TRUE;
}

/* FIXME: replace with and _add function to bind multiple shaders to */
/*        the same program, introduce some kind of data struct to keep
 *        track of current shaders */

gboolean
gst_gl_program_set_shader (GstGLProgram * program, GstGLShader * shader,
    GError ** error)
{
  GstGLProgramPrivate *priv;

  priv = program->priv;

  g_return_val_if_fail (priv->handle != 0, FALSE);

  if (!gst_gl_shader_compile (shader, error))
    return FALSE;

  gst_gl_program_attach_shader (shader, program);

  return gst_gl_program_link (program, error);
}

gboolean
gst_gl_program_detach_shader (GstGLProgram * program, GstGLShader * shader)
{
  GstGLShaderPrivate *shader_priv;
  GstGLProgramPrivate *program_priv;

  g_return_val_if_fail (GST_GL_IS_SHADER (shader), FALSE);
  g_return_val_if_fail (GST_GL_IS_PROGRAM (program), FALSE);

  shader_priv = shader->priv;
  program_priv = program->priv;

  g_return_val_if_fail (program_priv->handle, FALSE);

  if (shader_priv->vertex_handle) {
    glDetachObjectARB (program_priv->handle, shader_priv->vertex_handle);

  }
  if (shader_priv->fragment_handle) {
    glDetachObjectARB (program_priv->handle, shader_priv->fragment_handle);
  }
  return TRUE;
}

gboolean
gst_gl_program_use (GstGLProgram * program)
{
  GstGLProgramPrivate *priv;

  if (!program) {
    g_message ("No program object given, restoring fixed state");
    glUseProgramObjectARB (0);
    return TRUE;
  }

  priv = program->priv;

  glUseProgramObjectARB (priv->handle);

  return TRUE;
}

GQuark
gst_gl_shader_error_quark (void)
{
  return g_quark_from_static_string ("gst-gl-shader-error");
}
