#ifndef __GST_GL_H__
#define __GST_GL_H__

#include <GL/glew.h>
#include <GL/freeglut.h>

#include <gst/gst.h>
#include <gst/video/video.h>

#define GST_TYPE_GL_DISPLAY \
      (gst_gl_display_get_type())
#define GST_GL_DISPLAY(obj) \
      (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_DISPLAY,GstGLDisplay))
#define GST_GL_DISPLAY_CLASS(klass) \
      (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GL_DISPLAY,GstGLDisplayClass))
#define GST_IS_GL_DISPLAY(obj) \
      (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_DISPLAY))
#define GST_IS_GL_DISPLAY_CLASS(klass) \
      (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GL_DISPLAY))

typedef struct _GstGLDisplay GstGLDisplay;
typedef struct _GstGLDisplayClass GstGLDisplayClass;

//Message type
typedef enum {
    GST_GL_DISPLAY_ACTION_CREATE,
    GST_GL_DISPLAY_ACTION_DESTROY,
	GST_GL_DISPLAY_ACTION_VISIBLE,
    GST_GL_DISPLAY_ACTION_PREPARE,
    GST_GL_DISPLAY_ACTION_CHANGE,
    GST_GL_DISPLAY_ACTION_CLEAR,
    GST_GL_DISPLAY_ACTION_VIDEO,
    GST_GL_DISPLAY_ACTION_REDISPLAY
	
} GstGLDisplayAction;


//Message to communicate with the glut thread
typedef struct _GstGLDisplayMsg {
    GstGLDisplayAction action;
    gint glutWinId;
    GstGLDisplay* display; 
} GstGLDisplayMsg;


//Texture pool elements
typedef struct _GstGLDisplayTex {
    GLuint texture;
    GLuint texture_u;
    GLuint texture_v;
} GstGLDisplayTex;


//Client callbacks
typedef void (* CRCB) ( GLuint, GLuint );
typedef gboolean (* CDCB) ( GLuint, GLuint, GLuint);

struct _GstGLDisplay {
    GObject object;

    GMutex *mutex;

	GQueue* texturePool;
    
    GCond *cond_make;
    GCond *cond_fill;
    GCond *cond_clear;
    GCond *cond_video;

    GCond *cond_create;
    GCond *cond_destroy;
    gint glutWinId;
    gulong winId;
    GString *title;
    gint win_xpos;
    gint win_ypos;
    gint glcontext_width;
    gint glcontext_height;
    gboolean visible;

    //intput frame buffer object (video -> GL)
    GLuint fbo;
    GLuint depthBuffer;
    GLuint textureFBO;
    GLuint textureFBOWidth;
    GLuint textureFBOHeight;

    //graphic frame buffer object (GL texture -> GL scene)
    GLuint graphicFBO;
    GLuint graphicDepthBuffer;
    GLuint graphicTexture;

    GLuint requestedTexture;
    GLuint requestedTexture_u;
    GLuint requestedTexture_v;
	GstVideoFormat requestedVideo_format;
    GLuint requestedTextureWidth;
    GLuint requestedTextureHeight;
     
    GLuint candidateTexture;
    GLuint candidateTexture_u;
    GLuint candidateTexture_v;
    GstVideoFormat candidateVideo_format;
    GLuint candidateTextureWidth;
    GLuint candidateTextureHeight;
    gpointer candidateData;
    
    GLuint currentTexture;
    GLuint currentTexture_u;
    GLuint currentTexture_v;
    GstVideoFormat currentVideo_format;
    GLuint currentTextureWidth;
    GLuint currentTextureHeight;

    GLuint textureTrash;
    GLuint textureTrash_u;
    GLuint textureTrash_v;

    //output frame buffer object (GL -> video)
    GLuint videoFBO;
    GLuint videoDepthBuffer;
    GLuint videoTexture;
    GLuint videoTexture_u;
    GLuint videoTexture_v;
    gint outputWidth;
    gint outputHeight;
    GstVideoFormat outputVideo_format;
    gpointer outputData;
    GLenum multipleRT[3];

    //from video to texture

	gchar* textFProgram_YUY2_UYVY;
	GLhandleARB GLSLProgram_YUY2;
	GLhandleARB GLSLProgram_UYVY;
	
	gchar* textFProgram_I420_YV12;
	GLhandleARB GLSLProgram_I420_YV12;

	gchar* textFProgram_AYUV;
	GLhandleARB GLSLProgram_AYUV;

    //from texture to video
    
    gchar* textFProgram_to_YUY2_UYVY;
	GLhandleARB GLSLProgram_to_YUY2;
	GLhandleARB GLSLProgram_to_UYVY;

    gchar* textFProgram_to_I420_YV12;
	GLhandleARB GLSLProgram_to_I420_YV12;

    gchar* textFProgram_to_AYUV;
	GLhandleARB GLSLProgram_to_AYUV;
	
    //client callbacks
    CRCB clientReshapeCallback;
    CDCB clientDrawCallback;
};


struct _GstGLDisplayClass {
    GObjectClass object_class;
};

GType gst_gl_display_get_type (void);


//------------------------------------------------------------
//-------------------- Public d�clarations ------------------
//------------------------------------------------------------ 
GstGLDisplay *gst_gl_display_new (void);
void gst_gl_display_initGLContext (GstGLDisplay* display, 
                                   GLint x, GLint y, 
                                   GLint graphic_width, GLint graphic_height,
                                   GLint video_width, GLint video_height,
                                   gulong winId,
                                   gboolean visible);
void gst_gl_display_setClientReshapeCallback (GstGLDisplay* display, CRCB cb);
void gst_gl_display_setClientDrawCallback (GstGLDisplay* display, CDCB cb);
void gst_gl_display_setVisibleWindow (GstGLDisplay* display, gboolean visible);
void gst_gl_display_textureRequested (GstGLDisplay* display, GstVideoFormat format, 
                                      gint width, gint height, guint* texture,
                                      guint* texture_u, guint* texture_v);
void gst_gl_display_textureChanged (GstGLDisplay* display, GstVideoFormat video_format, 
                                    GLuint texture, GLuint texture_u, GLuint texture_v, 
                                    gint width, gint height, gpointer data);
void gst_gl_display_clearTexture (GstGLDisplay* display, guint texture, 
                                  guint texture_u, guint texture_v);

void gst_gl_display_videoChanged (GstGLDisplay* display, GstVideoFormat video_format,
                                  gpointer data);
void gst_gl_display_postRedisplay (GstGLDisplay* display);
void gst_gl_display_set_windowId (GstGLDisplay* display, gulong winId);

#endif