-- Alt 3: alegi folderul cu instrumente, apoi instrumentul din lista
local cale = debug.getinfo(1, "S").source:match("^@(.*[\\/])")
local S = dofile(cale .. "comun.lua")

local tr, fx = S.gaseste()
if not tr then S.spune("Pune întâi pluginul Ceamakito's toy pe o pistă."); return end

local start = reaper.GetResourcePath() .. S.SEP .. "Data" .. S.SEP .. "Ceamakito's toy" .. S.SEP .. "Instrumente"
local f = io.open(S.fisier_radacina(), "r")
if f then local r = f:read("*l"); f:close(); if r and r ~= "" then start = r end end

local ok, fisier = reaper.GetUserFileNameForRead(start .. S.SEP, "Intră în folderul cu instrumente și alege orice fișier din el", "")
if not ok or fisier == "" then return end
local dir = fisier:match("^(.*)[\\/][^\\/]*$")
if not dir then return end

-- daca ai ales un fisier chiar dintr-un instrument, radacina e folderul de deasupra
local function are_sunete(d)
  local i = 0
  while true do
    local n = reaper.EnumerateFiles(d, i)
    if not n then return false end
    n = n:lower()
    if n == "harta.txt" or n:match("%.wav$") or n:match("%.aiff?$") or n:match("%.flac$") or n:match("%.ogg$") or n:match("%.mp3$") then return true end
    i = i + 1
  end
end
local function are_instrumente(d)
  local i = 0
  while true do
    local s = reaper.EnumerateSubdirectories(d, i)
    if not s then return false end
    if are_sunete(d .. S.SEP .. s) then return true end
    i = i + 1
  end
end
if not are_instrumente(dir) and are_sunete(dir) then
  dir = dir:match("^(.*)[\\/][^\\/]*$") or dir
end

reaper.RecursiveCreateDirectory(os.getenv("APPDATA") .. S.SEP .. "Ceamakito's toy", 0)
f = io.open(S.fisier_radacina(), "w")
if not f then S.spune("Nu pot salva folderul ales."); return end
f:write(dir); f:close()

local p = S.param(tr, fx, "Reincarca folderul")
local v = reaper.TrackFX_GetParamNormalized(tr, fx, p)
reaper.TrackFX_SetParamNormalized(tr, fx, p, v > 0.5 and 0 or 1)

local incercari = 0
local function arata()
  local n = S.numar(tr, fx)
  if n == 0 and incercari < 20 then incercari = incercari + 1; reaper.defer(arata); return end
  if n == 0 then S.spune("În folderul ales nu sunt instrumente."); return end
  local meniu = {}
  for i = 0, n - 1 do meniu[#meniu + 1] = (S.nume(tr, fx, i) or tostring(i + 1)):gsub("|", "/") end
  gfx.init("Ceamakito's toy", 1, 1)
  gfx.x, gfx.y = 0, 0
  local ales = gfx.showmenu(table.concat(meniu, "|"))
  gfx.quit()
  if ales >= 1 then S.alege(tr, fx, ales - 1) end
end
arata()
