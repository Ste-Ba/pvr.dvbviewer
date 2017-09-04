#pragma once

#ifndef PVR_DVBVIEWER_TIMESHIFTBUFFER_H
#define PVR_DVBVIEWER_TIMESHIFTBUFFER_H

#include "IStreamReader.h"
#include "CircularCache.h"
#include "p8-platform/threads/threads.h"

namespace DVBViewer
{

class TimeshiftBuffer
  : public IStreamReader
  , public P8PLATFORM::CThread
{
public:
  TimeshiftBuffer(IStreamReader *strReader, const std::string &bufferPath);
  ~TimeshiftBuffer(void);
  bool Start() override;
  ssize_t ReadData(unsigned char *buffer, unsigned int size) override;
  int64_t Seek(long long position, int whence) override;
  int64_t Position() override;
  int64_t Length() override;
  time_t TimeStart() override;
  time_t TimeEnd() override;
  bool NearEnd() override;
  bool CanTimeshift() override;

private:
  virtual void *Process(void) override;
  ssize_t ReadDataFromCache(unsigned char *buffer, unsigned int size);
  ssize_t ReadDataFromFile(unsigned char *buffer, unsigned int size);

  std::string m_bufferPath;
  IStreamReader *m_strReader;
  void *m_filebufferReadHandle;
  void *m_filebufferWriteHandle;
  time_t m_start;
  uint64_t m_readPos;
  uint64_t m_fileReadPos;
  CCircularCache m_cache;
};

}
#endif
