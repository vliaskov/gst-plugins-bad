
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include "gstglaudiovisualizer.h"

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE_WITH_FEATURES
        (GST_CAPS_FEATURE_MEMORY_GL_MEMORY,
            "RGBA") "; "
            GST_VIDEO_CAPS_MAKE (GST_GL_COLOR_CONVERT_FORMATS))
    );

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "width = (int) 16, "
        "depth = (int) 16, "
        "endianness = (int) BYTE_ORDER, "
        "signed = (boolean) TRUE, " "channels = (int) { 1, 2 }, "
        "rate = (int) { 8000, 11250, 22500, 32000, 44100, 48000, 96000 }")
    );

GST_DEBUG_CATEGORY_STATIC (gl_audio_visualizer_debug);
#define GST_CAT_DEFAULT gl_audio_visualizer_debug

#define GST_TYPE_GL_AUDIO_VISUALIZER_STYLE (gst_gl_audio_visualizer_get_type ())

/*GType
gst_gl_audio_visualizer_get_type (void)
{
  static GType gtype = 0;

  if (gtype == 0) {
    static const GEnumValue values[] = {
      //{STYLE_DOTS, "draw dots (default)", "dots"},
      {0, NULL, NULL}
    };

    gtype = g_enum_register_static ("GstGLAudioVisualizerStyle", values);
  }
  return gtype;
}*/



/*static GstStateChangeReturn gst_visual_gl_change_state (GstElement * element,
    GstStateChange transition);
static gboolean gst_visual_gl_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static gboolean gst_visual_gl_src_event (GstPad * pad, GstObject * parent, GstEvent * event);
static gboolean gst_visual_gl_src_query (GstPad * pad, GstObject * parent, GstQuery * query);
static gboolean gst_visual_gl_sink_setcaps (GstPad * pad, GstObject *parent, GstCaps * caps);
static gboolean gst_visual_gl_src_setcaps (GstPad * pad, GstObject *parent, GstCaps * caps);
static GstCaps *gst_visual_gl_getcaps (GstPad * pad);*/

static gboolean
gst_gl_audio_visualizer_render (GstAudioVisualizer * base, GstBuffer * audio,
    GstVideoFrame * video);
static gboolean
gst_gl_audio_visualizer_decide_allocation (GstAudioVisualizer * scope,
GstQuery * query);
static void
gst_gl_audio_visualizer_finalize (GObject * object);


G_DEFINE_TYPE (GstGLAudioVisualizer, gst_gl_audio_visualizer, GST_TYPE_AUDIO_VISUALIZER);

static void
gst_gl_audio_visualizer_class_init (GstGLAudioVisualizerClass * g_class)
{
  GObjectClass *gobject_class = (GObjectClass *) g_class;
  GstElementClass *element_class = (GstElementClass *) g_class;
  GstAudioVisualizerClass *gl_visualizer_class = (GstAudioVisualizerClass *) g_class;

  gst_element_class_set_static_metadata (element_class, "GL visualizer",
      "GL Visualization",
      "GL visualizer", "Vasilis Liaskovitis <vliaskov@gmail.com>");

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&src_template));
  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&sink_template));

  //gobject_class->set_property = gst_gl_audio_visualizer_set_property;
  //gobject_class->get_property = gst_gl_audio_visualizer_get_property;

  gl_visualizer_class->render = GST_DEBUG_FUNCPTR (gst_gl_audio_visualizer_render);
  gl_visualizer_class->decide_allocation = GST_DEBUG_FUNCPTR (gst_gl_audio_visualizer_decide_allocation);
  gobject_class->finalize = gst_gl_audio_visualizer_finalize;

  /*g_object_class_install_property (gobject_class, PROP_STYLE,
      g_param_spec_enum ("style", "drawing style",
          "Drawing styles for the space scope display.",
          GST_TYPE_GL_AUDIO_VISUALIZER_STYLE, STYLE_DOTS,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));*/
}

static void
gst_gl_audio_visualizer_init (GstGLAudioVisualizer * scope)
{
  /* do nothing */
}

static void
gst_gl_audio_visualizer_finalize (GObject * object)
{
}

