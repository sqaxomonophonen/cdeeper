local static = {}
local mt = {}
mt.__index = mt

mt.dot = function (self, other)
	return self[1]*other[1] + self[2]*other[2] + self[3]*other[3]
end

mt.scale = function (self, scalar)
	local x = {}
	for i = 1,3 do
		x[i] = self[i] * scalar
	end
	return static.new(x)
end

static.new = function (a)
	return setmetatable(a, mt)
end

return setmetatable(static, {__call = function(_, ...) return static.new(...) end })
