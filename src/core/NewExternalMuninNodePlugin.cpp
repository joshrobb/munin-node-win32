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

		std::string envLine = valueName.substr(envPrefixLen) + "=" + value;

		m_envArray.push_back(envLine);
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

std::string NewExternalMuninNodePlugin::Run(const char *command)
{
  // Build the command-line
  char cmdLine[MAX_PATH];
  _snprintf(cmdLine, MAX_PATH, "\"%s\" %s", m_Command.c_str(), command);

  LPTSTR lpszVariable;
  LPTCH lpvEnv; 
  TCHAR chNewEnv[8196];
  LPTSTR lpszCurrentVariable;

  if (m_envArray.size() > 0) {	
	// Get a pointer to the environment block. 

	lpvEnv = GetEnvironmentStrings();

    // Variable strings are separated by NULL byte, and the block is 
    // terminated by a NULL byte. 

    lpszVariable = (LPTSTR) lpvEnv;
	lpszCurrentVariable = (LPTSTR) chNewEnv;

    while (*lpszVariable)
    {
        //_tprintf(TEXT("%s\n"), lpszVariable);
		if (FAILED(StringCchCopy(lpszCurrentVariable, 8196, lpszVariable)))
		{
			printf("String copy failed\n"); 
			return FALSE;
		}

        lpszCurrentVariable += lstrlen(lpszCurrentVariable) + 1; 
		lpszVariable += lstrlen(lpszVariable) + 1;
    }

	for (int i=0; i < m_envArray.size(); i++) {
		//_tprintf(TEXT("%s\n"), m_envArray[i].c_str());
		std::string newStr = m_envArray[i];

		TCHAR *param=new TCHAR[newStr.size()+1];
		param[newStr.size()]=0;
		//As much as we'd love to, we can't use memcpy() because
		//sizeof(TCHAR)==sizeof(char) may not be true:
		std::copy(newStr.begin(),newStr.end(),param);

		if (FAILED(StringCchCopy(lpszCurrentVariable, 8196, param)))
		{
			printf("String copy failed\n"); 
			return FALSE;
		}
		delete param;
	}

    lpszCurrentVariable += lstrlen(lpszCurrentVariable) + 1; 
    *lpszCurrentVariable = (TCHAR)0; 

	FreeEnvironmentStrings(lpvEnv);
  } else {
	lpvEnv = NULL;
  }

  PluginPipe pipe;
  if (pipe.Execute(A2TConvert(cmdLine).c_str(), lpvEnv) == CPEXEC_OK) {
    // Wait for the command to complete
    while (pipe.IsChildRunning())
      Sleep(100); // don't do a very tight loop -- that reduces CPU usage
    return T2AConvert(pipe.GetOutput());
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
