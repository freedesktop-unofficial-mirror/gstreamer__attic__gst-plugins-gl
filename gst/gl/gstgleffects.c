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
#include <gdk-pixbuf/gdk-pixbuf.h>

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
  GST_GL_EFFECT_SEPIA,
  GST_GL_EFFECT_CROSS,
  GST_GL_EFFECT_GLOW,
  GST_GL_EFFECT_EMBOSS,
  GST_GL_EFFECT_BACKGROUND,
  GST_GL_EFFECT_TEST,
  GST_GL_N_EFFECTS
} GstGLEffectsEffect;

typedef void (*GstGLEffectProcessFunc) (GstGLEffects * filter);

struct _GstGLEffects
{
  GstGLFilter filter;

  /* < private? > */

  GstGLEffectProcessFunc effect;
  gint current_effect;

  gboolean swap_horiz;

  char *bgfilename;
  gboolean bg_has_changed;

  GLuint intexture;
  GLuint midtexture[10];
  GLuint savedbgtexture;
  GLuint newbgtexture;
  GLuint outtexture;
  GLuint fbo;
};

/* TODO ? */
struct _GstGLEffectContainer
{
  gint idx;
  GstGLShader *shader;          //shader to apply
  GLuint texture;               //texture to render to
};

GHashTable *shaderstable;

const double mirrormatrix[16] = {
  -1.0, 0.0, 0.0, 0.0,
  0.0, 1.0, 0.0, 0.0,
  0.0, 0.0, 1.0, 0.0,
  0.0, 0.0, 0.0, 1.0
};

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
    {GST_GL_EFFECT_SEPIA, "Sepia Tone Effect", "sepia"},
    {GST_GL_EFFECT_CROSS, "Cross Processing Effect", "cross"},
    {GST_GL_EFFECT_GLOW, "Glow Lighting Effect", "glow"},
    {GST_GL_EFFECT_EMBOSS, "Emboss Convolution Effect", "emboss"},
    {GST_GL_EFFECT_BACKGROUND, "Difference Matte Effect", "background"},
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
  PROP_HSWAP,
  PROP_BG,
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

static GLuint gst_gl_effects_prepare_texture (GLint width, GLint height);
static GstFlowReturn gst_gl_effects_transform (GstBaseTransform * bt,
    GstBuffer * inbuf, GstBuffer * outbuf);
static void gst_gl_effects_identity (GstGLEffects * effects);

static void
gst_gl_effects_reset_resources (GstGLEffects * effects)
{
  GLenum err;
  gint i;

  g_message ("deleting textures");

  for (i = 0; i < 10; i++) {
    glDeleteTextures (1, &effects->midtexture[i]);
  }

  glDeleteTextures (1, &effects->savedbgtexture);
  glDeleteTextures (1, &effects->newbgtexture);

  g_message ("shaderstable unref");
  g_hash_table_unref (shaderstable);

  g_message ("deleting fbo");
  glDeleteFramebuffersEXT (1, &effects->fbo);
  err = glGetError ();
  if (err)
    g_warning (G_STRLOC "error: 0x%x", err);

  effects->fbo = 0;
}

static gboolean
gst_gl_effects_stop (GstGLFilter * filter)
{
  GstGLEffects *effects;

  effects = GST_GL_EFFECTS (filter);

  g_message ("stop!");

  if (filter->display) {
    gst_gl_display_lock (filter->display);
    gst_gl_effects_reset_resources (effects);
    gst_gl_display_unlock (filter->display);
  }

  return TRUE;
}

static void
gst_gl_effects_init_resources (GstGLEffects * effects)
{
  GstGLFilter *filter = GST_GL_FILTER (effects);
  gint i;

  if (effects->fbo) {
    /* fbo already exists (see FIXME in the start func) */
    return;
  }

  g_message ("creating fbo");
  glGenFramebuffersEXT (1, &effects->fbo);
  for (i = 0; i < 10; i++) {
    effects->midtexture[i] =
        gst_gl_effects_prepare_texture (filter->width, filter->height);
  }
  effects->savedbgtexture =
      gst_gl_effects_prepare_texture (filter->width, filter->height);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_WRAP_T, GL_CLAMP);
  glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  g_message ("creating shader table");
  shaderstable = g_hash_table_new_full (g_str_hash,
      g_str_equal, NULL, g_object_unref);
}

