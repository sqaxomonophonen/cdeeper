local vec3 = require('vec3')

local static = {}
local mt = {}
mt.__index = mt

mt.dot = function (self, other)
	return self[1]*other[1] + self[2]*other[2] + self[3]*other[3] + self[4]*other[4]
end

static.new = function (a)
	if #a == 3 then
		return setmetatable({a[1], a[2], a[3], 1}, mt)
	else
		return setmetatable(a, mt)
	end
end

mt.to_vec3 = function (self)
	return vec3{self[1], self[2], self[3]}
end

return setmetatable(static, {__call = function(_, ...) return static.new(...) end })

