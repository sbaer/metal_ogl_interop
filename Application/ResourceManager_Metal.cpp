#include "stdafx.h"

#import <Metal/Metal.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl.h>
#import <OpenGL/gl3.h>
#import <AppKit/AppKit.h>

class CRhOpenGLMetalInteropTexture
{
public:
  static CRhOpenGLMetalInteropTexture* Create(NSOpenGLContext* glContext, int width, int height);
  int Width() const { return m_width;}
  int Height() const { return m_height;}
private:
  CRhOpenGLMetalInteropTexture() = default;
  
private:
  int m_width;
  int m_height;
  MTLPixelFormat m_metal_pixel_format;
  CVOpenGLTextureRef m_glTextureRef;
  
  CVPixelBufferRef _CVPixelBuffer;
  CVMetalTextureRef _CVMTLTexture;
  CVOpenGLTextureCacheRef _CVGLTextureCache;
  CGLPixelFormatObj _CGLPixelFormat;
  CVMetalTextureCacheRef _CVMTLTextureCache;

public:
  GLuint m_glTextureId;
  id<MTLTexture> m_metalTexture;
};

class CRhResourceManagerMetal
{
public:
  static CRhResourceManagerMetal& Get();

  id<MTLDevice> MetalDevice();
  id<MTLCommandQueue> MetalCommandQueue();
#if defined(ON_RUNTIME_APPLE)
  ON_wString GpuName() const;
#endif
  class CRhOpenGLMetalInteropTexture* GetInteropTexture(NSOpenGLContext* glContext, int width, int height);
private:
  CRhResourceManagerMetal() = default;
  void Initialize();
  
  id<MTLDevice> m_metalDevice = nullptr;
  id<MTLLibrary> m_metalBuiltInShaders = nullptr;
  
public:
  id<MTLCommandQueue> m_metalCommandQueue = nullptr;
  id<MTLRenderPipelineState> m_metalPipelineState = nullptr;
  MTLRenderPassDescriptor* m_metalRenderPassDescriptor = nullptr;

  
  class CRhOpenGLMetalInteropTexture* m_interop_texture = nullptr;
  static CRhResourceManagerMetal theMetalManager;
};



typedef struct {
    int                 cvPixelFormat;
    MTLPixelFormat      mtlFormat;
    GLuint              glInternalFormat;
    GLuint              glFormat;
    GLuint              glType;
} AAPLTextureFormatInfo;

const AAPLTextureFormatInfo* const textureFormatInfoFromMetalPixelFormat(MTLPixelFormat pixelFormat)
{
  // Table of equivalent formats across CoreVideo, Metal, and OpenGL
  static const AAPLTextureFormatInfo AAPLInteropFormatTable[] =
  {
      // Core Video Pixel Format,               Metal Pixel Format,            GL internalformat, GL format,   GL type
      { kCVPixelFormatType_32BGRA,              MTLPixelFormatBGRA8Unorm,      GL_RGBA,           GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV },
      { kCVPixelFormatType_ARGB2101010LEPacked, MTLPixelFormatBGR10A2Unorm,    GL_RGB10_A2,       GL_BGRA,     GL_UNSIGNED_INT_2_10_10_10_REV },
      { kCVPixelFormatType_32BGRA,              MTLPixelFormatBGRA8Unorm_sRGB, GL_SRGB8_ALPHA8,   GL_BGRA,     GL_UNSIGNED_INT_8_8_8_8_REV },
      { kCVPixelFormatType_64RGBAHalf,          MTLPixelFormatRGBA16Float,     GL_RGBA,           GL_RGBA,     GL_HALF_FLOAT },
  };
  static const int AAPLNumInteropFormats = sizeof(AAPLInteropFormatTable) / sizeof(AAPLTextureFormatInfo);

  for(int i = 0; i < AAPLNumInteropFormats; i++) {
    if(pixelFormat == AAPLInteropFormatTable[i].mtlFormat) {
      return &AAPLInteropFormatTable[i];
    }
  }
  return NULL;
}


