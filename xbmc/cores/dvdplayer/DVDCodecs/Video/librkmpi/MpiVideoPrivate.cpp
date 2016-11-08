#include "MpiVideoPrivate.h"

#include <utils/String8.h>
extern "C" {
#include "libavcodec/avcodec.h"
}
#include "utils/log.h"
#include "utils/StringUtils.h"

#define ALIGN(value, x) ((value + (x-1)) & (~(x-1)))
#define private_err(fmt, ...) CLog::Log(LOGERROR, fmt, ## __VA_ARGS__)

RK_U32 rkv_debug = 0;
#define RK_FUN_T(tag) \
    do {\
        if (RK_DBG_FUNCTION & rkv_debug)\
            { private_err("%s: line(%d), func(%s)", tag, __LINE__, __FUNCTION__); }\
    } while (0)

static RK_S32 align_8(RK_S32 val)
{
  return ALIGN(val, 8);
}

static RK_S32 align_16(RK_S32 val)
{
  return ALIGN(val, 16);
}

static RK_S32 align_256(RK_S32 val)
{
  return ALIGN(val, 256);
}

static RK_S32 align_256_odd(RK_S32 val)
{
  return ALIGN(val, 256) | 256;
}

MpiVideoPrivate::MpiVideoPrivate() 
{
  m_pSurface = NULL;
  m_pSurfaceClient = NULL;
  m_pSurfaceControl = NULL;
  m_pNativeWindow = NULL;
}

MpiVideoPrivate::~ MpiVideoPrivate()
{
  
}

RK_RET MpiVideoPrivate::Open(CDVDStreamInfo &hints)
{  
  RK_FUN_T("FUN_I");
  
  AVCodecID codec_id = hints.codec;
  RK_U32 codec_tag = hints.codec_tag;
  MppCodingType mpi_coding = MPP_VIDEO_CodingUnused;
  
  if ((codec_tag == MKTAG('3', 'I', 'V', 'D')) ||
      (codec_tag == MKTAG('D', 'I', 'V', 'X')) ||
      (codec_tag == MKTAG('3', 'I', 'V', '2')) )
  {
    private_err("not support divx");
    return RK_NOK;
  }
  
  char name[5] = {0};
  if (name != NULL) 
  {
    name[0] = (codec_tag & 0xff);
    name[1] = (codec_tag & 0xff00) >> 8;
    name[2] = (codec_tag & 0x00ff0000) >> 16;
    name[3] = (codec_tag & 0xff000000) >> 24;
  }
  
  std::string codecName(name);
  StringUtils::ToLower(codecName);
  if (strstr(codecName.c_str(),"div") != 0 || strstr(codecName.c_str(),"vid") != 0 ||
            strstr(codecName.c_str(),"dx50") != 0) 
  {
    private_err("not support divx");
    return RK_NOK;
  }

  switch (codec_id) 
  {
    case AV_CODEC_ID_RV10:
    case AV_CODEC_ID_RV20:
    case AV_CODEC_ID_RV30:
    case AV_CODEC_ID_RV40:
      mpi_coding = MPP_VIDEO_CodingRV;
      return RK_NOK;
    case AV_CODEC_ID_VP6:
    case AV_CODEC_ID_VP6F:
    case AV_CODEC_ID_VP6A:
      mpi_coding = MPP_VIDEO_CodingVP6;
      return RK_NOK;
    case AV_CODEC_ID_MPEG1VIDEO:
    case AV_CODEC_ID_MPEG2VIDEO:
      mpi_coding = MPP_VIDEO_CodingMPEG2;
      return RK_NOK;
    case AV_CODEC_ID_MPEG4:
      mpi_coding = MPP_VIDEO_CodingMPEG4;
      if (hints.width >= 3840 || hints.height >= 2160)
      {
        private_err("mpeg not support video_size %dx%d", hints.width, hints.height);
        return RK_NOK;
      }
      break;
    case AV_CODEC_ID_FLV1:
      mpi_coding = MPP_VIDEO_CodingFLV1; 
      break;
    case AV_CODEC_ID_H264:
      mpi_coding = MPP_VIDEO_CodingAVC;
      break;
    case AV_CODEC_ID_VC1:
      mpi_coding = MPP_VIDEO_CodingVC1;
      break;
    case AV_CODEC_ID_WMV3:
      mpi_coding = MPP_VIDEO_CodingWMV;
      break;
    case AV_CODEC_ID_VP8:
      mpi_coding = MPP_VIDEO_CodingVP8;
      break;
    case AV_CODEC_ID_HEVC:
      mpi_coding = MPP_VIDEO_CodingHEVC;
      break;
    default:
      private_err("not support codec %d", codec_id);
      return RK_NOK;
  }

  if (InitSurfaceTexture())
  {
    private_err("init surface texture fail");
    return RK_NOK;
  }

  InitBufferParam param;
  param.codec = codec_id;
  param.width = hints.width;
  param.height = hints.height;
  param.type = BUFFER_TYPE_NATIVE;
  param.count = 18;

  if (InitOutputBuffers(param) || AllocateOutputBuffersFromNativeWindow())
  {
    private_err("allocate output buffers fail");
    m_pNativeWindow.clear();
    m_pSurfaceControl.clear();
    m_pSurfaceClient.clear();
    return RK_NOK;
  }

  m_pMpi = new MpiWrapper();
  if (m_pMpi->init(mpi_coding, hints.width, hints.height) < 0)
  {
    
  }
    
  RK_FUN_T("FUN_O");
  return RK_OK;
}