static void
check_gl_matrix (void)
{
  GLdouble projection_matrix[16];
  GLdouble modelview_matrix[16];
  gint i = 0;
  gint j = 0;

  glGetDoublev (GL_PROJECTION_MATRIX, projection_matrix);
  glGetDoublev (GL_MODELVIEW_MATRIX, modelview_matrix);

  for (j = 0; j < 4; ++j) {
    for (i = 0; i < 4; ++i) {
      if (projection_matrix[i + 4 * j] != projection_matrix[i + 4 * j])
        g_warning ("invalid projection matrix at coordiante %dx%d: %f\n", i, j,
            projection_matrix[i + 4 * j]);
      if (modelview_matrix[i + 4 * j] != modelview_matrix[i + 4 * j])
        g_warning ("invalid modelview_matrix matrix at coordiante %dx%d: %f\n",
            i, j, modelview_matrix[i + 4 * j]);
    }
  }
}

static void
actor_setup (GstGLContext * context, GstGLAudioVisualizer * visual)
{
  GstGLFuncs *gl = visual->context->gl_vtable;
  GST_DEBUG_OBJECT (visual, "%s called\n", __func__);
  /* save and clear top of the stack */
  gl->PushAttrib (GL_ALL_ATTRIB_BITS);

  gl->MatrixMode (GL_PROJECTION);
  gl->PushMatrix ();
  gl->LoadIdentity ();

  gl->MatrixMode (GL_MODELVIEW);
  gl->PushMatrix ();
  gl->LoadIdentity ();

  visual->actor_setup_result = visual_actor_realize (visual->actor);
  if (visual->actor_setup_result == 0) {
    /* store the actor's matrices for rendering the first frame */
    glGetDoublev (GL_MODELVIEW_MATRIX, visual->actor_modelview_matrix);
    glGetDoublev (GL_PROJECTION_MATRIX, visual->actor_projection_matrix);

    visual->is_enabled_gl_depth_test = glIsEnabled (GL_DEPTH_TEST);
    gl->GetIntegerv (GL_DEPTH_FUNC, &visual->gl_depth_func);

    visual->is_enabled_gl_blend = glIsEnabled (GL_BLEND);
    gl->GetIntegerv (GL_BLEND_SRC_ALPHA, &visual->gl_blend_src_alpha);

    /* retore matrix */
    gl->MatrixMode (GL_PROJECTION);
    gl->PopMatrix ();

    gl->MatrixMode (GL_MODELVIEW);
    gl->PopMatrix ();

    gl->PopAttrib ();
  }
  /*
  gst_gl_context_use_fbo_v2 (visual->context,
      visual->width, visual->height, visual->fbo, visual->depthbuffer,
      visual->midtexture, (GLCB_V2) gst_gl_audio_visualizer_render_frame, (gpointer *) visual);
  */
}

static void
actor_negotiate (GstGLContext * context, GstGLAudioVisualizer * visual)
{
  gint err = VISUAL_OK;
  GstAudioVisualizer *scope = GST_AUDIO_VISUALIZER (visual);

  err = visual_video_set_depth (visual->video, VISUAL_VIDEO_DEPTH_GL);
  if (err != VISUAL_OK)
    g_warning ("failed to visual_video_set_depth\n");

  err =
      visual_video_set_dimension (visual->video, GST_VIDEO_INFO_WIDTH
          (&scope->vinfo), GST_VIDEO_INFO_HEIGHT (&scope->vinfo) );
  if (err != VISUAL_OK)
    g_warning ("failed to visual_video_set_dimension\n");

  err = visual_actor_video_negotiate (visual->actor, 0, FALSE, FALSE);
  if (err != VISUAL_OK)
    g_warning ("failed to visual_actor_video_negotiate\n");
}