CRhOpenGLMetalInteropTexture* CRhOpenGLMetalInteropTexture::Create(NSOpenGLContext* glContext, int width, int height)
{
  CGLPixelFormatObj glPixelFormat = [[glContext pixelFormat] CGLPixelFormatObj];
  if (nullptr == glPixelFormat)
    return nullptr;

  const MTLPixelFormat metalPixelFormat = MTLPixelFormatBGRA8Unorm;
  const AAPLTextureFormatInfo* formatInfo = textureFormatInfoFromMetalPixelFormat(metalPixelFormat);
  if (nullptr == formatInfo)
  {
    ON_ERROR("METAL: no formatInfo");
    return nullptr;
  }

  CRhOpenGLMetalInteropTexture* texture = new CRhOpenGLMetalInteropTexture();

  //CVPixelBufferRef corevideoPixelBuffer = nullptr;
  NSDictionary* cvBufferProperties = @{
      (__bridge NSString*)kCVPixelBufferOpenGLCompatibilityKey : @YES,
      (__bridge NSString*)kCVPixelBufferMetalCompatibilityKey : @YES,
  };
  CVReturn cvret = CVPixelBufferCreate(kCFAllocatorDefault,
                          width, height,
                          formatInfo->cvPixelFormat,
                          (__bridge CFDictionaryRef)cvBufferProperties,
                          &texture->_CVPixelBuffer);
  if (kCVReturnSuccess != cvret)
  {
    ON_ERROR("METAL: Failed to create core video pixel buffer");
    return nullptr;
  }

  
  // Create an OpenGL CoreVideo texture cache from the pixel buffer.
  //CVOpenGLTextureCacheRef corevideoGLTextureCache = nullptr;
  cvret = CVOpenGLTextureCacheCreate(
                  kCFAllocatorDefault,
                  nil,
                  [glContext CGLContextObj],
                  glPixelFormat,
                  nil,
                  &texture->_CVGLTextureCache);
  if (kCVReturnSuccess != cvret)
  {
    ON_ERROR("METAL: Failed to create OpenGL Texture Cache");
    return nullptr;
  }

  // Create a CVPixelBuffer-backed OpenGL texture image from the texture cache.
  //CVOpenGLTextureRef corevideoGLTexture = nullptr;
  cvret = CVOpenGLTextureCacheCreateTextureFromImage(
                  kCFAllocatorDefault,
                  texture->_CVGLTextureCache,
                  texture->_CVPixelBuffer,
                  nil,
                  &texture->m_glTextureRef);
  if (kCVReturnSuccess != cvret)
  {
    ON_ERROR("METAL: Failed to create OpenGL Texture From Image");
    return nullptr;
  }

  // Get an OpenGL texture name from the CVPixelBuffer-backed OpenGL texture image.
  GLuint openGLTextureId = CVOpenGLTextureGetName(texture->m_glTextureRef);
  if (0 == openGLTextureId)
  {
    ON_ERROR("METAL: Failed to get OpenGL Texture Id");
    return nullptr;
  }

  
  // Create a Metal Core Video texture cache from the pixel buffer.
  //CVMetalTextureCacheRef corevideoMTLTextureCache = nullptr;
  id<MTLDevice> metalDevice = CRhResourceManagerMetal::Get().MetalDevice();
  cvret = CVMetalTextureCacheCreate(
                  kCFAllocatorDefault,
                  nil,
                  metalDevice,
                  nil,
                  &texture->_CVMTLTextureCache);
  if (kCVReturnSuccess != cvret)
  {
    ON_ERROR("METAL: Failed to create Metal texture cache");
    return nullptr;
  }

  // Create a CoreVideo pixel buffer backed Metal texture image from the texture cache.
  //CVMetalTextureRef corevideoMTLTexture = nullptr;
  cvret = CVMetalTextureCacheCreateTextureFromImage(
                  kCFAllocatorDefault,
                  texture->_CVMTLTextureCache,
                  texture->_CVPixelBuffer, nil,
                  formatInfo->mtlFormat,
                  width, height,
                  0,
                  &texture->_CVMTLTexture);
  if (kCVReturnSuccess != cvret)
  {
    ON_ERROR("METAL: Failed to create CoreVideo Metal texture from image");
    return nullptr;
  }

  // Get a Metal texture using the CoreVideo Metal texture reference.
  id<MTLTexture> metalTexture = CVMetalTextureGetTexture(texture->_CVMTLTexture);
  if (nullptr == metalTexture)
  {
    ON_ERROR("METAL: Failed to create Metal texture CoreVideo Metal Texture");
    return nullptr;
  }

  texture->m_width = width;
  texture->m_height = height;
  texture->m_metal_pixel_format = metalPixelFormat;
  //texture->m_glTextureRef = corevideoGLTexture;
  texture->m_glTextureId = openGLTextureId;
  texture->m_metalTexture = metalTexture;
  return texture;
}

