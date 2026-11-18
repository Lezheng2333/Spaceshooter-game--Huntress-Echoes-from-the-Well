-- ============================================================
-- HUNTRESS — 128x128 art-setting posters
-- Trainer "Swift" + NightElf.
-- Run with Aseprite:
--   aseprite -b --script art/posters/build_posters.lua --script-param out=art/posters/
-- Draws natively in Aseprite (layers, pixels), saves .aseprite + .png.
-- ============================================================

local W, H = 128, 128
local TAU = math.pi * 2
local RAD = math.pi / 180
local OUT = app.params["out"] or "art/posters/"
if string.sub(OUT, -1) ~= "/" then OUT = OUT .. "/" end

-- ---------- palette / color helpers ----------
local function hex(s)
  return {
    r = tonumber(string.sub(s, 1, 2), 16),
    g = tonumber(string.sub(s, 3, 4), 16),
    b = tonumber(string.sub(s, 5, 6), 16)
  }
end
local function mix(a, b, t)
  if t < 0 then t = 0 elseif t > 1 then t = 1 end
  return {
    r = math.floor(a.r + (b.r - a.r) * t + 0.5),
    g = math.floor(a.g + (b.g - a.g) * t + 0.5),
    b = math.floor(a.b + (b.b - a.b) * t + 0.5)
  }
end
local BAYER = {
  {0, 8, 2, 10},
  {12, 4, 14, 6},
  {3, 11, 1, 9},
  {15, 7, 13, 5}
}
local function bayerVal(x, y) return (BAYER[(y % 4) + 1][(x % 4) + 1] + 0.5) / 16 end

-- ---------- low-level drawing ----------
local function put(img, x, y, c, a)
  a = a or 255
  x = math.floor(x + 0.5); y = math.floor(y + 0.5)
  if x < 0 or y < 0 or x >= W or y >= H or a <= 0 then return end
  if a >= 255 then
    img:drawPixel(x, y, app.pixelColor.rgba(c.r, c.g, c.b, 255))
    return
  end
  local p = img:getPixel(x, y)
  local ca = app.pixelColor.rgbaA(p)
  if ca <= 0 then
    img:drawPixel(x, y, app.pixelColor.rgba(c.r, c.g, c.b, a))
    return
  end
  local na = a + ca * (255 - a) / 255
  local nr = (c.r * a + app.pixelColor.rgbaR(p) * ca * (255 - a) / 255) / na
  local ng = (c.g * a + app.pixelColor.rgbaG(p) * ca * (255 - a) / 255) / na
  local nb = (c.b * a + app.pixelColor.rgbaB(p) * ca * (255 - a) / 255) / na
  img:drawPixel(x, y, app.pixelColor.rgba(
    math.floor(nr + 0.5), math.floor(ng + 0.5), math.floor(nb + 0.5),
    math.floor(na + 0.5)))
end

local function line(img, x0, y0, x1, y1, c, a)
  x0 = math.floor(x0 + 0.5); y0 = math.floor(y0 + 0.5)
  x1 = math.floor(x1 + 0.5); y1 = math.floor(y1 + 0.5)
  local dx = math.abs(x1 - x0)
  local sx = x0 < x1 and 1 or -1
  local dy = -math.abs(y1 - y0)
  local sy = y0 < y1 and 1 or -1
  local err = dx + dy
  while true do
    put(img, x0, y0, c, a)
    if x0 == x1 and y0 == y1 then break end
    local e2 = 2 * err
    if e2 >= dy then err = err + dy; x0 = x0 + sx end
    if e2 <= dx then err = err + dx; y0 = y0 + sy end
  end
end

