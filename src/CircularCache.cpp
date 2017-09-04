/*
 *      Copyright (C) 2005-2014 Team XBMC
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

#include <algorithm>
#include "SystemClock.h"
#include "CircularCache.h"

using namespace DVBViewer;

CCircularCache::CCircularCache(size_t size)
 : m_beg(0)
 , m_end(0)
 , m_cur(0)
 , m_buf(NULL)
 , m_size(size)
#ifdef TARGET_WINDOWS
 , m_handle(INVALID_HANDLE_VALUE)
#endif
{
}

CCircularCache::~CCircularCache()
{
  Close();
}

int CCircularCache::Open()
{
#ifdef TARGET_WINDOWS
  m_handle = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, m_size, NULL);
  if (m_handle == NULL)
    return CACHE_RC_ERROR;
  m_buf = (uint8_t*)MapViewOfFile(m_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
#else
  m_buf = new uint8_t[m_size];
#endif
  if (m_buf == 0)
    return CACHE_RC_ERROR;
  m_beg = 0;
  m_end = 0;
  m_cur = 0;
  return CACHE_RC_OK;
}

void CCircularCache::Close()
{
#ifdef TARGET_WINDOWS
  UnmapViewOfFile(m_buf);
  CloseHandle(m_handle);
  m_handle = INVALID_HANDLE_VALUE;
#else
  delete[] m_buf;
#endif
  m_buf = NULL;
}

/**
 * Function will write to m_buf at m_end % m_size location
 * it will write at maximum m_size, but it will only write
 * as much it can without wrapping around in the buffer
 *
 * The following always apply:
 *  * m_beg <= m_cur <= m_end
 *  * m_end - m_beg <= m_size
 *
 * Multiple calls may be needed to fill buffer completely.
 */
ssize_t CCircularCache::WriteToCache(const unsigned char *buf, size_t len)
{
  P8PLATFORM::CLockObject lock(m_sync);

  // where are we in the buffer
  size_t pos  = m_end % m_size;
  size_t wrap = m_size - pos;

  // limit to wrap point
  if (len > wrap)
    len = wrap;

  if (len == 0)
    return 0;

  // write the data
  memcpy(m_buf + pos, buf, len);
  m_end += len;

  // drop history that was overwritten
  if (m_end - m_beg > (int64_t)m_size)
    m_beg = m_end - m_size;

  m_written.Broadcast();

  return len;
}

/**
 * Reads data from cache. Will only read up till
 * the buffer wrap point. So multiple calls
 * may be needed to empty the whole cache
 */
ssize_t CCircularCache::ReadFromCache(unsigned char *buf, size_t len)
{
  ssize_t read = ReadFromCache(m_cur, buf, len);
  if (read > 0)
    m_cur += read;
  return read;
}

ssize_t CCircularCache::ReadFromCache(int64_t ipos, unsigned char *buf,
    size_t len)
{
  P8PLATFORM::CLockObject lock(m_sync);

  if (ipos < m_beg)
    return CACHE_RC_ERROR;
  if (ipos + len > m_end)
    return CACHE_RC_WOULD_BLOCK;

  size_t pos   = ipos % m_size;
  size_t front = (size_t)(m_end - ipos);
  size_t avail = std::min(m_size - pos, front);

  if (avail == 0)
    return CACHE_RC_WOULD_BLOCK;

  if (len > avail)
    len = avail;

  if (len == 0)
    return 0;

  memcpy(buf, m_buf + pos, len);
  return len;
}

/* Wait "millis" milliseconds for "minimum" amount of data to come in.
 * Note that caller needs to make sure there's sufficient space in the forward
 * buffer for "minimum" bytes else we may block the full timeout time
 */
int64_t CCircularCache::WaitForData(unsigned int minimum,
    unsigned int millis)
{
  return WaitForData(m_cur, minimum, millis);
}

int64_t CCircularCache::WaitForData(int64_t pos, unsigned int minimum,
    unsigned int millis)
{
  P8PLATFORM::CLockObject lock(m_sync);
  int64_t avail = m_end - pos;

  if (millis == 0)
    return avail;

  if (minimum > m_size)
    minimum = m_size;

  Clock::EndTime endtime(millis);
  while (avail < minimum && !endtime.IsTimePast())
  {
    lock.Unlock();
    m_written.Wait(50); // may miss the deadline. shouldn't be a problem.
    lock.Lock();
    avail = m_end - pos;
  }

  return avail;
}

int64_t CCircularCache::Seek(int64_t pos)
{
  P8PLATFORM::CLockObject lock(m_sync);

  // if seek is a bit over what we have, try to wait a few seconds for the data to be available.
  // we try to avoid a (heavy) seek on the source
  if (pos >= m_end && pos < m_end + 100000)
  {
    /* Make everything in the cache (back & forward) back-cache, to make sure
     * there's sufficient forward space. Increasing it with only 100000 may not be
     * sufficient due to variable filesystem chunksize
     */
    m_cur = m_end;
    lock.Unlock();
    WaitForData((size_t)(pos - m_cur), 5000);
    lock.Lock();
  }

  if (pos >= m_beg && pos <= m_end)
  {
    m_cur = pos;
    return pos;
  }

  return CACHE_RC_ERROR;
}

int64_t CCircularCache::CachedDataEndPos()
{
  P8PLATFORM::CLockObject lock(m_sync);
  return m_end;
}

bool CCircularCache::IsCachedPosition()
{
  return IsCachedPosition(m_cur);
}

bool CCircularCache::IsCachedPosition(int64_t pos)
{
  P8PLATFORM::CLockObject lock(m_sync);
  return pos >= m_beg && pos <= m_end;
}
