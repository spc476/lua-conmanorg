/*********************************************************************
*
* Copyright 2013 by Sean Conner.  All Rights Reserved.
*
* This library is free software; you can redistribute it and/or modify it
* under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation; either version 3 of the License, or (at your
* option) any later version.
*
* This library is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
* or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
* License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this library; if not, see <http://www.gnu.org/licenses/>.
*
* Comments, questions and criticisms can be sent to: sean@conman.org
*
*********************************************************************/

#define _GNU_SOURCE

#include <time.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include <arpa/inet.h>
#include <openssl/evp.h>

#include "uuidlib.h"

/**************************************************************************/

	/* per RFC-4122 */

const uuid__t c_uuid_namespace_dns =
{
  .flat = { 
            0x6b , 0xa7 , 0xb8 , 0x10 , 0x9d , 0xad , 0x11 , 0xd1 ,
            0x80 , 0xb4 , 0x00 , 0xc0 , 0x4f , 0xd4 , 0x30 , 0xc8
          }
};

const uuid__t c_uuid_namespace_url =
{
  .flat = {
            0x6b , 0xa7 , 0xb8 , 0x11 , 0x9d , 0xad , 0x11 , 0xd1 ,
            0x80 , 0xb4 , 0x00 , 0xc0 , 0x4f , 0xd4 , 0x30 , 0xc8
          }
};

const uuid__t c_uuid_namespace_oid =
{
   .flat = {
             0x6b , 0xa7 , 0xb8 , 0x12 , 0x9d , 0xad , 0x11 , 0xd1 ,
             0x80 , 0xb4 , 0x00 , 0xc0 , 0x4f , 0xd4 , 0x30 , 0xc8
           }
};

const uuid__t c_uuid_namespace_x500 =
{
   .flat = {
             0x6b , 0xa7 , 0xb8 , 0x14 , 0x9d , 0xad , 0x11 , 0xd1 ,
             0x80 , 0xb4 , 0x00 , 0xc0 , 0x4f , 0xd4 , 0x30 , 0xc8
           }
};

const uuid__t c_uuid_null = 
{
  .flat = {
            0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 ,
            0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00 , 0x00
          }
};

static uint8_t m_mac[6] = { 0xDE , 0xCA , 0xFB , 0xAD , 0x02 , 0x01 };

/*************************************************************************/

int uuidlib_v1(uuid__t *const uuid)
{
  struct timespec now;
  int64_t         timestamp;
  
  assert(uuid != NULL);
  
  clock_gettime(CLOCK_REALTIME,&now);
  timestamp = (now.tv_sec * 10000000LL)
            + (now.tv_nsec / 100LL)
            + UUID_EPOCH;
  uuid->uuid.time_hi_and_version       = htons(timestamp >> 48);
  uuid->uuid.time_mid                  = htons((timestamp >> 32) & 0xFFFFLL);
  uuid->uuid.time_low                  = htonl(timestamp & 0xFFFFFFFFLL);
  uuid->uuid.clock_seq_hi_and_reserved = rand() & 0xFF;
  uuid->uuid.clock_seq_low             = rand() & 0xFF;
  memcpy(uuid->uuid.node,m_mac,6);
  
  uuid->flat[6] = (uuid->flat[6] & 0x0F) | 0x10;
  uuid->flat[8] = (uuid->flat[8] & 0x3F) | 0x80;
  return 0;
}

/*************************************************************************/

int uuidlib_v2(uuid__t *const uuid)
{
  assert(uuid != NULL);
  memset(uuid,0,sizeof(uuid__t));
  return ENOSYS;
}

/**************************************************************************/

int uuidlib_v3(
	uuid__t       *const uuid,
	const uuid__t *const namespace,
	const void    *const name,
	const size_t         len
)
{
  const EVP_MD *m = EVP_md5();
  EVP_MD_CTX    ctx;
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int  hashsize;
  
  assert(uuid      != NULL);
  assert(namespace != NULL);
  assert(name      != NULL);
  assert(len       >  0);
  
  EVP_DigestInit(&ctx,m);
  EVP_DigestUpdate(&ctx,namespace->flat,sizeof(struct uuid));
  EVP_DigestUpdate(&ctx,name,len);
  EVP_DigestFinal(&ctx,hash,&hashsize);
  
  memcpy(uuid->flat,hash,sizeof(struct uuid));
  uuid->flat[6] = (uuid->flat[6] & 0x0F) | 0x30;
  uuid->flat[8] = (uuid->flat[8] & 0x3F) | 0x80;
  return 0;
}

/**************************************************************************/

int uuidlib_v4(uuid__t *const uuid)
{
  assert(uuid != NULL);
  
  for (size_t i = 0 ; i < (sizeof(struct uuid) / sizeof(rand__t)) ; i++)
    uuid->rnd[i] = (unsigned)rand() + (unsigned)rand();
  
  uuid->flat[6] = (uuid->flat[6] & 0x0F) | 0x40;
  uuid->flat[8] = (uuid->flat[8] & 0x3F) | 0x80;
  return 0;
}

