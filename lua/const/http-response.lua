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
-- HTTP/1.0	RFC-1945
-- HTTP/1.1	RFC-7235 RFC-7233 RFC-7232 RFC-7231 RFC-2616 RFC-2068
-- HTTP/1.1-2.0	RFC-9110

return {
  PROCESS =
  {
    CONTINUE          = 100, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    SWITCHPROTO       = 101, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    PROCESSING        = 102, -- RFC-2518
    EARLYHINTS        = 103, -- RFC-8297
  },
  
  SUCCESS =
  {
    OKAY              = 200, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068 RFC-1945
    CREATED           = 201, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068 RFC-1945
    ACCEPTED          = 202, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068 RFC-1945
    NOAUTH            = 203, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    NOCONTENT         = 204, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068 RFC-1945
    RESETCONTENT      = 205, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    PARTIALCONTENT    = 206, -- RFC-9110 RFC-7233 RFC-2616 RFC-2068
    MULTISTATUS       = 207, -- RFC-4918
    ALREADYREPORTED   = 208, -- RFC-5842
    IMUSED            = 226, -- RFC-3229
  },
  
  REDIRECT =
  {
    MULTICHOICE       = 300, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068 RFC-1945
    MOVEPERM          = 301, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068 RFC-1945
    MOVETEMP          = 302, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068 RFC-1945
    SEEOTHER          = 303, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    NOTMODIFIED       = 304, -- RFC-9110 RFC-7232 RFC-2616 RFC-2068 RFC-1945
    USEPROXY          = 305, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    MOVETEMP_M        = 307, -- RFC-9110 RFC-7231 RFC-2616
    MOVEPERM_M        = 308, -- RFC-9110 RFC-7538 RFC-7238
  },
  
  CLIENT =
  {
    BADREQ            = 400, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068 RFC-1945
    UNAUTHORIZED      = 401, -- RFC-9110 RFC-7235 RFC-2616 RFC-2068 RFC-1945
    PAYMENTREQUIRED   = 402, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    FORBIDDEN         = 403, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068 RFC-1945
    NOTFOUND          = 404, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068 RFC-1945
    METHODNOTALLOWED  = 405, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    NOTACCEPTABLE     = 406, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    PROXYAUTHREQ      = 407, -- RFC-9110 RFC-7235 RFC-2616 RFC-2068
    TIMEOUT           = 408, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    CONFLICT          = 409, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    GONE              = 410, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    LENGTHREQ         = 411, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    PRECONDITION      = 412, -- RFC-9110 RFC-7232 RFC-2616 RFC-2068
    TOOLARGE          = 413, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    URITOOLONG        = 414, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    MEDIATYPE         = 415, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    RANGEERROR        = 416, -- RFC-9110 RFC-7233 RFC-2616
    EXPECTATION       = 417, -- RFC-9110 RFC-7231 RFC-2616
    IM_A_TEA_POT      = 418, -- RFC-2324 section 2.3.2
    MISDIRECT         = 421, -- RFC-9110
    UNPROCESSENTITY   = 422, -- RFC-9110
    LOCKED            = 423, -- RFC-4918
    FAILEDDEP         = 424, -- RFC-4918
    TOOEARLY          = 425, -- RFC-8470
    UPGRADE           = 426, -- RFC-9110 RFC-7231
    PRECONDITION2     = 428, -- RFC-6585
    TOOMANYREQUESTS   = 429, -- RFC-6585
    REQHDR2BIG        = 431, -- RFC-6585
    LEGALCENSOR       = 451, -- RFC-7725
  },
  
  SERVER =
  {
    INTERNALERR       = 500, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068 RFC-1945
    NOTIMP            = 501, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068 RFC-1945
    BADGATEWAY        = 502, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068 RFC-1945
    NOSERVICE         = 503, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    GATEWAYTIMEOUT    = 504, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    HTTPVERSION       = 505, -- RFC-9110 RFC-7231 RFC-2616 RFC-2068
    VARIANTALSO       = 506, -- RFC-2295
    ENOSPC            = 507, -- RFC-4918
    LOOP              = 508, -- RFC-5842
    EXCEEDBW          = 509, -- ?
    NOTEXTENDED       = 510, -- RFC-2774 (obsolete)
    NETAUTHREQ        = 511, -- RFC-6585
  }
}
