#pragma once

#include <Metal/Metal.h>
#include <AppKit/AppKit.h>

GLuint DrawMetal(NSOpenGLContext* glContext, CGSize textureSize);

void ON_ERROR(const char* msg);

