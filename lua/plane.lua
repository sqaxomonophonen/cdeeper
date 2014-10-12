local vec3 = require('vec3')
local vec4 = require('vec4')

local static = {}
local mt = {}
mt.__index = mt

static.new = function (a)
	return setmetatable(a, mt)
end

static.floor = function (z)
	return static.new{0,0,1,-z}
end

static.ceiling = function (z)
	return static.new{0,0,-1,z}
end

mt.normal = function (self)
	return vec3{self[1], self[2], self[3]}
end

mt.d = function (self)
	return self[4]
end

mt.transform = function (self, m44)
	local n = self:normal()
	local o = vec4(self:normal():scale(-self:d()))
	local m33 = m44:mat33()
	n = m33:apply(n)
	o = m44:apply(o):to_vec3()
	local result = static.new{n[1],n[2],n[3],n:dot(o)}
	return static.new{n[1],n[2],n[3],-n:dot(o)}
end

return setmetatable(static, {__call = function(_, ...) return static.new(...) end })
