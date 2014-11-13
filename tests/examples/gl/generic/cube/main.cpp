/*
 * GStreamer GLES2 Raspberry Pi example 
 * Based on http://cgit.freedesktop.org/gstreamer/gst-plugins-bad/tree/tests/examples/gl/generic/cube/main.cpp
 * Modified for Raspberry Pi/GLES2 by Arnaud Loonstra <arnaud@sphaero.org>
 * Orginal by Julien Isorce <julien.isorce@gmail.com>
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

/* Compile on a RPI with latest Gstreamer in /usr/local
$ g++ main.cpp -pthread -I/usr/include/gstreamer-1.0 \
 -I/usr/include/glib-2.0 \
 -I/usr/lib/arm-linux-gnueabihf/glib-2.0/include \
 -I/usr/include/libdrm \
 -I/opt/vc/include/ \
 -I/usr/local/include/gstreamer-1.0/ \
 -L/opt/vc/lib/ -lgstreamer-1.0 -lgobject-2.0 \
 -lglib-2.0 -lGLESv2 -lEGL
*/

#include <GLES/gl.h>
#include <GLES2/gl2.h>
#include <gst/gst.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <math.h>

static const gchar *simple_vertex_shader_str_gles2 =
      "attribute vec4 a_position;   \n"
      "attribute vec2 a_texCoord;   \n"
      "varying vec2 v_texCoord;     \n"
      "uniform mat4 proj_matrix; \n"
      "uniform mat4 rot_matrix; \n"
      "uniform mat4 view_matrix; \n"
      "uniform mat4 model_matrix; \n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = proj_matrix * view_matrix * model_matrix * a_position; \n"
      "   v_texCoord = a_texCoord;  \n"
      "}                            \n";

static const gchar *simple_fragment_shader_str_gles2 =
      "#ifdef GL_ES                                        \n"
      "precision mediump float;                            \n"
      "#endif                                              \n"
      "varying vec2 v_texCoord;                            \n"
      "uniform sampler2D tex;                              \n"
      "void main()                                         \n"
      "{                                                   \n"
      "  gl_FragColor = texture2D( tex, v_texCoord );      \n"
      "}                                                   \n";

GLint initGL();
GLuint LoadShader ( GLenum type, const char *shaderSrc );
GLuint vertexShader;
GLuint fragmentShader;
GLuint programObject;
GLint linked;
///
// Create a shader object, load the shader source, and
// compile the shader.
//
GLuint LoadShader ( GLenum type, const char *shaderSrc )
{
    GLuint shader;
    GLint compiled;
    // Create the shader object
    shader = glCreateShader ( type );
    if ( shader == 0 )
        return 0;
    // Load the shader source
    glShaderSource ( shader, 1, &shaderSrc, NULL );
    // Compile the shader
    glCompileShader ( shader );
    // Check the compile status
    glGetShaderiv ( shader, GL_COMPILE_STATUS, &compiled );
    if ( !compiled )
    {
        GLint infoLen = 0;
        glGetShaderiv ( shader, GL_INFO_LOG_LENGTH, &infoLen );
        if ( infoLen > 1 )
        {
            char* infoLog = (char*)malloc (sizeof(char) * infoLen );
            glGetShaderInfoLog ( shader, infoLen, NULL, infoLog );
            std::cout << "Error compiling shader:\n" << infoLog << "\n";
            free ( infoLog );
        }
        glDeleteShader ( shader );
        return 0;
    }
    return shader;
}

