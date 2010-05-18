#pragma once
#include "../../core/MuninNodePlugin.h"
#include "consolePipe.h"

class NewExternalMuninNodePlugin : public MuninNodePluginHelper
{
public:
  NewExternalMuninNodePlugin(const std::string &externalPlugin);
  virtual ~NewExternalMuninNodePlugin();

  virtual int GetConfig(char *buffer, int len);
  virtual int GetValues(char *buffer, int len);
  virtual bool IsLoaded() { return m_Loaded; };
  virtual bool IsThreadSafe() { return true; };
  static const char *SectionPrefix;
  static const char *EnvPrefix;

private:  
  bool OpenPlugin();
  void removeAllLF (std::string &str);
  void setEnv();
  void clearEnv();

  class PluginPipe : public CConsolePipe {
  public:
    PluginPipe();
    virtual ~PluginPipe();

    virtual void OnReceivedOutput(LPCTSTR pszText);
    TString GetOutput();
  protected:
    JCCritSec m_BufferCritSec;
    TString m_Buffer;
  };
  std::string Run(const char *command);

  bool m_Loaded;
  std::vector<std::string> m_envNames;
  std::vector<std::string> m_envValues;
  std::string m_Command;
  std::string m_SectionName;
};

extern JCCritSec g_ExternalEnvironmentCritSec;