/*
 *      Copyright (C) 2005-2013 Team XBMC
 *      http://xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, see
 *  <http://www.gnu.org/licenses/>.
 *
 */
 
#include "settings/Settings.h"
#include "settings/MediaSettings.h"
#include "settings/AdvancedSettings.h"
#include "video/VideoReferenceClock.h"
#include "DVDPlayerVideoRK.h"
#include "DVDVideoCodecRK.h"
#include "cores/dvdplayer/DVDPlayer.h"
#include "cores/dvdplayer/DVDPlayerVideo.h"
#include "cores/dvdplayer/DVDCodecs/DVDFactoryCodec.h"
#include "cores/dvdplayer/DVDCodecs/DVDCodecUtils.h"
#include "cores/dvdplayer/DVDCodecs/Video/DVDVideoPPFFmpeg.h"
#include "cores/dvdplayer/DVDCodecs/Video/DVDVideoCodecFFmpeg.h"
#include "cores/dvdplayer/DVDCodecs/Video/DVDVideoCodecLibMpeg2.h"
#include "DVDOverlayRenderer.h"
#include "guilib/GraphicContext.h"
#include "cores/VideoRenderers/RenderFlags.h"
#include "utils/log.h"
#include <sstream>
#include <iomanip>
#include <numeric>
#include <iterator>
#include <sys/prctl.h>

class CPulldownCorrection
{
public:
  CPulldownCorrection()
  {
    m_duration = 0.0;
    m_accum    = 0;
    m_total    = 0;
    m_next     = m_pattern.end();
  }

  void init(double fps, int *begin, int *end)
  {
    std::copy(begin, end, std::back_inserter(m_pattern));
    m_duration = DVD_TIME_BASE / fps;
    m_accum    = 0;
    m_total    = std::accumulate(m_pattern.begin(), m_pattern.end(), 0);
    m_next     = m_pattern.begin();
  }

  double pts()
  {
    double input  = m_duration * std::distance(m_pattern.begin(), m_next);
    double output = m_duration * m_accum / m_total;
    return output - input;
  }

  double dur()
  {
    return m_duration * m_pattern.size() * *m_next / m_total;
  }

  void next()
  {
    m_accum += *m_next;
    if(++m_next == m_pattern.end())
    {
      m_next  = m_pattern.begin();
      m_accum = 0;
    }
  }

  bool enabled()
  {
    return !m_pattern.empty();
  }
private:
  double                     m_duration;
  int                        m_total;
  int                        m_accum;
  std::vector<int>           m_pattern;
  std::vector<int>::iterator m_next;
};


class CDVDMsgVideoCodecChange : public CDVDMsg
{
public:
  CDVDMsgVideoCodecChange(const CDVDStreamInfo &hints, CDVDVideoCodec* codec)
    : CDVDMsg(GENERAL_STREAMCHANGE)
    , m_codec(codec)
    , m_hints(hints)
  {}
 ~CDVDMsgVideoCodecChange()
  {
    delete m_codec;
  }
  CDVDVideoCodec* m_codec;
  CDVDStreamInfo  m_hints;
};

CDVDPlayerVideoRK::CDVDPlayerVideoRK( CDVDClock* pClock
                                , CDVDOverlayContainer* pOverlayContainer
                                , CDVDMessageQueue& parent)
: CDVDPlayerVideo(pClock, pOverlayContainer, parent)
{

}

CDVDPlayerVideoRK::~CDVDPlayerVideoRK()
{

}

CDVDVideoCodec* CDVDPlayerVideoRK::OpenCodec(CDVDVideoCodec* pCodec, CDVDStreamInfo &hints, CDVDCodecOptions &options )
{
  try
  {
    CLog::Log(LOGDEBUG, "CDVDPlayerVideoRK - Video: %s - Opening", pCodec->GetName());
    if( pCodec->Open( hints, options ) )
    {
      CLog::Log(LOGDEBUG, "CDVDPlayerVideoRK - Video: %s - Opened", pCodec->GetName());
      return pCodec;
    }

    CLog::Log(LOGDEBUG, "CDVDPlayerVideoRK - Video: %s - Failed", pCodec->GetName());
    pCodec->Dispose();
    delete pCodec;
  }
  catch(...)
  {
    CLog::Log(LOGERROR, "CDVDPlayerVideoRK - Video: Failed with exception");
  }
  return NULL;
}