RK_RET MpiVideoPrivate::InitSurfaceTexture()
{
  RK_FUN_T("FUN_I");
  if (m_pNativeWindow != NULL)
    goto _FAIL;
  
  m_pSurfaceClient = new android::SurfaceComposerClient();
  if (m_pSurfaceClient == NULL)
    goto _FAIL;

  m_pSurfaceControl = m_pSurfaceClient->createSurface(android::String8(), 1280, 720, android::PIXEL_FORMAT_RGBA_8888, 0);
  if (!m_pSurfaceControl->isValid())
    goto _FAIL;
  
  android::SurfaceComposerClient::openGlobalTransaction();
  if (m_pSurfaceControl->setLayer(1000))
    goto _FAIL;
  if (m_pSurfaceControl->show())
    goto _FAIL;
  android::SurfaceComposerClient::closeGlobalTransaction();
  
  m_pSurface = m_pSurfaceControl->getSurface();

  if (m_pSurface == NULL)
    goto _FAIL;

  m_pNativeWindow = m_pSurface;
  
  RK_FUN_T("FUN_O");
  return RK_OK;
  
_FAIL:
  private_err("init surface texture failed.");
  m_pSurfaceClient.clear();
  m_pSurfaceControl.clear();
  m_pSurface.clear();
  m_pNativeWindow.clear();  
  RK_FUN_T("FUN_O");
  return RK_NOK;
}

RK_RET MpiVideoPrivate::InitOutputBuffers(const InitBufferParam p)
{  
  RK_FUN_T("FUN_I");
  m_codec = p.codec;
  m_videoWidth = m_displayWidth = p.width;
  m_videoHeight = m_displayHeight = p.height;
  m_bufferCount = p.count;
  
  switch(m_codec) 
  {
  case AV_CODEC_ID_HEVC:
    m_bufferWidth = align_256_odd(m_videoWidth);
    m_bufferHeight = align_8(m_videoHeight);
    break;
    
  case AV_CODEC_ID_H264:
    m_bufferWidth = align_16(m_videoWidth);
    if (m_videoWidth > 1920 || m_videoHeight > 1088)
      m_bufferWidth = align_256_odd(m_videoWidth);
    m_bufferHeight = align_16(m_videoHeight);
    break;
    
  default:
    m_bufferWidth = align_16(m_videoWidth);
    m_bufferHeight = align_16(m_videoHeight);
  }

  m_format = HAL_PIXEL_FORMAT_YCrCb_NV12;
  
  RK_FUN_T("FUN_O");
  return RK_OK;
}

