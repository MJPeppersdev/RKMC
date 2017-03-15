
#if __cplusplus >= 201103L
#define char16_t LIBRARY_char16_t
#define char32_t LIBRARY_char32_t
#endif
#include <ui/GraphicBuffer.h>
#include <ui/PixelFormat.h>
#include <gui/SurfaceTexture.h>
#include <surfaceflinger/SurfaceComposerClient.h>
#if __cplusplus >= 201103L
#undef char16_t
#undef char32_t
#endif
#include <utils/Vector.h>
#include "cores/dvdplayer/DVDStreamInfo.h"
#include "MpiWrapper.h"

#define RK_DBG_FUNCTION (0x00000001)

enum RequestBufferType {
  BUFFER_TYPE_NATIVE,
  BUFFER_TYPE_ION
};

enum BufferType {
  REGISTER_NONE,
  REGISTER_TO_VPU,
  REGISTER_TO_IEP,
  REGISTER_TO_RGA,
};

typedef struct InitBufferParam_t
{
  RK_S32 width;
  RK_S32 height;
  RK_S32 codec;
  RequestBufferType  type;
  RK_U8  count;
} InitBufferParam;

#define HAL_PIXEL_FORMAT_YCrCb_NV12	0x20

enum BufferStatus {
  OWNED_BY_US,
  OWNED_BY_COMPONENT,
  OWNED_BY_NATIVE_WINDOW,
  OWNED_BY_CLIENT,
  OWNED_BY_IEP,
  OWNED_BY_VPU,
};

struct BufferInfo {
  RK_S32 dup_fd;
  RK_S32 share_fd;
  RK_U64 phy_addr;
  BufferStatus status;
  RK_U32 size;
  android::sp<android::GraphicBuffer> graphic_buffer;
  android::sp<ANativeWindow> window;
  BufferType type;
  bool registerd;
  bool undequeued;
};


class MpiVideoPrivate
{
public:
  MpiVideoPrivate();
  ~MpiVideoPrivate();
  
  RK_RET InitSurfaceTexture();
  RK_RET InitOutputBuffers(const InitBufferParam p);
  RK_RET AllocateOutputBuffersFromNativeWindow();
  RK_RET CancelBufferToNativeWindow(BufferInfo *info);

public:
  RK_RET Open(CDVDStreamInfo &hints);

private:
  android::sp<android::SurfaceComposerClient> 	m_pSurfaceClient;
  android::sp<android::SurfaceControl>        	m_pSurfaceControl;
  android::sp<android::Surface>               	m_pSurface;
  android::sp<ANativeWindow> 			        m_pNativeWindow;

  android::Vector<BufferInfo> m_buffers;

  MpiWrapper* m_pMpi; 

  RK_S32 m_videoWidth;
  RK_S32 m_videoHeight;
  RK_S32 m_displayWidth;
  RK_S32 m_displayHeight;
  RK_S32 m_bufferWidth;
  RK_S32 m_bufferHeight;

  RK_S32 m_format;
  RK_S32 m_codec;

  RK_U8  m_bufferCount;
  
};