CDVDVideoCodec* CDVDPlayerVideoRK::CreateVideoCodec(CDVDStreamInfo &hint, const CRenderInfo &info)
{
  CDVDVideoCodec* pCodec = NULL;
  CDVDCodecOptions options;
  
  if(info.formats.empty())
    options.m_formats.push_back(RENDER_FMT_YUV420P);
  else
    options.m_formats = info.formats;

  options.m_opaque_pointer = info.opaque_pointer;
  if ((hint.codec == AV_CODEC_ID_MPEG2VIDEO || hint.codec == AV_CODEC_ID_MPEG1VIDEO) && !CSettings::GetInstance().GetBool(RKMC_SETTING_RKCODEC_MPEG2))
  {
    if ((pCodec = OpenCodec(new CDVDVideoCodecLibMpeg2(), hint, options))) return pCodec;
  }

  if (hint.codec == AV_CODEC_ID_VP9 && !CSettings::GetInstance().GetBool(RKMC_SETTING_RKCODEC_VP9))
  {
    if ( (pCodec = OpenCodec(new CDVDVideoCodecFFmpeg(), hint, options)) ) return pCodec;
  }
  
  if( (pCodec = OpenCodec(new CDVDVideoCodecRK(), hint, options)) ) return pCodec;
  
  if( (pCodec = OpenCodec(new CDVDVideoCodecFFmpeg(), hint, options)) ) return pCodec;

  return NULL;
}

bool CDVDPlayerVideoRK::OpenStream( CDVDStreamInfo &hint )
{
  CRenderInfo info;
  #ifdef HAS_VIDEO_PLAYBACK
  info = g_renderManager.GetRenderInfo();
  #endif

  m_pullupCorrection.ResetVFRDetection();
  if(hint.flags & AV_DISPOSITION_ATTACHED_PIC)
    return false;

  if (m_pVideoCodec)
  {
    delete m_pVideoCodec;
    m_pVideoCodec = NULL;
  }

  CLog::Log(LOGNOTICE, "Creating video codec with codec id: %i", hint.codec);
  CDVDVideoCodec* codec = CreateVideoCodec(hint, info);
  if(!codec)
  {
    CLog::Log(LOGERROR, "Unsupported video codec");
    return false;
  }

  g_VideoReferenceClock.Start();

  if(m_messageQueue.IsInited())
    m_messageQueue.Put(new CDVDMsgVideoCodecChange(hint, codec), 0);
  else
  {
    CDVDPlayerVideo::OpenStream(hint, codec);
    CLog::Log(LOGNOTICE, "Creating video thread");
    m_messageQueue.Init();
    Create();
  }
  return true;
}

