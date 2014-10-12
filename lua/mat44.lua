local mat33 = require('mat33')
local vec3 = require('vec3')
local vec4 = require('vec4')

local static = {}
local mt = {}
mt.__index = mt

mt.ati = function (self, i, j)
	return (i-1)*4+j
end

mt.at = function (self, i, j)
	return self[self:ati(i,j)]
end

mt.row = function (self, i)
	return vec4{self:at(i,1), self:at(i,2), self:at(i,3), self:at(i,4)}
end

mt.col = function (self, j)
	return vec4{self:at(1,j), self:at(2,j), self:at(3,j), self:at(4,j)}
end

mt.apply = function (self, v)
	local r = {}
	for i = 1,4 do
		table.insert(r, v:dot(self:row(i)))
	end
	return vec4(r)
end

mt.__mul = function (self, other)
	local product = {}
	for i = 1,4 do
		for j = 1,4 do
			table.insert(product, self:row(i):dot(other:col(j)))
		end
	end
	return static.new(product)
end

mt.mat33 = function (self)
	local r = mat33.identity()
	for i=1,3 do
		for j=1,3 do
			r[r:ati(i,j)] = self:at(i,j)
		end
	end
	return r
end

mt.dump = function (self)
	for i=1,4 do
		print(unpack(self:row(i)))
	end
end

static.new = function (a)
	return setmetatable(a, mt)
end

static.identity = function ()
	return static.new{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}
end

static.from_homogeneous_mat33 = function (m33)
	local result = static.identity()
	for i=1,2 do
		for j=1,2 do
			result[result:ati(i,j)] = m33:at(i,j)
		end
	end
	for i=1,2 do
		result[result:ati(i,4)] = m33:at(i,3)
	end
	return result
end

return setmetatable(static, {__call = function(_, ...) return static.new(...) end })
