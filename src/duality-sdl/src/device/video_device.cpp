/*
 * Copyright (C) 2021 fleroviux
 */

#include "video_device.hpp"

SDL2VideoDevice::SDL2VideoDevice(SDL_Window* window) : window(window) {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glEnable(GL_TEXTURE_2D);

  glGenTextures(2, &textures[0]);

  for (int i = 0; i < 2; i++) {
    glBindTexture(GL_TEXTURE_2D, textures[i]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  }

  glClearColor(0, 0, 0, 1);
}

SDL2VideoDevice::~SDL2VideoDevice() {
  glDeleteTextures(2, &textures[0]);
}

void SDL2VideoDevice::Draw(u32 const* top, u32 const* bottom) {
  buffer_top = top;
  buffer_bottom = bottom;
}

void SDL2VideoDevice::Present() {
  glClear(GL_COLOR_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, textures[0]);
  if (buffer_top != nullptr) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_BGRA, GL_UNSIGNED_BYTE, buffer_top);
  }

  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(-1.0f,  1.0f);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f( 1.0f,  1.0f);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f( 1.0f,  0.0f);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(-1.0f,  0.0f);
  glEnd();

  glBindTexture(GL_TEXTURE_2D, textures[1]);
  if (buffer_bottom != nullptr) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 192, 0, GL_BGRA, GL_UNSIGNED_BYTE, buffer_bottom);
  }

  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(-1.0f,  0.0f);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f( 1.0f,  0.0f);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f( 1.0f, -1.0f);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(-1.0f, -1.0f);
  glEnd();

  SDL_GL_SwapWindow(window);
}