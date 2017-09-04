#include "TimeshiftBuffer.h"
#include "StreamReader.h"
#include "client.h"
#include "p8-platform/util/util.h"

#define STREAM_READ_BUFFER_SIZE   32768
#define CACHE_BUFFER_SIZE         STREAM_READ_BUFFER_SIZE * 1024 // 32MB
//#define CACHE_BUFFER_SIZE       STREAM_READ_BUFFER_SIZE * 128 // 4MB
#define BUFFER_READ_TIMEOUT       10000

static_assert(CACHE_BUFFER_SIZE % STREAM_READ_BUFFER_SIZE == 0,
    "Cache size has to be multiple of read size");

using namespace DVBViewer;
using namespace ADDON;

TimeshiftBuffer::TimeshiftBuffer(IStreamReader *strReader,
    const std::string &bufferPath)
  : m_strReader(strReader)
  , m_bufferPath(bufferPath)
  , m_start(0)
  , m_readPos(0)
  , m_fileReadPos(0)
  , m_cache(CACHE_BUFFER_SIZE)
{
  m_bufferPath += "/tsbuffer.ts";
  m_filebufferWriteHandle = XBMC->OpenFileForWrite(m_bufferPath.c_str(), true);
  Sleep(100);
  m_filebufferReadHandle = XBMC->OpenFile(m_bufferPath.c_str(), XFILE::READ_NO_CACHE);
}

TimeshiftBuffer::~TimeshiftBuffer(void)
{
  StopThread(0);

  if (m_filebufferWriteHandle)
  {
    // XBMC->TruncateFile doesn't work for unknown reasons
    XBMC->CloseFile(m_filebufferWriteHandle);
    void *tmp;
    if ((tmp = XBMC->OpenFileForWrite(m_bufferPath.c_str(), true)) != nullptr)
      XBMC->CloseFile(tmp);
  }
  if (m_filebufferReadHandle)
    XBMC->CloseFile(m_filebufferReadHandle);
  SAFE_DELETE(m_strReader);
  XBMC->Log(LOG_DEBUG, "Timeshift: Stopped");
}

bool TimeshiftBuffer::Start()
{
  if (m_strReader == nullptr
      || m_filebufferWriteHandle == nullptr
      || m_filebufferReadHandle == nullptr)
    return false;
  if (IsRunning())
    return true;
  XBMC->Log(LOG_INFO, "Timeshift: Started");
  m_cache.Open();
  m_start = time(NULL);
  CreateThread();
  return true;
}

void *TimeshiftBuffer::Process()
{
  XBMC->Log(LOG_DEBUG, "Timeshift: Thread started");
  uint8_t buffer[STREAM_READ_BUFFER_SIZE];

  m_strReader->Start();
  while (!IsStopped())
  {
    ssize_t read = m_strReader->ReadData(buffer, sizeof(buffer));
    XBMC->WriteFile(m_filebufferWriteHandle, buffer, read);
    m_cache.WriteToCache(buffer, read); // guaranteed to succeed
  }
  XBMC->Log(LOG_DEBUG, "Timeshift: Thread stopped");
  return NULL;
}

int64_t TimeshiftBuffer::Seek(long long position, int whence)
{
  int64_t nextpos, end = Length();
  switch (whence)
  {
    case SEEK_SET:
      nextpos = position;
      break;
    case SEEK_CUR:
      nextpos = m_readPos + position;
      break;
    case SEEK_END:
      nextpos = end + position;
      break;
    default:
        return -1;
  }
  if (nextpos < 0 || nextpos > end)
  {
    XBMC->Log(LOG_ERROR, "Timeshift: Invalid seek to %ld", nextpos);
    return -1;
  }
  m_readPos = nextpos;
  return nextpos;
}

int64_t TimeshiftBuffer::Position()
{

  return XBMC->GetFilePosition(m_filebufferReadHandle);
}

int64_t TimeshiftBuffer::Length()
{
  return m_cache.CachedDataEndPos();
}

ssize_t TimeshiftBuffer::ReadData(unsigned char *buffer, unsigned int size)
{
  ssize_t read = ReadDataFromCache(buffer, size);
  if (read == CACHE_RC_ERROR)
    read = ReadDataFromFile(buffer, size);
  if (read < 0)
    return -1;
  m_readPos += read;
  return read;
}

ssize_t TimeshiftBuffer::ReadDataFromCache(unsigned char *buffer,
    unsigned int size)
{
  ssize_t read = m_cache.ReadFromCache(m_readPos, buffer, size);

  /* block if we want to read above the cache end */
  if (read == CACHE_RC_WOULD_BLOCK)
  {
    int64_t avail = m_cache.WaitForData(m_readPos, size, BUFFER_READ_TIMEOUT);
    if (avail < size)
    {
      XBMC->Log(LOG_DEBUG, "Timeshift: Cache timeout; waited %u",
          BUFFER_READ_TIMEOUT);
      return CACHE_RC_TIMEOUT;
    }
    return ReadDataFromCache(buffer, size);
  }

  return read;
}

ssize_t TimeshiftBuffer::ReadDataFromFile(unsigned char *buffer,
    unsigned int size)
{
  if (m_readPos != m_fileReadPos
      && XBMC->SeekFile(m_filebufferReadHandle, m_readPos, SEEK_SET) != m_readPos)
    return -1;
  ssize_t read = XBMC->ReadFile(m_filebufferReadHandle, buffer, size);
  m_fileReadPos += read;
  return read;
}

time_t TimeshiftBuffer::TimeStart()
{
  return m_start;
}

time_t TimeshiftBuffer::TimeEnd()
{
  return time(NULL);
}

bool TimeshiftBuffer::NearEnd()
{
  //FIXME as soon as we return false here the players current time value starts
  // flickering/jumping
  return true;

  // other PVRs use 10 seconds here, but we aren't doing any demuxing
  // we'll therefore just asume 1 secs needs about 1mb
  //return Length() - Position() <= 10 * 1048576;
}

bool TimeshiftBuffer::CanTimeshift()
{
  return true;
}