local function poly(img, pts, c, a)
  if #pts < 3 then return end
  local minY, maxY = 1e9, -1e9
  for i = 1, #pts do
    local p = pts[i]
    if p[2] < minY then minY = p[2] end
    if p[2] > maxY then maxY = p[2] end
  end
  for y = math.floor(minY), math.ceil(maxY) do
    local xs = {}
    for i = 1, #pts do
      local p = pts[i]
      local q = pts[(i % #pts) + 1]
      if (p[2] <= y and q[2] > y) or (q[2] <= y and p[2] > y) then
        xs[#xs + 1] = p[1] + (y - p[2]) * (q[1] - p[1]) / (q[2] - p[2])
      end
    end
    table.sort(xs)
    for i = 1, #xs - 1, 2 do
      for x = math.ceil(xs[i]), math.floor(xs[i + 1]) do
        put(img, x, y, c, a)
      end
    end
  end
end

local function rectFill(img, x0, y0, x1, y1, c, a)
  for y = y0, y1 do
    for x = x0, x1 do put(img, x, y, c, a) end
  end
end

local function disc(img, cx, cy, r, c, a)
  for y = -r, r do
    local w = math.floor(math.sqrt(math.max(0, r * r - y * y)))
    for x = -w, w do put(img, cx + x, cy + y, c, a) end
  end
end

local function ellipseRingRange(img, cx, cy, rx, ry, ang, thick, c, a, t0, t1)
  local co, si = math.cos(ang), math.sin(ang)
  for t = t0, t1, 0.008 do
    local ex, ey = rx * math.cos(t), ry * math.sin(t)
    local x = ex * co - ey * si
    local y = ex * si + ey * co
    for o = 0, thick - 1 do
      put(img, cx + x, cy + y + o, c, a)
    end
  end
end

local function gauss(x, y, cx, cy, sx, sy)
  local dx = (x - cx) / sx
  local dy = (y - cy) / sy
  return math.exp(-(dx * dx + dy * dy) * 0.5)
end

-- ---------- 3x5 font ----------
local FONT = {
  ["A"] = { "010", "101", "111", "101", "101" },
  ["B"] = { "110", "101", "110", "101", "110" },
  ["C"] = { "011", "100", "100", "100", "011" },
  ["D"] = { "110", "101", "101", "101", "110" },
  ["E"] = { "111", "100", "110", "100", "111" },
  ["F"] = { "111", "100", "110", "100", "100" },
  ["G"] = { "011", "100", "101", "101", "011" },
  ["H"] = { "101", "101", "111", "101", "101" },
  ["I"] = { "111", "010", "010", "010", "111" },
  ["J"] = { "001", "001", "001", "101", "010" },
  ["K"] = { "101", "101", "110", "101", "101" },
  ["L"] = { "100", "100", "100", "100", "111" },
  ["M"] = { "111", "111", "101", "101", "101" },
  ["N"] = { "101", "111", "111", "101", "101" },
  ["O"] = { "010", "101", "101", "101", "010" },
  ["P"] = { "110", "101", "110", "100", "100" },
  ["Q"] = { "010", "101", "101", "111", "011" },
  ["R"] = { "110", "101", "110", "101", "101" },
  ["S"] = { "011", "100", "010", "001", "110" },
  ["T"] = { "111", "010", "010", "010", "010" },
  ["U"] = { "101", "101", "101", "101", "111" },
  ["V"] = { "101", "101", "101", "101", "010" },
  ["W"] = { "101", "101", "111", "111", "101" },
  ["X"] = { "101", "101", "010", "101", "101" },
  ["Y"] = { "101", "101", "010", "010", "010" },
  ["Z"] = { "111", "001", "010", "100", "111" },
  ["0"] = { "111", "101", "101", "101", "111" },
  ["1"] = { "010", "110", "010", "010", "111" },
  ["2"] = { "111", "001", "111", "100", "111" },
  ["3"] = { "111", "001", "111", "001", "111" },
  ["4"] = { "101", "101", "111", "001", "001" },
  ["5"] = { "111", "100", "111", "001", "111" },
  ["6"] = { "111", "100", "111", "101", "111" },
  ["7"] = { "111", "001", "001", "001", "001" },
  ["8"] = { "111", "101", "111", "101", "111" },
  ["9"] = { "111", "101", "111", "001", "111" },
  ["-"] = { "000", "000", "111", "000", "000" },
  ["/"] = { "001", "001", "010", "100", "100" },
  ["."] = { "000", "000", "000", "000", "010" },
  [" "] = { "000", "000", "000", "000", "000" }
}

local TITLE_FONT = {
  ["A"] = { "01110", "10001", "11111", "10001", "10001" },
  ["B"] = { "11110", "10001", "11110", "10001", "11110" },
  ["C"] = { "01111", "10000", "10000", "10000", "01111" },
  ["D"] = { "11110", "10001", "10001", "10001", "11110" },
  ["E"] = { "11111", "10000", "11110", "10000", "11111" },
  ["F"] = { "11111", "10000", "11110", "10000", "10000" },
  ["G"] = { "01111", "10000", "10111", "10001", "01111" },
  ["H"] = { "10001", "10001", "11111", "10001", "10001" },
  ["I"] = { "11111", "00100", "00100", "00100", "11111" },
  ["J"] = { "00111", "00010", "00010", "10010", "01100" },
  ["K"] = { "10001", "10010", "11100", "10010", "10001" },
  ["L"] = { "10000", "10000", "10000", "10000", "11111" },
  ["M"] = { "10001", "11011", "10101", "10001", "10001" },
  ["N"] = { "10001", "11001", "10101", "10011", "10001" },
  ["O"] = { "01110", "10001", "10001", "10001", "01110" },
  ["P"] = { "11110", "10001", "11110", "10000", "10000" },
  ["Q"] = { "01110", "10001", "10101", "10010", "01101" },
  ["R"] = { "11110", "10001", "11110", "10010", "10001" },
  ["S"] = { "01111", "10000", "01110", "00001", "11110" },
  ["T"] = { "11111", "00100", "00100", "00100", "00100" },
  ["U"] = { "10001", "10001", "10001", "10001", "01110" },
  ["V"] = { "10001", "10001", "10001", "01010", "00100" },
  ["W"] = { "10001", "10001", "10101", "11011", "01110" },
  ["X"] = { "10001", "01010", "00100", "01010", "10001" },
  ["Y"] = { "10001", "01010", "00100", "00100", "00100" },
  ["Z"] = { "11111", "00010", "00100", "01000", "11111" },
  [" "] = { "00000", "00000", "00000", "00000", "00000" }
}

local function titleText(img, str, x, y, scale, c, a)
  str = string.upper(str)
  local cx = x
  for i = 1, #str do
    local ch = string.sub(str, i, i)
    local glyph = TITLE_FONT[ch]
    if glyph then
      for row = 1, 5 do
        local bits = glyph[row]
        for col = 1, 5 do
          if string.sub(bits, col, col) == "1" then
            for dy = 0, scale - 1 do
              for dx = 0, scale - 1 do
                put(img, cx + (col - 1) * scale + dx + 1,
                        y + (row - 1) * scale + dy + 1, hex("02040a"), math.floor(a * 0.65))
              end
            end
            for dy = 0, scale - 1 do
              for dx = 0, scale - 1 do
                put(img, cx + (col - 1) * scale + dx,
                        y + (row - 1) * scale + dy, c, a)
              end
            end
          end
        end
      end
    end
    cx = cx + 6 * scale
  end
end

local function text(img, str, x, y, scale, c, a)
  str = string.upper(str)
  local cx = x
  for i = 1, #str do
    local ch = string.sub(str, i, i)
    local glyph = FONT[ch]
    if glyph then
      for row = 1, 5 do
        local bits = glyph[row]
        for col = 1, 3 do
          if string.sub(bits, col, col) == "1" then
            for dy = 0, scale - 1 do
              for dx = 0, scale - 1 do
                put(img, cx + (col - 1) * scale + dx + 1,
                        y + (row - 1) * scale + dy + 1, hex("02040a"), math.floor(a * 0.65))
              end
            end
            for dy = 0, scale - 1 do
              for dx = 0, scale - 1 do
                put(img, cx + (col - 1) * scale + dx,
                        y + (row - 1) * scale + dy, c, a)
              end
            end
          end
        end
      end
    end
    cx = cx + 4 * scale
  end
end

-- ---------- background ----------
local function spaceBg(img, pal)
  for y = 0, H - 1 do
    for x = 0, W - 1 do
      local t = y / (H - 1)
      local c = mix(pal.top, pal.bottom, t)
      local dx = (x - 22) / 150
      local dy = (y - 110) / 95
      local g = math.max(0, 1 - (dx * dx + dy * dy))
      c = mix(c, pal.glow, g * 0.32)
      local vx = (x - 64) / 64
      local vy = (y - 64) / 64
      local v = math.min(1, (vx * vx + vy * vy) * 0.55)
      c = mix(c, { r = 0, g = 0, b = 0 }, v * 0.32)
      local d = (bayerVal(x, y) - 0.5) * 0.035
      if d > 0 then c = mix(c, { r = 0, g = 0, b = 0 }, d) end
      img:drawPixel(x, y, app.pixelColor.rgba(c.r, c.g, c.b, 255))
    end
  end
end

local function nebula(img, pal, seed)
  math.randomseed(seed)
  for y = 0, H - 1 do
    for x = 0, W - 1 do
      local n = 1.00 * gauss(x, y, 70, 82, 60, 27)
              + 0.65 * gauss(x, y, 26, 57, 30, 24)
              + 0.30 * gauss(x, y, 104, 101, 34, 22)
              - 0.55 * gauss(x, y, 55, 55, 15, 11)
      if n > 0.16 then
        local t = math.min(1, (n - 0.16) / 0.92)
        local c = mix(pal.nebA, pal.nebB, math.min(1, t * 1.45))
        local a = math.floor(85 * t)
        if bayerVal(x, y) < t then put(img, x, y, c, a) end
      end
      local dust = gauss(x, y, 50, 62, 20, 9)
      if dust > 0.45 then
        put(img, x, y, hex("02040a"), math.floor(55 * dust))
      end
    end
  end
end

local function starGlint(img, x, y, r, c, a)
  line(img, x - r, y, x + r, y, c, math.floor(a * 0.35))
  line(img, x, y - r, x, y + r, c, math.floor(a * 0.35))
  put(img, x, y, c, a)
end

local function starfield(img, seed, count, colors)
  math.randomseed(seed)
  for i = 1, count do
    local x = math.random(0, W - 1)
    local y = math.random(0, H - 1)
    local c = colors[math.random(1, #colors)]
    local a = math.random(35, 200)
    put(img, x, y, c, a)
    if math.random() < 0.12 then put(img, x + 1, y, c, math.floor(a * 0.45)) end
    if math.random() < 0.06 then
      line(img, x - 1, y, x + 1, y, c, 45)
      line(img, x, y - 1, x, y + 1, c, 45)
    end
  end
  local spots = {
    { 18, 18, 1, 170 }, { 96, 22, 2, 200 }, { 110, 96, 1, 150 },
    { 22, 92, 2, 190 }, { 64, 16, 1, 140 }, { 88, 112, 1, 120 }
  }
  for i, s in ipairs(spots) do
    starGlint(img, s[1], s[2], s[3] + 1, colors[1], s[4])
  end
end

-- ---------- celestial bodies ----------
local function planet(img, cx, cy, r, pal)
  -- soft halo outside the disc
  if pal.halo then
    for rr = 1, 5 do disc(img, cx, cy, r + rr, pal.halo, 5) end
  end
  local ringThick = pal.ringThick or 1
  if pal.ring then
    ellipseRingRange(img, cx, cy, pal.rx, pal.ry, pal.ang, ringThick,
      pal.ringColor, pal.ringAlpha, math.pi, TAU)
  end
  for y = -r, r do
    for x = -r, r do
      local d2 = (x * x + y * y) / (r * r)
      if d2 <= 1 then
        local nx, ny = x / r, y / r
        local nz = math.sqrt(math.max(0, 1 - d2))
        local light = math.max(0, nx * -0.55 + ny * -0.70 + nz * 0.45)
        local shade = 0.10 + 0.90 * light
        local c = mix(pal.dark, pal.lit, shade)
        local band = 0.5 + 0.5 * math.sin(ny * 15 + nx * 2.4 + pal.bandPhase)
        c = mix(c, pal.band, 0.15 * band)
        local d = bayerVal(cx + x, cy + y)
        if shade < d * 0.10 then c = mix(c, pal.dark, 0.18) end
        img:drawPixel(cx + x, cy + y, app.pixelColor.rgba(c.r, c.g, c.b, 255))
        if d2 > 0.94 then
          if light > 0.15 then
            put(img, cx + x, cy + y, pal.rim, 205)
          else
            put(img, cx + x, cy + y, pal.rim, 70)
          end
        end
        if d2 > 0.98 and light > 0.50 then
          put(img, cx + x, cy + y, pal.rim, 240)
        end
      end
    end
  end
  for i, cr in ipairs(pal.craters or {}) do
    disc(img, cx + cr[1], cy + cr[2], cr[3], pal.crater, 120)
    for t = math.pi, TAU, 0.22 do
      put(img, cx + cr[1] + math.cos(t) * cr[3],
              cy + cr[2] + math.sin(t) * cr[3], pal.rim, 85)
    end
    for t = 0, math.pi, 0.22 do
      put(img, cx + cr[1] + math.cos(t) * cr[3],
              cy + cr[2] + math.sin(t) * cr[3], pal.dark, 85)
    end
  end
  if pal.storm then
    local sx, sy, rx, ry = cx + pal.storm[1], cy + pal.storm[2], pal.storm[3], pal.storm[4]
    for t = 0, TAU, 0.03 do
      put(img, sx + math.cos(t) * rx, sy + math.sin(t) * ry, pal.band, 120)
    end
  end
  if pal.ring then
    ellipseRingRange(img, cx, cy, pal.rx, pal.ry, pal.ang, ringThick,
      pal.ringColor, pal.ringAlpha, 0, math.pi)
  end
end

local function lightBeam(img, x0, y0, x1, y1, w0, w1, c, a)
  poly(img, {
    { x0 - w0, y0 }, { x0 + w0, y0 },
    { x1 + w1, y1 }, { x1 - w1, y1 }
  }, c, a)
  poly(img, {
    { x0 - 2, y0 }, { x0 + 2, y0 },
    { x1 + 7, y1 }, { x1 - 7, y1 }
  }, c, math.floor(a * 1.5))
end

-- ---------- ship helpers ----------
local function makeXf(cx, cy, scale, ang, squash, depth)
  local co, si = math.cos(ang), math.sin(ang)
  return function(lx, ly)
    local rx = lx * co - ly * si
    local ry = lx * si + ly * co
    return cx + rx * scale, cy + ry * scale * squash + (depth or 0)
  end
end
local function mapPts(xf, pts)
  local out = {}
  for i = 1, #pts do
    local x, y = xf(pts[i][1], pts[i][2])
    out[i] = { x, y }
  end
  return out
end
local function polyL(img, xf, pts, c, a) poly(img, mapPts(xf, pts), c, a) end
local function lineL(img, xf, x0, y0, x1, y1, c, a)
  local ax, ay = xf(x0, y0)
  local bx, by = xf(x1, y1)
  line(img, ax, ay, bx, by, c, a)
end
local function dotL(img, xf, x, y, c, a)
  local px, py = xf(x, y)
  put(img, px, py, c, a)
end
local function mirror(pts)
  local out = {}
  for i = #pts, 1, -1 do out[#out + 1] = { -pts[i][1], pts[i][2] } end
  return out
end

local function sideStrip(img, xf, pts, depth, c, a)
  local top = mapPts(xf, pts)
  local polyPts = {}
  for i = 1, #top do polyPts[#polyPts + 1] = top[i] end
  for i = #top, 1, -1 do
    polyPts[#polyPts + 1] = { top[i][1], top[i][2] + depth }
  end
  poly(img, polyPts, c, a)
end

local function plume(img, xf, lx, ly, len, w0, w1, outer, inner)
  local p1 = {
    { lx - w0, ly }, { lx + w0, ly },
    { lx + w1, ly + len * 0.62 }, { lx, ly + len },
    { lx - w1, ly + len * 0.62 }
  }
  polyL(img, xf, p1, outer, 80)
  local p2 = {
    { lx - w0 + 1, ly }, { lx + w0 - 1, ly },
    { lx, ly + len * 0.62 }
  }
  polyL(img, xf, p2, inner, 155)
  local ax, ay = xf(lx, ly)
  local bx, by = xf(lx, ly + len * 0.45)
  line(img, ax, ay, bx, by, hex("f0ffff"), 210)
end

local function glowAt(img, x, y, r, c, a)
  for d = 0, r do
    local aa = math.floor(a * (1 - d / (r + 1)) * (1 - d / (r + 1)))
    if aa > 0 then disc(img, x, y, r - d, c, aa) end
  end
end

-- ---------- Swift (B) ----------
local function drawSwift(ship, glow)
  local cx, cy = 76, 68
  local scale, ang, squash = 2.18, 30 * RAD, 0.86
  local xf = makeXf(cx, cy, scale, ang, squash, 0)

  local side   = hex("04070d")
  local hullD  = hex("091321")
  local hullM  = hex("182a40")
  local hullL  = hex("2c4861")
  local wingF  = hex("2c586a")
  local wingFL = hex("b9eef9")
  local wingN  = hex("1e3c4b")
  local wingNL = hex("4a7f92")
  local wingD  = hex("142c39")
  local rim    = hex("e8fdff")
  local ice    = hex("9fe8ff")
  local amber  = hex("ffb45e")
  local amberD = hex("9f5b2f")
  local glass  = hex("1c6f8e")
  local glassL = hex("8ff5ff")
  local eng    = hex("7fe8ff")
  local engC   = hex("f0ffff")

  -- exhaust behind the hull
  plume(ship, xf, 0, 13, 14, 2, 1, hex("0c4a66"), eng)

  local HULL = { {0,-19},{2,-14},{3,-6},{2,3},{1,9},{0,13},{-1,9},{-2,3},{-3,-6},{-2,-14} }
  local WING_R = { {3,-2},{14,7},{13,10},{3,6} }
  local WING_L = mirror(WING_R)
  local TAIL_R = { {1,9},{6,13},{5,14},{0,12} }
  local TAIL_L = mirror(TAIL_R)

  -- near-side thickness strips (3/4 depth)
  sideStrip(ship, xf, { {3,-6},{2,3},{1,9},{0,13} }, 2, hex("07131f"), 255)
  sideStrip(ship, xf, { {13,10},{3,6} }, 2, hex("0a1a29"), 255)
  sideStrip(ship, xf, { {5,14},{0,12} }, 1, hex("0a1a29"), 255)

  -- far wing
  polyL(ship, xf, WING_L, wingF, 255)
  polyL(ship, xf, { {-3,-2},{-14,7},{-12,8},{-3,1} }, wingFL, 255)
  polyL(ship, xf, { {-3,2},{-12,8},{-13,10},{-3,6} }, wingD, 255)
  lineL(ship, xf, -3,-2, -14,7, rim, 190)
  lineL(ship, xf, -14,7, -13,10, ice, 100)
  lineL(ship, xf, -13,10, -3,6, hex("335d6c"), 100)
  lineL(ship, xf, -6,-1, -6,6, amber, 140)

  -- near wing
  polyL(ship, xf, WING_R, wingN, 255)
  polyL(ship, xf, { {3,-2},{14,7},{12,8},{3,1} }, wingNL, 255)
  polyL(ship, xf, { {3,2},{12,8},{13,10},{3,6} }, wingD, 255)
  lineL(ship, xf, 3,-2, 14,7, rim, 240)
  lineL(ship, xf, 14,7, 13,10, ice, 160)
  lineL(ship, xf, 13,10, 3,6, hex("335d6c"), 110)
  lineL(ship, xf, 6,-1, 6,6, amber, 190)
  lineL(ship, xf, 5,0, 5,5, amberD, 100)
  lineL(ship, xf, 9,2, 12,7, hex("1e4657"), 100)
  -- training chevron
  dotL(ship, xf, 9,4, amber, 230)
  dotL(ship, xf, 10,4, amber, 230)
  dotL(ship, xf, 9,5, amber, 170)
  dotL(ship, xf, 8,5, amber, 170)

  -- tail planes
  polyL(ship, xf, TAIL_L, wingF, 255)
  polyL(ship, xf, TAIL_R, wingN, 255)
  lineL(ship, xf, 1,9, 6,13, rim, 180)
  lineL(ship, xf, -1,9, -6,13, ice, 130)
  lineL(ship, xf, 6,13, 5,14, ice, 100)
  lineL(ship, xf, -6,13, -5,14, ice, 70)
  -- side-strip top highlights
  lineL(ship, xf, 3,-6, 2,3, hex("2f5a70"), 150)
  lineL(ship, xf, 2,3, 1,9, hex("2f5a70"), 130)
  lineL(ship, xf, 1,9, 0,13, hex("2f5a70"), 110)
  lineL(ship, xf, 13,10, 3,6, hex("2f5a70"), 120)

  -- fuselage
  polyL(ship, xf, HULL, hullD, 255)
  polyL(ship, xf, {
    {0,-18},{2,-13},{3,-6},{2,2},{1,8},{0,11},{-1,8},{-2,2},{-3,-6},{-2,-13}
  }, hullM, 255)
  polyL(ship, xf, {
    {0,-18},{1,-10},{1,7},{0,10},{-1,7},{-1,-10}
  }, hullL, 255)
  lineL(ship, xf, 0,-19, 2,-14, rim, 240)
  lineL(ship, xf, 2,-14, 3,-6, ice, 150)
  lineL(ship, xf, 0,-19, -2,-14, rim, 200)
  lineL(ship, xf, -2,-14, -3,-6, ice, 100)
  lineL(ship, xf, 3,-6, 1,9, hex("2a4356"), 120)
  lineL(ship, xf, -3,-6, -1,9, hex("1b2c3b"), 120)
  lineL(ship, xf, -2,-15, 2,-15, amber, 220)
  lineL(ship, xf, -1,4, 1,4, hex("7ec8dd"), 100)
  lineL(ship, xf, 0,-12, 0,10, hex("5b8aa2"), 90)
  -- sensor
  dotL(ship, xf, 0,-18, engC, 220)

  -- canopy
  local CK = { {0,-11},{2.4,-6},{2.4,1},{0,3},{-2.4,1},{-2.4,-6} }
  polyL(ship, xf, CK, hex("03070d"), 255)
  polyL(ship, xf, { {0,-10},{1.8,-6},{1.8,0},{0,2},{-1.8,0},{-1.8,-6} }, glass, 255)
  polyL(ship, xf, { {0,-10},{1.2,-7},{1.2,-3},{0,-2},{-1.0,-3},{-1.0,-7} }, glassL, 180)
  lineL(ship, xf, 0,-10, -1.5,-5, hex("eaffff"), 225)
  dotL(ship, xf, 0,-8, hex("ffffff"), 235)

  -- engine nozzle
  polyL(ship, xf, { {-2,11},{2,11},{2,14},{-2,14} }, hex("03060b"), 255)
  lineL(ship, xf, -2,11, 2,11, ice, 180)
  lineL(ship, xf, -2,14, 2,14, ice, 180)
  dotL(ship, xf, 0,12, engC, 255)

  -- nav lights
  dotL(ship, xf, -13,9, hex("ff5b52"), 235)
  dotL(ship, xf, 13,9, hex("5bffa0"), 235)

  -- glow
  local gx, gy = xf(0,13)
  glowAt(glow, gx, gy, 6, eng, 65)
  local px, py = xf(0,22)
  glowAt(glow, px, py, 4, eng, 30)
end

-- ---------- NightElf (C) ----------
local function drawNightElf(ship, glow)
  local cx, cy = 78, 69
  local scale, ang, squash = 2.12, 30 * RAD, 0.85
  local xf = makeXf(cx, cy, scale, ang, squash, 0)

  local side   = hex("03050b")
  local hullD  = hex("060c16")
  local hullM  = hex("18263c")
  local hullL  = hex("2c4560")
  local wingF  = hex("1e334a")
  local wingFL = hex("416a86")
  local wingN  = hex("16253a")
  local wingNL = hex("335a72")
  local wingD  = hex("0a1422")
  local rim    = hex("dcf5ff")
  local cyan   = hex("8fe8ff")
  local gold   = hex("ffcf7a")
  local goldD  = hex("9c642f")
  local violet = hex("b58cff")
  local glass  = hex("1a5f86")
  local glassL = hex("9ff8ff")
  local eng    = hex("8fe8ff")
  local engC   = hex("ffffff")

  -- three exhausts behind
  plume(ship, xf, -13, 14, 11, 2, 1, hex("0b3d58"), eng)
  plume(ship, xf, 13, 14, 11, 2, 1, hex("0b3d58"), eng)
  plume(ship, xf, 0, 16, 13, 2, 1, hex("123a66"), cyan)

  local HULL = {
    {0,-25},{2,-18},{3,-10},{4,-2},{3,5},{1,11},{0,14},
    {-1,11},{-3,5},{-4,-2},{-3,-10},{-2,-18}
  }
  local WING_R = { {4,-4},{12,2},{14,9},{13,13},{4,8} }
  local WING_L = mirror(WING_R)
  local POD_R = { {11,4},{16,6},{16,13},{12,14},{10,10} }
  local POD_L = mirror(POD_R)
  local CAN_R = { {4,-13},{10,-9},{8,-7},{3,-10} }
  local CAN_L = mirror(CAN_R)
  local TAIL = { {-3,11},{3,11},{3,17},{-3,17} }

  -- near-side thickness strips (3/4 depth)
  sideStrip(ship, xf, { {4,-2},{3,5},{1,11},{0,14} }, 2, hex("060d19"), 255)
  sideStrip(ship, xf, { {14,9},{13,13},{4,8} }, 2, hex("070f1c"), 255)
  sideStrip(ship, xf, { {16,13},{12,14} }, 1, hex("070f1c"), 255)
  sideStrip(ship, xf, { {3,17},{-3,17} }, 1, hex("070f1c"), 255)

  -- far wing
  polyL(ship, xf, WING_L, wingF, 255)
  polyL(ship, xf, { {-4,-4},{-12,2},{-12,7},{-4,2} }, wingFL, 255)
  polyL(ship, xf, { {-4,3},{-12,7},{-13,13},{-4,8} }, wingD, 255)
  lineL(ship, xf, -4,-4, -12,2, rim, 200)
  lineL(ship, xf, -12,2, -14,9, cyan, 130)
  lineL(ship, xf, -14,9, -13,13, hex("3f6d8a"), 110)
  lineL(ship, xf, -7,-1, -7,8, gold, 160)
  lineL(ship, xf, -9,2, -11,8, violet, 85)

  -- near wing
  polyL(ship, xf, WING_R, wingN, 255)
  polyL(ship, xf, { {4,-4},{12,2},{12,5},{4,1} }, wingNL, 255)
  polyL(ship, xf, { {4,3},{12,7},{13,13},{4,8} }, wingD, 255)
  lineL(ship, xf, 4,-4, 12,2, rim, 240)
  lineL(ship, xf, 12,2, 14,9, cyan, 190)
  lineL(ship, xf, 14,9, 13,13, hex("3f6d8a"), 120)
  lineL(ship, xf, 7,-1, 7,8, gold, 200)
  lineL(ship, xf, 8,0, 8,7, goldD, 110)
  lineL(ship, xf, 9,2, 11,8, violet, 95)
  lineL(ship, xf, 5,4, 10,8, hex("152a40"), 100)

  -- wingtip nacelles (three-engine read)
  polyL(ship, xf, POD_L, wingF, 255)
  polyL(ship, xf, POD_R, wingN, 255)
  polyL(ship, xf, { {11,4},{16,6},{16,9},{11,8} }, wingNL, 255)
  lineL(ship, xf, 11,4, 16,6, rim, 180)
  lineL(ship, xf, 16,6, 16,13, cyan, 160)
  lineL(ship, xf, 12,14, 16,13, gold, 190)
  lineL(ship, xf, -11,4, -16,6, cyan, 140)
  lineL(ship, xf, -12,14, -16,13, gold, 140)

  -- canards
  polyL(ship, xf, CAN_L, wingF, 255)
  polyL(ship, xf, CAN_R, wingN, 255)
  polyL(ship, xf, { {4,-13},{10,-9},{9,-8},{4,-11} }, wingNL, 255)
  lineL(ship, xf, 4,-13, 10,-9, gold, 230)
  lineL(ship, xf, -4,-13, -10,-9, gold, 180)

  -- tail engine block
  polyL(ship, xf, TAIL, hullM, 255)
  lineL(ship, xf, -3,11, 3,11, cyan, 120)
  lineL(ship, xf, -3,17, 3,17, cyan, 100)
  -- side-strip top highlights
  lineL(ship, xf, 4,-2, 3,5, hex("2f556f"), 145)
  lineL(ship, xf, 3,5, 1,11, hex("2f556f"), 125)
  lineL(ship, xf, 1,11, 0,14, hex("2f556f"), 105)
  lineL(ship, xf, 14,9, 13,13, hex("2f556f"), 120)

  -- fuselage
  polyL(ship, xf, HULL, hullD, 255)
  polyL(ship, xf, {
    {0,-24},{2,-17},{3,-10},{3,-2},{2,4},{1,9},{0,12},
    {-1,9},{-2,4},{-3,-2},{-3,-10},{-2,-17}
  }, hullM, 255)
  polyL(ship, xf, {
    {0,-24},{1,-14},{1.5,-4},{1,5},{0,10},{-1,5},{-1.5,-4},{-1,-14}
  }, hullL, 255)
  lineL(ship, xf, 0,-25, 2,-18, rim, 240)
  lineL(ship, xf, 2,-18, 3,-10, cyan, 170)
  lineL(ship, xf, 3,-10, 4,-2, cyan, 110)
  lineL(ship, xf, 0,-25, -2,-18, rim, 190)
  lineL(ship, xf, -2,-18, -3,-10, cyan, 110)
  lineL(ship, xf, 4,-2, 1,11, hex("243a52"), 120)
  lineL(ship, xf, -4,-2, -1,11, hex("182838"), 120)

  -- gold spine / markings
  lineL(ship, xf, 0,-17, 0,-11, gold, 200)
  lineL(ship, xf, 0,-4, 0,7, goldD, 110)
  lineL(ship, xf, -3,-3, 3,-3, violet, 80)
  -- moon emblem on near wing
  dotL(ship, xf, 9,4, cyan, 230)
  dotL(ship, xf, 10,4, cyan, 200)
  dotL(ship, xf, 9,5, gold, 200)
  dotL(ship, xf, 8,5, gold, 170)

  -- canopy
  local CK = { {0,-11},{2.4,-6},{2.4,1},{0,3},{-2.4,1},{-2.4,-6} }
  polyL(ship, xf, CK, hex("03060c"), 255)
  polyL(ship, xf, { {0,-10},{1.8,-6},{1.8,0},{0,2},{-1.8,0},{-1.8,-6} }, glass, 255)
  polyL(ship, xf, { {0,-10},{1.2,-7},{1.2,-3},{0,-2},{-1.0,-3},{-1.0,-7} }, glassL, 180)
  lineL(ship, xf, 0,-10, -1.5,-5, hex("eaffff"), 225)
  dotL(ship, xf, 0,-8, hex("ffffff"), 235)

  -- three nozzles
  polyL(ship, xf, { {-2,14},{2,14},{2,17},{-2,17} }, hex("03060b"), 255)
  lineL(ship, xf, -2,14, 2,14, cyan, 180)
  lineL(ship, xf, -2,17, 2,17, cyan, 180)
  dotL(ship, xf, 0,15, engC, 255)
  polyL(ship, xf, { {-15,11},{-11,11},{-11,14},{-15,14} }, hex("03060b"), 255)
  lineL(ship, xf, -15,11, -11,11, cyan, 160)
  lineL(ship, xf, -15,14, -11,14, cyan, 160)
  dotL(ship, xf, -13,12, engC, 240)
  polyL(ship, xf, { {11,11},{15,11},{15,14},{11,14} }, hex("03060b"), 255)
  lineL(ship, xf, 11,11, 15,11, cyan, 160)
  lineL(ship, xf, 11,14, 15,14, cyan, 160)
  dotL(ship, xf, 13,12, engC, 240)

  -- nav lights
  dotL(ship, xf, -16,10, hex("ff5b52"), 230)
  dotL(ship, xf, 16,10, hex("5bffa0"), 230)

  -- glow
  local gx, gy = xf(0,16)
  glowAt(glow, gx, gy, 7, eng, 70)
  local lx, ly = xf(-13,14)
  glowAt(glow, lx, ly, 4, eng, 45)
  local rx, ry = xf(13,14)
  glowAt(glow, rx, ry, 4, eng, 45)
  local px, py = xf(0,26)
  glowAt(glow, px, py, 4, cyan, 25)
end

-- ---------- frame / title ----------
local function frame(img, title, sub, accent, pale, index)
  -- translucent bottom title band
  rectFill(img, 4, 102, W - 5, H - 5, hex("03060b"), 120)
  line(img, 4, 102, W - 5, 102, accent, 120)
  -- outer border + corner brackets
  line(img, 4, 4, W - 5, 4, accent, 90)
  line(img, 4, H - 5, W - 5, H - 5, accent, 90)
  line(img, 4, 4, 4, H - 5, accent, 70)
  line(img, W - 5, 4, W - 5, H - 5, accent, 70)
  local L = 7
  line(img, 4, 4, 4 + L, 4, pale, 170)
  line(img, 4, 4, 4, 4 + L, pale, 170)
  line(img, W - 5, 4, W - 5 - L, 4, pale, 170)
  line(img, W - 5, 4, W - 5, 4 + L, pale, 170)
  line(img, 4, H - 5, 4 + L, H - 5, pale, 170)
  line(img, 4, H - 5, 4, H - 5 - L, pale, 170)
  line(img, W - 5, H - 5, W - 5 - L, H - 5, pale, 170)
  line(img, W - 5, H - 5, W - 5, H - 5 - L, pale, 170)
  -- index / brand
  rectFill(img, 5, 5, 39, 15, hex("02040a"), 80)
  text(img, "HUNTRESS", 7, 7, 1, pale, 130)
  line(img, 7, 14, 31, 14, accent, 70)
  text(img, index, W - 7 - #index * 4, 7, 1, accent, 130)
  -- divider
  line(img, 7, 102, 74, 102, accent, 160)
  local dx = 78
  line(img, dx, 101, dx + 2, 102, accent, 180)
  line(img, dx + 2, 102, dx, 103, accent, 180)
  -- title
  titleText(img, title, 7, 105, 2, pale, 235)
  text(img, sub, 7, 117, 1, accent, 210)
end

-- ---------- poster assembly ----------
local function newCanvas()
  app.command.NewFile { width = W, height = H, colorMode = ColorMode.RGB }
  local spr = app.activeSprite
  local first = spr.layers[1]
  first.name = "BG · DEEP SPACE"
  local L = { spr = spr, bg = first:cel(1).image }
  local function add(name)
    local l = spr:newLayer()
    l.name = name
    spr:newCel(l, 1)
    return l:cel(1).image
  end
  L.neb = add("NEBULA")
  L.stars = add("STARFIELD")
  L.world = add("WORLD")
  L.light = add("LIGHT")
  L.ship = add("SHIP")
  L.glow = add("ENGINE GLOW")
  L.frame = add("FRAME + TITLE")
  return L
end

local function save(L, base)
  L.spr:saveAs(OUT .. base .. ".aseprite")
  L.spr:saveCopyAs(OUT .. base .. ".png")
  print("saved " .. OUT .. base)
end

local function buildSwift()
  local L = newCanvas()
  spaceBg(L.bg, {
    top = hex("04060e"), bottom = hex("071522"),
    glow = hex("0d3a44")
  })
  nebula(L.neb, {
    nebA = hex("0b3a4a"), nebB = hex("166a75")
  }, 101)
  starfield(L.stars, 211, 95, {
    hex("dff2ff"), hex("9fd8ff"), hex("ffe0a8"), hex("7fc8d8")
  })
  planet(L.world, 30, 31, 17, {
    dark = hex("35202c"), lit = hex("ffd6a0"), band = hex("c9824c"),
    rim = hex("ffeccf"), crater = hex("5b3440"),
    bandPhase = 0.7,
    ring = true, rx = 24, ry = 7.0, ang = -20 * RAD,
    ringColor = hex("d8a47e"), ringAlpha = 190, ringThick = 1,
    storm = { 3, 4, 7, 2.5 }
  })
  drawSwift(L.ship, L.glow)
  frame(L.frame, "SWIFT", "TRAINER / TR-01", hex("ffb45e"), hex("e8fbff"), "01")
  save(L, "trainer_swift_128")
end

local function buildNightElf()
  local L = newCanvas()
  spaceBg(L.bg, {
    top = hex("04040c"), bottom = hex("0e0a1e"),
    glow = hex("2b1a52")
  })
  nebula(L.neb, {
    nebA = hex("1b1646"), nebB = hex("4b2a6a")
  }, 303)
  starfield(L.stars, 433, 100, {
    hex("dfe8ff"), hex("aebcff"), hex("c8a8ff"), hex("9fe8ff")
  })
  planet(L.world, 35, 33, 23, {
    dark = hex("4a4868"), lit = hex("f8fbff"), band = hex("a8b2d8"),
    rim = hex("b9f4ff"), crater = hex("8a90b4"),
    bandPhase = 2.1,
    ring = false,
    craters = {
      { -7, -6, 2 }, { 5, 2, 3 }, { -2, 10, 2 },
      { 11, -9, 2 }, { -12, 5, 2 }, { 2, -11, 2 }
    }
  })
  -- moonwell light shaft (subtle)
  lightBeam(L.light, 35, 52, 44, 128, 3, 11, hex("55c8ee"), 8)
  lightBeam(L.light, 35, 52, 38, 128, 2, 5, hex("9fe8ff"), 5)
  drawNightElf(L.ship, L.glow)
  frame(L.frame, "NIGHTELF", "TRIMOTOR / NE-02", hex("8fe8ff"), hex("e8f4ff"), "02")
  save(L, "nightelf_128")
end

buildSwift()
buildNightElf()
print("posters done")
