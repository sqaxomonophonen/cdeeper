-- lua simple object notation or whatever

local function encode(enc, value, seen)
	local t = type(value)
	local e = enc[t]
	if e == nil then
		error("no encoder for type '" .. t .. "'")
	end
	return e(value, seen)
end

local enc0 = {
	["nil"] = tostring,
	boolean = tostring,
	number = tostring,
	string = function (v) return string.format("%q", v) end
}

local enc1
enc1 = {
	["nil"] = enc0["nil"],
	boolean = enc0.boolean,
	number = enc0.number,
	string = enc0.string,
	table = function (v, seen)
		if seen[v] then
			error("table ref encountered more than once")
		end
		seen[v] = true

		local s = "{"
		local first = true
		for k,v in pairs(v) do
			if not first then
				s = s .. ","
			end
			s = s .. "[" .. encode(enc0, k) .. "]=" .. encode(enc1, v, seen)
			first = false
		end
		return s .. "}"
	end
}

return function (value, comment)
	local lson
	if comment then
		lson = "-- " .. comment .. "\n"
	else
		lson = ""
	end
	return lson .. "return " .. encode(enc1, value, {}) .. "\n"
end

