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

#ifndef UUIDLIB_H
#define UUIDLIB_H

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

#if RAND_MAX == SHORT_MAX
  typedef unsigned short rand__t;
#else
  typedef unsigned int   rand__t;
#endif

#define UUID_EPOCH	0x01B21DD213814000LL

/*******************************************************************/

#ifdef __SunOS
#pragma pack(1)
#endif

struct uuid
{
  uint32_t time_low;
  uint16_t time_mid;
  uint16_t time_hi_and_version;
  uint8_t  clock_seq_hi_and_reserved;
  uint8_t  clock_seq_low;
  uint8_t  node[6];
} __attribute__((packed));

typedef union 
{
  struct uuid uuid;
  uint8_t     flat[sizeof(struct uuid)];
  rand__t     rnd [sizeof(struct uuid) / sizeof(rand__t)];
} __attribute__((packed)) uuid__t;

#ifdef __SunOS
#pragma pack()
#endif

/*******************************************************************/

extern const uuid__t c_uuid_namespace_dns;
extern const uuid__t c_uuid_namespace_url;
extern const uuid__t c_uuid_namespace_oid;
extern const uuid__t c_uuid_namespace_x500;
extern const uuid__t c_uuid_null;

int	uuidlib_init		(void);
int	uuidlib_v1		(uuid__t *const);
int	uuidlib_v2		(uuid__t *const) __attribute__((unused));
int	uuidlib_v3		(uuid__t *const,const uuid__t *const,const void *const,const size_t);
int	uuidlib_v4		(uuid__t *const);
int	uuidlib_v5		(uuid__t *const,const uuid__t *const,const void *const,const size_t);
int	uuidlib_cmp		(const uuid__t *const restrict,const uuid__t *const restrict);
int	uuidlib_parse_seg	(uuid__t *const,size_t,const char **,const size_t);
int	uuidlib_parse		(uuid__t *const,const char *);
size_t	uuidlib_toa		(const uuid__t *const,char *dest,size_t);

#endif