/*
static void gst_gl_audio_visualizer_render_frame (gpointer stuff)
{
  GstGLAudioVisualizer *visual = GST_GL_AUDIO_VISUALIZER (stuff);
  const guint16 *data;
  VisBuffer *lbuf, *rbuf;
  guint16 ldata[VISUAL_SAMPLES], rdata[VISUAL_SAMPLES];

  data = visual->amap.data;
  lbuf = visual_buffer_new_with_buffer (ldata, sizeof (ldata), NULL);
  rbuf = visual_buffer_new_with_buffer (rdata, sizeof (rdata), NULL);

  if (visual->channels == 2) {
    for (i = 0; i < VISUAL_SAMPLES; i++) {
      ldata[i] = *data++;
      rdata[i] = *data++;
    }
  } else {
    for (i = 0; i < VISUAL_SAMPLES; i++) {
      ldata[i] = *data;
      rdata[i] = *data++;
    }
  }

  visual_audio_samplepool_input_channel (visual->audio->samplepool,
      lbuf, visual->libvisual_rate, VISUAL_AUDIO_SAMPLE_FORMAT_S16,
      VISUAL_AUDIO_CHANNEL_LEFT);
  visual_audio_samplepool_input_channel (visual->audio->samplepool,
      rbuf, visual->libvisual_rate, VISUAL_AUDIO_SAMPLE_FORMAT_S16,
      VISUAL_AUDIO_CHANNEL_RIGHT);

  visual_object_unref (VISUAL_OBJECT (lbuf));
  visual_object_unref (VISUAL_OBJECT (rbuf));

  visual_audio_analyze (visual->audio);

  // apply the matrices that the actor set up 
  glPushAttrib (GL_ALL_ATTRIB_BITS);

  glMatrixMode (GL_PROJECTION);
  glPushMatrix ();
  glLoadMatrixd (visual->actor_projection_matrix);

  glMatrixMode (GL_MODELVIEW);
  glPushMatrix ();
  glLoadMatrixd (visual->actor_modelview_matrix);

  //name = gst_element_get_name (GST_ELEMENT (visual));
  //if (g_ascii_strncasecmp (name, "visualglprojectm", 16) == 0
    //  && !HAVE_PROJECTM_TAKING_CARE_OF_EXTERNAL_FBO)
    //glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0);
  //g_free (name);

  actor_negotiate (visual->context, visual);

  if (visual->is_enabled_gl_depth_test) {
    glEnable (GL_DEPTH_TEST);
    glDepthFunc (visual->gl_depth_func);
  }

  if (visual->is_enabled_gl_blend) {
    glEnable (GL_BLEND);
    glBlendFunc (visual->gl_blend_src_alpha, GL_ZERO);
  }

  visual_actor_run (visual->actor, visual->audio);

  check_gl_matrix ();

  glMatrixMode (GL_PROJECTION);
  glPopMatrix ();

  glMatrixMode (GL_MODELVIEW);
  glPopMatrix ();

  glPopAttrib ();

  glDisable (GL_DEPTH_TEST);
  glDisable (GL_BLEND);

  //glDisable (GL_LIGHT0);
    // glDisable (GL_LIGHTING);
    // glDisable (GL_POLYGON_OFFSET_FILL);
   //  glDisable (GL_COLOR_MATERIAL);
    // glDisable (GL_CULL_FACE); 

  GST_DEBUG_OBJECT (visual, "rendered one frame");
}*/

