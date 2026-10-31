-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at https://mozilla.org/MPL/2.0/.
-- Music of a SNES game: logs every write of the CPU to the ports of the sound chip ($2140-$2143) as "frame port value", and
-- writes .spc files (the SPC700 sound file format) of the chip at the frames in SPC_FRAMES.
-- Environment: TRACE_OUT (ports log), TRACE_FRAMES, SPC_DIR + SPC_NAME + SPC_FRAMES (comma separated)
local OUT = os.getenv("TRACE_OUT")
local FRAMES = tonumber(os.getenv("TRACE_FRAMES") or "3000")
local want = {}
for n in string.gmatch(os.getenv("SPC_FRAMES") or "", "%d+") do want[tonumber(n)] = true end
local f = io.open(OUT, "w")
local frame = 0
local function le16(v) return string.char(v & 255, (v >> 8) & 255) end

local function dump(name)
  local st = emu.getState()
  local ram = {}
  for i = 0, 65535 do ram[i] = emu.read(i, emu.memType.spcRam) end
  local dsp = {}
  for i = 0, 127 do dsp[i] = emu.read(i, emu.memType.spcDspRegisters) end
  -- the I/O registers of the chip live at $F0-$FF; the RAM under them is not what the chip sees
  local control = (st["spc.timer0.enabled"] and 1 or 0) | (st["spc.timer1.enabled"] and 2 or 0) | (st["spc.timer2.enabled"] and 4 or 0)
  ram[0xF0] = 0x0A; ram[0xF1] = control; ram[0xF2] = st["spc.dspReg"] & 255
  for i = 0, 3 do ram[0xF4 + i] = st["spc.cpuRegs[" .. i .. "]"] & 255 end
  ram[0xFA] = st["spc.timer0.target"] & 255; ram[0xFB] = st["spc.timer1.target"] & 255; ram[0xFC] = st["spc.timer2.target"] & 255
  ram[0xFD] = 0; ram[0xFE] = 0; ram[0xFF] = 0
  local head = "SNES-SPC700 Sound File Data v0.30" .. string.char(26, 26, 26, 30)
  head = head .. le16(st["spc.pc"]) .. string.char(st["spc.a"] & 255, st["spc.x"] & 255, st["spc.y"] & 255, st["spc.ps"] & 255, st["spc.sp"] & 255) .. string.char(0, 0)
  head = head .. string.rep("\0", 0x100 - #head)
  local o = io.open(os.getenv("SPC_DIR") .. "/" .. name .. ".spc", "wb")
  o:write(head)
  local chunk = {}
  for i = 0, 65535 do chunk[#chunk + 1] = string.char(ram[i]); if #chunk == 4096 then o:write(table.concat(chunk)); chunk = {} end end
  for i = 0, 127 do chunk[#chunk + 1] = string.char(dsp[i]) end
  o:write(table.concat(chunk)); o:write(string.rep("\0", 64)); o:write(string.rep("\0", 64))
  o:close()
end

emu.addMemoryCallback(function(a, v) f:write(frame, " ", a & 3, " ", v, "\n") end, emu.callbackType.write, 0x2140, 0x2143, emu.cpuType.snes)
emu.addEventCallback(function()
  local t = {}
  local phase = (frame // 20) % 8
  if phase == 0 then t.start = true elseif phase == 1 then t.a = true elseif phase == 2 then t.right = true
  elseif phase == 3 then t.b = true elseif phase == 4 then t.right = true t.b = true elseif phase == 5 then t.a = true
  elseif phase == 6 then t.start = true else t.right = true end
  pcall(emu.setInput, t, 0, 0)
end, emu.eventType.inputPolled)
emu.addEventCallback(function()
  frame = frame + 1
  if want[frame] then dump(string.format("%s_%05d", os.getenv("SPC_NAME") or "snap", frame)) end
  if frame >= FRAMES then f:close(); emu.stop(0) end
end, emu.eventType.endFrame)
