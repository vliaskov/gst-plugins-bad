/* GStreamer
 * Copyright (C) 2011 David Schleef <ds@entropywave.com>
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstdecklinkvideosink.h"
#include <string.h>

GST_DEBUG_CATEGORY_STATIC (gst_decklink_video_sink_debug);
#define GST_CAT_DEFAULT gst_decklink_video_sink_debug

enum
{
  PROP_0,
  PROP_MODE,
  PROP_DEVICE_NUMBER
};

static void gst_decklink_video_sink_set_property (GObject * object,
    guint property_id, const GValue * value, GParamSpec * pspec);
static void gst_decklink_video_sink_get_property (GObject * object,
    guint property_id, GValue * value, GParamSpec * pspec);
static void gst_decklink_video_sink_finalize (GObject * object);

static GstStateChangeReturn
gst_decklink_video_sink_change_state (GstElement * element,
    GstStateChange transition);
static GstClock *gst_decklink_video_sink_provide_clock (GstElement * element);

static GstCaps *gst_decklink_video_sink_get_caps (GstBaseSink * bsink,
    GstCaps * filter);
static GstFlowReturn gst_decklink_video_sink_prepare (GstBaseSink * bsink,
    GstBuffer * buffer);
static GstFlowReturn gst_decklink_video_sink_render (GstBaseSink * bsink,
    GstBuffer * buffer);
static gboolean gst_decklink_video_sink_open (GstBaseSink * bsink);
static gboolean gst_decklink_video_sink_close (GstBaseSink * bsink);
static gboolean gst_decklink_video_sink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query);

#define parent_class gst_decklink_video_sink_parent_class
G_DEFINE_TYPE (GstDecklinkVideoSink, gst_decklink_video_sink,
    GST_TYPE_BASE_SINK);

static void
gst_decklink_video_sink_class_init (GstDecklinkVideoSinkClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstBaseSinkClass *basesink_class = GST_BASE_SINK_CLASS (klass);
  GstCaps *templ_caps;

  gobject_class->set_property = gst_decklink_video_sink_set_property;
  gobject_class->get_property = gst_decklink_video_sink_get_property;
  gobject_class->finalize = gst_decklink_video_sink_finalize;

  element_class->change_state =
      GST_DEBUG_FUNCPTR (gst_decklink_video_sink_change_state);
  element_class->provide_clock =
      GST_DEBUG_FUNCPTR (gst_decklink_video_sink_provide_clock);

  basesink_class->get_caps =
      GST_DEBUG_FUNCPTR (gst_decklink_video_sink_get_caps);
  basesink_class->prepare = GST_DEBUG_FUNCPTR (gst_decklink_video_sink_prepare);
  basesink_class->render = GST_DEBUG_FUNCPTR (gst_decklink_video_sink_render);
  // FIXME: These are misnamed in basesink!
  basesink_class->start = GST_DEBUG_FUNCPTR (gst_decklink_video_sink_open);
  basesink_class->stop = GST_DEBUG_FUNCPTR (gst_decklink_video_sink_close);
  basesink_class->propose_allocation =
      GST_DEBUG_FUNCPTR (gst_decklink_video_sink_propose_allocation);

  g_object_class_install_property (gobject_class, PROP_MODE,
      g_param_spec_enum ("mode", "Playback Mode",
          "Video Mode to use for playback",
          GST_TYPE_DECKLINK_MODE, GST_DECKLINK_MODE_NTSC,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  g_object_class_install_property (gobject_class, PROP_DEVICE_NUMBER,
      g_param_spec_int ("device-number", "Device number",
          "Output device instance to use", 0, G_MAXINT, 0,
          (GParamFlags) (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
              G_PARAM_CONSTRUCT)));

  templ_caps = gst_decklink_mode_get_template_caps ();
  gst_element_class_add_pad_template (element_class,
      gst_pad_template_new ("sink", GST_PAD_SINK, GST_PAD_ALWAYS, templ_caps));
  gst_caps_unref (templ_caps);

  gst_element_class_set_static_metadata (element_class, "Decklink Video Sink",
      "Video/Sink", "Decklink Sink", "David Schleef <ds@entropywave.com>, "
      "Sebastian Dröge <sebastian@centricular.com>");

  GST_DEBUG_CATEGORY_INIT (gst_decklink_video_sink_debug, "decklinkvideosink",
      0, "debug category for decklinkvideosink element");
}

static void
gst_decklink_video_sink_init (GstDecklinkVideoSink * self)
{
  self->mode = GST_DECKLINK_MODE_NTSC;
  self->device_number = 0;
}

void
gst_decklink_video_sink_set_property (GObject * object, guint property_id,
    const GValue * value, GParamSpec * pspec)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (object);

  switch (property_id) {
    case PROP_MODE:
      self->mode = (GstDecklinkModeEnum) g_value_get_enum (value);
      break;
    case PROP_DEVICE_NUMBER:
      self->device_number = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_decklink_video_sink_get_property (GObject * object, guint property_id,
    GValue * value, GParamSpec * pspec)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (object);

  switch (property_id) {
    case PROP_MODE:
      g_value_set_enum (value, self->mode);
      break;
    case PROP_DEVICE_NUMBER:
      g_value_set_int (value, self->device_number);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

void
gst_decklink_video_sink_finalize (GObject * object)
{
  //GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (object);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GstCaps *
gst_decklink_video_sink_get_caps (GstBaseSink * bsink, GstCaps * filter)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (bsink);
  GstCaps *mode_caps, *caps;

  mode_caps = gst_decklink_mode_get_caps (self->mode);
  if (filter) {
    caps =
        gst_caps_intersect_full (filter, mode_caps, GST_CAPS_INTERSECT_FIRST);
    gst_caps_unref (mode_caps);
  } else {
    caps = mode_caps;
  }

  return caps;
}

static GstFlowReturn
gst_decklink_video_sink_render (GstBaseSink * bsink, GstBuffer * buffer)
{
  return GST_FLOW_OK;
}

static GstFlowReturn
gst_decklink_video_sink_prepare (GstBaseSink * bsink, GstBuffer * buffer)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (bsink);
  GstVideoFrame vframe;
  IDeckLinkMutableVideoFrame *frame;
  guint8 *outdata, *indata;
  GstFlowReturn flow_ret;
  HRESULT ret;
  GstClockTime timestamp, duration, running_time, running_time_duration;
  gint i;
  GstClock *clock = NULL, *audio_clock = NULL;

  GST_DEBUG_OBJECT (self, "Preparing buffer %p", buffer);

  // FIXME: Handle no timestamps
  if (!GST_BUFFER_TIMESTAMP_IS_VALID (buffer)) {
    return GST_FLOW_ERROR;
  }

  ret = self->output->output->CreateVideoFrame (self->info.width,
      self->info.height, self->info.stride[0], bmdFormat8BitYUV,
      bmdFrameFlagDefault, &frame);
  if (ret != S_OK) {
    GST_ELEMENT_ERROR (self, STREAM, FAILED,
        (NULL), ("Failed to create video frame: 0x%08x", ret));
    return GST_FLOW_ERROR;
  }

  if (!gst_video_frame_map (&vframe, &self->info, buffer, GST_MAP_READ)) {
    GST_ERROR_OBJECT (self, "Failed to map video frame");
    flow_ret = GST_FLOW_ERROR;
    goto out;
  }

  frame->GetBytes ((void **) &outdata);
  indata = (guint8 *) GST_VIDEO_FRAME_PLANE_DATA (&vframe, 0);
  for (i = 0; i < self->info.height; i++) {
    memcpy (outdata, indata, GST_VIDEO_FRAME_WIDTH (&vframe) * 2);
    indata += GST_VIDEO_FRAME_PLANE_STRIDE (&vframe, 0);
    outdata += frame->GetRowBytes ();
  }
  gst_video_frame_unmap (&vframe);

  timestamp = GST_BUFFER_TIMESTAMP (buffer);
  duration = GST_BUFFER_DURATION (buffer);
  if (duration == GST_CLOCK_TIME_NONE) {
    duration =
        gst_util_uint64_scale_int (GST_SECOND, self->info.fps_d,
        self->info.fps_n);
  }
  running_time =
      gst_segment_to_running_time (&GST_BASE_SINK_CAST (self)->segment,
      GST_FORMAT_TIME, timestamp);
  running_time_duration =
      gst_segment_to_running_time (&GST_BASE_SINK_CAST (self)->segment,
      GST_FORMAT_TIME, timestamp + duration) - running_time;

  clock = gst_element_get_clock (GST_ELEMENT_CAST (self));
  audio_clock = gst_decklink_output_get_audio_clock (self->output);
  if (clock && clock != self->output->clock && clock != audio_clock) {
    // TODO: Adjust time if pipeline clock is not our clock
    //g_assert_not_reached ();
  }

  GST_LOG_OBJECT (self, "Scheduling video frame at %" GST_TIME_FORMAT
      " with duration %" GST_TIME_FORMAT, GST_TIME_ARGS (running_time),
      GST_TIME_ARGS (running_time_duration));

  ret = self->output->output->ScheduleVideoFrame (frame,
      running_time, running_time_duration, GST_SECOND);
  if (ret != S_OK) {
    GST_ELEMENT_ERROR (self, STREAM, FAILED,
        (NULL), ("Failed to schedule frame: 0x%08x", ret));
    flow_ret = GST_FLOW_ERROR;
    goto out;
  }

  flow_ret = GST_FLOW_OK;

out:

  if (clock)
    gst_object_unref (clock);
  if (audio_clock)
    gst_object_unref (audio_clock);

  frame->Release ();

  return flow_ret;
}

static gboolean
gst_decklink_video_sink_open (GstBaseSink * bsink)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (bsink);
  const GstDecklinkMode *mode;
  GstCaps *caps;
  HRESULT ret;

  GST_DEBUG_OBJECT (self, "Starting");

  self->output =
      gst_decklink_acquire_nth_output (self->device_number,
      GST_ELEMENT_CAST (self), FALSE);
  if (!self->output) {
    GST_ERROR_OBJECT (self, "Failed to acquire output");
    return FALSE;
  }

  mode = gst_decklink_get_mode (self->mode);
  g_assert (mode != NULL);

  ret = self->output->output->EnableVideoOutput (mode->mode,
      bmdVideoOutputFlagDefault);
  if (ret != S_OK) {
    GST_WARNING_OBJECT (self, "Failed to enable video output");
    gst_decklink_release_nth_output (self->device_number,
        GST_ELEMENT_CAST (self), FALSE);
    return FALSE;
  }

  g_mutex_lock (&self->output->lock);
  self->output->mode = mode;
  g_mutex_unlock (&self->output->lock);

  caps = gst_decklink_mode_get_caps (self->mode);
  gst_video_info_from_caps (&self->info, caps);
  gst_caps_unref (caps);

  return TRUE;
}

static gboolean
gst_decklink_video_sink_close (GstBaseSink * bsink)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (bsink);

  GST_DEBUG_OBJECT (self, "Stopping");

  if (self->output) {
    g_mutex_lock (&self->output->lock);
    self->output->mode = NULL;
    g_mutex_unlock (&self->output->lock);

    self->output->output->DisableVideoOutput ();
    gst_decklink_release_nth_output (self->device_number,
        GST_ELEMENT_CAST (self), FALSE);
    self->output = NULL;
  }

  return TRUE;
}

static GstStateChangeReturn
gst_decklink_video_sink_change_state (GstElement * element,
    GstStateChange transition)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (element);
  GstStateChangeReturn ret;


  switch (transition) {
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_element_post_message (element,
          gst_message_new_clock_provide (GST_OBJECT_CAST (element),
              self->output->clock, TRUE));
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:{
      GstClock *clock, *audio_clock;

      clock = gst_element_get_clock (GST_ELEMENT_CAST (self));
      audio_clock = gst_decklink_output_get_audio_clock (self->output);
      if (clock && clock != self->output->clock && clock != audio_clock) {
        gst_clock_set_master (self->output->clock, clock);
      }
      if (clock)
        gst_object_unref (clock);
      if (audio_clock)
        gst_object_unref (audio_clock);

      break;
    }
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_element_post_message (element,
          gst_message_new_clock_lost (GST_OBJECT_CAST (element),
              self->output->clock));
      gst_clock_set_master (self->output->clock, NULL);
      break;
    case GST_STATE_CHANGE_PLAYING_TO_PAUSED:{
      GstClockTime start_time = gst_element_get_start_time (element);
      HRESULT res;
      GstClock *clock, *audio_clock;

      // FIXME: This will probably not work
      if (start_time == GST_CLOCK_TIME_NONE)
        start_time = 0;

      clock = gst_element_get_clock (GST_ELEMENT_CAST (self));
      audio_clock = gst_decklink_output_get_audio_clock (self->output);
      if (clock && clock != self->output->clock && clock != audio_clock) {
        // TODO: Adjust time if pipeline clock is not our clock
        //g_assert_not_reached ();
      }
      if (clock)
        gst_object_unref (clock);
      if (audio_clock)
        gst_object_unref (audio_clock);

      // The start time is now the running time when we stopped
      // playback

      GST_DEBUG_OBJECT (self,
          "Stopping scheduled playback at %" GST_TIME_FORMAT,
          GST_TIME_ARGS (start_time));
      res =
          self->output->output->StopScheduledPlayback (start_time, 0,
          GST_SECOND);
      if (res != S_OK) {
        GST_ELEMENT_ERROR (self, STREAM, FAILED,
            (NULL), ("Failed to stop scheduled playback: 0x%08x", res));
        ret = GST_STATE_CHANGE_FAILURE;
      }
      break;
    }
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:{
      GstClockTime start_time = gst_element_get_start_time (element);
      HRESULT res;
      GstClock *clock, *audio_clock;

      // FIXME: This will probably not work
      if (start_time == GST_CLOCK_TIME_NONE)
        start_time = 0;

      clock = gst_element_get_clock (GST_ELEMENT_CAST (self));
      audio_clock = gst_decklink_output_get_audio_clock (self->output);
      if (clock && clock != self->output->clock && clock != audio_clock) {
        // TODO: Adjust time if pipeline clock is not our clock
        //g_assert_not_reached ();
      }
      if (clock)
        gst_object_unref (clock);
      if (audio_clock)
        gst_object_unref (audio_clock);

      GST_DEBUG_OBJECT (self,
          "Starting scheduled playback at %" GST_TIME_FORMAT,
          GST_TIME_ARGS (start_time));

      res =
          self->output->output->StartScheduledPlayback (start_time,
          GST_SECOND, 1.0);
      if (res != S_OK) {
        GST_ELEMENT_ERROR (self, STREAM, FAILED,
            (NULL), ("Failed to start scheduled playback: 0x%08x", res));
        ret = GST_STATE_CHANGE_FAILURE;
      }
      break;
    }
    default:
      break;
  }

  return ret;
}

static GstClock *
gst_decklink_video_sink_provide_clock (GstElement * element)
{
  GstDecklinkVideoSink *self = GST_DECKLINK_VIDEO_SINK_CAST (element);

  if (!self->output)
    return NULL;

  return GST_CLOCK_CAST (gst_object_ref (self->output->clock));
}

static gboolean
gst_decklink_video_sink_propose_allocation (GstBaseSink * bsink,
    GstQuery * query)
{
  GstCaps *caps;
  GstVideoInfo info;
  GstBufferPool *pool;
  guint size;

  gst_query_parse_allocation (query, &caps, NULL);

  if (caps == NULL)
    return FALSE;

  if (!gst_video_info_from_caps (&info, caps))
    return FALSE;

  size = GST_VIDEO_INFO_SIZE (&info);

  if (gst_query_get_n_allocation_pools (query) == 0) {
    GstStructure *structure;
    GstAllocator *allocator = NULL;
    GstAllocationParams params = { (GstMemoryFlags) 0, 15, 0, 0 };

    if (gst_query_get_n_allocation_params (query) > 0)
      gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
    else
      gst_query_add_allocation_param (query, allocator, &params);

    pool = gst_video_buffer_pool_new ();

    structure = gst_buffer_pool_get_config (pool);
    gst_buffer_pool_config_set_params (structure, caps, size, 0, 0);
    gst_buffer_pool_config_set_allocator (structure, allocator, &params);

    if (allocator)
      gst_object_unref (allocator);

    if (!gst_buffer_pool_set_config (pool, structure))
      goto config_failed;

    gst_query_add_allocation_pool (query, pool, size, 0, 0);
    gst_object_unref (pool);
    gst_query_add_allocation_meta (query, GST_VIDEO_META_API_TYPE, NULL);
  }

  return TRUE;
  // ERRORS
config_failed:
  {
    GST_ERROR_OBJECT (bsink, "failed to set config");
    gst_object_unref (pool);
    return FALSE;
  }
}