void CDVDPlayerVideoRK::Process()
{
  CLog::Log(LOGNOTICE, "running thread: video_thread");

  DVDVideoPicture picture;
  CPulldownCorrection pulldown;
  CDVDVideoPPFFmpeg mPostProcess("");
  std::string sPostProcessType;
  bool bPostProcessDeint = false;

  memset(&picture, 0, sizeof(DVDVideoPicture));

  double pts = 0;
  double frametime = (double)DVD_TIME_BASE / m_fFrameRate;

  int iDropped = 0; //frames dropped in a row
  bool bRequestDrop = false;
  int iDropDirective;

  prctl(PR_SET_NAME, (unsigned long)"CDVDPlayerVideoRK", 0, 0, 0);
  m_videoStats.Start();
  m_droppingStats.Reset();

  while (!m_bStop)
  {
    int iQueueTimeOut = (int)(m_stalled ? frametime / 4 : frametime * 10) / 1000;
    int iPriority = (m_speed == DVD_PLAYSPEED_PAUSE && m_started) ? 1 : 0;

    CDVDMsg* pMsg;
    MsgQueueReturnCode ret = m_messageQueue.Get(&pMsg, iQueueTimeOut, iPriority);

    if (MSGQ_IS_ERROR(ret))
    {
      CLog::Log(LOGERROR, "Got MSGQ_ABORT or MSGO_IS_ERROR return true");
      break;
    }
    else if (ret == MSGQ_TIMEOUT)
    {
      // if we only wanted priority messages, this isn't a stall
      if( iPriority )
        continue;

      //Okey, start rendering at stream fps now instead, we are likely in a stillframe
      if( !m_stalled )
      {
        if(m_started)
          CLog::Log(LOGINFO, "CDVDPlayerVideo - Stillframe detected, switching to forced %f fps", m_fFrameRate);
        m_stalled = true;
        pts+= frametime*4;
      }

      //Waiting timed out, output last picture
      if( picture.iFlags & DVP_FLAG_ALLOCATED )
      {
        //Remove interlaced flag before outputting
        //no need to output this as if it was interlaced
        picture.iFlags &= ~DVP_FLAG_INTERLACED;
        picture.iFlags |= DVP_FLAG_NOSKIP;
        OutputPicture(&picture, pts);
        pts+= frametime;
      }

      continue;
    }

    if (pMsg->IsType(CDVDMsg::GENERAL_SYNCHRONIZE))
    {
      if(((CDVDMsgGeneralSynchronize*)pMsg)->Wait(100, SYNCSOURCE_VIDEO))
      {
        CLog::Log(LOGDEBUG, "CDVDPlayerVideo - CDVDMsg::GENERAL_SYNCHRONIZE");

        /* we may be very much off correct pts here, but next picture may be a still*/
        /* make sure it isn't dropped */
        m_iNrOfPicturesNotToSkip = 5;
      }
      else
        m_messageQueue.Put(pMsg->Acquire(), 1); /* push back as prio message, to process other prio messages */
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_RESYNC))
    {
      CDVDMsgGeneralResync* pMsgGeneralResync = (CDVDMsgGeneralResync*)pMsg;

      if(pMsgGeneralResync->m_timestamp != DVD_NOPTS_VALUE)
        pts = pMsgGeneralResync->m_timestamp;

      double absolute = m_pClock->GetAbsoluteClock();
      double delay = m_FlipTimeStamp - absolute;
      if( delay > frametime ) delay = frametime;
      else if( delay < 0 )    delay = 0;
      m_FlipTimePts = pts -frametime;

      if(pMsgGeneralResync->m_clock)
      {
        CLog::Log(LOGDEBUG, "CDVDPlayerVideo - CDVDMsg::GENERAL_RESYNC(%f, 1)", pts);
        m_pClock->Discontinuity(m_FlipTimePts, absolute);
      }
      else
        CLog::Log(LOGDEBUG, "CDVDPlayerVideo - CDVDMsg::GENERAL_RESYNC(%f, 0)", pts);

      pMsgGeneralResync->Release();
      continue;
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_DELAY))
    {
      if (m_speed != DVD_PLAYSPEED_PAUSE)
      {
        double timeout = static_cast<CDVDMsgDouble*>(pMsg)->m_value;

        CLog::Log(LOGDEBUG, "CDVDPlayerVideo - CDVDMsg::GENERAL_DELAY(%f)", timeout);

        timeout *= (double)DVD_PLAYSPEED_NORMAL / abs(m_speed);
        timeout += CDVDClock::GetAbsoluteClock();

        while(!m_bStop && CDVDClock::GetAbsoluteClock() < timeout)
          Sleep(1);
      }
    }
    else if (pMsg->IsType(CDVDMsg::VIDEO_SET_ASPECT))
    {
      CLog::Log(LOGDEBUG, "CDVDPlayerVideo - CDVDMsg::VIDEO_SET_ASPECT");
      m_fForcedAspectRatio = *((CDVDMsgDouble*)pMsg);
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_RESET))
    {
      if(m_pVideoCodec)
        m_pVideoCodec->Reset();
      picture.iFlags &= ~DVP_FLAG_ALLOCATED;
      m_packets.clear();
      m_started = false;
      m_droppingStats.Reset();
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_FLUSH)) // private message sent by (CDVDPlayerVideo::Flush())
    {
      if(m_pVideoCodec)
        m_pVideoCodec->Reset();
      picture.iFlags &= ~DVP_FLAG_ALLOCATED;
      m_packets.clear();

      m_pullupCorrection.Flush();
      //we need to recalculate the framerate
      //TODO: this needs to be set on a streamchange instead
      ResetFrameRateCalc();
      m_droppingStats.Reset();

      m_stalled = true;
      m_started = false;

      g_renderManager.DiscardBuffer();
    }
    else if (pMsg->IsType(CDVDMsg::VIDEO_NOSKIP))
    {
      // libmpeg2 is also returning incomplete frames after a dvd cell change
      // so the first few pictures are not the correct ones to display in some cases
      // just display those together with the correct one.
      // (setting it to 2 will skip some menu stills, 5 is working ok for me).
      m_iNrOfPicturesNotToSkip = 5;
    }
    else if (pMsg->IsType(CDVDMsg::PLAYER_SETSPEED))
    {
      m_speed = static_cast<CDVDMsgInt*>(pMsg)->m_value;
      if(m_speed == DVD_PLAYSPEED_PAUSE)
        m_iNrOfPicturesNotToSkip = 0;
      if (m_pVideoCodec)
        m_pVideoCodec->SetSpeed(m_speed);
      m_droppingStats.Reset();
    }
    else if (pMsg->IsType(CDVDMsg::PLAYER_STARTED))
    {
      if(m_started)
        m_messageParent.Put(new CDVDMsgInt(CDVDMsg::PLAYER_STARTED, DVDPLAYER_VIDEO));
    }
    else if (pMsg->IsType(CDVDMsg::PLAYER_DISPLAYTIME))
    {
      CDVDPlayer::SPlayerState& state = ((CDVDMsgType<CDVDPlayer::SPlayerState>*)pMsg)->m_value;

      if(state.time_src == CDVDPlayer::ETIMESOURCE_CLOCK)
      {
        double pts = GetCurrentPts();
        if (pts == DVD_NOPTS_VALUE)
          pts = m_pClock->GetClock();
        state.time = DVD_TIME_TO_MSEC(pts + state.time_offset);
        state.disptime = state.time;
        state.timestamp = CDVDClock::GetAbsoluteClock();
      }
      else
        state.timestamp = CDVDClock::GetAbsoluteClock();
      state.player    = DVDPLAYER_VIDEO;
      m_messageParent.Put(pMsg->Acquire());
    }
    else if (pMsg->IsType(CDVDMsg::GENERAL_STREAMCHANGE))
    {
      CDVDMsgVideoCodecChange* msg(static_cast<CDVDMsgVideoCodecChange*>(pMsg));
      CDVDPlayerVideo::OpenStream(msg->m_hints, msg->m_codec);
      msg->m_codec = NULL;
      picture.iFlags &= ~DVP_FLAG_ALLOCATED;
    }

    if (pMsg->IsType(CDVDMsg::DEMUXER_PACKET))
    {
      DemuxPacket* pPacket = ((CDVDMsgDemuxerPacket*)pMsg)->GetPacket();
      bool bPacketDrop     = ((CDVDMsgDemuxerPacket*)pMsg)->GetPacketDrop();

      if (m_stalled)
      {
        CLog::Log(LOGINFO, "CDVDPlayerVideo - Stillframe left, switching to normal playback");
        m_stalled = false;

        //don't allow the first frames after a still to be dropped
        //sometimes we get multiple stills (long duration frames) after each other
        //in normal mpegs
        m_iNrOfPicturesNotToSkip = 5;
      }
      else if( iDropped*frametime > DVD_MSEC_TO_TIME(100) && m_iNrOfPicturesNotToSkip == 0 )
      { // if we dropped too many pictures in a row, insert a forced picture
        m_iNrOfPicturesNotToSkip = 1;
      }

      if (!m_pVideoCodec)
      {
        Sleep(100);
        continue;
      }

      bRequestDrop = false;
      iDropDirective = CalcDropRequirement(pts, false);
      if (iDropDirective & EOS_VERYLATE)
      {
        if (m_bAllowDrop)
        {
          m_pullupCorrection.Flush();
          bRequestDrop = true;
        }
      }
      int codecControl = 0;
      if (iDropDirective & EOS_BUFFER_LEVEL)
        codecControl |= DVD_CODEC_CTRL_DRAIN;
      if (m_speed > DVD_PLAYSPEED_NORMAL)
        codecControl |= DVD_CODEC_CTRL_NO_POSTPROC;
      m_pVideoCodec->SetCodecControl(codecControl);
      if (iDropDirective & EOS_DROPPED)
      {
        m_iDroppedFrames++;
        iDropped++;
      }

      if (m_messageQueue.GetDataSize() == 0
      ||  m_speed < 0)
      {
        bRequestDrop = false;
        m_iDroppedRequest = 0;
        m_iLateFrames     = 0;
      }

      // if player want's us to drop this packet, do so nomatter what
      if(bPacketDrop)
        bRequestDrop = true;

      // tell codec if next frame should be dropped
      // problem here, if one packet contains more than one frame
      // both frames will be dropped in that case instead of just the first
      // decoder still needs to provide an empty image structure, with correct flags
      m_pVideoCodec->SetDropState(bRequestDrop);

      // ask codec to do deinterlacing if possible
      EDEINTERLACEMODE mDeintMode = CMediaSettings::GetInstance().GetCurrentVideoSettings().m_DeinterlaceMode;
      EINTERLACEMETHOD mInt       = g_renderManager.AutoInterlaceMethod(CMediaSettings::GetInstance().GetCurrentVideoSettings().m_InterlaceMethod);

      unsigned int     mFilters = 0;

      if (mDeintMode != VS_DEINTERLACEMODE_OFF)
      {
        if (mInt == VS_INTERLACEMETHOD_DEINTERLACE)
          mFilters = CDVDVideoCodec::FILTER_DEINTERLACE_ANY;
        else if(mInt == VS_INTERLACEMETHOD_DEINTERLACE_HALF)
          mFilters = CDVDVideoCodec::FILTER_DEINTERLACE_ANY | CDVDVideoCodec::FILTER_DEINTERLACE_HALFED;

        if (mDeintMode == VS_DEINTERLACEMODE_AUTO && mFilters)
          mFilters |=  CDVDVideoCodec::FILTER_DEINTERLACE_FLAGGED;
      }

      if (!g_renderManager.Supports(RENDERFEATURE_ROTATION))
        mFilters |= CDVDVideoCodec::FILTER_ROTATE;

      mFilters = m_pVideoCodec->SetFilters(mFilters);

      int iDecoderState = m_pVideoCodec->Decode(pPacket->pData, pPacket->iSize, pPacket->dts, pPacket->pts);

      if (iDecoderState & VC_BYPASS)
      {
        if (m_speed == DVD_PLAYSPEED_NORMAL || iPriority <= 150)
        {
          m_messageQueue.Put(pMsg->Acquire(), iPriority + 10);
        }
        else
        {
          CLog::Log(LOGDEBUG, "Warning! DVDPlayerVideoRK drop by pass packet!");
        }
      }
      else
      {
        // buffer packets so we can recover should decoder flush for some reason^M
        if(m_pVideoCodec->GetConvergeCount() > 0)
        {
          m_packets.push_back(DVDMessageListItem(pMsg, 0));
          if(m_packets.size() > m_pVideoCodec->GetConvergeCount()
          || m_packets.size() * frametime > DVD_SEC_TO_TIME(10))
            m_packets.pop_front();
        }
        m_videoStats.AddSampleBytes(pPacket->iSize);
      }

      // reset the request, the following while loop may break before
      // setting the flag to a new value
      bRequestDrop = false;

      // loop while no error
      while (!m_bStop)
      {
        // if decoder was flushed, we need to seek back again to resume rendering
        if (iDecoderState & VC_FLUSHED)
        {
          CLog::Log(LOGDEBUG, "CDVDPlayerVideo - video decoder was flushed");
          while(!m_packets.empty())
          {
            CDVDMsgDemuxerPacket* msg = (CDVDMsgDemuxerPacket*)m_packets.front().message->Acquire();
            m_packets.pop_front();

            // all packets except the last one should be dropped
            // if prio packets and current packet should be dropped, this is likely a new reset
            msg->m_drop = !m_packets.empty() || (iPriority > 0 && bPacketDrop);
            m_messageQueue.Put(msg, iPriority + 10);
          }

          m_pVideoCodec->Reset();
          m_packets.clear();
          picture.iFlags &= ~DVP_FLAG_ALLOCATED;
          g_renderManager.DiscardBuffer();
          break;
        }

        if (iDecoderState & VC_REOPEN)
        {
          while(!m_packets.empty())
          {
            CDVDMsgDemuxerPacket* msg = (CDVDMsgDemuxerPacket*)m_packets.front().message->Acquire();
            msg->m_drop = false;
            m_packets.pop_front();
            m_messageQueue.Put(msg, iPriority + 10);
          }

          m_pVideoCodec->Reopen();
          m_packets.clear();
          picture.iFlags &= ~DVP_FLAG_ALLOCATED;
          g_renderManager.DiscardBuffer();
          break;
        }

        // if decoder had an error, tell it to reset to avoid more problems
        if (iDecoderState & VC_ERROR)
        {
          CLog::Log(LOGDEBUG, "CDVDPlayerVideo - video decoder returned error");
          break;
        }

        // check for a new picture
        if (iDecoderState & VC_PICTURE)
        {

          // try to retrieve the picture (should never fail!), unless there is a demuxer bug ofcours
          m_pVideoCodec->ClearPicture(&picture);
          if (m_pVideoCodec->GetPicture(&picture))
          {
            sPostProcessType.clear();

            if(picture.iDuration == 0.0)
              picture.iDuration = frametime;

            if(bPacketDrop)
              picture.iFlags |= DVP_FLAG_DROPPED;

            if (m_iNrOfPicturesNotToSkip > 0)
            {
              picture.iFlags |= DVP_FLAG_NOSKIP;
              m_iNrOfPicturesNotToSkip--;
            }

            // validate picture timing,
            // if both dts/pts invalid, use pts calulated from picture.iDuration
            // if pts invalid use dts, else use picture.pts as passed
            if (picture.dts == DVD_NOPTS_VALUE && picture.pts == DVD_NOPTS_VALUE)
              picture.pts = pts;
            else if (picture.pts == DVD_NOPTS_VALUE)
              picture.pts = picture.dts;

            /* use forced aspect if any */
            if( m_fForcedAspectRatio != 0.0f )
              picture.iDisplayWidth = (int) (picture.iDisplayHeight * m_fForcedAspectRatio);

            //Deinterlace if codec said format was interlaced or if we have selected we want to deinterlace
            //this video
            if ((mDeintMode == VS_DEINTERLACEMODE_AUTO && (picture.iFlags & DVP_FLAG_INTERLACED)) || mDeintMode == VS_DEINTERLACEMODE_FORCE)
            {
              if(mInt == VS_INTERLACEMETHOD_SW_BLEND)
              {
                if (!sPostProcessType.empty())
                  sPostProcessType += ",";
                sPostProcessType += g_advancedSettings.m_videoPPFFmpegDeint;
                bPostProcessDeint = true;
              }
            }

            if (CMediaSettings::GetInstance().GetCurrentVideoSettings().m_PostProcess)
            {
              if (!sPostProcessType.empty())
                sPostProcessType += ",";
              // This is what mplayer uses for its "high-quality filter combination"
              sPostProcessType += g_advancedSettings.m_videoPPFFmpegPostProc;
            }

            if (!sPostProcessType.empty())
            {
              mPostProcess.SetType(sPostProcessType, bPostProcessDeint);
              if (mPostProcess.Process(&picture))
                mPostProcess.GetPicture(&picture);
            }

            /* if frame has a pts (usually originiating from demux packet), use that */
            if(picture.pts != DVD_NOPTS_VALUE)
            {
              if(pulldown.enabled())
                picture.pts += pulldown.pts();

              pts = picture.pts;
            }

            if(pulldown.enabled())
            {
              picture.iDuration = pulldown.dur();
              pulldown.next();
            }

            if (picture.iRepeatPicture)
              picture.iDuration *= picture.iRepeatPicture + 1;

            int iResult = OutputPicture(&picture, pts);

            frametime = (double)DVD_TIME_BASE/m_fFrameRate;

            if(m_started == false && !(picture.iFlags & DVP_FLAG_DROPPED))
            {
              m_codecname = m_pVideoCodec->GetName();
              m_started = true;
              m_messageParent.Put(new CDVDMsgInt(CDVDMsg::PLAYER_STARTED, DVDPLAYER_VIDEO));
            }

            // guess next frame pts. iDuration is always valid
            if (m_speed != 0)
              pts += picture.iDuration * m_speed / abs(m_speed);

            if( iResult & EOS_ABORT )
            {
              //if we break here and we directly try to decode again wihout
              //flushing the video codec things break for some reason
              //i think the decoder (libmpeg2 atleast) still has a pointer
              //to the data, and when the packet is freed that will fail.
              iDecoderState = m_pVideoCodec->Decode(NULL, 0, DVD_NOPTS_VALUE, DVD_NOPTS_VALUE);
              break;
            }

            if( (iResult & EOS_DROPPED) && !bPacketDrop )
            {
              m_iDroppedFrames++;
              iDropped++;
            }
            else
              iDropped = 0;
          }
          else
          {
            CLog::Log(LOGWARNING, "Decoder Error getting videoPicture.");
            m_pVideoCodec->Reset();
          }
        }

        // if the decoder needs more data, we just break this loop
        // and try to get more data from the videoQueue
        if (iDecoderState & VC_BUFFER)
          break;

        if (!m_pVideoCodec || m_bStop)
          break;
        
        // update dropping stats
        CalcDropRequirement(pts, true);

        // the decoder didn't need more data, flush the remaning buffer
        iDecoderState = m_pVideoCodec->Decode(NULL, 0, DVD_NOPTS_VALUE, DVD_NOPTS_VALUE);
      }
    }

    // all data is used by the decoder, we can safely free it now
    pMsg->Release();
  }

  // we need to let decoder release any picture retained resources.
  m_pVideoCodec->ClearPicture(&picture);
}