/*************************************************************************/

int uuidlib_v5(
	uuid__t       *const uuid,
	const uuid__t *const namespace,
	const void    *const name,
	const size_t         len
)
{
  const EVP_MD *m = EVP_sha1();
  EVP_MD_CTX    ctx;
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int  hashsize;
  
  assert(uuid      != NULL);
  assert(namespace != NULL);
  assert(name      != NULL);
  assert(len       >  0);
  
  EVP_DigestInit(&ctx,m);
  EVP_DigestUpdate(&ctx,namespace->flat,sizeof(struct uuid));
  EVP_DigestUpdate(&ctx,name,len);
  EVP_DigestFinal(&ctx,hash,&hashsize);
  
  memcpy(uuid->flat,hash,sizeof(struct uuid));
  uuid->flat[6] = (uuid->flat[6] & 0x0F) | 0x50;
  uuid->flat[8] = (uuid->flat[8] & 0x3F) | 0x80;
  return 0;
}

/**************************************************************************/

int uuidlib_cmp(
	const uuid__t *const restrict uuid1,
	const uuid__t *const restrict uuid2
)
{
  uint32_t a32;
  uint32_t b32;
  uint16_t a16;
  uint16_t b16;
  
  assert(uuid1 != NULL);
  assert(uuid2 != NULL);
  
  a32 = ntohl(uuid1->uuid.time_low);
  b32 = ntohl(uuid2->uuid.time_low);
  
  if (a32 < b32)
    return -1;
  else if (a32 > b32)
    return 1;
  
  a16 = ntohs(uuid1->uuid.time_mid);
  b16 = ntohs(uuid2->uuid.time_mid);
  
  if (a16 < b16)
    return -1;
  else if (a16 > b16)
    return 1;
  
  a16 = ntohs(uuid1->uuid.time_hi_and_version);
  b16 = ntohs(uuid2->uuid.time_hi_and_version);
  
  if (a16 < b16)
    return -1;
  else if (a16 > b16)
    return 1;
      
  if (uuid1->uuid.clock_seq_hi_and_reserved < uuid2->uuid.clock_seq_hi_and_reserved)
    return -1;
  else if (uuid1->uuid.clock_seq_hi_and_reserved > uuid2->uuid.clock_seq_hi_and_reserved)
    return 1;
  
  if (uuid1->uuid.clock_seq_low < uuid2->uuid.clock_seq_low)
    return -1;
  else if (uuid1->uuid.clock_seq_low > uuid2->uuid.clock_seq_low)
    return 1;
  
  return memcmp(uuid1->uuid.node,uuid2->uuid.node,6);
}

/*************************************************************************/

int uuidlib_parse_seg(
	uuid__t     *const uuid,
	size_t             idx,
	const char       **ptext,
	const size_t       bytes
)
{
  char        buf[3];
  const char *p;
  
  assert(uuid   != NULL);
  assert(idx    <  sizeof(struct uuid));
  assert(ptext  != NULL);
  assert(*ptext != NULL);
  assert(bytes  >  0);
  
  errno = 0;
  p     = *ptext;
  
  for (size_t i = 0 ; i < bytes ; i++)
  {
    buf[0] = *p++;
    buf[1] = *p++;
    buf[2] = '\0';
    
    errno = 0;
    uuid->flat[idx++] = strtoul(buf,NULL,16);
    if (errno != 0) return errno;
  }
  
  if ((*p != '-') && (*p != '\0')) 
    return EINVAL;
  
  *ptext = p + 1;
  return 0;
}

/************************************************************************/

int uuidlib_parse(uuid__t *const uuid,const char *text)
{
  int rc;
  
  assert(uuid != NULL);
  assert(text != NULL);
  
  if ((rc = uuidlib_parse_seg(uuid, 0,&text,4)) != 0) return rc;
  if ((rc = uuidlib_parse_seg(uuid, 4,&text,2)) != 0) return rc;
  if ((rc = uuidlib_parse_seg(uuid, 6,&text,2)) != 0) return rc;
  if ((rc = uuidlib_parse_seg(uuid, 8,&text,2)) != 0) return rc;
  if ((rc = uuidlib_parse_seg(uuid,10,&text,6)) != 0) return rc;
  
  return 0;
}

/***************************************************************************/

size_t uuidlib_toa(uuid__t *const uuid,char *dest,size_t size)
{
  assert(uuid != NULL);
  assert(dest != NULL);
  assert(size >= 37);
  
  return snprintf(
  	dest,
  	size,
  	"%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
  	uuid->flat[0],
  	uuid->flat[1],
  	uuid->flat[2],
  	uuid->flat[3],
  	uuid->flat[4],
  	uuid->flat[5],
  	uuid->flat[6],
  	uuid->flat[7],
  	uuid->flat[8],
  	uuid->flat[9],
  	uuid->flat[10],
  	uuid->flat[11],
  	uuid->flat[12],
  	uuid->flat[13],
  	uuid->flat[14],
  	uuid->flat[15]
  );
}

/***********************************************************************/