CRhResourceManagerMetal CRhResourceManagerMetal::theMetalManager;

CRhResourceManagerMetal& CRhResourceManagerMetal::Get()
{
  theMetalManager.Initialize();
  return theMetalManager;
}

void CRhResourceManagerMetal::Initialize()
{
  if (m_metalDevice != nullptr)
    return;

  m_metalDevice = MTLCreateSystemDefaultDevice();
  
  // Load all the shader files with a .metal file extension in the project
  NSBundle* bundle = NSBundle.mainBundle;
  NSError* error = nullptr;
  m_metalBuiltInShaders = [m_metalDevice newDefaultLibraryWithBundle: bundle error: &error];

  // Create the command queue
  m_metalCommandQueue = [m_metalDevice newCommandQueue];

  // Load the vertex/fragment functions from the library
  id<MTLFunction> vertexFunction = [m_metalBuiltInShaders newFunctionWithName:@"vertexShader"];
  id<MTLFunction> fragmentFunction = [m_metalBuiltInShaders newFunctionWithName:@"fragmentShader"];

  // Create a reusable pipeline state
  MTLRenderPipelineDescriptor* pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
  [pipelineStateDescriptor setLabel:@"MyPipeline"];
  [pipelineStateDescriptor setVertexFunction:vertexFunction];
  [pipelineStateDescriptor setFragmentFunction:fragmentFunction];
  pipelineStateDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

  m_metalPipelineState = [m_metalDevice newRenderPipelineStateWithDescriptor:pipelineStateDescriptor error:&error];
  
  m_metalRenderPassDescriptor = [MTLRenderPassDescriptor new];
  m_metalRenderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 1, 0, 1);
  m_metalRenderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
  m_metalRenderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
}

id<MTLDevice> CRhResourceManagerMetal::MetalDevice()
{
  return m_metalDevice;
}

id<MTLCommandQueue> CRhResourceManagerMetal::MetalCommandQueue()
{
  return m_metalCommandQueue;
}

CRhOpenGLMetalInteropTexture* CRhResourceManagerMetal::GetInteropTexture(NSOpenGLContext* glContext, int width, int height)
{
  if(m_interop_texture)
  {
    if( m_interop_texture->Width() == width && m_interop_texture->Height() == height)
      return m_interop_texture;
  }
  CRhOpenGLMetalInteropTexture* texture = CRhOpenGLMetalInteropTexture::Create(glContext, width, height);
  if( texture)
  {
    if( m_interop_texture)
      delete m_interop_texture;
    m_interop_texture = texture;
    return m_interop_texture;
  }
  return nullptr;
}

