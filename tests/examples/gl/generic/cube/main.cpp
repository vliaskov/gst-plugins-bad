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

GLuint vbo[4];

static const gchar *simple_vertex_shader_str_gles2 =
      "attribute vec4 a_position;   \n"
      "attribute vec2 a_texCoord;   \n"
      "varying vec2 v_texCoord;     \n"
      "uniform mat4 proj_matrix; \n"
      "uniform mat4 rot_matrix; \n"
      "uniform mat4 view_matrix; \n"
      "uniform mat4 model_matrix; \n"
      "uniform float anglex;\n"
      "uniform float angley;\n"
      "uniform float anglez;\n"
      "mat4 rotz_matrix = mat4(cos(anglez), -sin(anglez), 0.0, 0.0, sin(anglez), cos(anglez), 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0);\n"
      "mat4 roty_matrix = mat4(cos(angley), 0.0, sin(angley), 0.0, 0.0, 1.0, 0.0, 0.0, -sin(angley), 0.0, cos(angley), 0.0, 0.0, 0.0, 0.0, 1.0);\n"
      "mat4 rotx_matrix = mat4(1.0, 0.0, 0.0, 0.0, 0.0, cos(anglex), -sin(anglex), 0.0, 0.0, sin(anglex), cos(anglex), 0.0, 0.0, 0.0, 0.0, 1.0);\n"
      "void main()                  \n"
      "{                            \n"
      "   gl_Position = gl_ProjectionMatrix * gl_ModelViewMatrix * a_position; \n"
      "   v_texCoord = a_texCoord;  \n"
      "}                            \n";
      //"   gl_Position = proj_matrix * view_matrix * model_matrix * a_position; \n"

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

#define FRONTX -0.5f
#define FRONTY -0.5f
#define FRONTZ 0.5f
#define WIDTH 1.0f
#define HEIGHT 1.0f
#define DEPTH 1.0f

GLfloat vVertices[] = { 
        FRONTX, FRONTY, FRONTZ, // Position 0
        FRONTX + WIDTH, FRONTY, FRONTZ, // Position 1
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ, // Position 2
        FRONTX, FRONTY + HEIGHT, FRONTZ, // Position 3
        FRONTX, FRONTY, FRONTZ + DEPTH, // Position 4
        FRONTX + WIDTH, FRONTY, FRONTZ + DEPTH, // Position 5
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ + DEPTH, // Position 6
        FRONTX, FRONTY + HEIGHT, FRONTZ + DEPTH, // Position 7
};

