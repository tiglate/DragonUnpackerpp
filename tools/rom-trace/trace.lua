-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at https://mozilla.org/MPL/2.0/.
-- Trace of the graphics traffic of a SNES game (Mesen2 Lua): DMA to VRAM / CGRAM / OAM with their source,
-- palette snapshots and the palette each background tile and sprite uses. Output: JSON lines in $TRACE_OUT.
-- Environment: TRACE_OUT (file), TRACE_FRAMES (frames to run), TRACE_SNAP (frames between snapshots)
local OUT = os.getenv("TRACE_OUT") or "trace.jsonl"
local FRAMES = tonumber(os.getenv("TRACE_FRAMES") or "3000")
local SNAP = tonumber(os.getenv("TRACE_SNAP") or "120")
local f = io.open(OUT, "w")
local function emit(s) f:write(s, "\n") end

local MEM, CPU = emu.memType, emu.cpuType.snes
local frame = 0

-- shadow registers (the PPU write registers cannot be read back)
local reg = {}
local dma = {}
for c = 0, 7 do dma[c] = {p = 0, b = 0, a = 0, bank = 0, n = 0} end
local vramAddr, vramInc, vramInc2 = 0, 1, 0
local cgAddr = 0
local ppu = {}

local function rd(addr) return emu.read(addr, MEM.snesMemory) end

-- where a CPU address of the A bus lives: ROM offset when it is cartridge ROM, else the RAM
local function where(addr)
  local bank = (addr >> 16) & 0xFF
  if bank == 0x7E or bank == 0x7F then return {"wram", addr - 0x7E0000} end
  local ok, res = pcall(emu.convertAddress, addr, MEM.snesMemory, CPU)
  if ok and res and res.memType == MEM.snesPrgRom then return {"rom", res.address} end
  if ok and res and res.memType == MEM.snesWorkRam then return {"wram", res.address} end
  return {"other", addr}
end

local function fnv(bytes)
  local h = 2166136261
  for i = 1, #bytes do h = ((h ~ bytes[i]) * 16777619) & 0xFFFFFFFF end
  return h
end

local seen = {}
local function hexof(bytes)
  local t = {}
  for i = 1, #bytes do t[i] = string.format("%02x", bytes[i]) end
  return table.concat(t)
end

-- TRACE_READS=1: runs of consecutive ROM bytes that the CPU read since the last transfer to VRAM (a decompressor reads its
-- input sequentially), to find what a game decompresses. Runs shorter than 32 bytes are dropped.
local runs, cur = {}, nil
if os.getenv("TRACE_READS") then
  local function onRead(a)
    if cur and (a == cur[2] or a == cur[2] + 1) then cur[2] = a
    else
      if cur and cur[2] - cur[1] >= 31 and #runs < 400 then runs[#runs + 1] = cur end
      cur = {a, a}
    end
  end
  emu.addMemoryCallback(onRead, emu.callbackType.read, 0x400000, 0x7DFFFF, CPU)
  emu.addMemoryCallback(onRead, emu.callbackType.read, 0xC00000, 0xFFFFFF, CPU)
