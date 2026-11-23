-- Ceamakito's toy (VST3): functii comune pentru scripturile Alt 1, Alt 2, Alt 3
local M = {}
local SEP = package.config:sub(1, 1)
local NUME = "Ceamakito's toy"
M.SEP = SEP

function M.spune(t)
  if reaper.osara_outputMessage then reaper.osara_outputMessage(t) end
end

local function potrivit(n)
  local l = n:lower():gsub("[^%w]", "")
  return l:find("ceamakito", 1, true) ~= nil and not n:find("^JS")
end

local function nume_fx(tr, i)
  local ok, n = reaper.TrackFX_GetNamedConfigParm(tr, i, "fx_name")
  if not ok or n == "" then _, n = reaper.TrackFX_GetFXName(tr, i, "") end
  return n or ""
end

-- cauta pe pista: efectele obisnuite, apoi cele de la intrare (Input FX)
local function fx_pe(tr)
  for i = 0, reaper.TrackFX_GetCount(tr) - 1 do
    if potrivit(nume_fx(tr, i)) then return i end
  end
  for i = 0, reaper.TrackFX_GetRecCount(tr) - 1 do
    local idx = 0x1000000 + i
    if potrivit(nume_fx(tr, idx)) then return idx end
  end
end

-- pista: intai cea selectata, apoi una armata pentru inregistrare, apoi prima care are pluginul, apoi master
function M.gaseste()
  local tr = reaper.GetSelectedTrack(0, 0)
  if tr then local f = fx_pe(tr); if f then return tr, f end end
  local prima, primaf
  for i = 0, reaper.CountTracks(0) - 1 do
    local t = reaper.GetTrack(0, i)
    local f = fx_pe(t)
    if f then
      if reaper.GetMediaTrackInfo_Value(t, "I_RECARM") == 1 then return t, f end
      if not prima then prima, primaf = t, f end
    end
  end
  if prima then return prima, primaf end
  local m = reaper.GetMasterTrack(0)
  local f = fx_pe(m)
  if f then return m, f end
end

-- cand nu-l gaseste, spune ce efecte sunt pe pista selectata, ca sa vedem de ce
function M.lipsa()
  local tr = reaper.GetSelectedTrack(0, 0)
  if not tr then M.spune("Nu găsesc pluginul Ceamakito's toy pe nicio pistă. Selectează pista pe care l-ai pus."); return end
  local nume = {}
  for i = 0, reaper.TrackFX_GetCount(tr) - 1 do nume[#nume + 1] = nume_fx(tr, i) end
  if #nume == 0 then M.spune("Pe pista selectată nu e niciun plugin. Pune pluginul Ceamakito's toy pe ea."); return end
  M.spune("Nu găsesc Ceamakito's toy. Pe pista selectată sunt: " .. table.concat(nume, ", "))
end

function M.param(tr, fx, nume)
  for i = 0, reaper.TrackFX_GetNumParams(tr, fx) - 1 do
    local _, n = reaper.TrackFX_GetParamName(tr, fx, i, "")
    if n == nume then return i end
  end
end

function M.numar(tr, fx)
  local p = M.param(tr, fx, "Numar instrumente")
  if not p then return 0 end
  return math.floor(reaper.TrackFX_GetParamNormalized(tr, fx, p) * 200 + 0.5)
end

function M.curent(tr, fx)
  local p = M.param(tr, fx, "Instrument")
  if not p then return 0 end
  return math.floor(reaper.TrackFX_GetParamNormalized(tr, fx, p) * 199 + 0.5)
end

function M.nume(tr, fx, i)
  local p = M.param(tr, fx, "Instrument")
  local _, s = reaper.TrackFX_FormatParamValueNormalized(tr, fx, p, i / 199, "")
  return s
end

function M.alege(tr, fx, i)
  local p = M.param(tr, fx, "Instrument")
  if not p then return end
  reaper.TrackFX_SetParamNormalized(tr, fx, p, i / 199)
  M.spune(M.nume(tr, fx, i))
end

function M.pas(d)
  local tr, fx = M.gaseste()
  if not tr then M.lipsa(); return end
  local n = M.numar(tr, fx)
  if n == 0 then M.spune("Alege întâi folderul cu instrumente, cu Alt 3."); return end
  local i = ((M.curent(tr, fx) + d) % n + n) % n
  local p = M.param(tr, fx, "Instrument")
  reaper.TrackFX_SetParamNormalized(tr, fx, p, i / 199)
  local t = M.nume(tr, fx, i)
  if n > 1 and i == 0 then t = t .. ", primul" elseif n > 1 and i == n - 1 then t = t .. ", ultimul" end
  M.spune(t)
end

function M.fisier_radacina()
  return os.getenv("APPDATA") .. SEP .. "Ceamakito's toy" .. SEP .. "radacina.txt"
end

return M
