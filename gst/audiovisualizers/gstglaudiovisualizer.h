/* GStreamer
 * Copyright (C) <2014>  <vliaskov@gmail.com>
 *
 * gstglaudiovisualizer.c: base class for opengl visualisation elements
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __GST_GL_AUDIO_VISUALIZER_H__
#define __GST_GL_AUDIO_VISUALIZER_H__

#include <gst/gst.h>
#include <gst/base/gstbasetransform.h>

#include <gst/video/video.h>
#include <gst/audio/audio.h>
#include <gst/base/gstadapter.h>

G_BEGIN_DECLS
#define GST_TYPE_GL_AUDIO_VISUALIZER            (gst_gl_audio_visualizer_get_type())
#define GST_GL_AUDIO_VISUALIZER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GL_AUDIO_VISUALIZER,GstGLAudioVisualizer))
#define GST_GL_AUDIO_VISUALIZER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GL_AUDIO_VISUALIZER,GstGLAudioVisualizerClass))
#define GST_GL_AUDIO_VISUALIZER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),GST_TYPE_GL_AUDIO_VISUALIZER,GstGLAudioVisualizerClass))
#define GST_IS_SYNAESTHESIA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GL_AUDIO_VISUALIZER))
#define GST_IS_SYNAESTHESIA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GL_AUDIO_VISUALIZER))
typedef struct _GstGLAudioVisualizer GstGLAudioVisualizer;
typedef struct _GstGLAudioVisualizerClass GstGLAudioVisualizerClass;
typedef struct _GstGLAudioVisualizerPrivate GstGLAudioVisualizerPrivate;

typedef void (*GstGLAudioVisualizerShaderFunc)(GstGLAudioVisualizer *scope, const GstVideoFrame *s, GstVideoFrame *d);

/**
 * GstGLAudioVisualizerShader:
 * @GST_GL_AUDIO_VISUALIZER_SHADER_NONE: no shading
 * @GST_GL_AUDIO_VISUALIZER_SHADER_FADE: plain fading
 * @GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_UP: fade and move up
 * @GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_DOWN: fade and move down
 * @GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_LEFT: fade and move left
 * @GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_RIGHT: fade and move right
 * @GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_HORIZ_OUT: fade and move horizontally out
 * @GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_HORIZ_IN: fade and move horizontally in
 * @GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_VERT_OUT: fade and move vertically out
 * @GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_VERT_IN: fade and move vertically in
 *
 * Different types of supported background shading functions.
 */
typedef enum {
  GST_GL_AUDIO_VISUALIZER_SHADER_NONE,
  GST_GL_AUDIO_VISUALIZER_SHADER_FADE,
  GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_UP,
  GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_DOWN,
  GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_LEFT,
  GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_RIGHT,
  GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_HORIZ_OUT,
  GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_HORIZ_IN,
  GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_VERT_OUT,
  GST_GL_AUDIO_VISUALIZER_SHADER_FADE_AND_MOVE_VERT_IN
} GstGLAudioVisualizerShader;

struct _GstGLAudioVisualizer
{
  GstAudioVisualizer parent;
  
  GstBufferPool *pool;

  /* GL stuff */
  GstGLDisplay *display;
  GstGLContext *context;
  GLuint fbo;
  GLuint depthbuffer;
  GLuint midtexture;
  GLdouble actor_projection_matrix[16];
  GLdouble actor_modelview_matrix[16];
  GLboolean is_enabled_gl_depth_test;
  GLint gl_depth_func;
  GLboolean is_enabled_gl_blend;
  GLint gl_blend_src_alpha;

};

struct _GstGLAudioVisualizerClass
{
  GstElementClass parent_class;

  /* virtual function, called whenever the format changes */
  gboolean (*setup) (GstGLAudioVisualizer * scope);

  /* virtual function for rendering a frame */
  gboolean (*render) (GstGLAudioVisualizer * scope, GstBuffer * audio, GstVideoFrame * video);

  gboolean (*decide_allocation)   (GstGLAudioVisualizer * scope, GstQuery *query);
};

GType gst_gl_audio_visualizer_get_type (void);

G_END_DECLS
#endif /* __GST_GL_AUDIO_VISUALIZER_H__ */