end
local function readsJson()
  if cur and cur[2] - cur[1] >= 31 then runs[#runs + 1] = cur end
  cur = nil
  local parts = {}
  for _, r in ipairs(runs) do
    local w = where(r[1])
    if w[1] == "rom" then parts[#parts + 1] = string.format("[%d,%d]", w[2], r[2] - r[1] + 1) end
  end
  runs = {}
  return table.concat(parts, ",")
end

-- games that copy graphics with the CPU ($2118/$2119) instead of DMA: the writes are gathered in batches of consecutive
-- addresses, and each batch is logged like a DMA (kind "vram", src "cpu") with the ROM blocks read before it
local cb = {bytes = {}, start = 0, nextWord = -1, lo = 0}
local function flushCpu()
  local n = #cb.bytes
  if n >= 32 then
    local h = fnv(cb.bytes)
    local rec = string.format('{"t":"dma","frame":%d,"kind":"vram","src":"cpu","at":0,"size":%d,"fixed":false,"vram":%d,"inc":1,"hash":"%08x"',
      frame, n, cb.start, h)
    if not seen[h] then seen[h] = true; rec = rec .. ',"data":"' .. hexof(cb.bytes) .. '"' end
    if os.getenv("TRACE_READS") then rec = rec .. ',"reads":[' .. readsJson() .. "]" end
    emit(rec .. "}")
  end
  cb.bytes = {}; cb.nextWord = -1
end

local function onDma(mask)
  for c = 0, 7 do
    if (mask >> c) & 1 == 1 then
      local d = dma[c]
      if (d.p & 0x80) == 0 then               -- A bus to B bus only
        local size = d.n
        if size == 0 then size = 0x10000 end
        local dest = d.b
        local kind = nil
        if dest == 0x18 or dest == 0x19 then kind = "vram"
        elseif dest == 0x22 then kind = "cgram"
        elseif dest == 0x04 then kind = "oam" end
        if kind then
          local fixed = (d.p & 0x08) ~= 0
          local src = (d.bank << 16) | d.a
          local bytes = {}
          if not fixed and size <= 0x10000 then
            for i = 0, size - 1 do bytes[#bytes + 1] = rd((d.bank << 16) | ((d.a + i) & 0xFFFF)) end
          end
          local w = where(src)
          local rec = string.format('{"t":"dma","frame":%d,"kind":"%s","src":"%s","at":%d,"size":%d,"fixed":%s',
            frame, kind, w[1], w[2], size, tostring(fixed))
          if kind == "vram" then rec = rec .. string.format(',"vram":%d,"inc":%d', vramAddr, vramInc) end
          if kind == "cgram" then rec = rec .. string.format(',"cg":%d', cgAddr) end
          if #bytes > 0 then
            local h = fnv(bytes)
            rec = rec .. string.format(',"hash":"%08x"', h)
            if not seen[h] then seen[h] = true; rec = rec .. ',"data":"' .. hexof(bytes) .. '"' end
          end
          if kind == "vram" and w[1] == "wram" and os.getenv("TRACE_READS") then rec = rec .. ',"reads":[' .. readsJson() .. "]" end
          emit(rec .. "}")
          if kind == "vram" then vramAddr = (vramAddr + (size // 2) * vramInc) & 0x7FFF
          elseif kind == "cgram" then cgAddr = (cgAddr + size) & 0x1FF end
        end
      end
    end
  end
end

local calls = 0
local hist = {}
emu.addMemoryCallback(function(a, v)
  calls = calls + 1
  hist[a] = (hist[a] or 0) + 1
  if a == 0x420B then onDma(v)
  elseif a >= 0x4300 and a <= 0x437F then
    local c, r = (a >> 4) & 7, a & 0xF
    local d = dma[c]
    if r == 0 then d.p = v elseif r == 1 then d.b = v
    elseif r == 2 then d.a = (d.a & 0xFF00) | v elseif r == 3 then d.a = (d.a & 0xFF) | (v << 8)
    elseif r == 4 then d.bank = v
    elseif r == 5 then d.n = (d.n & 0xFF00) | v elseif r == 6 then d.n = (d.n & 0xFF) | (v << 8) end
  elseif a == 0x2118 or a == 0x2119 then
    if cb.nextWord ~= vramAddr then flushCpu(); cb.start = vramAddr * 2 end
    cb.bytes[#cb.bytes + 1] = v
    local high = (a == 0x2119)
    if high == ((vramInc2 & 0x80) ~= 0) then vramAddr = (vramAddr + vramInc) & 0x7FFF end
    cb.nextWord = vramAddr
  elseif a == 0x2115 then
    local m = v & 3
    vramInc = (m == 0) and 1 or ((m == 1) and 32 or 128)
    vramInc2 = v
  elseif a == 0x2116 then vramAddr = (vramAddr & 0x7F00) | v
  elseif a == 0x2117 then vramAddr = (vramAddr & 0xFF) | ((v & 0x7F) << 8)
  elseif a == 0x2121 then cgAddr = v * 2
  elseif a >= 0x2100 and a <= 0x2133 then ppu[a] = v end
end, emu.callbackType.write, 0x2100, 0x437F, CPU)

-- snapshots: CGRAM, and the palette used by each tile of the backgrounds and sprites
local lastPal = ""
local function snapshot()
  local pal = {}
  for i = 0, 511 do pal[#pal + 1] = string.format("%02x", emu.read(i, MEM.snesCgRam)) end
  local palhex = table.concat(pal)
  if palhex ~= lastPal then
    lastPal = palhex
    emit(string.format('{"t":"cgram","frame":%d,"data":"%s"}', frame, palhex))
  end
  local mode = (ppu[0x2105] or 0) & 7
  local bgs = {}
  local nbg = (mode == 0) and 4 or ((mode == 1) and 3 or 2)
  for bg = 1, nbg do
    local sc = ppu[0x2106 + bg] or 0
    local base = (sc & 0xFC) << 8       -- word address of the tilemap
    local size = sc & 3
    local chrReg = ppu[(bg <= 2) and 0x210B or 0x210C] or 0
    local chr = ((bg % 2 == 1) and (chrReg & 0x0F) or (chrReg >> 4)) << 12
    local maps = (size == 0) and 1 or ((size == 3) and 4 or 2)
    local used = {}
    for m = 0, maps - 1 do
      for i = 0, 1023 do
        local wa = (base + m * 1024 + i) & 0x7FFF
        local lo = emu.read(wa * 2, MEM.snesVideoRam)
        local hi = emu.read(wa * 2 + 1, MEM.snesVideoRam)
        local tile = lo | ((hi & 3) << 8)
        local pal = (hi >> 2) & 7
        used[tile * 8 + pal] = true
      end
    end
    local list = {}
    for k in pairs(used) do list[#list + 1] = k end
    table.sort(list)
    emit(string.format('{"t":"bg","frame":%d,"bg":%d,"mode":%d,"chr":%d,"map":%d,"tiles":[%s]}',
      frame, bg, mode, chr, base, table.concat(list, ",")))
  end
  if frame % (SNAP * 5) == 0 or frame >= FRAMES then
    local v = {}
    for i = 0, 65535 do v[#v + 1] = string.format("%02x", emu.read(i, MEM.snesVideoRam)) end
    emit(string.format('{"t":"vram","frame":%d,"data":"%s"}', frame, table.concat(v)))
  end
  local obsel = ppu[0x2101] or 0
  local namebase = (obsel & 7) << 14
  local gap = (((obsel >> 3) & 3) + 1) << 13
  local oam = {}
  for i = 0, 543 do oam[#oam + 1] = string.format("%02x", emu.read(i, MEM.snesSpriteRam)) end
  emit(string.format('{"t":"oam","frame":%d,"mode":%d,"obsel":%d,"name":%d,"gap":%d,"data":"%s"}',
    frame, mode, obsel, namebase, gap, table.concat(oam)))
end

-- input: press buttons so that the game goes past its menus and moves around. Each phase lasts 20 frames; every 1200 frames
-- the walking direction changes so that a character does not stay against the same wall.
local AUTOPLAY = os.getenv("TRACE_AUTOPLAY") ~= "0"   -- TRACE_AUTOPLAY=0: a person plays, nothing is pressed
local function press()
  if not AUTOPLAY then return end
  local t = {}
  local phase = (frame // 20) % 12
  local walk = ({"right", "down", "left", "up"})[(frame // 1200) % 4 + 1]
  if phase == 0 then t.start = true elseif phase == 1 then t.a = true elseif phase == 2 then t[walk] = true
  elseif phase == 3 then t.b = true elseif phase == 4 then t[walk] = true t.b = true
  elseif phase == 5 then t.a = true elseif phase == 6 then t.start = true elseif phase == 7 then t[walk] = true t.a = true
  elseif phase == 8 then t.x = true elseif phase == 9 then t[walk] = true t.y = true
  elseif phase == 10 then t.down = true t.a = true else t[walk] = true end
  pcall(emu.setInput, t, 0, 0)
end
emu.addEventCallback(press, emu.eventType.inputPolled)

emu.addEventCallback(function()
  frame = frame + 1
  flushCpu()
  if frame % 300 == 0 then f:flush() end
  if frame % 60 == 0 then
    local parts = {}
    for a, n in pairs(hist) do parts[#parts + 1] = string.format('"%x":%d', a, n) end
    emit(string.format('{"t":"hb","frame":%d,"calls":%d,"regs":{%s}}', frame, calls, table.concat(parts, ",")))
    hist = {}
  end
  if frame % SNAP == 0 then
    local ok, err = pcall(snapshot)
    if not ok then emit(string.format('{"t":"error","frame":%d,"msg":%q}', frame, tostring(err))) end
  end
  if frame % 1500 == 0 then
    local ok, png = pcall(emu.takeScreenshot)
    if ok and png then
      local pf = io.open(OUT .. string.format(".f%05d.png", frame), "wb")
      if pf then pf:write(png) pf:close() end
    end
  end
  if frame >= FRAMES then
    snapshot()
    f:close()
    emu.stop(0)
  end
end, emu.eventType.endFrame)
