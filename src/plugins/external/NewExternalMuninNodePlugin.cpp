/* This file is part of munin-node-win32
 * Copyright (C) 2006-2008 Jory Stone (jcsston@jory.info)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "StdAfx.h"
#include "NewExternalMuninNodePlugin.h"
#include <strsafe.h>

#include "../../core/Service.h"

const char *NewExternalMuninNodePlugin::SectionPrefix = "ExternalPlugin_";
const char *NewExternalMuninNodePlugin::EnvPrefix = "env.";


NewExternalMuninNodePlugin::NewExternalMuninNodePlugin(const std::string &sectionName) 
  : m_SectionName(sectionName)
{
  m_Loaded = OpenPlugin();
}

NewExternalMuninNodePlugin::~NewExternalMuninNodePlugin()
{

}

void NewExternalMuninNodePlugin::removeAllLF (std::string &str)
{
    std::string temp;
    for (unsigned int i = 0; i < str.length(); i++)
        if ((str[i] != '\r')) temp += str[i];
    str = temp;
}


bool NewExternalMuninNodePlugin::OpenPlugin()
{
  m_Name = m_SectionName.substr(strlen(NewExternalMuninNodePlugin::SectionPrefix));
  m_Command = g_Config.GetValue(m_SectionName, "Command", "");

  if (m_Command.empty()) { return false;}

  const char *envPrefix = NewExternalMuninNodePlugin::EnvPrefix;
  size_t envPrefixLen = strlen(envPrefix);
  for (size_t i = 0; i < g_Config.GetNumValues(m_SectionName); i++) {
	std::string valueName = g_Config.GetValueName(m_SectionName, i);
	if (valueName.compare(0, envPrefixLen, envPrefix) == 0) {
		std::string value = g_Config.GetValue(m_SectionName, valueName, "");

		m_envNames.push_back(valueName.substr(envPrefixLen));
		m_envValues.push_back(value);
	}
  }

  static const int BUFFER_SIZE = 8096;
  char buffer[BUFFER_SIZE] = {0};
  memset(buffer, 0, BUFFER_SIZE);
  int ret = GetConfig(buffer, BUFFER_SIZE);
  if (ret < 0) { return false; }

  return true;
}


int NewExternalMuninNodePlugin::GetConfig(char *buffer, int len) 
{
  std::string output = Run("config");
  if (output.length() > 0) {
	removeAllLF(output);
	strncpy(buffer, output.c_str(), len);
	return 0;
  } 
  return -1;
}

int NewExternalMuninNodePlugin::GetValues(char *buffer, int len) 
{
  std::string output = Run("");
  if (output.length() > 0) {
	removeAllLF(output);
	strncpy(buffer, output.c_str(), len);
	return 0;
  } 
  return -1;
}

std::wstring s2ws(const std::string& s)
{
    int len;
    int slength = (int)s.length() + 1;
    len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0); 
    wchar_t* buf = new wchar_t[len];
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
    std::wstring r(buf);
    delete[] buf;
    return r;
}

void NewExternalMuninNodePlugin::setEnv ()
{
	if (m_envNames.size() > 0) {
		for (int i=0; i < m_envNames.size(); i++) {
			std::wstring name = s2ws(m_envNames[i]);
			std::wstring value = s2ws(m_envValues[i]);
			SetEnvironmentVariable(name.c_str(), value.c_str());
		}
	}
}

void NewExternalMuninNodePlugin::clearEnv ()
{
	if (m_envNames.size() > 0) {
		for (int i=0; i < m_envNames.size(); i++) {
			std::wstring name = s2ws(m_envNames[i]);
			SetEnvironmentVariable(name.c_str(), NULL);
		}
	}
}


std::string NewExternalMuninNodePlugin::Run(const char *command)
{
  // Build the command-line
  char cmdLine[MAX_PATH];
  _snprintf(cmdLine, MAX_PATH, "\"%s\" %s", m_Command.c_str(), command);

  PluginPipe pipe;

  // the munin master should execute plugins one after the other
  // so for now we don't care about NOT modifying our own environment
  // this is damn ugly, but works
  // well... don't set existing variables because they're killed off instead
  // of getting restored
  // we'll lock for sure and unlock once the environment is "back to normal"
  // if someone knows a better solution... SPEAK UP!

  g_ExternalEnvironmentCritSec.Lock();
  setEnv();

  if (pipe.Execute(A2TConvert(cmdLine).c_str(), NULL) == CPEXEC_OK) {
	  clearEnv();
	  g_ExternalEnvironmentCritSec.Unlock();
    // Wait for the command to complete
    while (pipe.IsChildRunning())
      Sleep(100); // don't do a very tight loop -- that reduces CPU usage
    return T2AConvert(pipe.GetOutput());
  } else {
	  clearEnv();
	  g_ExternalEnvironmentCritSec.Unlock();
  }
  // Command failed, empty string
  return "";
}

NewExternalMuninNodePlugin::PluginPipe::PluginPipe()
  : CConsolePipe(CPF_NOCONVERTOEM|CPF_CAPTURESTDOUT|CPF_NOAUTODELETE) 
{

}

NewExternalMuninNodePlugin::PluginPipe::~PluginPipe()
{

}

void NewExternalMuninNodePlugin::PluginPipe::OnReceivedOutput(LPCTSTR pszText) 
{
  if (pszText != NULL) {
    JCAutoLockCritSec lock(&m_BufferCritSec);
    m_Buffer += pszText;
  }
}

TString NewExternalMuninNodePlugin::PluginPipe::GetOutput() 
{
  JCAutoLockCritSec lock(&m_BufferCritSec);
  return m_Buffer;
}


JCCritSec g_ExternalEnvironmentCritSec;