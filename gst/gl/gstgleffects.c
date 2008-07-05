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
#include "gstgleffectssources.h"
#include "gstgleffectstextures.h"

#define GST_CAT_DEFAULT gst_gl_effects_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define GST_TYPE_GL_EFFECTS            (gst_gl_effects_get_type())
#define GST_GL_EFFECTS(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_EFFECTS,GstGLEffects))
#define GST_IS_GL_EFFECTS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_EFFECTS))
#define GST_GL_EFFECTS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass) ,GST_TYPE_GL_EFFECTS,GstGLEffectsClass))
#define GST_IS_GL_EFFECTS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass) ,GST_TYPE_GL_EFFECTS))
#define GST_GL_EFFECTS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj) ,GST_TYPE_GL_EFFECTS,GstGLEffectsClass))
typedef struct _GstGLEffects GstGLEffects;
typedef struct _GstGLEffectsClass GstGLEffectsClass;
typedef struct _GstGLEffectContainer GstGLEffectContainer;

typedef enum
{
  GST_GL_EFFECT_IDENTITY,
  GST_GL_EFFECT_SQUEEZE,
  GST_GL_EFFECT_STRETCH,
  GST_GL_EFFECT_TUNNEL,
  GST_GL_EFFECT_FISHEYE,
  GST_GL_EFFECT_TWIRL,
  GST_GL_EFFECT_BULGE,
  GST_GL_EFFECT_SQUARE,
  GST_GL_EFFECT_MIRROR,
  GST_GL_EFFECT_HEAT,
  GST_GL_EFFECT_CROSS,
  GST_GL_EFFECT_TEST
} GstGLEffectsEffect;

typedef void (*GstGLEffectProcessFunc) (GstGLEffects * filter);

struct _GstGLEffects
{
  GstGLFilter filter;

  /* < private? > */

  GstGLEffectProcessFunc effect;
  GstGLShader *current_shader;
  gint current_effect;
  gboolean effect_changed;
  gboolean is_mirrored;
  GLuint intexture;
  GLuint midtexture[10];
  GLuint outtexture;
  GLuint fbo;
};

struct _GstGLEffectContainer
{
  gint idx;
  GstGLShader *shader;          //shader to apply
  GLuint texture;               //texture to render to
};

GHashTable *shaderstable;

#define GST_TYPE_GL_EFFECTS_EFFECT (gst_gl_effects_effect_get_type ())
static GType
gst_gl_effects_effect_get_type (void)
{
  static GType gl_effects_effect_type = 0;
  static const GEnumValue effect_types[] = {
    {GST_GL_EFFECT_IDENTITY, "Do nothing Effect", "identity"},
    {GST_GL_EFFECT_SQUEEZE, "Squeeze Effect", "squeeze"},
    {GST_GL_EFFECT_STRETCH, "Stretch Effect", "stretch"},
    {GST_GL_EFFECT_FISHEYE, "FishEye Effect", "fisheye"},
    {GST_GL_EFFECT_TWIRL, "Twirl Effect", "twirl"},
    {GST_GL_EFFECT_BULGE, "Bulge Effect", "bulge"},
    {GST_GL_EFFECT_TUNNEL, "Light Tunnel Effect", "tunnel"},
    {GST_GL_EFFECT_SQUARE, "Square Effect", "square"},
    {GST_GL_EFFECT_MIRROR, "Mirror Effect", "mirror"},
    {GST_GL_EFFECT_HEAT, "Heat Signature Effect", "heat"},
    {GST_GL_EFFECT_CROSS, "Cross Processing Effect", "cross"},
    {GST_GL_EFFECT_TEST, "Test Effect", "test"},
    {0, NULL, NULL}
  };

  if (!gl_effects_effect_type) {
    gl_effects_effect_type =
        g_enum_register_static ("GstGLEffectsEffect", effect_types);
  }
  return gl_effects_effect_type;
}

struct _GstGLEffectsClass
{
  GstGLFilterClass filter_class;
};

static const GstElementDetails element_details =
GST_ELEMENT_DETAILS ("Gstreamer OpenGL Effects",
    "Filter/Effect",
    "GL Shading Language effects",
    "Filippo Argiolas <filippo.argiolas@gmail.com>");

