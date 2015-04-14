/*
 * GStreamer
 * Copyright (C) 2009 Julien Isorce <julien.isorce@gmail.com>
 * Copyright (C) 2009 Andrey Nechypurenko <andreynech@gmail.com>
 * Copyright (C) 2015 Vasilis Liaskovitis <vliaskov@gmail.com>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "qglrenderer.h"
#include "pipeline.h"


Pipeline::Pipeline (GstGLDisplay *display,
    GstGLContext * context, const QString & videoLocation, QObject * parent)
    :
QObject (parent),
m_videoLocation (videoLocation),
m_loop (NULL),
m_bus (NULL),
m_pipeline (NULL)
{
  fprintf(stderr, "%s FramePipeline %p\n", __func__, this);
  this->display = display;
  this->context = context;
  this->renderer = (QGLRenderer*)parent;
  //this->configure ();
}

Pipeline::~Pipeline ()
{
}

static gboolean
myexecuteCallback (gpointer data)
{
  Pipeline *p = (Pipeline*) data;
  g_mutex_lock (&p->app_lock);

  p->notifyNewFrame ();
  //PaintGL ();

  //g_cond_signal (&p->app_cond);
  g_mutex_unlock (&p->app_lock);

  return FALSE;
}

GstSample* Pipeline::getFrame()
{
    GstBuffer *buffer;
    buffer = gst_sample_get_buffer (this->frame);
    fprintf(stderr, "%s framepipeline %p sample %p buffer %p\n", __func__, this, this->frame, buffer);
    return frame;
}

void Pipeline::setFrame(GstSample *sample)
{
    GstBuffer *buffer;
    this->frame = sample;
    buffer = gst_sample_get_buffer (this->frame);
    fprintf(stderr, "%s framepipeline %p sample %p buffer %p\n", __func__, this, this->frame, buffer);
}

static gboolean
on_client_draw (GstElement * glsink, GstGLContext * context, GstSample * sample,
    gpointer data)
{
  Pipeline* pipeline = (Pipeline*) data;
  g_mutex_lock (&pipeline->app_lock);
  //g_cond_wait (&pipeline->app_cond, &pipeline->app_lock);
  pipeline->setFrame(sample);
  //fprintf(stderr, "%s framepipeline %p sample frame %p\n", __func__, pipeline,
    //      pipeline->getFrame());
  //g_idle_add_full (G_PRIORITY_HIGH, myexecuteCallback, data, NULL);
  pipeline->notifyNewFrame ();

  //pipeline->renderer->newFrame();//updateGL ();

  //g_idle_add (myexecuteCallback, data);
  g_cond_wait (&pipeline->app_cond, &pipeline->app_lock);
  g_mutex_unlock (&pipeline->app_lock);

  return TRUE;
}

void
Pipeline::configure ()
{

#ifdef Q_WS_WIN
  m_loop = g_main_loop_new (NULL, FALSE);
#endif

  if (m_videoLocation.isEmpty ()) {
    qDebug ("No video file specified. Using video test source.");
    m_pipeline =
        GST_PIPELINE (gst_parse_launch
        ("videotestsrc ! "
            "video/x-raw, width=640, height=480, "
            "framerate=(fraction)30/1 ! "
            " glimagesink name=glimagesink0", NULL));
  } else {
    QByteArray ba = m_videoLocation.toLocal8Bit ();
    qDebug ("Loading video: %s", ba.data ());
    gchar *pipeline = g_strdup_printf ("filesrc name=f ! "
        "decodebin ! glimagesink name=glimagesink0");
    m_pipeline = GST_PIPELINE (gst_parse_launch (pipeline, NULL));
    GstElement *f = gst_bin_get_by_name (GST_BIN (m_pipeline), "f");
    g_object_set (G_OBJECT (f), "location", ba.data (), NULL);
    gst_object_unref (GST_OBJECT (f));
    g_free (pipeline);
  }

  GstElement *glimagesink = gst_bin_get_by_name (GST_BIN (m_pipeline), "glimagesink0");                           
  g_signal_connect (G_OBJECT (glimagesink), "client-draw",                                          
      G_CALLBACK (on_client_draw), this);                                                           
  gst_object_unref (glimagesink); 

  m_bus = gst_pipeline_get_bus (GST_PIPELINE (m_pipeline));
  gst_bus_add_watch (m_bus, (GstBusFunc) bus_call, this);
  gst_bus_enable_sync_message_emission (m_bus);
  g_signal_connect (m_bus, "sync-message", G_CALLBACK (sync_bus_call), this);
  gst_object_unref (m_bus);

  gst_element_set_state (GST_ELEMENT (this->m_pipeline), GST_STATE_PAUSED);
  GstState state = GST_STATE_PAUSED;
  if (gst_element_get_state (GST_ELEMENT (this->m_pipeline),
          &state, NULL, GST_CLOCK_TIME_NONE)
      != GST_STATE_CHANGE_SUCCESS) {
    qDebug ("failed to pause pipeline");
    return;
  }
}

void
Pipeline::start ()
{
  // set a callback to retrieve the gst gl textures

  GstStateChangeReturn ret =
      gst_element_set_state (GST_ELEMENT (this->m_pipeline), GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    qDebug ("Failed to start up pipeline!");

    /* check if there is an error message with details on the bus */
    GstMessage *msg = gst_bus_poll (this->m_bus, GST_MESSAGE_ERROR, 0);
    if (msg) {
      GError *err = NULL;
      gst_message_parse_error (msg, &err, NULL);
      qDebug ("ERROR: %s", err->message);
      g_error_free (err);
      gst_message_unref (msg);
    }
    return;
  }
