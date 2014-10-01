local static = {}
local mt = {}
mt.__index = mt

mt.dot = function (self, other)
	return self[1]*other[1] + self[2]*other[2] + self[3]*other[3]
end

static.new = function (a)
	return setmetatable(a, mt)
end

return setmetatable(static, {__call = function(_, ...) return static.new(...) end })