RK_RET MpiVideoPrivate::AllocateOutputBuffersFromNativeWindow()
{  
  RK_FUN_T("FUN_I");
  RK_S32 err = 0;
  RK_S32 queues = 0;
  RK_U32 usage = 0;
  RK_S32 min_undequeue_bufs = 0;

  if (m_pNativeWindow == NULL) {
    private_err("no native window.");
    return RK_NOK;
  }
  err = native_window_set_buffers_geometry(m_pNativeWindow.get(), m_bufferWidth, m_bufferHeight, m_format);
  native_window_set_scaling_mode(m_pNativeWindow.get(), NATIVE_WINDOW_SCALING_MODE_SCALE_TO_WINDOW);
  if (err != 0) {
    private_err("native_window_set_buffers_geometry failed: %s (%d)",strerror(-err), -err);
    return RK_NOK;
  }

  android_native_rect_t crop;
  crop.left = crop.top = 0;
  crop.right = m_displayWidth;
  crop.bottom = m_displayHeight;
  err = native_window_set_crop(m_pNativeWindow.get(), &crop);
  if (err != 0) {
    private_err("native_window_set_crop failed: %s (%d)",strerror(-err), -err);
    return RK_NOK;
  }

  err = m_pNativeWindow->query(m_pNativeWindow.get(),
                               NATIVE_WINDOW_QUEUES_TO_WINDOW_COMPOSER, &queues);
  if (err != 0 && queues != 1) {
    private_err("native window could not be authenticated: %s (%d)",strerror(-err), -err);
  }

  usage = GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_EXTERNAL_DISP;
  err = native_window_set_usage(m_pNativeWindow.get(),  usage);
  if (err != 0) {
    private_err("native_window_set_usage failed: %s (%d)", strerror(-err), -err);
    return RK_NOK;
  }

  err = m_pNativeWindow->query(m_pNativeWindow.get(),
                               NATIVE_WINDOW_MIN_UNDEQUEUED_BUFFERS, &min_undequeue_bufs);
  if (err != 0) {
    private_err("native_window_min_undequeued_buffers query failed: %s (%d)",strerror(-err), -err);
    return RK_NOK;
  }

  err = native_window_set_buffer_count(m_pNativeWindow.get(), m_bufferCount);
  if (err != 0) {
    private_err("native_window_set_buffer_count failed: %s (%d)", strerror(-err),-err);
    return RK_NOK;
  }

  for (int i = 0; i < m_bufferCount; i++) {    
    ANativeWindowBuffer* buf;
    err = m_pNativeWindow->dequeueBuffer(m_pNativeWindow.get(), &buf);
    if (err != 0) {
        private_err("dequeueBuffer failed: %s (%d)", strerror(-err), -err);
        break;
    }

    android::sp<android::GraphicBuffer> graphicBuffer(new android::GraphicBuffer(buf, false));
    BufferInfo info;
    info.status = OWNED_BY_US;
    info.graphic_buffer = graphicBuffer;
    info.registerd = false;
    info.undequeued = false;
    info.share_fd = buf->handle->data[0];
    info.type = REGISTER_NONE;
    info.dup_fd = 0;

    m_buffers.push(info);
  }

  RK_U32 cancelStart;
  RK_U32 cancelEnd;

  if (err != 0) {
    cancelStart = 0;
    cancelEnd = m_buffers.size();
  } else {
    cancelStart = m_bufferCount- min_undequeue_bufs;
    cancelEnd = m_bufferCount;
  }

  for (RK_U32 i = cancelStart; i < cancelEnd; i++) {
    BufferInfo *info = &m_buffers.editItemAt(i);
    CancelBufferToNativeWindow(info);
  }
  
  RK_FUN_T("FUN_O");
  return RK_OK;
}

RK_RET MpiVideoPrivate::CancelBufferToNativeWindow(BufferInfo *info) 
{  
  RK_FUN_T("FUN_I");
  if (info->status == OWNED_BY_US)
  {
    int err = m_pNativeWindow->cancelBuffer(m_pNativeWindow.get(), info->graphic_buffer.get());
    if (err != 0)
    {
      private_err("cancelBuffer fail failed: %s (%d)",strerror(-err), -err);
      return RK_NOK;
    }
    info->status = OWNED_BY_NATIVE_WINDOW;
  } else {
    private_err("buffer already cancled fd: %d", info->share_fd);
  }
  
  RK_FUN_T("FUN_O");
  return RK_OK;
}