#if defined(ON_RUNTIME_APPLE)
 ON_wString CRhResourceManagerMetal::GpuName() const
 {
   NSString* name = [m_metalDevice  name];
   ON_wString n = [name ONwString];
   return n;
 }
 
 class CRhMetalConduit : public CRhinoDisplayConduit
 {
 public:
   CRhMetalConduit() : CRhinoDisplayConduit(CSupportChannels::SC_DRAWFOREGROUND){}

   bool ExecConduit(CRhinoDisplayPipeline& dp, UINT nChannelID, bool& bTerminate) override
   {
     CRhinoDisplayEngine_OGL* engine = dynamic_cast<CRhinoDisplayEngine_OGL*>(dp.Engine());
     if (engine)
     {
       CRhEngine_GL33::DrawBitmap(*engine, m_metal_texture, 200, 200, 100, 50);
       // engine->DrawRoundedRectangle(ON_2fPoint(200,200), 100, 50, 6, ON_Color(255,0,0), 2, ON_Color(0,255,0));
     }
     return true;
   }
   
   GLuint m_metal_texture = 0;
 };

 class CCommandTestMetal : public CRhinoTestCommand
 {
   ON_UUID m_id = ON_nil_uuid;
 public:
   CCommandTestMetal() {
     ON_CreateUuid(m_id);
   }
   UUID CommandUUID() override
   {
     return m_id;
   }

   const wchar_t* EnglishCommandName() override { return L"TestMetal"; }
   CRhinoCommand::result RunCommand(const CRhinoCommandContext&) override;
   CRhMetalConduit m_conduit;
 };
 static CCommandTestMetal theTestMetalCommand;

 static GLuint DrawMetalToTexture(int width, int height)
 {
   CRhResourceManagerMetal& mgr = CRhResourceManagerMetal::Get();
   NSOpenGLContext* glContext = CRhResourceManager_OGL::SharedContext();
   
   CRhOpenGLMetalInteropTexture* interopTexture = mgr.GetInteropTexture(512, 512);
   if( nullptr == interopTexture)
     return 0;
   
   // Create a new command buffer for each renderpass to the current drawable
   id<MTLCommandBuffer> commandBuffer = [mgr.m_metalCommandQueue commandBuffer];

   mgr.m_metalRenderPassDescriptor.colorAttachments[0].texture = interopTexture->m_metalTexture;

   // If a renderPassDescriptor has been obtained, render to the drawable, otherwise skip
   // any rendering this frame because there is no drawable to draw to
   id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:mgr.m_metalRenderPassDescriptor];

   // Done encoding commands
   [renderEncoder endEncoding];
   [commandBuffer commit];

   return interopTexture->m_glTextureId;
 }

 CRhinoCommand::result CCommandTestMetal::RunCommand(const CRhinoCommandContext& context)
 {
   CRhResourceManagerMetal& mgr = CRhResourceManagerMetal::Get();
   ON_wString n = mgr.GpuName();
   RhinoApp().Print(L"Metal Info\n");
   RhinoApp().Print(L"- device name: %S\n", n.Array());

   GLuint textureId = DrawMetalToTexture(512, 512);
   m_conduit.m_metal_texture = textureId;
  
   m_conduit.Enable();
   context.m_doc.Redraw();
   return CRhinoCommand::success;
 }
#endif


GLuint DrawMetal(NSOpenGLContext* glContext, CGSize textureSize)
{
  CRhResourceManagerMetal& mgr = CRhResourceManagerMetal::Get();
  CRhOpenGLMetalInteropTexture* interopTexture = mgr.GetInteropTexture(glContext, textureSize.width, textureSize.height);
    
  // Create a new command buffer for each renderpass to the current drawable
  id<MTLCommandBuffer> commandBuffer = [mgr.m_metalCommandQueue commandBuffer];

  mgr.m_metalRenderPassDescriptor.colorAttachments[0].texture = interopTexture->m_metalTexture;

  // If a renderPassDescriptor has been obtained, render to the drawable, otherwise skip
  // any rendering this frame because there is no drawable to draw to

  id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:mgr.m_metalRenderPassDescriptor];

  // Done encoding commands
  [renderEncoder endEncoding];

  [commandBuffer commit];

  return interopTexture->m_glTextureId;
}