enum
{
  PROP_0,
  PROP_EFFECT,
  PROP_MIRROR
};

#define DEBUG_INIT(bla)							\
  GST_DEBUG_CATEGORY_INIT (gst_gl_effects_debug, "gleffects", 0, "gleffects element");

GST_BOILERPLATE_FULL (GstGLEffects, gst_gl_effects, GstGLFilter,
    GST_TYPE_GL_FILTER, DEBUG_INIT);

static void gst_gl_effects_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gl_effects_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_gl_effects_set_target (GstGLEffects * effects, GLuint tex);
static void gst_gl_effects_draw_texture (GstGLEffects * effects, GLuint tex);

static GstFlowReturn gst_gl_effects_transform (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer * outbuf);
static void gst_gl_effects_identity (GstGLEffects * effects);

static gboolean
gst_gl_effects_stop (GstGLFilter * filter)
{
  GstGLEffects *effects;

  int i;

  effects = GST_GL_EFFECTS (filter);

  gst_gl_display_lock (filter->display);

  for (i = 0; i < 10; i++) {
    glDeleteTextures (1, &effects->midtexture[i]);
  }

  g_hash_table_unref (shaderstable);

  glDeleteFramebuffersEXT (1, &effects->fbo);
  GLenum err = glGetError ();
  if (err)
    g_warning (G_STRLOC "error: 0x%x", err);

  gst_gl_display_unlock (filter->display);

  return TRUE;
}

static gboolean
gst_gl_effects_start (GstGLFilter * filter)
{
  GstGLEffects *effects = GST_GL_EFFECTS (filter);

  effects->effect_changed = FALSE;
  effects->current_shader = NULL;

  shaderstable = g_hash_table_new_full (g_str_hash,
      g_str_equal, NULL, g_object_unref);

  return TRUE;
}

static void
gst_gl_effects_base_init (gpointer klass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);

  gst_element_class_set_details (element_class, &element_details);
}

