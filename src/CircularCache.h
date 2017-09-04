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

#ifndef CACHECIRCULAR_H
#define CACHECIRCULAR_H

#include "client.h"
#include "p8-platform/threads/threads.h"

#define CACHE_RC_OK           0
#define CACHE_RC_ERROR       -1
#define CACHE_RC_WOULD_BLOCK -2
#define CACHE_RC_TIMEOUT     -3

namespace DVBViewer
{

class CCircularCache
{
public:
    CCircularCache(size_t size);
    ~CCircularCache();

    int Open();
    void Close();

    ssize_t WriteToCache(const unsigned char *buf, size_t len);
    ssize_t ReadFromCache(unsigned char *buf, size_t len);
    ssize_t ReadFromCache(int64_t ipos, unsigned char *buf, size_t len);
    int64_t WaitForData(unsigned int minimum, unsigned int iMillis);
    int64_t WaitForData(int64_t pos, unsigned int minimum,
        unsigned int iMillis);

    int64_t Seek(int64_t pos);

    int64_t CachedDataEndPos();
    bool IsCachedPosition();
    bool IsCachedPosition(int64_t pos);

protected:
    int64_t  m_beg;  /**< index in file (not buffer) of beginning of valid data */
    int64_t  m_end;  /**< index in file (not buffer) of end of valid data */
    int64_t  m_cur;  /**< current reading index in file */
    uint8_t *m_buf;  /**< buffer holding data */
    size_t   m_size; /**< size of data buffer used (m_buf) */
    P8PLATFORM::CMutex m_sync;
    P8PLATFORM::CEvent m_written;
#ifdef TARGET_WINDOWS
    HANDLE   m_handle;
#endif
};

}
#endif
