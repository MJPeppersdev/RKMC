/*
 *      Copyright (C) 2013 Team XBMC
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

#include "DisplayOutputManager.h"
#include "JNIBase.h"
#include "jutils/jutils-details.hpp"

using namespace jni;

const char* CJNIDisplayOutputManager::m_classname = "android/os/DisplayOutputManager";
int CJNIDisplayOutputManager::MAIN_DISPLAY = 0;
int CJNIDisplayOutputManager::AUX_DISPLAY = 0;
int CJNIDisplayOutputManager::DISPLAY_IFACE_TV = 0;
int CJNIDisplayOutputManager::DISPLAY_IFACE_YPbPr = 0;
int CJNIDisplayOutputManager::DISPLAY_IFACE_VGA = 0;
int CJNIDisplayOutputManager::DISPLAY_IFACE_HDMI = 0;
int CJNIDisplayOutputManager::DISPLAY_IFACE_LCD = 0;

CJNIDisplayOutputManager::CJNIDisplayOutputManager() : CJNIBase(m_classname)
{
  m_object = new_object(m_classname, "<init>", "()V");
  m_object.setGlobal();

  MAIN_DISPLAY = get_field<int>(m_object, "MAIN_DISPLAY");
  AUX_DISPLAY  = get_field<int>(m_object, "AUX_DISPLAY");

  
  DISPLAY_IFACE_TV = get_field<int>(m_object, "DISPLAY_IFACE_TV");
  DISPLAY_IFACE_YPbPr = get_field<int>(m_object, "DISPLAY_IFACE_YPbPr");
  DISPLAY_IFACE_VGA = get_field<int>(m_object, "DISPLAY_IFACE_VGA");
  DISPLAY_IFACE_HDMI = get_field<int>(m_object, "DISPLAY_IFACE_HDMI");
  DISPLAY_IFACE_LCD = get_field<int>(m_object, "DISPLAY_IFACE_LCD");

}

std::string CJNIDisplayOutputManager::getCurrentMode(int display, int type)
{
  return jcast<std::string>(call_method<jhstring>(m_object,
    "getCurrentMode", "(II)Ljava/lang/String;", display, type));
}

void CJNIDisplayOutputManager::setMode(int display, int type, std::string mode)
{
  call_method<void>(m_object,
    "setMode", "(IILjava/lang/String;)V", display, type,  jcast<jhstring>(mode));
}

void CJNIDisplayOutputManager::set3DMode(int display, int type, int mode)
{
  call_method<void>(m_object,
    "set3DMode", "(III)V", display, type, mode);
}