GLint initGL() {
    // load vertext/fragment shader
    vertexShader = LoadShader ( GL_VERTEX_SHADER, simple_vertex_shader_str_gles2 );
    fragmentShader = LoadShader ( GL_FRAGMENT_SHADER, simple_fragment_shader_str_gles2 );

    // Create the program object
    programObject = glCreateProgram();
    if ( programObject == 0 )
    {
        std::cout << "error program object\n";
       return 0;
    }

    glAttachShader(programObject, vertexShader);
    glAttachShader(programObject, fragmentShader);
    // Bind vPosition to attribute 0
    glBindAttribLocation(programObject, 0, "a_position");
    // Link the program
    glLinkProgram(programObject);
    // Check the link status
    glGetProgramiv ( programObject, GL_LINK_STATUS, &linked );
    if ( !linked )
    {
        GLint infoLen = 0;
        glGetProgramiv ( programObject, GL_INFO_LOG_LENGTH, &infoLen );
        if ( infoLen > 1 )
        {
            char* infoLog = (char*)malloc (sizeof(char) * infoLen );
            glGetProgramInfoLog ( programObject, infoLen, NULL, infoLog );
            std::cout << "Error linking program:\n" << infoLog << "\n";
            free ( infoLog );
        }
        glDeleteProgram ( programObject );
        return GL_FALSE;
    }
    return GL_TRUE;
}

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop*)data;

    switch (GST_MESSAGE_TYPE (msg))
    {
        case GST_MESSAGE_EOS:
              g_print ("End-of-stream\n");
              g_main_loop_quit (loop);
              break;
        case GST_MESSAGE_ERROR:
          {
              gchar *debug = NULL;
              GError *err = NULL;

              gst_message_parse_error (msg, &err, &debug);

              g_print ("Error: %s\n", err->message);
              g_error_free (err);

              if (debug)
              {
                  g_print ("Debug deails: %s\n", debug);
                  g_free (debug);
              }

              g_main_loop_quit (loop);
              break;
          }
        default:
          break;
    }

    return TRUE;
}

//client reshape callback
static gboolean reshapeCallback (void *gl_sink, void *gl_ctx, GLuint width, GLuint height, gpointer data)
{
    std::cout << "Reshape: width=" << width << " height=" << height << "\n";
    glViewport(0, 0, width, height);

    //glMatrixMode(GL_PROJECTION);
    //glLoadIdentity();
    //gluPerspective(45, (gfloat)width/(gfloat)height, 0.1, 100);
    //glMatrixMode(GL_MODELVIEW);

    return TRUE;
}

GLfloat vVertices[] = { 
        -0.5f, 0.5f, 0.0f, // Position 0
        -0.5f, -0.5f, 0.0f, // Position 1
        0.5f, -0.5f, 0.0f, // Position 2
        0.5f, 0.5f, 0.0f, // Position 3, skewed a bit
        -0.5f, 0.5f, -1.0f, // Position 0
        -0.5f, -0.5f, -1.0f, // Position 1
        0.5f, -0.5f, -1.0f, // Position 2
        0.5f, 0.5f, -1.0f, // Position 3, skewed a bit
};

GLfloat vTextures[] = { 
        0.0f, 0.0f, // TexCoord 0
        0.0f, 1.0f, // TexCoord 1
        1.0f, 1.0f, // TexCoord 2
        1.0f, 0.0f, // TexCoord 3
        0.0f, 0.0f, // TexCoord 0
        0.0f, 1.0f, // TexCoord 1
        1.0f, 1.0f, // TexCoord 2
        1.0f, 0.0f // TexCoord 3
};

/*GLfloat vVertCoord[] = { 
     // For cube we would need only 8 vertices but we have to
     // duplicate vertex for each face because texture coordinate
     // is different.
        -0.5f, 0.5f, 0.0f, // Position 0
        -0.5f, -0.5f, 0.0f, // Position 1
        0.5f, -0.5f, 0.0f, // Position 2
        0.5f, 0.5f, 0.0f, // Position 3, skewed a bit
        -0.5f, 0.5f, -1.0f, // Position 4
        -0.5f, -0.5f, -1.0f, // Position 5
        0.5f, -0.5f, -1.0f, // Position 6
        0.5f, 0.5f, -1.0f, // Position 7, skewed a bit
};*/