GLfloat v_vertices[] = { 

        FRONTX, FRONTY, FRONTZ + DEPTH, 0.0, 0.0,
        FRONTX + WIDTH, FRONTY, FRONTZ + DEPTH, 1.0, 0.0,
        FRONTX, FRONTY + HEIGHT, FRONTZ + DEPTH, 0.0, 1.0,
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ + DEPTH, 1.0, 1.0,

        FRONTX + WIDTH, FRONTY, FRONTZ + DEPTH, 0.0, 0.0,
        FRONTX + WIDTH, FRONTY, FRONTZ, 1.0, 0.0,
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ + DEPTH, 0.0, 1.0,
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ, 1.0, 1.0,

        FRONTX + WIDTH, FRONTY, FRONTZ, 0.0, 0.0,
        FRONTX, FRONTY, FRONTZ, 1.0, 0.0,
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ, 0.0, 1.0,
        FRONTX, FRONTY + HEIGHT, FRONTZ, 0.0, 1.0,

        FRONTX, FRONTY, FRONTZ, 0.0, 0.0,
        FRONTX, FRONTY, FRONTZ + DEPTH, 1.0, 0.0,
        FRONTX, FRONTY + HEIGHT, FRONTZ, 0.0, 1.0,
        FRONTX, FRONTY + HEIGHT, FRONTZ + DEPTH, 0.0, 1.0,

        FRONTX, FRONTY, FRONTZ, 0.0, 0.0,
        FRONTX + WIDTH, FRONTY, FRONTZ, 1.0, 0.0,
        FRONTX, FRONTY, FRONTZ + DEPTH, 0.0, 1.0,
        FRONTX + WIDTH, FRONTY, FRONTZ + DEPTH, 0.0, 1.0,

        FRONTX, FRONTY + HEIGHT, FRONTZ + DEPTH, 0.0, 0.0,
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ + DEPTH, 1.0, 0.0,
        FRONTX, FRONTY + HEIGHT, FRONTZ, 0.0, 1.0,
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ, 0.0, 1.0,

};

  const GLfloat vvertices[] = {
 /*|     Vertex     | TexCoord |*/ 
    /* front face */
     1.0,  1.0, -1.0, 1.0, 0.0,
     1.0, -1.0, -1.0, 1.0, 1.0,
    -1.0, -1.0, -1.0, 0.0, 1.0,
    -1.0,  1.0, -1.0, 0.0, 0.0,
    /* back face */
     1.0,  1.0,  1.0, 1.0, 0.0,
    -1.0,  1.0,  1.0, 0.0, 0.0,
    -1.0, -1.0,  1.0, 0.0, 1.0,
     1.0, -1.0,  1.0, 1.0, 1.0,
    /* right face */
     1.0,  1.0,  1.0, 1.0, 0.0,
     1.0, -1.0,  1.0, 0.0, 0.0,
     1.0, -1.0, -1.0, 0.0, 1.0,
     1.0,  1.0, -1.0, 1.0, 1.0,
    /* left face */
    -1.0,  1.0,  1.0, 1.0, 0.0,
    -1.0,  1.0, -1.0, 1.0, 1.0,
    -1.0, -1.0, -1.0, 0.0, 1.0,
    -1.0, -1.0,  1.0, 0.0, 0.0,
    /* top face */
     1.0, -1.0,  1.0, 1.0, 0.0,
    -1.0, -1.0,  1.0, 0.0, 0.0,
    -1.0, -1.0, -1.0, 0.0, 1.0,
     1.0, -1.0, -1.0, 1.0, 1.0,
    /* bottom face */
     1.0,  1.0,  1.0, 1.0, 0.0,
     1.0,  1.0, -1.0, 1.0, 1.0,
    -1.0,  1.0, -1.0, 0.0, 1.0,
    -1.0,  1.0,  1.0, 0.0, 0.0
  };

/* *INDENT-ON* */
GLfloat vStripVertices[] = { 

        FRONTX, FRONTY, FRONTZ + DEPTH,
        FRONTX + WIDTH, FRONTY, FRONTZ + DEPTH,
        FRONTX, FRONTY + HEIGHT, FRONTZ + DEPTH,
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ + DEPTH,

        FRONTX + WIDTH, FRONTY, FRONTZ + DEPTH,
        FRONTX + WIDTH, FRONTY, FRONTZ,
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ + DEPTH,
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ,

        FRONTX + WIDTH, FRONTY, FRONTZ,
        FRONTX, FRONTY, FRONTZ,
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ,
        FRONTX, FRONTY + HEIGHT, FRONTZ,

        FRONTX, FRONTY, FRONTZ,
        FRONTX, FRONTY, FRONTZ + DEPTH,
        FRONTX, FRONTY + HEIGHT, FRONTZ,
        FRONTX, FRONTY + HEIGHT, FRONTZ + DEPTH,

        FRONTX, FRONTY, FRONTZ,
        FRONTX + WIDTH, FRONTY, FRONTZ,
        FRONTX, FRONTY, FRONTZ + DEPTH,
        FRONTX + WIDTH, FRONTY, FRONTZ + DEPTH,

        FRONTX, FRONTY + HEIGHT, FRONTZ + DEPTH,
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ + DEPTH,
        FRONTX, FRONTY + HEIGHT, FRONTZ,
        FRONTX + WIDTH, FRONTY + HEIGHT, FRONTZ,

};

GLfloat vStripTextures[] = { 

        0.0, 0.0,
        1.0, 0.0,
        0.0, 1.0,
        1.0, 1.0,

        0.0, 0.0,
        1.0, 0.0,
        0.0, 1.0,
        1.0, 1.0,

        0.0, 0.0,
        1.0, 0.0,
        0.0, 1.0,
        0.0, 1.0,

        0.0, 0.0,
        1.0, 0.0,
        0.0, 1.0,
        0.0, 1.0,

        0.0, 0.0,
        1.0, 0.0,
        0.0, 1.0,
        0.0, 1.0,

        0.0, 0.0,
        1.0, 0.0,
        0.0, 1.0,
        0.0, 1.0,

};

