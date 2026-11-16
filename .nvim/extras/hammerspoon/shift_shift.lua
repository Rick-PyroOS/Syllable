-- Shift Shift for Neovim (Search Everywhere, like CLion), without Karabiner.
-- Two taps of Shift within 0.3 s, with no other key in between, send F19 to
-- the front window when its title starts with "nvim" (neovim-clion sets it to
-- "nvim · file.cpp"), so the shell and other apps never see it. Neovim maps
-- F19 to Search Everywhere (lua/config/clion_keys.lua).
--
-- install.sh adds this line to ~/.hammerspoon/init.lua:
--   dofile(os.getenv("HOME") .. "/.config/nvim/extras/hammerspoon/shift_shift.lua")
-- Hammerspoon needs Accessibility permission (System Settings > Privacy & Security).

local GAP = 0.3
local last, down, other = 0, false, false

local function front_is_nvim()
  local win = hs.window.frontmostWindow()
  local title = win and win:title() or ""
  return title:find("nvim", 1, true) ~= nil
end

-- Kept in a global so they aren't garbage-collected (which stops them).
ShiftShift = {
  mods = hs.eventtap.new({ hs.eventtap.event.types.flagsChanged }, function(e)
    local f = e:getFlags()
    local shift_only = f.shift and not (f.cmd or f.alt or f.ctrl or f.fn)
    if shift_only and not down then
      down, other = true, false
    elseif not f.shift and down then
      down = false
      if not other then
        local now = hs.timer.secondsSinceEpoch()
        if now - last < GAP then
          last = 0
          if front_is_nvim() then hs.eventtap.keyStroke({}, "f19", 0) end
        else
          last = now
        end
      end
    elseif not shift_only then
      other, last = true, 0
    end
    return false
  end):start(),
  keys = hs.eventtap.new({ hs.eventtap.event.types.keyDown }, function()
    other, last = true, 0
    return false
  end):start(),
}