GLushort indices[] = { 0, 1, 2, 0, 2, 3,
                       4, 5, 6, 4, 6, 7,
                       0, 4, 5, 0, 5, 1,  
                       1, 2, 5, 1, 5, 6,  
                       0, 4, 7, 0, 7, 3,
                       3, 7, 6, 3, 6, 2
};
//client draw callback
static gboolean drawCallback (void * gl_sink, void * gl_ctx, GLuint texture, GLuint width, GLuint height, gpointer data)
{
    //std::cout << "draw:" << vertexShader << ":" << fragmentShader << ":" << programObject << ":" << linked << "\n"; 
    if (!linked) {
        initGL();
    }

     static GLfloat	xrot = 0;
    //static GLfloat	yrot = 0;
    //static GLfloat	zrot = 0;
    static GTimeVal current_time;
    static glong last_sec = current_time.tv_sec;
    static gint nbFrames = 0;

    g_get_current_time (&current_time);
    nbFrames++ ;

    if ((current_time.tv_sec - last_sec) >= 1)
    {
        std::cout << "GRPHIC FPS = " << nbFrames << std::endl;
        nbFrames = 0;
        last_sec = current_time.tv_sec;
    }

    //std::cout << "draw:" << vertexShader << ":" << fragmentShader << ":" << programObject << ":" << linked << "\n"; 
    /*if (!linked) {
        initGL();
    }*/

    glClear ( GL_COLOR_BUFFER_BIT );
    glUseProgram ( programObject );


    //float w_reciprocal = 1.0/(float)width;
    //float h_reciprocal = 1.0/(float)height;

  float near = 0.1;
  float far = 100;
  float aspectRatio = height / width;
  float DEG2RAD = 3.14159f / 180.0f;
  float fov = 90*DEG2RAD;
  float h = cosf(0.5f*fov)/sinf(0.5f*fov);
  float w = h * aspectRatio;
  float a =  - (near+far)/(near - far);
  float b = - ((2*far*near)/(far-near));


    GLfloat projection_matrix[] = {w, 0.0, 0.0, 0.0,
                       0.0, h, 0.0, 0.0,
                       0.0, 0.0, a, 1.0,
                       0.0, 0.0, b, 0.0};
/*
valType h = glm::cos(valType(0.5) * rad) / glm::sin(valType(0.5) * rad);
            valType w = h * height / width; ///todo max(width , Height) /
min(width , Height)?

            detail::tmat4x4<valType> Result(valType(0));
            Result[0][0] = w;
            Result[1][1] = h;
            Result[2][2] = - (zFar + zNear) / (zFar - zNear);
            Result[2][3] = - valType(1);
            Result[3][2] = - (valType(2) * zFar * zNear) / (zFar - zNear);
            return Result;
    */

 /*GLM_FUNC_QUALIFIER detail::tmat4x4<T, P> lookAt
  (
    detail::tvec3<T, P> const & eye,
    detail::tvec3<T, P> const & center,
    detail::tvec3<T, P> const & up
  )
  {
    detail::tvec3<T, P> f(normalize(center - eye));
    detail::tvec3<T, P> s(normalize(cross(f, up)));
    detail::tvec3<T, P> u(cross(s, f));

    detail::tmat4x4<T, P> Result(1);
    Result[0][0] = s.x;
    Result[1][0] = s.y;
    Result[2][0] = s.z;
    Result[0][1] = u.x;
    Result[1][1] = u.y;
    Result[2][1] = u.z;
    Result[0][2] =-f.x;
    Result[1][2] =-f.y;
    Result[2][2] =-f.z;
    Result[3][0] =-dot(s, eye);
    Result[3][1] =-dot(u, eye);
    Result[3][2] = dot(f, eye);
    return Result;
  }
    eye = (0, 0, 1);
    center = 0, 0, 0;
    up = (0, 1, 0);
    f = center - eye = (0, 0, -1);
    s = (-1, 0, 0);
    u = (0, -1, 0);*/

    GLfloat view_matrix[] = {-1.0, 0.0, 0.0, 0.0,
                       0.0, -1.0, 0.0, 0.0,
                       0.0, 0.0, 1.0, 0.0,
                       0.0, 0.0, -1.0, 1.0 };

    float xrad = xrot * M_PI / 180.0;

    GLfloat xrotation_matrix[] = {
            1.0, 0.0, 0.0, 
            0.0, cos (xrad), -sin (xrad), 
            1.0, -sin (xrad), cos (xrad)
          };

    // Calculate model view transformation
    GLfloat model_matrix[] = {1.0, 0.0, 0.0, 0.0,
              0.0, 1.0, 0.0, 0.0,
              0.0, 0.0, 1.0, 0.0,
              0.0, 0.0, 0.0, 1.0};

     // Set modelview-projection matrix

    GLint projMatrixLoc = glGetAttribLocation ( programObject, "proj_matrix");
    GLint viewMatrixLoc = glGetAttribLocation ( programObject, "view_matrix");
    GLint modelMatrixLoc = glGetAttribLocation ( programObject, "model_matrix");
    GLint rotMatrixLoc = glGetAttribLocation ( programObject, "rot_matrix");
    glUniformMatrix4fv(projMatrixLoc, 1, GL_TRUE, projection_matrix);
    glUniformMatrix4fv(modelMatrixLoc, 1, GL_TRUE, model_matrix);
    glUniformMatrix4fv(viewMatrixLoc, 1, GL_TRUE, view_matrix);
    glUniformMatrix4fv(rotMatrixLoc, 1, GL_TRUE, xrotation_matrix);

    
    // Load the vertex position
    GLint positionLoc = glGetAttribLocation ( programObject, "a_position" );
    glVertexAttribPointer ( positionLoc, 3, GL_FLOAT, GL_FALSE, 0, vVertices );
    // Load the texture coordinate
    GLint texCoordLoc = glGetAttribLocation ( programObject, "a_texCoord");
    glVertexAttribPointer ( texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, vTextures);
    glEnableVertexAttribArray ( positionLoc );
    glEnableVertexAttribArray ( texCoordLoc );


    glActiveTexture ( GL_TEXTURE0 );
    glBindTexture (GL_TEXTURE_2D, texture);
    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    //Set the texture sampler to texture unit 0
    GLint tex = glGetUniformLocation ( programObject, "tex");
    glUniform1i ( tex, 0 );

    glDrawElements ( GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, indices );
    return GST_FLOW_OK;
}


