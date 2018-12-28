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

return {
  SUCCESS     =  0, -- standard C exit code
  FAILURE     =  1, -- standard C exit code
  
  OK          =  0, -- successful termination
  USAGE       = 64, -- command line usage error
  DATAERR     = 65, -- data format error
  NOINPUT     = 66, -- cannot open input
  NOUSER      = 67, -- addressee unknown
  NOHOST      = 68, -- host name unknown
  UNAVAILABLE = 69, -- service unavailable
  SOFTWARE    = 70, -- internal software error
  OSERR       = 71, -- system error (e.g., can't fork)
  OSFILE      = 72, -- critical OS file missing
  CANTCREAT   = 73, -- can't create (user) output file
  IOERR       = 74, -- input/output error
  TEMPFAIL    = 75, -- temp failure; user is invited to retry
  PROTOCOL    = 76, -- remote error in protocol
  NOPERM      = 77, -- permission denied
  CONFIG      = 78, -- configuration error
  NOTFOUND    = 79, -- required file not found
}