static void
gst_gl_effects_class_init (GstGLEffectsClass * klass)
{
  GObjectClass *gobject_class;

  gobject_class = (GObjectClass *) klass;
  gobject_class->set_property = gst_gl_effects_set_property;
  gobject_class->get_property = gst_gl_effects_get_property;

  GST_BASE_TRANSFORM_CLASS (klass)->transform = gst_gl_effects_transform;
  GST_GL_FILTER_CLASS (klass)->start = gst_gl_effects_start;
  GST_GL_FILTER_CLASS (klass)->stop = gst_gl_effects_stop;

  g_object_class_install_property (gobject_class,
      PROP_EFFECT,
      g_param_spec_enum ("effect",
          "Effect",
          "Select which effect apply to GL video texture",
          GST_TYPE_GL_EFFECTS_EFFECT,
          GST_GL_EFFECT_IDENTITY, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (gobject_class,
      PROP_MIRROR,
      g_param_spec_boolean ("mirror",
          "Reflect horizontally",
          "Reflects images horizontally, usefull with webcams",
          FALSE, G_PARAM_READWRITE));

}

static void
gst_gl_effects_init (GstGLEffects * effects, GstGLEffectsClass * klass)
{
  effects->effect = gst_gl_effects_identity;
}

static void
gst_gl_effects_identity (GstGLEffects * effects)
{

  gst_gl_effects_set_target (effects, effects->outtexture);
  gst_gl_effects_draw_texture (effects, effects->intexture);

}

static void
gst_gl_effects_mirror (GstGLEffects * effects)
{
  gfloat tex_size[2];

  GstGLShader *shader;
  gboolean is_compiled = FALSE;

  shader = g_hash_table_lookup (shaderstable, "mirror0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "mirror0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_object_get (G_OBJECT (shader), "compiled", &is_compiled, NULL);

  if (!is_compiled) {
    GError *error = NULL;

    gst_gl_shader_set_fragment_source (shader, mirror_fragment_source);
    gst_gl_shader_compile (shader, &error);
    if (error) {
      GST_ERROR ("%s", error->message);
      g_error_free (error);
      error = NULL;
      gst_gl_shader_use (NULL);
      return;
    }
  }

  gst_gl_shader_use (shader);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);

  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  gst_gl_shader_set_uniform_1i (shader, "tex", 0);

  tex_size[0] = GST_GL_FILTER (effects)->width / 2.0;
  tex_size[1] = GST_GL_FILTER (effects)->height / 2.0;

  gst_gl_shader_set_uniform_1f (shader, "width", tex_size[0]);
  gst_gl_shader_set_uniform_1f (shader, "height", tex_size[1]);

  gst_gl_effects_draw_texture (effects, effects->intexture);
}

static void
gst_gl_effects_tunnel (GstGLEffects * effects)
{
  gfloat tex_size[2];

  GstGLShader *shader;
  gboolean is_compiled = FALSE;

  shader = g_hash_table_lookup (shaderstable, "tunnel0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "tunnel0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_object_get (G_OBJECT (shader), "compiled", &is_compiled, NULL);

  if (!is_compiled) {
    GError *error = NULL;

    gst_gl_shader_set_fragment_source (shader, tunnel_fragment_source);
    gst_gl_shader_compile (shader, &error);
    if (error) {
      GST_ERROR ("%s", error->message);
      g_error_free (error);
      error = NULL;
      gst_gl_shader_use (NULL);
      return;
    }
  }

  gst_gl_shader_use (shader);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);

  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  gst_gl_shader_set_uniform_1i (shader, "tex", 0);

  tex_size[0] = GST_GL_FILTER (effects)->width / 2.0;
  tex_size[1] = GST_GL_FILTER (effects)->height / 2.0;

  gst_gl_shader_set_uniform_1f (shader, "width", tex_size[0]);
  gst_gl_shader_set_uniform_1f (shader, "height", tex_size[1]);

  gst_gl_effects_draw_texture (effects, effects->intexture);
}

static void
gst_gl_effects_twirl (GstGLEffects * effects)
{
  gfloat tex_size[2];

  GstGLShader *shader;
  gboolean is_compiled = FALSE;

  shader = g_hash_table_lookup (shaderstable, "twirl0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "twirl0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_object_get (G_OBJECT (shader), "compiled", &is_compiled, NULL);

  if (!is_compiled) {
    GError *error = NULL;

    gst_gl_shader_set_fragment_source (shader, twirl_fragment_source);
    gst_gl_shader_compile (shader, &error);
    if (error) {
      GST_ERROR ("%s", error->message);
      g_error_free (error);
      error = NULL;
      gst_gl_shader_use (NULL);
      return;
    }
  }

  gst_gl_shader_use (shader);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);

  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  gst_gl_shader_set_uniform_1i (shader, "tex", 0);

  tex_size[0] = GST_GL_FILTER (effects)->width / 2.0;
  tex_size[1] = GST_GL_FILTER (effects)->height / 2.0;

  gst_gl_shader_set_uniform_1f (shader, "width", tex_size[0]);
  gst_gl_shader_set_uniform_1f (shader, "height", tex_size[1]);

  gst_gl_effects_draw_texture (effects, effects->intexture);
}

static void
gst_gl_effects_bulge (GstGLEffects * effects)
{
  gfloat tex_size[2];

  GstGLShader *shader;
  gboolean is_compiled = FALSE;

  shader = g_hash_table_lookup (shaderstable, "bulge0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "bulge0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_object_get (G_OBJECT (shader), "compiled", &is_compiled, NULL);

  if (!is_compiled) {
    GError *error = NULL;

    gst_gl_shader_set_fragment_source (shader, bulge_fragment_source);
    gst_gl_shader_compile (shader, &error);
    if (error) {
      GST_ERROR ("%s", error->message);
      g_error_free (error);
      error = NULL;
      gst_gl_shader_use (NULL);
      return;
    }
  }

  gst_gl_shader_use (shader);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);

  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  gst_gl_shader_set_uniform_1i (shader, "tex", 0);

  tex_size[0] = GST_GL_FILTER (effects)->width / 2.0;
  tex_size[1] = GST_GL_FILTER (effects)->height / 2.0;

  gst_gl_shader_set_uniform_1f (shader, "width", tex_size[0]);
  gst_gl_shader_set_uniform_1f (shader, "height", tex_size[1]);

  gst_gl_effects_draw_texture (effects, effects->intexture);
}

static void
gst_gl_effects_stretch (GstGLEffects * effects)
{
  gfloat tex_size[2];

  GstGLShader *shader;
  gboolean is_compiled = FALSE;

  shader = g_hash_table_lookup (shaderstable, "stretch0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "stretch0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_object_get (G_OBJECT (shader), "compiled", &is_compiled, NULL);

  if (!is_compiled) {
    GError *error = NULL;

    gst_gl_shader_set_fragment_source (shader, stretch_fragment_source);
    gst_gl_shader_compile (shader, &error);
    if (error) {
      GST_ERROR ("%s", error->message);
      g_error_free (error);
      error = NULL;
      gst_gl_shader_use (NULL);
      return;
    }
  }

  gst_gl_shader_use (shader);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);

  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  gst_gl_shader_set_uniform_1i (shader, "tex", 0);

  tex_size[0] = GST_GL_FILTER (effects)->width / 2.0;
  tex_size[1] = GST_GL_FILTER (effects)->height / 2.0;

  gst_gl_shader_set_uniform_1f (shader, "width", tex_size[0]);
  gst_gl_shader_set_uniform_1f (shader, "height", tex_size[1]);

  gst_gl_effects_draw_texture (effects, effects->intexture);
}

static void
gst_gl_effects_squeeze (GstGLEffects * effects)
{
  gfloat tex_size[2];

  GstGLShader *shader;
  gboolean is_compiled = FALSE;

  shader = g_hash_table_lookup (shaderstable, "squeeze0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "squeeze0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_object_get (G_OBJECT (shader), "compiled", &is_compiled, NULL);

  if (!is_compiled) {
    GError *error = NULL;

    gst_gl_shader_set_fragment_source (shader, squeeze_fragment_source);
    gst_gl_shader_compile (shader, &error);
    if (error) {
      GST_ERROR ("%s", error->message);
      g_error_free (error);
      error = NULL;
      gst_gl_shader_use (NULL);
      return;
    }
  }

  gst_gl_shader_use (shader);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);

  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  gst_gl_shader_set_uniform_1i (shader, "tex", 0);

  tex_size[0] = GST_GL_FILTER (effects)->width / 2.0;
  tex_size[1] = GST_GL_FILTER (effects)->height / 2.0;

  gst_gl_shader_set_uniform_1f (shader, "width", tex_size[0]);
  gst_gl_shader_set_uniform_1f (shader, "height", tex_size[1]);

  gst_gl_effects_draw_texture (effects, effects->intexture);
}

static void
gst_gl_effects_fisheye (GstGLEffects * effects)
{
  gfloat tex_size[2];

  GstGLShader *shader;
  gboolean is_compiled = FALSE;

  shader = g_hash_table_lookup (shaderstable, "fisheye0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "fisheye0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_object_get (G_OBJECT (shader), "compiled", &is_compiled, NULL);

  if (!is_compiled) {
    GError *error = NULL;

    gst_gl_shader_set_fragment_source (shader, fisheye_fragment_source);
    gst_gl_shader_compile (shader, &error);
    if (error) {
      GST_ERROR ("%s", error->message);
      g_error_free (error);
      error = NULL;
      gst_gl_shader_use (NULL);
      return;
    }
  }

  gst_gl_shader_use (shader);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);

  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  gst_gl_shader_set_uniform_1i (shader, "tex", 0);

  tex_size[0] = GST_GL_FILTER (effects)->width / 2.0;
  tex_size[1] = GST_GL_FILTER (effects)->height / 2.0;

  gst_gl_shader_set_uniform_1f (shader, "width", tex_size[0]);
  gst_gl_shader_set_uniform_1f (shader, "height", tex_size[1]);

  gst_gl_effects_draw_texture (effects, effects->intexture);
}

static void
gst_gl_effects_square (GstGLEffects * effects)
{
  gfloat tex_size[2];

  GstGLShader *shader;
  gboolean is_compiled = FALSE;

  shader = g_hash_table_lookup (shaderstable, "square0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "square0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_object_get (G_OBJECT (shader), "compiled", &is_compiled, NULL);

  if (!is_compiled) {
    GError *error = NULL;

    gst_gl_shader_set_fragment_source (shader, square_fragment_source);
    gst_gl_shader_compile (shader, &error);
    if (error) {
      GST_ERROR ("%s", error->message);
      g_error_free (error);
      error = NULL;
      gst_gl_shader_use (NULL);
      return;
    }
  }

  gst_gl_shader_use (shader);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);

  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  gst_gl_shader_set_uniform_1i (shader, "tex", 0);

  tex_size[0] = GST_GL_FILTER (effects)->width / 2.0;
  tex_size[1] = GST_GL_FILTER (effects)->height / 2.0;

  gst_gl_shader_set_uniform_1f (shader, "width", tex_size[0]);
  gst_gl_shader_set_uniform_1f (shader, "height", tex_size[1]);

  gst_gl_effects_draw_texture (effects, effects->intexture);
}


static void
gst_gl_effects_test (GstGLEffects * effects)
{
  GstGLShader *shader0;
  GstGLShader *shader1;
  GstGLShader *shader2;
//  GstGLFilter *filter = GST_GL_FILTER (effects);
  gboolean is_compiled = FALSE;
  /* gfloat kernel[9] = {  0.0, -1.0,  0.0, */
  /*                   -1.0,  4.0, -1.0, */
  /*                    0.0,  -1.0,  0.0 }; */
  gfloat kernel[9] = { 1.0, 2.0, 1.0,
    2.0, 4.0, 2.0,
    1.0, 2.0, 1.0
  };

  shader0 = g_hash_table_lookup (shaderstable, "test0");
  shader1 = g_hash_table_lookup (shaderstable, "test1");
  shader2 = g_hash_table_lookup (shaderstable, "test2");

  if (!shader0) {
    shader0 = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "test0", shader0);
  }
  if (!shader1) {
    shader1 = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "test1", shader1);
  }
  if (!shader2) {
    shader2 = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "test2", shader2);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_object_get (G_OBJECT (shader0), "compiled", &is_compiled, NULL);

  if (!is_compiled) {
    GError *error = NULL;

    gst_gl_shader_set_fragment_source (shader0, test_fragment_source);
    gst_gl_shader_compile (shader0, &error);
    if (error) {
      g_error ("%s", error->message);
      g_error_free (error);
      error = NULL;
      gst_gl_shader_use (NULL);
      return;
    }
  }

  gst_gl_shader_use (shader0);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);

  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  gst_gl_shader_set_uniform_1i (shader0, "tex", 0);

  gst_gl_effects_draw_texture (effects, effects->intexture);

  return;

  gst_gl_effects_set_target (effects, effects->midtexture[1]);

  g_object_get (G_OBJECT (shader1), "compiled", &is_compiled, NULL);

  if (!is_compiled) {
    GError *error = NULL;

    gst_gl_shader_set_fragment_source (shader1, convolution_fragment_source);
    gst_gl_shader_compile (shader1, &error);
    if (error) {
      g_error ("%s", error->message);   //GST_ERROR
      g_error_free (error);
      error = NULL;
      gst_gl_shader_use (NULL);
      return;
    }
  }

  gst_gl_shader_use (shader1);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->midtexture[0]);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader1, "tex", 0);

  gst_gl_shader_set_uniform_1fv (shader1, "kernel", 9, kernel);
  gst_gl_shader_set_uniform_1f (shader1, "norm_const", 16.0);
  gst_gl_shader_set_uniform_1f (shader1, "norm_offset", 0.0);

  gst_gl_effects_draw_texture (effects, effects->midtexture[0]);

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_object_get (G_OBJECT (shader2), "compiled", &is_compiled, NULL);

  if (!is_compiled) {
    GError *error = NULL;

    gst_gl_shader_set_fragment_source (shader2, blend_fragment_source);
    gst_gl_shader_compile (shader2, &error);
    if (error) {
      g_error ("%s", error->message);   //GST_ERROR
      g_error_free (error);
      error = NULL;
      gst_gl_shader_use (NULL);
      return;
    }
  }

  gst_gl_shader_use (shader2);

  glActiveTexture (GL_TEXTURE2);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader2, "base", 2);

  glActiveTexture (GL_TEXTURE1);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->midtexture[1]);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader2, "blend", 1);

  gst_gl_effects_draw_texture (effects, effects->midtexture[1]);
}

