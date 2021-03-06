-- ************************************************************************
--
-- Copyright 2018 by Sean Conner.  All Rights Reserved.
--
-- This library is free software; you can redistribute it and/or modify it
-- under the terms of the GNU Lesser General Public License as published by
-- the Free Software Foundation; either version 3 of the License, or (at your
-- option) any later version.
--
-- This library is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
-- or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
-- License for more details.
--
-- You should have received a copy of the GNU Lesser General Public License
-- along with this library; if not, see <http://www.gnu.org/licenses/>.
--
-- Comments, questions and criticisms can be sent to: sean@conman.org
--
-- ************************************************************************
-- luacheck: ignore 611
-- https://www.iana.org/assignments/http-status-codes/http-status-codes.xhtml

return {
  PROCESS =
  {
    CONTINUE          = 100,
    SWITCHPROTO       = 101,
    PROCESSING        = 102,
    EARLYHINTS        = 103,
  },
  
  SUCCESS =
  {
    OKAY              = 200,
    CREATED           = 201,
    ACCEPTED          = 202,
    NOAUTH            = 203,
    NOCONTENT         = 204,
    RESETCONTENT      = 205,
    PARTIALCONTENT    = 206,
    MULTISTATUS       = 207,
    ALREADYREPORTED   = 208,
    IMUSED            = 226,
  },
  
  REDIRECT =
  {
    MULTICHOICE       = 300,
    MOVEPERM          = 301,
    MOVETEMP          = 302,
    SEEOTHER          = 303,
    NOTMODIFIED       = 304,
    USEPROXY          = 305,
    MOVETEMP_M        = 307,
    MOVEPERM_M        = 308,
  },
  
  CLIENT =
  {
    BADREQ            = 400,
    UNAUTHORIZED      = 401,
    PAYMENTREQUIRED   = 402,
    FORBIDDEN         = 403,
    NOTFOUND          = 404,
    METHODNOTALLOWED  = 405,
    NOTACCEPTABLE     = 406,
    PROXYAUTHREQ      = 407,
    TIMEOUT           = 408,
    CONFLICT          = 409,
    GONE              = 410,
    LENGTHREQ         = 411,
    PRECONDITION      = 412,
    TOOLARGE          = 413,
    URITOOLONG        = 414,
    MEDIATYPE         = 415,
    RANGEERROR        = 416,
    EXPECTATION       = 417,
    IM_A_TEA_POT      = 418, -- RFC-2324 section 2.3.2
    MISDIRECT         = 421,
    UNPROCESSENTITY   = 422,
    LOCKED            = 423,
    FAILEDDEP         = 424,
    TOOEARLY          = 425,
    UPGRADE           = 426,
    PRECONDITION2     = 428,
    TOOMANYREQUESTS   = 429,
    REQHDR2BIG        = 431,
    LEGALCENSOR       = 451,
  },
  
  SERVER =
  {
    INTERNALERR       = 500,
    NOTIMP            = 501,
    BADGATEWAY        = 502,
    NOSERVICE         = 503,
    GATEWAYTIMEOUT    = 504,
    HTTPVERSION       = 505,
    VARIANTALSO       = 506,
    ENOSPC            = 507,
    LOOP              = 508,
    EXCEEDBW          = 509,
    NOTEXTENDED       = 510,
    NETAUTHREQ        = 511,
  }
}
