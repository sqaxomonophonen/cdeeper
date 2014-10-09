local static = {}
local mt = {}
mt.__index = mt

mt.contains = function (self, p)
	return p[1] >= self[1] and p[1] < (self[1] + self[3]) and p[2] >= self[2] and p[2] < (self[2] + self[4])
end

static.new = function (a)
	return setmetatable(a, mt)
end

return setmetatable(static, {__call = function(_, ...) return static.new(...) end })
