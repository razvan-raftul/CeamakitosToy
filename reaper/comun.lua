-- Ceamakito's toy (VST3): functii comune pentru scripturile Alt 1, Alt 2, Alt 3
local M = {}
local SEP = package.config:sub(1, 1)
local NUME = "Ceamakito's toy"
M.SEP = SEP

function M.spune(t)
  if reaper.osara_outputMessage then reaper.osara_outputMessage(t) end
end

local function fx_pe(tr)
  for i = 0, reaper.TrackFX_GetCount(tr) - 1 do
    local _, n = reaper.TrackFX_GetFXName(tr, i, "")
    local l = n:lower():gsub("[^%w]", "")
    if l:find("ceamakito", 1, true) and not n:find("^JS") then return i end
  end
end

-- pista: intai cea selectata, apoi una armata pentru inregistrare, apoi prima care are pluginul
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
  return prima, primaf
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
  if not tr then M.spune("Pune întâi pluginul Ceamakito's toy pe o pistă."); return end
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