std::string CDVDPlayerVideoRK::GetPlayerInfo()
{
  std::ostringstream s;
  s << "fr:"     << std::fixed << std::setprecision(3) << m_fFrameRate;
  s << ", vq:"   << std::setw(2) << std::min(99,GetLevel()) << "%";
  s << ", dc:"   << m_codecname;
  s << ", Mb/s:" << std::fixed << std::setprecision(2) << (double)GetVideoBitrate() / (1024.0*1024.0);
  s << ", drop:" << 0; // useless for rkcodec
  s << ", skip:" << 0;

  int pc = m_pullupCorrection.GetPatternLength();
  if (pc > 0)
    s << ", pc:" << pc;
  else
    s << ", pc:none";

  return s.str();
}

using namespace RenderManager;

static std::string GetRenderFormatName(ERenderFormat format)
{
  switch(format)
  {
    case RENDER_FMT_YUV420P:   return "YV12";
    case RENDER_FMT_YUV420P16: return "YV12P16";
    case RENDER_FMT_YUV420P10: return "YV12P10";
    case RENDER_FMT_NV12:      return "NV12";
    case RENDER_FMT_UYVY422:   return "UYVY";
    case RENDER_FMT_YUYV422:   return "YUY2";
    case RENDER_FMT_VDPAU:     return "VDPAU";
    case RENDER_FMT_VDPAU_420: return "VDPAU_420";
    case RENDER_FMT_DXVA:      return "DXVA";
    case RENDER_FMT_VAAPI:     return "VAAPI";
    case RENDER_FMT_VAAPINV12: return "VAAPI_NV12";
    case RENDER_FMT_OMXEGL:    return "OMXEGL";
    case RENDER_FMT_CVBREF:    return "BGRA";
    case RENDER_FMT_EGLIMG:    return "EGLIMG";
    case RENDER_FMT_BYPASS:    return "BYPASS";
    case RENDER_FMT_MEDIACODEC:return "MEDIACODEC";
    case RENDER_FMT_MEDIACODECSURFACE:return "MEDIACODECSURFACE";
    case RENDER_FMT_IMXMAP:    return "IMXMAP";
    case RENDER_FMT_MMAL:      return "MMAL";
    case RENDER_FMT_NONE:      return "NONE";
  }
  return "UNKNOWN";
}

