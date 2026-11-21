-- Alt 2: instrumentul urmator
local cale = debug.getinfo(1, "S").source:match("^@(.*[\\/])")
dofile(cale .. "comun.lua").pas(1)