static gboolean
gst_gl_audio_visualizer_render (GstAudioVisualizer * base, GstBuffer * audio,
    GstVideoFrame * video)
{
  GstGLAudioVisualizer *visual = GST_GL_AUDIO_VISUALIZER (base);
  GstGLFuncs *gl = visual->context->gl_vtable;
  guint num_samples;

  gst_buffer_map (audio, &visual->amap, GST_MAP_READ);

  /*num_samples =
      amap.size / (GST_AUDIO_INFO_CHANNELS (&base->ainfo) * sizeof (gint16));
  scope->process (base, (guint32 *) GST_VIDEO_FRAME_PLANE_DATA (video, 0),
      (gint16 *) amap.data, num_samples);*/



  /* apply the matrices that the actor set up */
  gl->PushAttrib (GL_ALL_ATTRIB_BITS);

  gl->MatrixMode (GL_PROJECTION);
  gl->PushMatrix ();
  gl->LoadMatrixf (visual->actor_projection_matrix);

  gl->MatrixMode (GL_MODELVIEW);
  gl->PushMatrix ();
  gl->LoadMatrixf (visual->actor_modelview_matrix);

  /* This line try to hacks compatiblity with libprojectM
   * If libprojectM version <= 2.0.0 then we have to unbind our current
   * fbo to see something. But it's incorrect and we cannot use fbo chainning (append other glfilters
   * after libvisual_gl_projectM will not work)
   * To have full compatibility, libprojectM needs to take care of our fbo.
   * Indeed libprojectM has to unbind it before the first rendering pass
   * and then rebind it before the final pass. It's done from 2.0.1
   */
  //name = gst_element_get_name (GST_ELEMENT (visual));
  /*if (g_ascii_strncasecmp (name, "visualglprojectm", 16) == 0
      && !HAVE_PROJECTM_TAKING_CARE_OF_EXTERNAL_FBO)
    glBindFramebufferEXT (GL_FRAMEBUFFER_EXT, 0);*/
  //g_free (name);

  //actor_negotiate (visual->context, visual);

  if (visual->is_enabled_gl_depth_test) {
    glEnable (GL_DEPTH_TEST);
    gl->DepthFunc (visual->gl_depth_func);
  }

  if (visual->is_enabled_gl_blend) {
    gl->Enable (GL_BLEND);
    gl->BlendFunc (visual->gl_blend_src_alpha, GL_ZERO);
  }

  //visual_actor_run (visual->actor, visual->audio);
  check_gl_matrix ();

  gl->MatrixMode (GL_PROJECTION);
  gl->PopMatrix ();

  gl->MatrixMode (GL_MODELVIEW);
  gl->PopMatrix ();

  gl->PopAttrib ();

  gl->Disable (GL_DEPTH_TEST);
  gl->Disable (GL_BLEND);



  gst_buffer_unmap (audio, &visual->amap);
  return TRUE;
}