static gboolean
gst_gl_effects_start (GstGLFilter * filter)
{
  GstGLEffects *effects = GST_GL_EFFECTS (filter);
  effects->bg_has_changed = FALSE;

  g_message ("start!");

  /* FIXME: display is retrieved from inbuf in prepare_output_buffer
   * so does not exist yet here and I have to do first allocation in
   * the transform function */

  /* if (filter->display) { /\* this should never be true *\/ */
  /*   g_message ("found display"); */
  /*   gst_gl_display_lock (filter->display); */
  /*   gst_gl_effects_init_resources (effects); */
  /*   gst_gl_display_unlock (filter->display); */
  /* } */

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
      PROP_HSWAP,
      g_param_spec_boolean ("horizontal-swap",
          "Reflect horizontally",
          "Reflects images horizontally, usefull with webcams",
          FALSE, G_PARAM_READWRITE));
  g_object_class_install_property (gobject_class,
      PROP_BG,
      g_param_spec_string ("background",
          "Save background", "Save background", NULL, G_PARAM_READWRITE));

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
  GstGLShader *shader;
  gfloat tex_size[2];

  shader = g_hash_table_lookup (shaderstable, "mirror0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "mirror0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader,
          mirror_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
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
  GstGLShader *shader;
  gfloat tex_size[2];

  shader = g_hash_table_lookup (shaderstable, "tunnel0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "tunnel0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader,
          tunnel_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
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
  GstGLShader *shader;
  gfloat tex_size[2];

  shader = g_hash_table_lookup (shaderstable, "twirl0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "twirl0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader,
          twirl_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
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
  GstGLShader *shader;
  gfloat tex_size[2];

  shader = g_hash_table_lookup (shaderstable, "bulge0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "bulge0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader,
          bulge_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
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
  GstGLShader *shader;
  gfloat tex_size[2];

  shader = g_hash_table_lookup (shaderstable, "stretch0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "stretch0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader,
          stretch_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
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
  GstGLShader *shader;
  gfloat tex_size[2];

  shader = g_hash_table_lookup (shaderstable, "squeeze0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "squeeze0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader,
          squeeze_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
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
  GstGLShader *shader;
  gfloat tex_size[2];

  shader = g_hash_table_lookup (shaderstable, "fisheye0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "fisheye0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader,
          fisheye_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
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
  GstGLShader *shader;
  gfloat tex_size[2];

  shader = g_hash_table_lookup (shaderstable, "square0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "square0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader,
          square_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
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

  g_return_if_fail (gst_gl_shader_compile_and_check (shader0,
          test_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
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

  g_return_if_fail (gst_gl_shader_compile_and_check (shader1,
          convolution_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
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

  g_return_if_fail (gst_gl_shader_compile_and_check (shader2,
          blend_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
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
gst_gl_effects_luma_to_curve (GstGLEffects * effects,
    GstGLEffectsTexture1D curve)
{
  GstGLShader *shader;
  GLuint curve_texture;

  shader = g_hash_table_lookup (shaderstable, "lumamap0");

  if (!shader) {
    shader = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "lumamap0", shader);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader,
          luma_to_curve_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
  gst_gl_shader_use (shader);

  glGenTextures (1, &curve_texture);
  glEnable (GL_TEXTURE_1D);
  glBindTexture (GL_TEXTURE_1D, curve_texture);

  glActiveTexture (GL_TEXTURE5);
  gst_gl_shader_set_uniform_1i (shader, "curve", 5);

  /* this parameters are needed to have a right, predictable, mapping */

  glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP);
  glTexParameteri (GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP);

  /* FIXME: load pixels only once */
  glTexImage1D (GL_TEXTURE_1D, 0, curve.bytes_per_pixel,
      curve.width, 0, GL_RGB, GL_UNSIGNED_BYTE, curve.pixel_data);

  glDisable (GL_TEXTURE_1D);
  glDeleteTextures (1, &curve_texture);

  gst_gl_effects_draw_texture (effects, effects->intexture);
}

static void
gst_gl_effects_heat (GstGLEffects * effects)
{
  gst_gl_effects_luma_to_curve (effects, heat_curve);
}

static void
gst_gl_effects_cross (GstGLEffects * effects)
{
  gst_gl_effects_luma_to_curve (effects, cross_curve);
}

static void
gst_gl_effects_sepia (GstGLEffects * effects)
{
  gst_gl_effects_luma_to_curve (effects, sepia_curve);
}

static void
gst_gl_effects_glow (GstGLEffects * effects)
{
  GstGLShader *shader0;
  GstGLShader *shader1;
  GstGLShader *shader2;
  GstGLShader *shader3;
//  GstGLFilter *filter = GST_GL_FILTER (effects);
  /* gfloat kernel[9] = {  0.0, -1.0,  0.0, */
  /*                   -1.0,  4.0, -1.0, */
  /*                    0.0,  -1.0,  0.0 }; */
  /* gfloat kernel[9] = {  1.0,  2.0,  1.0, */
  /*                    2.0,  4.0,  2.0, */
  /*                    1.0,  2.0,  1.0 }; */


  gfloat k1[9] = { 0.060493, 0.075284, 0.088016,
    0.096667, 0.099736, 0.096667,
    0.088016, 0.075284, 0.060493
  };

/*  gfloat gauss2[25] = { 1.0,  2.0,  4.0,  2.0, 1.0,
			2.0,  4.0, 16.0,  4.0, 2.0,
			4.0, 16.0, 64.0, 16.0, 4.0,
			2.0,  4.0, 16.0,  4.0, 2.0,
			1.0,  2.0,  4.0,  2.0, 1.0 };
*/

  shader0 = g_hash_table_lookup (shaderstable, "glow0");
  shader1 = g_hash_table_lookup (shaderstable, "glow1");
  shader2 = g_hash_table_lookup (shaderstable, "glow2");
  shader3 = g_hash_table_lookup (shaderstable, "glow3");

  if (!shader0) {
    shader0 = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "glow0", shader0);
  }
  if (!shader1) {
    shader1 = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "glow1", shader1);
  }
  if (!shader2) {
    shader2 = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "glow2", shader2);
  }
  if (!shader3) {
    shader3 = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "glow3", shader3);
  }

  gst_gl_effects_set_target (effects, effects->midtexture[0]);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader0,
          luma_threshold_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
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

  gst_gl_effects_set_target (effects, effects->midtexture[1]);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader1,
          hconv9_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
  gst_gl_shader_use (shader1);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->midtexture[0]);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader1, "tex", 0);

  gst_gl_shader_set_uniform_1fv (shader1, "kernel", 9, k1);
  gst_gl_shader_set_uniform_1f (shader1, "norm_const", 0.740656);
  gst_gl_shader_set_uniform_1f (shader1, "norm_offset", 0.0);

  gst_gl_effects_draw_texture (effects, effects->midtexture[0]);

  gst_gl_effects_set_target (effects, effects->midtexture[2]);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader2,
          vconv9_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
  gst_gl_shader_use (shader2);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->midtexture[1]);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader2, "tex", 0);

  gst_gl_shader_set_uniform_1fv (shader2, "kernel", 9, k1);
  gst_gl_shader_set_uniform_1f (shader2, "norm_const", 0.740656);
  gst_gl_shader_set_uniform_1f (shader2, "norm_offset", 0.0);

  gst_gl_effects_draw_texture (effects, effects->midtexture[1]);

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader3,
          blend_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
  gst_gl_shader_use (shader3);

  glActiveTexture (GL_TEXTURE2);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader3, "base", 2);

  glActiveTexture (GL_TEXTURE1);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->midtexture[2]);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader3, "blend", 1);

  gst_gl_effects_draw_texture (effects, effects->midtexture[2]);
}

static void
gst_gl_effects_background (GstGLEffects * effects)
{
  GstGLShader *shader0;
  GstGLShader *shader1;
  GstGLShader *shader2;
  GstGLShader *shader3;

//  gfloat gauss_kernel[5] = { 0.120985, 0.176033, 0.199471, 0.176033, 0.120985 };

  /* Denoise with gaussian blur, would nonlinear diffusion be better
   * to preserve edges? */
  gfloat gauss_kernel[9] = { 0.026995, 0.064759, 0.120985,
    0.176033, 0.199471, 0.176033,
    0.120985, 0.064759, 0.026995
  };
  /* std = 2.0, sum = 0.977016 */

  shader0 = g_hash_table_lookup (shaderstable, "background0");
  if (!shader0) {
    shader0 = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "background0", shader0);
  }
  shader1 = g_hash_table_lookup (shaderstable, "background1");
  if (!shader1) {
    shader1 = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "background1", shader1);
  }
  shader2 = g_hash_table_lookup (shaderstable, "background2");
  if (!shader2) {
    shader2 = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "background2", shader2);
  }
  shader3 = g_hash_table_lookup (shaderstable, "background3");
  if (!shader3) {
    shader3 = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "background3", shader3);
  }

  /* t: midtexture[0] i: intexture */
  gst_gl_effects_set_target (effects, effects->midtexture[0]);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader0,
          difference_matte_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
  gst_gl_shader_use (shader0);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader0, "front", 0);

  glActiveTexture (GL_TEXTURE1);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->savedbgtexture);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader0, "back", 1);

  glActiveTexture (GL_TEXTURE2);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->newbgtexture);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader0, "new", 2);

  gst_gl_effects_draw_texture (effects, effects->intexture);

  /* t: midtexture[1] i: midtexture[0] */
  gst_gl_effects_set_target (effects, effects->midtexture[1]);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader1,
          vconv9_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
  gst_gl_shader_use (shader1);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->midtexture[0]);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader1, "tex", 0);

  gst_gl_shader_set_uniform_1fv (shader1, "kernel", 9, gauss_kernel);
  gst_gl_shader_set_uniform_1f (shader1, "norm_const", 0.977016);
  gst_gl_shader_set_uniform_1f (shader1, "norm_offset", 0.0);

  gst_gl_effects_draw_texture (effects, effects->midtexture[0]);

  /* t: midtexture[2] i: midtexture[1] */
  gst_gl_effects_set_target (effects, effects->midtexture[2]);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader2,
          hconv9_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
  gst_gl_shader_use (shader2);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->midtexture[1]);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader2, "tex", 0);

  gst_gl_shader_set_uniform_1fv (shader2, "kernel", 9, gauss_kernel);
  gst_gl_shader_set_uniform_1f (shader2, "norm_const", 0.977016);       //0.793507
  gst_gl_shader_set_uniform_1f (shader2, "norm_offset", 0.0);

  gst_gl_effects_draw_texture (effects, effects->midtexture[1]);

  /* t: outtexture i: ? */
  gst_gl_effects_set_target (effects, effects->outtexture);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader3,
          interpolate_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
  gst_gl_shader_use (shader3);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->midtexture[2]);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader3, "alpha", 0);

  glActiveTexture (GL_TEXTURE1);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader3, "front", 1);

  glActiveTexture (GL_TEXTURE2);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->newbgtexture);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader3, "back", 2);

  gst_gl_effects_draw_texture (effects, effects->midtexture[2]);
}

