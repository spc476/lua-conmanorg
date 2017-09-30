
local soundex = require "org.conman.string".soundex

io.stdout:write("Testing soundex ... ") io.stdout:flush()

assert(soundex "Conner"   == "C560")
assert(soundex "Sean"     == "S500")
assert(soundex "Sean    " == "S500")

-- https://en.wikipedia.org/wiki/Soundex

assert(soundex "Robert"   == "R163")
assert(soundex "Rupert"   == "R163")
assert(soundex "Rubin"    == "R150")
assert(soundex "Ashcraft" == "A261")
assert(soundex "Ashcroft" == "A261")
--assert(soundex "Tymczak"  == "T522")
assert(soundex "Pfister"  == "P236")

-- http://www.avotaynu.com/soundex.htm

assert(soundex "Miller"    == "M460")
assert(soundex "Peterson"  == "P362")
assert(soundex "Peters"    == "P362")
assert(soundex "Auerbach"  == "A612")
assert(soundex "Uhrbach"   == "U612")
assert(soundex "Moskowitz" == "M232")
assert(soundex "Moskovitz" == "M213")

io.stdout:write("GO!\n")