GLuint stripindices[] = {
  0,  1,  2,  3,  3,     // Face 0 - triangle strip ( v0,  v1,  v2,  v3)
  4,  4,  5,  6,  7,  7, // Face 1 - triangle strip ( v4,  v5,  v6,  v7)
  8,  8,  9, 10, 11, 11, // Face 2 - triangle strip ( v8,  v9, v10, v11)
  12, 12, 13, 14, 15, 15, // Face 3 - triangle strip (v12, v13, v14, v15)
  16, 16, 17, 18, 19, 19, // Face 4 - triangle strip (v16, v17, v18, v19)
  20, 20, 21, 22, 23,      // Face 5 - triangle strip (v20, v21, v22, v23)
};

GLushort indices[] = {
    0, 1, 2,
    0, 2, 3,
    4, 5, 6,
    4, 6, 7,
    8, 9, 10,
    8, 10, 11,
    12, 13, 14,
    12, 14, 15,
    16, 17, 18,
    16, 18, 19,
    20, 21, 22,
    20, 22, 23
  };

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
    GLfloat view_matrix[] = {1.0, 0.0, 0.0, 0.0,
                       0.0, 1.0, 0.0, 0.0,
                       0.0, 0.0, 1.0, 0.0,
                       0.0, 0.0, -1.0, 1.0 };

    float rad = xrot * M_PI / 180.0;

    GLfloat rot_matrix[] = {
          cos(rad), -sin(rad), 0.0, 0.0,
          sin(rad), cos(rad), 0.0, 0.0,
                  0.0,     0.0, 1.0, 0.0,
                  0.0,     0.0, 0.0, 1.0
        };
                  
    // Calculate model view transformation
    GLfloat model_matrix[] = {1.0, 0.0, 0.0, 0.0,
              0.0, 1.0, 0.0, 0.0,
              0.0, 0.0, 1.0, 0.0,
              0.0, 0.0, 0.0, 1.0};

    // Set modelview-projection matrix
    glEnable (GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //glClearColor (cube_filter->red, cube_filter->green, cube_filter->blue, 0.0);

    GLint anglexLoc = glGetUniformLocation ( programObject, "anglex");
    GLint angleyLoc = glGetUniformLocation ( programObject, "angley");
    GLint anglezLoc = glGetUniformLocation ( programObject, "anglez");
    glUniform1f(anglexLoc, rad);
    glUniform1f(angleyLoc, rad);
    glUniform1f(anglezLoc, rad);

    glEnableClientState (GL_TEXTURE_COORD_ARRAY);
    glEnableClientState (GL_VERTEX_ARRAY);
    // Load the vertex position
    GLint positionLoc = glGetAttribLocation ( programObject, "a_position" );
    glVertexAttribPointer ( positionLoc, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), v_vertices);
    glEnableVertexAttribArray ( positionLoc );

    // Load the texture coordinate
    GLint texCoordLoc = glGetAttribLocation ( programObject, "a_texCoord");
    glVertexAttribPointer ( texCoordLoc, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), &v_vertices[3]);
    glEnableVertexAttribArray ( texCoordLoc );

    glActiveTexture ( GL_TEXTURE0 );
    glBindTexture (GL_TEXTURE_2D, texture);
    GLint tex = glGetUniformLocation ( programObject, "tex");
    glUniform1i ( tex, 0 );

    //xrot += 5.0f;

    glDrawElements ( GL_TRIANGLES, 36, GL_UNSIGNED_SHORT, indices );
    //glDrawElements ( GL_TRIANGLE_STRIP, 34, GL_UNSIGNED_SHORT, stripindices );

    glDisableVertexAttribArray ( positionLoc );
    glDisableVertexAttribArray ( texCoordLoc );
    glDisable (GL_DEPTH_TEST);
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
