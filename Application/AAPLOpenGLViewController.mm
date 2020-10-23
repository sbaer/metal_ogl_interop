/*
See LICENSE folder for this sample’s licensing information.

Abstract:
Implementation of the cross-platform OpenGL view controller AND a minimal cross-platform OpenGL View
*/
#import "AAPLOpenGLViewController.h"
#import "AAPLOpenGLRenderer.h"
#import "stdafx.h"


@implementation AAPLOpenGLView
@end


@implementation AAPLOpenGLViewController
{
  AAPLOpenGLRenderer* _openGLRenderer;
  NSOpenGLContext* _context;
  CVDisplayLinkRef _displayLink;
}

- (void)viewDidLoad
{
  [super viewDidLoad];

  NSOpenGLPixelFormatAttribute attrs[] =
  {
    NSOpenGLPFAColorSize, 32,
    NSOpenGLPFADoubleBuffer,
    NSOpenGLPFADepthSize, 24,
    0
  };
  NSOpenGLPixelFormat *pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
  NSAssert(pixelFormat, @"No OpenGL pixel format");

  _context = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];

  NSOpenGLView* view = (NSOpenGLView *)self.view;
  view.pixelFormat = pixelFormat;
  view.openGLContext = _context;

  CVDisplayLinkCreateWithActiveCGDisplays(&_displayLink);
  // Set the renderer output callback function
  CVDisplayLinkSetOutputCallback(_displayLink, &OpenGLDisplayLinkCallback, (__bridge void*)self);
  CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(_displayLink, _context.CGLContextObj, pixelFormat.CGLPixelFormatObj);

  [_context makeCurrentContext];

  _openGLRenderer = [[AAPLOpenGLRenderer alloc] init];

  CGSize viewSizePoints = self.view.bounds.size;
  CGSize viewSizePixels = [self.view convertSizeToBacking:viewSizePoints];
  [_openGLRenderer resize:viewSizePixels];
}

static CVReturn OpenGLDisplayLinkCallback(CVDisplayLinkRef displayLink,
                                         const CVTimeStamp* now,
                                         const CVTimeStamp* outputTime,
                                         CVOptionFlags flagsIn,
                                         CVOptionFlags* flagsOut,
                                         void* displayLinkContext)
{
    AAPLOpenGLViewController *viewController = (__bridge AAPLOpenGLViewController*)displayLinkContext;

    [viewController draw];
    return YES;
}

- (void)draw
{
  CGLLockContext(_context.CGLContextObj);

  [_context makeCurrentContext];

  GLuint glTextureId = DrawMetal(_context, AAPLInteropTextureSize);

  // Set texture that OpenGL renders as the interop texture
  [_openGLRenderer useInteropTextureAsBaseMap:glTextureId];

  [_openGLRenderer draw];

  CGLFlushDrawable(_context.CGLContextObj);
  CGLUnlockContext(_context.CGLContextObj);
}

- (void)viewDidLayout
{
  NSSize viewSizePoints = self.view.bounds.size;
  NSSize viewSizePixels = [self.view convertSizeToBacking:viewSizePoints];
  [_openGLRenderer resize:viewSizePixels];

  if(!CVDisplayLinkIsRunning(_displayLink))
  {
    CVDisplayLinkStart(_displayLink);
  }
}

- (void) viewWillDisappear
{
  CVDisplayLinkStop(_displayLink);
}

- (void)dealloc
{
  CVDisplayLinkStop(_displayLink);
  CVDisplayLinkRelease(_displayLink);
}

@end