int CDVDPlayerVideoRK::OutputPicture(const DVDVideoPicture* src, double pts)
{
  if (src->format != RENDER_FMT_BYPASS)
    return CDVDPlayerVideo::OutputPicture(src, pts);

  /* picture buffer is not allowed to be modified in this call */
  DVDVideoPicture picture(*src);
  DVDVideoPicture* pPicture = &picture;

  /* figure out steremode expected based on user settings and hints */
  unsigned int stereo_flags = GetStereoModeFlags(GetStereoMode());

#ifdef HAS_VIDEO_PLAYBACK
  double config_framerate = m_bFpsInvalid ? 0.0 : m_fFrameRate;
  double render_framerate = g_graphicsContext.GetFPS();
  if (CSettings::GetInstance().GetInt(CSettings::SETTING_VIDEOPLAYER_ADJUSTREFRESHRATE) == ADJUST_REFRESHRATE_OFF)
    render_framerate = config_framerate;
  bool changerefresh = !m_bFpsInvalid &&
                       (m_output.framerate == 0.0 || fmod(m_output.framerate, config_framerate) != 0.0) &&
                       (render_framerate != config_framerate);

  /* check so that our format or aspect has changed. if it has, reconfigure renderer */
  if (!g_renderManager.IsConfigured()
   || ( m_output.width           != pPicture->iWidth )
   || ( m_output.height          != pPicture->iHeight )
   || ( m_output.dwidth          != pPicture->iDisplayWidth )
   || ( m_output.dheight         != pPicture->iDisplayHeight )
   || changerefresh
   || ( m_output.color_format    != (unsigned int)pPicture->format )
   || ( m_output.extended_format != pPicture->extended_format )
   || ( m_output.color_matrix    != pPicture->color_matrix    && pPicture->color_matrix    != 0 ) // don't reconfigure on unspecified
   || ( m_output.chroma_position != pPicture->chroma_position && pPicture->chroma_position != 0 )
   || ( m_output.color_primaries != pPicture->color_primaries && pPicture->color_primaries != 0 )
   || ( m_output.color_transfer  != pPicture->color_transfer  && pPicture->color_transfer  != 0 )
   || ( m_output.color_range     != pPicture->color_range )
   || ( m_output.stereo_flags    != stereo_flags))
  {
    CLog::Log(LOGNOTICE, " fps: %f, pwidth: %i, pheight: %i, dwidth: %i, dheight: %i"
                       , config_framerate
                       , pPicture->iWidth
                       , pPicture->iHeight
                       , pPicture->iDisplayWidth
                       , pPicture->iDisplayHeight);

    unsigned flags = 0;
    if(pPicture->color_range == 1)
      flags |= CONF_FLAGS_YUV_FULLRANGE;

    flags |= GetFlagsChromaPosition(pPicture->chroma_position)
          |  GetFlagsColorMatrix(pPicture->color_matrix, pPicture->iWidth, pPicture->iHeight)
          |  GetFlagsColorPrimaries(pPicture->color_primaries)
          |  GetFlagsColorTransfer(pPicture->color_transfer);

    std::string formatstr = GetRenderFormatName(pPicture->format);

    if(m_bAllowFullscreen)
    {
      flags |= CONF_FLAGS_FULLSCREEN;
      m_bAllowFullscreen = false; // only allow on first configure
    }

    flags |= stereo_flags;

    CLog::Log(LOGDEBUG,"%s - change configuration. %dx%d. framerate: %4.2f. format: %s",__FUNCTION__,pPicture->iWidth, pPicture->iHeight, config_framerate, formatstr.c_str());
    if(!g_renderManager.Configure(pPicture->iWidth
                                , pPicture->iHeight
                                , pPicture->iDisplayWidth
                                , pPicture->iDisplayHeight
                                , config_framerate
                                , flags
                                , pPicture->format
                                , pPicture->extended_format
                                , m_hints.orientation
                                , m_pVideoCodec->GetAllowedReferences()))
    {
      CLog::Log(LOGERROR, "%s - failed to configure renderer", __FUNCTION__);
      return EOS_ABORT;
    }

    m_output.width           = pPicture->iWidth;
    m_output.height          = pPicture->iHeight;
    m_output.dwidth          = pPicture->iDisplayWidth;
    m_output.dheight         = pPicture->iDisplayHeight;
    m_output.framerate       = config_framerate;
    m_output.color_format    = pPicture->format;
    m_output.extended_format = pPicture->extended_format;
    m_output.color_matrix    = pPicture->color_matrix;
    m_output.chroma_position = pPicture->chroma_position;
    m_output.color_primaries = pPicture->color_primaries;
    m_output.color_transfer  = pPicture->color_transfer;
    m_output.color_range     = pPicture->color_range;
    m_output.stereo_flags    = stereo_flags;
  }

  int    result  = 0;

  if (!g_renderManager.IsStarted()) {
    CLog::Log(LOGERROR, "%s - renderer not started", __FUNCTION__);
    return EOS_ABORT;
  }

  //correct any pattern in the timestamps
  if (m_output.color_format != RENDER_FMT_BYPASS)
  {
    m_pullupCorrection.Add(pts);
    pts += m_pullupCorrection.GetCorrection();
  }

  //try to calculate the framerate
  CalcFrameRate();

  // remember original pts, we need it later for overlaying subtitles
  double pts_org = pts;

  // signal to clock what our framerate is, it may want to adjust it's
  // speed to better match with our video renderer's output speed
  double interval;
  int refreshrate = m_pClock->UpdateFramerate(m_fFrameRate, &interval);
  if (refreshrate > 0) //refreshrate of -1 means the videoreferenceclock is not running
  {//when using the videoreferenceclock, a frame is always presented half a vblank interval too late
    pts -= DVD_TIME_BASE * interval;
  }

  if (m_output.color_format != RENDER_FMT_BYPASS)
  {
    // Correct pts by user set delay and rendering delay
    pts += m_iVideoDelay - DVD_SEC_TO_TIME(g_renderManager.GetDisplayLatency());
  }

  // calculate the time we need to delay this picture before displaying
  double iSleepTime, iClockSleep, iFrameSleep, iPlayingClock, iCurrentClock;

  iPlayingClock = m_pClock->GetClock(iCurrentClock, false); // snapshot current clock

  // correct sleep times based on speed
  if(m_speed)
  {
    iClockSleep = (pts - iPlayingClock) * DVD_PLAYSPEED_NORMAL / m_speed;
    iFrameSleep = (pts - m_FlipTimePts) * DVD_PLAYSPEED_NORMAL / m_speed - (iCurrentClock - m_FlipTimeStamp);
  }
  else
  {
    iClockSleep = 0;
    iFrameSleep = 0;
  }

  if( m_started == false )
    iSleepTime = 0.0;
  else if( m_stalled || m_pClock->GetMaster() == MASTER_CLOCK_VIDEO)
    iSleepTime = iFrameSleep;
  else
    iSleepTime = iClockSleep;

  if (m_speed < 0)
  {
    double sleepTime, renderPts;
    int queued, discard;
    double inputPts = m_droppingStats.m_lastPts;
    g_renderManager.GetStats(sleepTime, renderPts, queued, discard);
    if (pts_org > renderPts || queued > 0)
    {
      if (inputPts >= renderPts)
      {
        Sleep(50);
      }
      return result | EOS_DROPPED;
    }
    else if (pts_org < iPlayingClock)
    {
      return result | EOS_DROPPED;
    }

    if (iSleepTime > DVD_MSEC_TO_TIME(20))
      iSleepTime = DVD_MSEC_TO_TIME(20);
  }
  else if (m_speed > DVD_PLAYSPEED_NORMAL)
  {
    double sleepTime, renderPts;
    int bufferLevel, queued, discard;
    g_renderManager.GetStats(sleepTime, renderPts, queued, discard);
    bufferLevel = queued + discard;

    // estimate the time it will take for the next frame to get rendered
    // drop the frame if it's late in regard to this estimation
    double diff = pts_org - renderPts;
    double mindiff = DVD_SEC_TO_TIME(1/m_fFrameRate) * (bufferLevel + 1);
    if (diff < mindiff)
    {
      m_droppingStats.AddOutputDropGain(pts, 1/m_fFrameRate);
      return result | EOS_DROPPED;
    }
  }

  // sync clock if we are master
  if(m_pClock->GetMaster() == MASTER_CLOCK_VIDEO)
  {
    m_pClock->Update( iPlayingClock + iClockSleep - iFrameSleep
                    , iCurrentClock
                    , DVD_MSEC_TO_TIME(10)
                    , "CDVDPlayerVideo::OutputPicture");
  }

  // timestamp when we think next picture should be displayed based on current duration
  m_FlipTimeStamp  = iCurrentClock;
  m_FlipTimeStamp += std::max(0.0, iSleepTime);
  m_FlipTimePts    = pts;

  if ((pPicture->iFlags & DVP_FLAG_DROPPED))
  {
    m_droppingStats.AddOutputDropGain(pts, 1/m_fFrameRate);
    CLog::Log(LOGDEBUG,"%s - dropped in output", __FUNCTION__);
    return result | EOS_DROPPED;
  }

  // set fieldsync if picture is interlaced
  EFIELDSYNC mDisplayField = FS_NONE;
  if( pPicture->iFlags & DVP_FLAG_INTERLACED )
  {
    if( pPicture->iFlags & DVP_FLAG_TOP_FIELD_FIRST )
      mDisplayField = FS_TOP;
    else
      mDisplayField = FS_BOT;
  }

  ProcessOverlays(pPicture, pts_org);

  g_renderManager.AddVideoPicture(*pPicture);
  g_renderManager.FlipPage(CThread::m_bStop, (iCurrentClock + iSleepTime) / DVD_TIME_BASE, pts_org, -1, mDisplayField);

  return result;
#else
  // no video renderer, let's mark it as dropped
  return EOS_DROPPED;
#endif
}



