/* MegaZeux
 *
 * Copyright (C) 2008 Joel Bouchard Lamontagne <logicow@gmail.com>
 * Copyright (C) 2006-2007 Gilead Kutnick <exophase@adelphia.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "render.h"
#include "renderers.h"
#include "util.h"
#include "data.h"

#ifdef CONFIG_SDL
#include "render_sdl.h"
#endif

#ifdef CONFIG_EGL
#include <GLES2/gl2.h>
#include "render_egl.h"
#endif

#include "render_gl.h"

struct glsl_syms
{
  int syms_loaded;

  void (GL_APIENTRY *glBindTexture)(GLenum target, GLuint texture);
  void (GL_APIENTRY *glBlendFunc)(GLenum sfactor, GLenum dfactor);
  void (GL_APIENTRY *glClear)(GLbitfield mask);
  void (GL_APIENTRY *glColorPointer)(GLint size, GLenum type, GLsizei stride,
   const GLvoid *pointer);
  void (GL_APIENTRY *glCopyTexImage2D)(GLenum target, GLint level,
   GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height,
   GLint border);
  void (GL_APIENTRY *glDisable)(GLenum cap);
  void (GL_APIENTRY *glDisableClientState)(GLenum cap);
  void (GL_APIENTRY *glDrawArrays)(GLenum mode, GLint first, GLsizei count);
  void (GL_APIENTRY *glEnable)(GLenum cap);
  void (GL_APIENTRY *glEnableClientState)(GLenum cap);
  void (GL_APIENTRY *glGenTextures)(GLsizei n, GLuint *textures);
  const GLubyte* (GL_APIENTRY *glGetString)(GLenum name);
  void (GL_APIENTRY *glTexCoordPointer)(GLint size, GLenum type,
   GLsizei stride, const GLvoid *ptr);
  void (GL_APIENTRY *glTexImage2D)(GLenum target, GLint level,
   GLint internalformat, GLsizei width, GLsizei height, GLint border,
   GLenum format, GLenum type, const GLvoid *pixels);
  void (GL_APIENTRY *glTexParameterf)(GLenum target, GLenum pname,
   GLfloat param);
  void (GL_APIENTRY *glTexSubImage2D)(GLenum target, GLint level,
   GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
   GLenum format, GLenum type, const void *pixels);
  void (GL_APIENTRY *glVertexPointer)(GLint size, GLenum type,
   GLsizei stride, const GLvoid *ptr);
  void (GL_APIENTRY *glViewport)(GLint x, GLint y, GLsizei width,
   GLsizei height);

  // These are loaded as extensions on at least Windows
  GLenum (GL_APIENTRY *glCreateProgram)(void);
  GLenum (GL_APIENTRY *glCreateShader)(GLenum type);
  void (GL_APIENTRY *glShaderSource)(GLuint shader, GLsizei count,
   const char **strings, const GLint *length);
  void (GL_APIENTRY *glCompileShader)(GLuint shader);
  void (GL_APIENTRY *glAttachShader)(GLuint program, GLuint shader);
  void (GL_APIENTRY *glUseProgram)(GLuint program);
  void (GL_APIENTRY *glLinkProgram)(GLuint program);
  void (GL_APIENTRY *glGetProgramInfoLog)(GLuint program, GLsizei bufsize,
   GLsizei *len, char *infolog);
};

struct glsl_render_data
{
  Uint32 *pixels;
  Uint32 charset_texture[CHAR_H * CHARSET_SIZE * CHAR_W * 2];
  Uint32 background_texture[SCREEN_W * SCREEN_H];
  GLuint texture_number[3];
  GLubyte palette[3 * SMZX_PAL_SIZE];
  Uint8 remap_texture;
  Uint8 remap_char[CHARSET_SIZE * 2];
  Uint8 ignore_linear;
  struct glsl_syms gl;
  char gl_tilemap_fragment_shader_path[MAX_PATH];
  char gl_tilemap_vertex_shader_path[MAX_PATH];
  char gl_scaling_fragment_shader_path[MAX_PATH];
  char gl_scaling_vertex_shader_path[MAX_PATH];
  Uint32 gl_tilemap;
  Uint32 gl_scaling;
  enum ratio_type ratio;
};

static int glsl_load_syms(struct glsl_syms *gl)
{
  if(gl->syms_loaded)
    return true;

  GL_LOAD_SYM(gl, glBindTexture)
  GL_LOAD_SYM(gl, glBlendFunc)
  GL_LOAD_SYM(gl, glClear)
  GL_LOAD_SYM(gl, glColorPointer)
  GL_LOAD_SYM(gl, glCopyTexImage2D)
  GL_LOAD_SYM(gl, glDisable)
  GL_LOAD_SYM(gl, glDisableClientState)
  GL_LOAD_SYM(gl, glDrawArrays)
  GL_LOAD_SYM(gl, glEnable)
  GL_LOAD_SYM(gl, glEnableClientState)
  GL_LOAD_SYM(gl, glGenTextures)
  GL_LOAD_SYM(gl, glGetString)
  GL_LOAD_SYM(gl, glTexCoordPointer)
  GL_LOAD_SYM(gl, glTexImage2D)
  GL_LOAD_SYM(gl, glTexParameterf)
  GL_LOAD_SYM(gl, glTexSubImage2D)
  GL_LOAD_SYM(gl, glVertexPointer)
  GL_LOAD_SYM(gl, glViewport)

  // Even though these aren't "extensions" in OpenGL 2.0, on Windows
  // these symbols are too new to be present in OPENGL32.DLL, so we
  // must dynamically load them. For OpenGL ES 2.0 it's a non-issue,
  // and these are resolved at link time.
  GL_LOAD_SYM_EXT(gl, glCreateProgram)
  GL_LOAD_SYM_EXT(gl, glCreateShader)
  GL_LOAD_SYM_EXT(gl, glShaderSource)
  GL_LOAD_SYM_EXT(gl, glCompileShader);
  GL_LOAD_SYM_EXT(gl, glAttachShader);
  GL_LOAD_SYM_EXT(gl, glUseProgram);
  GL_LOAD_SYM_EXT(gl, glLinkProgram);
  GL_LOAD_SYM_EXT(gl, glGetProgramInfoLog);

  gl->syms_loaded = true;
  return true;
}

static char *glsl_loadstring(char *filename)
{
  char *buffer = NULL;
  unsigned long size;
  FILE *f;

  f = fopen(filename, "rb");
  if(!f)
    goto err_out;

  size = ftell_and_rewind(f);
  if(!size)
    goto err_close;

  buffer = malloc(size + 1);
  buffer[size] = '\0';

  if(fread(buffer, size, 1, f) != 1)
  {
    free(buffer);
    buffer = NULL;
  }

err_close:
  fclose(f);
err_out:
  return buffer;
}

#define SHADER_INFO_MAX 1000

static void glsl_verify_shader_compile(struct glsl_render_data *render_data,
 GLenum shader)
{
  struct glsl_syms *gl = &render_data->gl;
  char buffer[SHADER_INFO_MAX];
  int len = 0;

  gl->glGetProgramInfoLog(shader, SHADER_INFO_MAX - 1, &len, buffer);
  buffer[len] = 0;

  if(len > 0)
    warn("%s", buffer);
}

static void glsl_load_shaders(struct graphics_data *graphics)
{
  int len;
  GLenum shader_vert_scaling;
  GLenum shader_frag_scaling;
  GLenum shader_vert_tilemap;
  GLenum shader_frag_tilemap;
  GLenum scaling_program;
  GLenum tilemap_program;
  const char *shader_vert_source_scaling;
  const char *shader_frag_source_scaling;
  const char *shader_vert_source_tilemap;
  const char *shader_frag_source_tilemap;
  struct glsl_syms *gl;
  struct glsl_render_data *render_data = graphics->render_data;
  gl = &render_data->gl;

  shader_vert_source_tilemap =
    glsl_loadstring(render_data->gl_tilemap_vertex_shader_path);
  shader_frag_source_tilemap =
    glsl_loadstring(render_data->gl_tilemap_fragment_shader_path);

  if(shader_vert_source_tilemap != NULL && shader_frag_source_tilemap != NULL)
  {
    tilemap_program = gl->glCreateProgram();
    shader_vert_tilemap = gl->glCreateShader(GL_VERTEX_SHADER);
    shader_frag_tilemap = gl->glCreateShader(GL_FRAGMENT_SHADER);

    len = (int)strlen(shader_vert_source_tilemap);
    gl->glShaderSource(shader_vert_tilemap, 1, &shader_vert_source_tilemap,
     &len);

    len = (int)strlen(shader_frag_source_tilemap);
    gl->glShaderSource(shader_frag_tilemap, 1, &shader_frag_source_tilemap,
     &len);

    gl->glCompileShader(shader_vert_tilemap);
    glsl_verify_shader_compile(render_data, shader_vert_tilemap);
    gl->glCompileShader(shader_frag_tilemap);
    glsl_verify_shader_compile(render_data, shader_frag_tilemap);

    gl->glAttachShader(tilemap_program, shader_vert_tilemap);
    gl->glAttachShader(tilemap_program, shader_frag_tilemap);
    gl->glLinkProgram(tilemap_program);

    free((void *)shader_vert_source_tilemap);
    free((void *)shader_frag_source_tilemap);
  }
  else
  {
    if(shader_vert_source_tilemap != NULL)
       free((char *)shader_vert_source_tilemap);
    if(shader_frag_source_tilemap != NULL)
       free((char *)shader_frag_source_tilemap);
    tilemap_program = 0;
  }

  shader_vert_source_scaling =
    glsl_loadstring(render_data->gl_scaling_vertex_shader_path);
  shader_frag_source_scaling =
    glsl_loadstring(render_data->gl_scaling_fragment_shader_path);

  if(shader_vert_source_scaling != NULL && shader_frag_source_scaling != NULL)
  {
    scaling_program = gl->glCreateProgram();
    shader_vert_scaling = gl->glCreateShader(GL_VERTEX_SHADER);
    shader_frag_scaling = gl->glCreateShader(GL_FRAGMENT_SHADER);

    len = (int)strlen(shader_vert_source_scaling);
    gl->glShaderSource(shader_vert_scaling, 1, &shader_vert_source_scaling,
     &len);
    len = (int)strlen(shader_frag_source_scaling);
    gl->glShaderSource(shader_frag_scaling, 1, &shader_frag_source_scaling,
     &len);

    gl->glCompileShader(shader_vert_scaling);
    glsl_verify_shader_compile(render_data, shader_vert_scaling);
    gl->glCompileShader(shader_frag_scaling);
    glsl_verify_shader_compile(render_data, shader_frag_scaling);

    gl->glAttachShader(scaling_program, shader_vert_scaling);
    gl->glAttachShader(scaling_program, shader_frag_scaling);
    gl->glLinkProgram(scaling_program);

    free((void *)shader_vert_source_scaling);
    free((void *)shader_frag_source_scaling);
  }
  else
  {
    if(shader_vert_source_scaling != NULL)
       free((void *)shader_vert_source_scaling);
    if(shader_frag_source_scaling != NULL)
       free((void *)shader_frag_source_scaling);
    scaling_program = 0;
  }

  render_data->gl_scaling = scaling_program;
  render_data->gl_tilemap = tilemap_program;
}

static bool glsl_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct glsl_render_data *render_data;
  const char *version, *extensions;
  struct glsl_syms *gl;

  render_data = malloc(sizeof(struct glsl_render_data));
  if(!render_data)
    return false;

  gl = &render_data->gl;

  if(!GL_CAN_USE)
    goto err_free;

  graphics->render_data = render_data;
  gl->syms_loaded = false;

  graphics->gl_vsync = conf->gl_vsync;
  graphics->allow_resize = conf->allow_resize;
  graphics->gl_filter_method = conf->gl_filter_method;
  sprintf(render_data->gl_tilemap_fragment_shader_path, "%s/%s", current_dir,
    conf->gl_tilemap_fragment_shader);
  sprintf(render_data->gl_tilemap_vertex_shader_path, "%s/%s", current_dir,
    conf->gl_tilemap_vertex_shader);
  sprintf(render_data->gl_scaling_fragment_shader_path, "%s/%s", current_dir,
    conf->gl_scaling_fragment_shader);
  sprintf(render_data->gl_scaling_vertex_shader_path, "%s/%s", current_dir,
    conf->gl_scaling_vertex_shader);
  graphics->bits_per_pixel = 32;

  // OpenGL only supports 16/32bit colour
  if(conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics->bits_per_pixel = conf->force_bpp;

  render_data->pixels = malloc(sizeof(Uint32) * GL_POWER_2_WIDTH *
   GL_POWER_2_HEIGHT);
  if(!render_data->pixels)
    goto err_free;

  render_data->ratio = conf->video_ratio;
  if(!set_video_mode())
    goto err_free;

  // NOTE: These must come AFTER set_video_mode()!
  version = (const char *)gl->glGetString(GL_VERSION);
  extensions = (const char *)gl->glGetString(GL_EXTENSIONS);

  // we need a specific "version" of OpenGL compatibility
  if(version && atof(version) < 1.1)
  {
    warn("OpenGL implementation is too old (need v1.1).\n");
    goto err_free;
  }

  // we also need to be able to utilise shader extensions
  if(!(extensions && strstr(extensions, "GL_ARB_shading_language_100")))
  {
    warn("OpenGL missing GL_ARB_shading_language_100 extension.\n");
    goto err_free;
  }

  return true;

err_free:
  free(render_data);
  return false;
}

static void glsl_free_video(struct graphics_data *graphics)
{
  free(graphics->render_data);
  graphics->render_data = NULL;
}

static void glsl_remap_charsets(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;
  render_data->remap_texture = true;
}

// FIXME: Many magic numbers
static void glsl_resize_screen(struct graphics_data *graphics,
 int width, int height)
{
  struct glsl_render_data *render_data = graphics->render_data;
  struct glsl_syms *gl = &render_data->gl;

  render_data->ignore_linear = false;

  gl->glViewport(0, 0, width, height);

  gl->glGenTextures(3, render_data->texture_number);

  gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[0]);

  gl_set_filter_method(CONFIG_GL_FILTER_LINEAR, gl->glTexParameterf);

  gl->glEnable(GL_TEXTURE_2D);

  gl->glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

  memset(render_data->pixels, 255,
   sizeof(Uint32) * GL_POWER_2_WIDTH * GL_POWER_2_HEIGHT);

  gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GL_POWER_2_WIDTH,
   GL_POWER_2_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
   render_data->pixels);

  gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);

  gl_set_filter_method(CONFIG_GL_FILTER_NEAREST, gl->glTexParameterf);

  gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA,
   GL_UNSIGNED_BYTE, render_data->pixels);

  glsl_remap_charsets(graphics);

  gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[2]);

  gl_set_filter_method(CONFIG_GL_FILTER_NEAREST, gl->glTexParameterf);

  gl->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 128, 32, 0, GL_RGBA,
   GL_UNSIGNED_BYTE, render_data->pixels);

  glsl_load_shaders(graphics);
}

static bool glsl_set_video_mode(struct graphics_data *graphics,
 int width, int height, int depth, bool fullscreen, bool resize)
{
  struct glsl_render_data *render_data = graphics->render_data;
  struct glsl_syms *gl = &render_data->gl;

  gl_set_attributes(graphics);

  if(!gl_set_video_mode(graphics, width, height, depth, fullscreen, resize))
    return false;

  if(!glsl_load_syms(gl))
    return false;

  glsl_resize_screen(graphics, width, height);
  return true;
}

static void glsl_remap_char(struct graphics_data *graphics, Uint16 chr)
{
  struct glsl_render_data *render_data = graphics->render_data;
  render_data->remap_char[chr] = true;
}

static void glsl_remap_charbyte(struct graphics_data *graphics,
 Uint16 chr, Uint8 byte)
{
  glsl_remap_char(graphics, chr);
}

static int *glsl_char_bitmask_to_texture(signed char *c, int *p)
{
  // This tests the 7th bit, if 0, result is 0.
  // If 1, result is 255 (because of sign extended bit shift!).
  // Note the use of char constants to force 8bit calculation.
  *(p++) = *c << 24 >> 31;
  *(p++) = *c << 25 >> 31;
  *(p++) = *c << 26 >> 31;
  *(p++) = *c << 27 >> 31;
  *(p++) = *c << 28 >> 31;
  *(p++) = *c << 29 >> 31;
  *(p++) = *c << 30 >> 31;
  *(p++) = *c << 31 >> 31;
  return p;
}

static inline void glsl_do_remap_charsets(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;
  struct glsl_syms *gl = &render_data->gl;
  signed char *c = (signed char *)graphics->charset;
  int *p = (int *)render_data->charset_texture;
  unsigned int i, j, k;

  for(i = 0; i < 16; i++, c += -14 + 32 * 14)
    for(j = 0; j < 14; j++, c += -32 * 14 + 1)
      for(k = 0; k < 32; k++, c += 14)
        p = glsl_char_bitmask_to_texture(c, p);

  gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 32 * 8, 16 * 14, GL_RGBA,
    GL_UNSIGNED_BYTE, render_data->charset_texture);
}

static inline void glsl_do_remap_char(struct graphics_data *graphics,
 Uint16 chr)
{
  struct glsl_render_data *render_data = graphics->render_data;
  struct glsl_syms *gl = &render_data->gl;
  signed char *c = (signed char *)graphics->charset;
  int *p = (int *)render_data->charset_texture;
  unsigned int i;

  c += chr * 14;

  for(i = 0; i < 14; i++, c++)
    p = glsl_char_bitmask_to_texture(c, p);
  gl->glTexSubImage2D(GL_TEXTURE_2D, 0, chr % 32 * 8, chr / 32 * 14, 8, 14,
   GL_RGBA, GL_UNSIGNED_BYTE, render_data->charset_texture);
}

static void glsl_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, Uint32 count)
{
  struct glsl_render_data *render_data = graphics->render_data;
  Uint32 i;
  for(i = 0; i < count; i++)
  {
#if PLATFORM_BYTE_ORDER == PLATFORM_BIG_ENDIAN
    graphics->flat_intensity_palette[i] = (palette[i].r << 24) |
     (palette[i].g << 16) | (palette[i].b << 8);
#else
    graphics->flat_intensity_palette[i] = (palette[i].b << 16) |
     (palette[i].g << 8) | palette[i].r;
#endif
    render_data->palette[i*3  ] = (GLubyte)palette[i].r;
    render_data->palette[i*3+1] = (GLubyte)palette[i].g;
    render_data->palette[i*3+2] = (GLubyte)palette[i].b;
  }
}

static void glsl_render_graph(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;
  struct char_element *src = graphics->text_video;
  struct glsl_syms *gl = &render_data->gl;
  Uint32 *colorptr, *dest, i;
  int width, height;

  static const float tex_coord_array_single[2 * 4] = {
    0.0f, 0.0f,
    0.0f, 1.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
  };

  gl->glUseProgram(render_data->gl_tilemap);

  gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);
  if(render_data->remap_texture)
  {
    glsl_do_remap_charsets(graphics);
    render_data->remap_texture = false;
    memset(render_data->remap_char, false, sizeof(Uint8) * CHARSET_SIZE * 2);
  }
  else
  {
    for(i = 0; i < CHARSET_SIZE * 2; i++)
    {
      if(render_data->remap_char[i])
      {
        glsl_do_remap_char(graphics, i);
        render_data->remap_char[i] = false;
      }
    }
  }

  get_context_width_height(graphics, &width, &height);
  if(width < 640 || height < 350)
  {
    if(width >= 1024)
      width = 1024;
    if(height >= 512)
      height = 512;
  }
  else
  {
    width = 640;
    height = 350;
  }

  gl->glViewport(0, 0, width, height);

  dest = render_data->background_texture;

  for(i = 0; i < SCREEN_W * SCREEN_H; i++, dest++, src++)
    *dest = (src->char_value<<16) + (src->bg_color<<8) + src->fg_color;

  gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);

  gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 225, SCREEN_W, SCREEN_H, GL_RGBA,
    GL_UNSIGNED_BYTE, render_data->background_texture);

  colorptr = graphics->flat_intensity_palette;
  dest = render_data->background_texture;

  for(i = 0; i < 512; i++, dest++, colorptr++)
    *dest = *colorptr;

  gl->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 224, 256, 1, GL_RGBA,
    GL_UNSIGNED_BYTE, render_data->background_texture);

  gl->glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  gl->glEnableClientState(GL_VERTEX_ARRAY);

  gl->glTexCoordPointer(2, GL_FLOAT, 0, tex_coord_array_single);
  gl->glVertexPointer(2, GL_FLOAT, 0, vertex_array_single);

  gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  gl->glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  gl->glDisableClientState(GL_VERTEX_ARRAY);
}

static void glsl_render_cursor(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 color, Uint8 lines, Uint8 offset)
{
  struct glsl_render_data *render_data = graphics->render_data;
  GLubyte *pal_base = &render_data->palette[color * 3];
  struct glsl_syms *gl = &render_data->gl;

  const float vertex_array[2 * 4] = {
    (x * 8)*2.0f/640.0f-1.0f,     (y * 14 + offset)*-2.0f/350.0f+1.0f,
    (x * 8)*2.0f/640.0f-1.0f,     (y * 14 + lines + offset)*-2.0f/350.0f+1.0f,
    (x * 8 + 8)*2.0f/640.0f-1.0f, (y * 14 + offset)*-2.0f/350.0f+1.0f,
    (x * 8 + 8)*2.0f/640.0f-1.0f, (y * 14 + lines + offset)*-2.0f/350.0f+1.0f
  };

  const GLubyte color_array[3 * 4] = {
    pal_base[0], pal_base[1], pal_base[2],
    pal_base[0], pal_base[1], pal_base[2],
    pal_base[0], pal_base[1], pal_base[2],
    pal_base[0], pal_base[1], pal_base[2]
  };

  gl->glDisable(GL_TEXTURE_2D);
  gl->glUseProgram(0);

  gl->glEnableClientState(GL_VERTEX_ARRAY);
  gl->glEnableClientState(GL_COLOR_ARRAY);

  gl->glVertexPointer(2, GL_FLOAT, 0, vertex_array);
  gl->glColorPointer(3, GL_UNSIGNED_BYTE, 0, color_array);

  gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  gl->glDisableClientState(GL_VERTEX_ARRAY);
  gl->glDisableClientState(GL_COLOR_ARRAY);

  gl->glEnable(GL_TEXTURE_2D);
}

static void glsl_render_mouse(struct graphics_data *graphics,
 Uint32 x, Uint32 y, Uint8 w, Uint8 h)
{
  struct glsl_render_data *render_data = graphics->render_data;
  struct glsl_syms *gl = &render_data->gl;

  const float vertex_array[2 * 4] = {
     x*2.0f/640.0f-1.0f,       y*-2.0f/350.0f+1.0f,
     x*2.0f/640.0f-1.0f,      (y + h)*-2.0f/350.0f+1.0f,
    (x + w)*2.0f/640.0f-1.0f,  y*-2.0f/350.0f+1.0f,
    (x + w)*2.0f/640.0f-1.0f, (y + h)*-2.0f/350.0f+1.0f
  };

  gl->glDisable(GL_TEXTURE_2D);
  gl->glEnable(GL_BLEND);
  gl->glUseProgram(0);

  gl->glEnableClientState(GL_VERTEX_ARRAY);

  gl->glVertexPointer(2, GL_FLOAT, 0, vertex_array);

  gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  gl->glDisableClientState(GL_VERTEX_ARRAY);

  gl->glEnable(GL_TEXTURE_2D);
  gl->glDisable(GL_BLEND);
}

static void glsl_sync_screen(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;
  struct glsl_syms *gl = &render_data->gl;
  int v_width, v_height, width, height;

  gl->glUseProgram(render_data->gl_scaling);

  gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[0]);
  gl->glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 0, 0, 1024, 512, 0);

  get_context_width_height(graphics, &width, &height);
  fix_viewport_ratio(width, height, &v_width, &v_height, render_data->ratio);
  gl->glViewport((width - v_width) >> 1, (height - v_height) >> 1,
   v_width, v_height);

  if(width < 640 || height < 350)
  {
    width = (width < 1024) ? width : 1024;
    height = (height < 512) ? height : 512;
  }
  else
  {
    width = 640;
    height = 350;
  }

  gl->glClear(GL_COLOR_BUFFER_BIT);

  gl->glEnableClientState(GL_TEXTURE_COORD_ARRAY);
  gl->glEnableClientState(GL_VERTEX_ARRAY);

  {
    const float tex_coord_array_single[2 * 4] = {
       0.0f,            height / 512.0f,
       0.0f,            0.0f,
       width / 1024.0f, height / 512.0f,
       width / 1024.0f, 0.0f,
    };

    gl->glTexCoordPointer(2, GL_FLOAT, 0, tex_coord_array_single);
  }

  gl->glVertexPointer(2, GL_FLOAT, 0, vertex_array_single);

  gl->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  gl->glDisableClientState(GL_TEXTURE_COORD_ARRAY);
  gl->glDisableClientState(GL_VERTEX_ARRAY);

  gl->glBindTexture(GL_TEXTURE_2D, render_data->texture_number[1]);

  gl_swap_buffers(graphics);

  gl->glClear(GL_COLOR_BUFFER_BIT);
}

void render_glsl_register(struct renderer *renderer)
{
  memset(renderer, 0, sizeof(struct renderer));
  renderer->init_video = glsl_init_video;
  renderer->free_video = glsl_free_video;
  renderer->check_video_mode = gl_check_video_mode;
  renderer->set_video_mode = glsl_set_video_mode;
  renderer->update_colors = glsl_update_colors;
  renderer->resize_screen = resize_screen_standard;
  renderer->remap_charsets = glsl_remap_charsets;
  renderer->remap_char = glsl_remap_char;
  renderer->remap_charbyte = glsl_remap_charbyte;
  renderer->get_screen_coords = get_screen_coords_scaled;
  renderer->set_screen_coords = set_screen_coords_scaled;
  renderer->render_graph = glsl_render_graph;
  renderer->render_cursor = glsl_render_cursor;
  renderer->render_mouse = glsl_render_mouse;
  renderer->sync_screen = glsl_sync_screen;
}
