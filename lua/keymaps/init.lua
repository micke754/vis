local M = {
	profile = "vim",
	valid = {
		vim = true,
		helix = true,
	},
}

local managed_modes = {
	vis.modes.NORMAL,
	vis.modes.INSERT,
	vis.modes.VISUAL,
	vis.modes.VISUAL_LINE,
}

local overlays = setmetatable({}, { __mode = "k" })

local function overlay_get(win)
	local overlay = overlays[win]
	if overlay then
		return overlay
	end
	overlay = {}
	overlays[win] = overlay
	return overlay
end

local function overlay_track(win, mode, key)
	local overlay = overlay_get(win)
	if not overlay[mode] then
		overlay[mode] = {}
	end
	overlay[mode][key] = true
end

local function overlay_clear(win)
	local overlay = overlays[win]
	if not overlay then
		return
	end
	for mode, keys in pairs(overlay) do
		for key, _ in pairs(keys) do
			win:unmap(mode, key)
		end
	end
	overlays[win] = nil
end

function M.bind(win, mode, key, rhs, help)
	if not win:map(mode, key, rhs, help) then
		return false
	end
	overlay_track(win, mode, key)
	return true
end

function M.shadow_defaults(win)
	for _, mode in ipairs(managed_modes) do
		for key, _ in pairs(vis:mappings(mode)) do
			if not M.bind(win, mode, key, "<vis-nop>") then
				return false
			end
		end
	end
	return true
end

function M.apply_window(win, profile)
	profile = profile or M.profile
	overlay_clear(win)
	if profile == "vim" then
		return true
	end
	local ok, impl = pcall(require, "keymaps/" .. profile)
	if not ok then
		return false, string.format("Failed to load keymap profile `%s`: %s", profile, tostring(impl))
	end
	if type(impl) ~= "table" or type(impl.apply) ~= "function" then
		return false, string.format("Invalid keymap profile module: `%s`", profile)
	end
	if not impl.apply(M, win) then
		return false, string.format("Failed to apply keymap profile to window: `%s`", profile)
	end
	return true
end

function M.apply(profile)
	profile = profile or M.profile
	if not M.valid[profile] then
		return false, "Unknown keymap profile: " .. tostring(profile)
	end
	for win in vis:windows() do
		local ok, err = M.apply_window(win, profile)
		if not ok then
			return false, err or ("Failed to apply keymap profile: " .. profile)
		end
	end
	M.profile = profile
	return true
end

vis.events.subscribe(vis.events.WIN_OPEN, function(win)
	local ok, err = M.apply_window(win, M.profile)
	if not ok and err then
		vis:info(err)
	end
end)

vis:option_register("keymap", "string", function(name, toggle)
	if toggle then
		vis:info("`set keymap!` is not supported")
		return false
	end
	if not name or name == "" then
		name = "vim"
	end
	local ok, err = M.apply(name)
	if not ok then
		vis:info(err)
		return false
	end
	return true
end, "Keymap profile to use (`vim` or `helix`)")

return M
