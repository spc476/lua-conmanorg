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

return setmetatable(
{
  ['0'] = 'file',     -- RFC-1436
  ['1'] = 'dir',
  ['2'] = 'CSO',
  ['3'] = 'error',
  ['4'] = 'binhex',
  ['5'] = 'EXE',
  ['6'] = 'uuencode',
  ['7'] = 'search',
  ['8'] = 'telnet',
  ['9'] = 'binary',
  ['+'] = 'server',
  ['T'] = 'tn3270',
  ['g'] = 'gif',
  ['I'] = 'image',
  ['i'] = 'info',     -- extensions
  ['c'] = 'calendar',
  ['d'] = 'worddoc',
  ['h'] = 'html',
  ['P'] = 'PDF',
  ['m'] = 'mail',
  ['M'] = 'MIME',
  ['s'] = 'sound',
  ['x'] = 'xml',
  ['p'] = 'png',
  [';'] = 'video',
  
  file       = '0',
  dir        = '1',
  CSO        = '2',
  error      = '3',
  binhex     = '4',
  EXE        = '5',
  uuencode   = '6',
  search     = '7',
  telnet     = '8',
  binary     = '9',
  server     = '+',
  tn3270     = 'T',
  gif        = 'g',
  image      = 'I',
  info       = 'i',
  calendar   = 'c',
  worddoc    = 'd',
  html       = 'h',
  png        = 'p',
  mail       = 'm',
  sound      = 's',
  xml        = 'x',
  PDF        = 'P',
  MIME       = 'M',
  video      = ';',
},
{
  __index = function(_,key) return key end
}
)
