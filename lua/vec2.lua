local static = {}
local mt = {}
mt.__index = mt

mt.scale = function (self, scalar)
	return static.new{self[1] * scalar, self[2] * scalar}
end

mt.dot = function (self, other)
	return self[1]*other[1] + self[2]*other[2]
end

mt.cross = function (self, other)
	-- XXX NO?
	return self:dot(other:normal())
end

mt.length = function (self)
	return math.sqrt(self:dot(self))
end

mt.normal = function (self)
	-- right hand normal
	return static.new{-self[2], self[1]}
end

mt.normalize = function (self)
	return self:scale(1/self:length())
end

mt.__add = function (self, other)
	return static.new{self[1] + other[1], self[2] + other[2]}
end

mt.__sub = function (self, other)
	return static.new{self[1] - other[1], self[2] - other[2]}
end

mt.__unm = function (self)
	return static.new{-self[1], -self[2]}
end

static.new = function (a)
	return setmetatable(a, mt)
end

return setmetatable(static, {__call = function(_, ...) return static.new(...) end })
