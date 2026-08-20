/**************************************************************************\
 * Copyright (c) 2026 FreeCAD contributors                              *
 *                                                                        *
 * This file is part of Coin.                                            *
 *                                                                        *
 * Coin is free software; you can redistribute it and/or modify it under *
 * the terms of the GNU General Public License as published by the Free  *
 * Software Foundation; either version 2 of the License, or (at your      *
 * option) any later version.                                            *
\**************************************************************************/

#ifndef COIN_TEST_GLTESTCONTEXT_H
#define COIN_TEST_GLTESTCONTEXT_H

#include <cstdint>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "GLTestFramebuffer.h"

enum class GLTestProfile {
  Core,
  Compatibility
};

struct GLTestContextConfig {
  GLTestProfile profile = GLTestProfile::Core;
  int major = 3;
  int minor = 3;
  int width = 64;
  int height = 64;
  bool visible = false;
  bool vsync = false;
};

class GLTestContext {
public:
  GLTestContext();
  ~GLTestContext();

  GLTestContext(const GLTestContext &) = delete;
  GLTestContext & operator=(const GLTestContext &) = delete;

  bool initialize(const GLTestContextConfig & config);
  void shutdown();

  bool makeCurrent();
  bool resizeFramebuffer(int width, int height);
  void bindFramebuffer();
  void present();
  void pollEvents();
  bool shouldClose() const;
  GLFWwindow * window() const { return this->window; }
  std::vector<uint8_t> readPixels() const;

  bool isCoreProfile() const { return profile_ == GLTestProfile::Core; }
  int majorVersion() const { return majorVersion_; }
  int minorVersion() const { return minorVersion_; }
  int contextId() const { return contextId_; }
  bool initialized() const { return window_ != NULL && framebuffer_.isInitialized(); }

private:
  GLFWwindow * window_;
  GLTestFramebuffer framebuffer_;
  GLTestProfile profile_;
  int majorVersion_;
  int minorVersion_;
  int contextId_;
  bool glfwUser_;
};

#endif // COIN_TEST_GLTESTCONTEXT_H