static void
gst_gl_effects_heat (GstGLEffects * effects)
{
  GstGLShader *shader;
  gboolean is_compiled = FALSE;
  GLuint heat_texture;

  shader = g_hash_table_lookup (shaderstable, "heat0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "heat0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_object_get (G_OBJECT (shader), "compiled", &is_compiled, NULL);

  if (!is_compiled) {
    GError *error = NULL;

    gst_gl_shader_set_fragment_source (shader, heat_fragment_source);
    gst_gl_shader_compile (shader, &error);
    if (error) {
      g_error ("%s", error->message);   //GST_ERROR
      g_error_free (error);
      error = NULL;
      gst_gl_shader_use (NULL);
      return;
    }
  }

  gst_gl_shader_use (shader);

  glGenTextures (1, &heat_texture);
  glEnable (GL_TEXTURE_1D);
  glBindTexture (GL_TEXTURE_1D, heat_texture);

  glActiveTexture (GL_TEXTURE5);
  gst_gl_shader_set_uniform_1i (shader, "heat_tex", 5);

  /* this parameters are needed to have a right, predictable, mapping */

  glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  glTexImage1D (GL_TEXTURE_1D,
      0,
      heat_curve.bytes_per_pixel,
      heat_curve.width, 0, GL_RGB, GL_UNSIGNED_BYTE, heat_curve.pixel_data);

  glDisable (GL_TEXTURE_1D);

  gst_gl_effects_draw_texture (effects, effects->intexture);

  glDeleteTextures (1, &heat_texture);
}

static void
gst_gl_effects_cross (GstGLEffects * effects)
{
  GstGLShader *shader;
  gboolean is_compiled = FALSE;
  GLuint cross_texture;

  shader = g_hash_table_lookup (shaderstable, "cross0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "cross0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_object_get (G_OBJECT (shader), "compiled", &is_compiled, NULL);

  if (!is_compiled) {
    GError *error = NULL;

    gst_gl_shader_set_fragment_source (shader, cross_fragment_source);
    gst_gl_shader_compile (shader, &error);
    if (error) {
      g_error ("%s", error->message);   //GST_ERROR
      g_error_free (error);
      error = NULL;
      gst_gl_shader_use (NULL);
      return;
    }
  }

  gst_gl_shader_use (shader);

  glGenTextures (1, &cross_texture);
  glEnable (GL_TEXTURE_1D);
  glBindTexture (GL_TEXTURE_1D, cross_texture);

  glActiveTexture (GL_TEXTURE5);
  gst_gl_shader_set_uniform_1i (shader, "cross_tex", 5);

  /* this parameters are needed to have a right, predictable, mapping */

  glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  glTexImage1D (GL_TEXTURE_1D,
      0,
      cross_curve.bytes_per_pixel,
      cross_curve.width, 0, GL_RGB, GL_UNSIGNED_BYTE, cross_curve.pixel_data);

  glDisable (GL_TEXTURE_1D);

  gst_gl_effects_draw_texture (effects, effects->intexture);

  glDeleteTextures (1, &cross_texture);
}

static void
gst_gl_effects_set_effect (GstGLEffects * filter, gint effect_type)
{
  GST_DEBUG ("current: %d new: %d\n", filter->current_effect, effect_type);

  if (filter->current_effect == effect_type) {
    filter->effect_changed = FALSE;
    return;
  }

  switch (effect_type) {
    case GST_GL_EFFECT_IDENTITY:
      filter->effect = (GstGLEffectProcessFunc) gst_gl_effects_identity;
      break;
    case GST_GL_EFFECT_SQUEEZE:
      filter->effect = (GstGLEffectProcessFunc) gst_gl_effects_squeeze;
      break;
    case GST_GL_EFFECT_TUNNEL:
      filter->effect = (GstGLEffectProcessFunc) gst_gl_effects_tunnel;
      break;
    case GST_GL_EFFECT_TWIRL:
      filter->effect = (GstGLEffectProcessFunc) gst_gl_effects_twirl;
      break;
    case GST_GL_EFFECT_BULGE:
      filter->effect = (GstGLEffectProcessFunc) gst_gl_effects_bulge;
      break;
    case GST_GL_EFFECT_FISHEYE:
      filter->effect = (GstGLEffectProcessFunc) gst_gl_effects_fisheye;
      break;
    case GST_GL_EFFECT_STRETCH:
      filter->effect = (GstGLEffectProcessFunc) gst_gl_effects_stretch;
      break;
    case GST_GL_EFFECT_SQUARE:
      filter->effect = (GstGLEffectProcessFunc) gst_gl_effects_square;
      break;
    case GST_GL_EFFECT_MIRROR:
      filter->effect = (GstGLEffectProcessFunc) gst_gl_effects_mirror;
      break;
    case GST_GL_EFFECT_HEAT:
      filter->effect = (GstGLEffectProcessFunc) gst_gl_effects_heat;
      break;
    case GST_GL_EFFECT_CROSS:
      filter->effect = (GstGLEffectProcessFunc) gst_gl_effects_cross;
      break;
    case GST_GL_EFFECT_TEST:
      filter->effect = (GstGLEffectProcessFunc) gst_gl_effects_test;
      break;
    default:
      g_assert_not_reached ();
  }

  GST_ERROR ("setting effect_changed");

  filter->effect_changed = TRUE;
  filter->current_effect = effect_type;
}

static void
gst_gl_effects_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGLEffects *effects = GST_GL_EFFECTS (object);

  switch (prop_id) {
    case PROP_EFFECT:
      gst_gl_effects_set_effect (effects, g_value_get_enum (value));
      break;
    case PROP_MIRROR:
      effects->is_mirrored = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_effects_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  //GstGLEffects *filter = GST_GL_EFFECTS (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gl_effects_draw_texture (GstGLEffects * effects, GLuint tex)
{
  GstGLFilter *filter = GST_GL_FILTER (effects);

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
gst_gl_effects_set_target (GstGLEffects * effects, GLuint tex)
{
  glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, effects->fbo);

  glFramebufferTexture2DEXT (GL_FRAMEBUFFER_EXT,
      GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_RECTANGLE_ARB, tex, 0);

  gst_gl_shader_use (NULL);

  g_assert (glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT) ==
      GL_FRAMEBUFFER_COMPLETE_EXT);

  glDrawBuffer (GL_COLOR_ATTACHMENT0_EXT);
  glReadBuffer (GL_COLOR_ATTACHMENT0_EXT);
}

GLuint
gst_gl_effects_prepare_texture (GLint width, GLint height)
{
  GLuint tex;

  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glGenTextures (1, &tex);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, tex);
  glTexImage2D (GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA,
      width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  return tex;
}

/* override GstGLFilter Transform function, I need to do it to control
 * when to render to the outbuf texture and add multiple rendering
 * passes in the middle. This is the most unintrusive way I found, any
 * other idea? */
static GstFlowReturn
gst_gl_effects_transform (GstBaseTransform * bt, GstBuffer * bt_inbuf,
    GstBuffer * bt_outbuf)
{
  GstGLFilter *filter;
  GstGLEffects *effects;
  GstGLBuffer *inbuf = GST_GL_BUFFER (bt_inbuf);
  GstGLBuffer *outbuf = GST_GL_BUFFER (bt_outbuf);
  GstGLDisplay *display = inbuf->display;
  gint i;

  filter = GST_GL_FILTER (bt);
  effects = GST_GL_EFFECTS (filter);

  effects->intexture = inbuf->texture;
  effects->outtexture = outbuf->texture;

  gst_gl_display_lock (display);

  if (!effects->fbo) {
    glGenFramebuffersEXT (1, &effects->fbo);
    for (i = 0; i < 10; i++) {
      effects->midtexture[i] =
          gst_gl_effects_prepare_texture (filter->width, filter->height);
    }
    glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER,
        GL_LINEAR);
    glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  }

  glViewport (0, 0, filter->width, filter->height);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  const double mirrormat[16] = {
    -1.0, 0.0, 0.0, 0.0,
    0.0, 1.0, 0.0, 0.0,
    0.0, 0.0, 1.0, 0.0,
    0.0, 0.0, 0.0, 1.0
  };

  if (effects->is_mirrored)
    glLoadMatrixd (mirrormat);
  else
    glLoadIdentity ();

  effects->effect (effects);    // ugly.. maybe changing plugin name to GstGLEffectFactory?

  gst_gl_display_unlock (display);

  return GST_FLOW_OK;
}