#ifdef Q_WS_WIN
  g_main_loop_run (m_loop);
#endif
}

void
Pipeline::stop ()
{
#ifdef Q_WS_WIN
  g_main_loop_quit (m_loop);
#else
  emit stopRequested ();
#endif
}

void
Pipeline::unconfigure ()
{
  gst_element_set_state (GST_ELEMENT (this->m_pipeline), GST_STATE_NULL);

  //GstBuffer *buf;
  //gst_sample_unref (frame);

  gst_object_unref (m_pipeline);
}

gboolean Pipeline::bus_call (GstBus * bus, GstMessage * msg, Pipeline * p)
{
  Q_UNUSED (bus)

      switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      qDebug ("End-of-stream received. Stopping.");
      p->stop ();
      break;

    case GST_MESSAGE_ERROR:
    {
      gchar *
          debug = NULL;
      GError *
          err = NULL;
      gst_message_parse_error (msg, &err, &debug);
      qDebug ("Error: %s", err->message);
      g_error_free (err);
      if (debug) {
        qDebug ("Debug deails: %s", debug);
        g_free (debug);
      }
      p->stop ();
      break;
    }

    default:
      break;
  }

  return TRUE;
}

gboolean Pipeline::sync_bus_call (GstBus * bus, GstMessage * msg, Pipeline * p)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_NEED_CONTEXT:
    {
      const gchar *
          context_type;

      gst_message_parse_context_type (msg, &context_type);
      g_print ("got need context %s\n", context_type);

      if (g_strcmp0 (context_type, GST_GL_DISPLAY_CONTEXT_TYPE) == 0) {
        GstContext *display_context = gst_context_new (GST_GL_DISPLAY_CONTEXT_TYPE, TRUE);
        gst_context_set_gl_display (display_context, p->display);
        gst_element_set_context (GST_ELEMENT (msg->src), display_context);
      } else if (g_strcmp0 (context_type, "gst.gl.app_context") == 0) {
        GstContext *app_context = gst_context_new ("gst.gl.app_context", TRUE);
        GstStructure *s = gst_context_writable_structure (app_context);
        gst_structure_set (s, "context", GST_GL_TYPE_CONTEXT, p->context, NULL);
        gst_element_set_context (GST_ELEMENT (msg->src), app_context);
      }
      break;
    }
    default:
      break;
  }
  return FALSE;
}
