-- Alt 1: instrumentul anterior
local cale = debug.getinfo(1, "S").source:match("^@(.*[\\/])")
dofile(cale .. "comun.lua").pas(-1)