static gboolean
gst_gl_audio_visualizer_decide_allocation (GstAudioVisualizer * scope, GstQuery * query)
{
  GstCaps *outcaps;
  GstBufferPool *pool;
  guint size, min, max, idx;
  guint out_width, out_height;
  GstAllocator *allocator;
  GstAllocationParams params;
  GstStructure *config;
  gboolean update_allocator;
  gboolean update_pool;
  GError *error = NULL;
  GstGLContext *other_context;

  GstGLAudioVisualizer *visual = GST_GL_AUDIO_VISUALIZER (scope);
  gst_query_parse_allocation (query, &outcaps, NULL);

  GST_DEBUG_OBJECT (visual, "%s called", __func__);
  /* we got configuration from our peer or the decide_allocation method,
   * parse them */
  if (gst_query_get_n_allocation_params (query) > 0) {
    /* try the allocator */
    gst_query_parse_nth_allocation_param (query, 0, &allocator, &params);
    update_allocator = TRUE;
  } else {
    allocator = NULL;
    gst_allocation_params_init (&params);
    update_allocator = FALSE;
  }

  gst_video_info_init (&scope->vinfo);
  gst_video_info_from_caps (&scope->vinfo, outcaps);

  if (gst_query_get_n_allocation_pools (query) > 0) {
    gst_query_parse_nth_allocation_pool (query, 0, &pool, &size, &min, &max);
    update_pool = TRUE;
  } else {
    pool = NULL;
    size = GST_VIDEO_INFO_SIZE (&scope->vinfo);
    min = max = 0;
    update_pool = FALSE;
  }

  if (!gst_gl_ensure_display (visual, &visual->display)) {
    GST_DEBUG_OBJECT (visual, "%s failed to create GL display");
    return FALSE;
  }

  if (gst_query_find_allocation_meta (query,
          GST_VIDEO_GL_TEXTURE_UPLOAD_META_API_TYPE, &idx)) {
    GstGLContext *context;
    const GstStructure *upload_meta_params;
    gpointer handle;
    gchar *type;
    gchar *apis;
    GST_DEBUG_OBJECT (visual, "%s GST_VIDEO_GL_TEXTURE_UPLOAD_META_API_TYPE meta found");

    gst_query_parse_nth_allocation_meta (query, idx, &upload_meta_params);
    if (upload_meta_params) {
      if (gst_structure_get (upload_meta_params, "gst.gl.GstGLContext",
              GST_GL_TYPE_CONTEXT, &context, NULL) && context) {
        GstGLContext *old = visual->context;
        GST_DEBUG_OBJECT (visual, "%s gst.gl.GstGLContext param found");

        visual->context = context;
        if (old)
          gst_object_unref (old);
      } else if (gst_structure_get (upload_meta_params, "gst.gl.context.handle",
              G_TYPE_POINTER, &handle, "gst.gl.context.type", G_TYPE_STRING,
              &type, "gst.gl.context.apis", G_TYPE_STRING, &apis, NULL)
          && handle) {
        GstGLPlatform platform = GST_GL_PLATFORM_NONE;
        GstGLAPI gl_apis;
        GST_DEBUG_OBJECT (visual, "%s gst.gl.context.apis param found");

        GST_DEBUG ("got GL context handle 0x%p with type %s and apis %s",
            handle, type, apis);

        platform = gst_gl_platform_from_string (type);
        gl_apis = gst_gl_api_from_string (apis);

        if (gl_apis && platform)
          other_context =
              gst_gl_context_new_wrapped (visual->display, (guintptr) handle,
              platform, gl_apis);
      }
    }
  }

  if (!visual->context) {
    GST_DEBUG_OBJECT (visual, "%s create CONTEXT");
    visual->context = gst_gl_context_new (visual->display);
    if (!gst_gl_context_create (visual->context, other_context, &error))
      goto context_error;
  }

        /*visual->actor =
            visual_actor_new (GST_VISUAL_GL_GET_CLASS (visual)->plugin->info->
            plugname);
        visual->video = visual_video_new ();
        visual->audio = visual_audio_new ();

        if (!visual->actor || !visual->video)
          goto context_error;*/

  out_width = GST_VIDEO_INFO_WIDTH (&scope->vinfo);
  out_height = GST_VIDEO_INFO_HEIGHT (&scope->vinfo);

  if (visual->fbo) {
    gst_gl_context_del_fbo (visual->context, visual->fbo, visual->depthbuffer);
    visual->fbo = 0;
    visual->depthbuffer = 0;
  }

  if (!gst_gl_context_gen_fbo (visual->context, out_width, out_height,
          &visual->fbo, &visual->depthbuffer))
    goto context_error;

  if (visual->out_tex_id)
    gst_gl_context_del_texture (visual->context, &visual->out_tex_id);
  gst_gl_context_gen_texture (visual->context, &visual->out_tex_id,
      GST_VIDEO_FORMAT_RGBA, out_width, out_height);
  if (pool == NULL) {
    /* we did not get a pool, make one ourselves then */
    pool = gst_video_buffer_pool_new ();
  }

  config = gst_buffer_pool_get_config (pool);
  gst_buffer_pool_config_set_params (config, outcaps, size, min, max);
  gst_buffer_pool_config_set_allocator (config, allocator, &params);
  gst_buffer_pool_config_add_option (config, GST_BUFFER_POOL_OPTION_VIDEO_META);
  gst_buffer_pool_set_config (pool, config);

  if (update_allocator)
    gst_query_set_nth_allocation_param (query, 0, allocator, &params);
  else
    gst_query_add_allocation_param (query, allocator, &params);

  if (allocator)
    gst_object_unref (allocator);

  if (update_pool)
    gst_query_set_nth_allocation_pool (query, 0, pool, size, min, max);
  else
    gst_query_add_allocation_pool (query, pool, size, min, max);

  if (pool)
    gst_object_unref (pool);

  return TRUE;

  /*config_failed:
  {
    GST_DEBUG_OBJECT (visual, "failed setting config");
    return FALSE;
  }*/
  context_error:
  {
    GST_ELEMENT_ERROR (visual, RESOURCE, NOT_FOUND, ("%s", error->message),
        (NULL));
    return FALSE;
  }
}

gboolean
gst_gl_audio_visualizer_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gl_audio_visualizer_debug, "glscore", 0, "glscope");

  return gst_element_register (plugin, "glscope", GST_RANK_NONE,
      GST_TYPE_GL_AUDIO_VISUALIZER);
}