//gst-launch-1.0 videotestsrc num_buffers=400 ! video/x-raw, width=320, height=240 !
//glgraphicmaker ! glfiltercube ! video/x-raw, width=800, height=600 ! glimagesink
gint main (gint argc, gchar *argv[])
{
    GstStateChangeReturn ret;
    GstElement *pipeline, *videosrc, *glimagesink;

    GMainLoop *loop;
    GstBus *bus;

    /* initialization */
    gst_init (&argc, &argv);
    loop = g_main_loop_new (NULL, FALSE);

    /* create elements */
    pipeline = gst_pipeline_new ("pipeline");

    /* watch for messages on the pipeline's bus (note that this will only
     * work like this when a GLib main loop is running) */
    bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
    gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    /* create elements */
    videosrc = gst_element_factory_make ("videotestsrc", "videotestsrc0");
    glimagesink  = gst_element_factory_make ("glimagesink", "glimagesink0");


    if (!videosrc || !glimagesink)
    {
        g_print ("one element could not be found \n");
        return -1;
    }

    /* change video source caps */
    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, "RGB",
                                        "width", G_TYPE_INT, 320,
                                        "height", G_TYPE_INT, 240,
                                        "framerate", GST_TYPE_FRACTION, 25, 1,
                                        NULL) ;

    /* configure elements */
    g_object_set(G_OBJECT(videosrc), "num-buffers", 400, NULL);
    g_signal_connect(G_OBJECT(glimagesink), "client-reshape", G_CALLBACK (reshapeCallback), NULL);
    g_signal_connect(G_OBJECT(glimagesink), "client-draw", G_CALLBACK (drawCallback), NULL);

    /* add elements */
    gst_bin_add_many (GST_BIN (pipeline), videosrc, glimagesink, NULL);

    /* link elements */
    gboolean link_ok = gst_element_link_filtered(videosrc, glimagesink, caps) ;
    gst_caps_unref(caps) ;
    if(!link_ok)
    {
        g_warning("Failed to link videosrc to glimagesink!\n") ;
        return -1 ;
    }

    ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE)
    {
        g_print ("Failed to start up pipeline!\n");

        /* check if there is an error message with details on the bus */
        GstMessage* msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, 0);
        if (msg)
        {
          GError *err = NULL;

          gst_message_parse_error (msg, &err, NULL);
          g_print ("ERROR: %s\n", err->message);
          g_error_free (err);
          gst_message_unref (msg);
        }
        return -1;
    }

    // run loop
    g_main_loop_run (loop);

    /* clean up */
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return 0;
}