static void
gst_gl_effects_emboss (GstGLEffects * effects)
{
  GstGLShader *shader0;
  gfloat kernel[9] = { 2.0, 0.0, 0.0,
    0.0, -1.0, 0.0,
    0.0, 0.0, -1.0
  };


  shader0 = g_hash_table_lookup (shaderstable, "emboss0");

  if (!shader0) {
    shader0 = gst_gl_shader_new ();
    g_hash_table_insert (shaderstable, "emboss0", shader0);
  }

  gst_gl_effects_set_target (effects, effects->outtexture);

  g_return_if_fail (gst_gl_shader_compile_and_check (shader0,
          convolution_fragment_source, GST_GL_SHADER_FRAGMENT_SOURCE));
  gst_gl_shader_use (shader0);

  glActiveTexture (GL_TEXTURE0);
  glEnable (GL_TEXTURE_RECTANGLE_ARB);
  glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->intexture);
  glDisable (GL_TEXTURE_RECTANGLE_ARB);

  gst_gl_shader_set_uniform_1i (shader0, "tex", 0);

  gst_gl_shader_set_uniform_1fv (shader0, "kernel", 9, kernel);
  gst_gl_shader_set_uniform_1f (shader0, "norm_const", 1.0);
  gst_gl_shader_set_uniform_1f (shader0, "norm_offset", 0.5);

  gst_gl_effects_draw_texture (effects, effects->intexture);
}

static void
gst_gl_effects_set_effect (GstGLEffects * effects, gint effect_type)
{

  switch (effect_type) {
    case GST_GL_EFFECT_IDENTITY:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_identity;
      break;
    case GST_GL_EFFECT_SQUEEZE:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_squeeze;
      break;
    case GST_GL_EFFECT_TUNNEL:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_tunnel;
      break;
    case GST_GL_EFFECT_TWIRL:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_twirl;
      break;
    case GST_GL_EFFECT_BULGE:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_bulge;
      break;
    case GST_GL_EFFECT_FISHEYE:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_fisheye;
      break;
    case GST_GL_EFFECT_STRETCH:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_stretch;
      break;
    case GST_GL_EFFECT_SQUARE:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_square;
      break;
    case GST_GL_EFFECT_MIRROR:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_mirror;
      break;
    case GST_GL_EFFECT_HEAT:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_heat;
      break;
    case GST_GL_EFFECT_SEPIA:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_sepia;
      break;
    case GST_GL_EFFECT_CROSS:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_cross;
      break;
    case GST_GL_EFFECT_GLOW:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_glow;
      break;
    case GST_GL_EFFECT_EMBOSS:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_emboss;
      break;
    case GST_GL_EFFECT_BACKGROUND:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_background;
      break;
    case GST_GL_EFFECT_TEST:
      effects->effect = (GstGLEffectProcessFunc) gst_gl_effects_test;
      break;
    default:
      g_assert_not_reached ();
  }
  effects->current_effect = effect_type;
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
    case PROP_HSWAP:
      effects->swap_horiz = g_value_get_boolean (value);
      break;
    case PROP_BG:
      g_free (effects->bgfilename);
      effects->bg_has_changed = TRUE;
      effects->bgfilename = g_value_dup_string (value);
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
  GstGLEffects *effects = GST_GL_EFFECTS (object);

  switch (prop_id) {
    case PROP_EFFECT:
      g_value_set_enum (value, effects->current_effect);
      break;
    case PROP_HSWAP:
      g_value_set_boolean (value, effects->swap_horiz);
      break;
    case PROP_BG:
      g_value_set_string (value, effects->bgfilename);
      break;
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

  if ((tex == effects->outtexture) && (effects->swap_horiz)) {
    glMatrixMode (GL_MODELVIEW);
    glLoadMatrixd (mirrormatrix);
  }

  gst_gl_shader_use (NULL);

  g_assert (glCheckFramebufferStatusEXT (GL_FRAMEBUFFER_EXT) ==
      GL_FRAMEBUFFER_COMPLETE_EXT);

  glDrawBuffer (GL_COLOR_ATTACHMENT0_EXT);
  glReadBuffer (GL_COLOR_ATTACHMENT0_EXT);
}

static GLuint
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
  GdkPixbuf *obuf;
  GdkPixbuf *pbuf;

  filter = GST_GL_FILTER (bt);
  effects = GST_GL_EFFECTS (filter);

  effects->intexture = inbuf->texture;
  effects->outtexture = outbuf->texture;

  gst_gl_display_lock (display);

  gst_gl_effects_init_resources (effects);

  glViewport (0, 0, filter->width, filter->height);

  glMatrixMode (GL_PROJECTION);
  glLoadIdentity ();

  glMatrixMode (GL_MODELVIEW);
  glLoadIdentity ();

  /* saves background */
  if (effects->bg_has_changed && effects->bgfilename) {
    g_message ("saving current frame");
    gst_gl_effects_set_target (effects, effects->savedbgtexture);
    gst_gl_effects_draw_texture (effects, effects->intexture);

    obuf = gdk_pixbuf_new_from_file (effects->bgfilename, NULL);
    pbuf = gdk_pixbuf_scale_simple (obuf,
        filter->width, filter->height, GDK_INTERP_BILINEAR);
    gdk_pixbuf_unref (obuf);

    glEnable (GL_TEXTURE_RECTANGLE_ARB);
    glDeleteTextures (1, &effects->newbgtexture);
    glGenTextures (1, &effects->newbgtexture);
    glBindTexture (GL_TEXTURE_RECTANGLE_ARB, effects->newbgtexture);
    glTexImage2D (GL_TEXTURE_RECTANGLE_ARB, 0, GL_RGBA,
        filter->width, filter->height, 0,
        gdk_pixbuf_get_has_alpha (pbuf) ? GL_RGBA : GL_RGB,
        GL_UNSIGNED_BYTE, gdk_pixbuf_get_pixels (pbuf));

    gdk_pixbuf_unref (pbuf);

    effects->bg_has_changed = FALSE;
  }

  effects->effect (effects);    // ugly.. maybe changing plugin name to GstGLEffectFactory?

  gst_gl_display_unlock (display);

  return GST_FLOW_OK;
}
