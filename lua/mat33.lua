local vec2 = require('vec2')
local vec3 = require('vec3')

local static = {}
local mt = {}
mt.__index = mt

mt.at = function (self, i, j)
	return self[(i-1)*3+j]
end

mt.row = function (self, i)
	return vec3{self:at(i,1), self:at(i,2), self:at(i,3)}
end

mt.col = function (self, j)
	return vec3{self:at(1,j), self:at(2,j), self:at(3,j)}
end

mt.apply = function (self, v)
	v3 = vec3{v[1], v[2], 1}
	local r = {}
	for i = 1,3 do
		table.insert(r, v3:dot(self:row(i)))
	end
	local v2 = vec2{r[1], r[2]}
	return v2:scale(1/r[3])
end

mt.determinant = function (self)
	local colwrap = function (i, j)
		return self:at(i, ((j-1)%3)+1)
	end
	local d = 0
	for x = 0,2 do
		local s = 1
		for i = 1,3 do
			s = s * colwrap(i,i+x)
		end

		local t = 1
		for i = 1,3 do
			t = t * colwrap(i,4-i+x)
		end
		d = d + s - t
	end
	return d
end

mt.minors = function (self)
	local M = {{2,3},{1,3},{1,2}}
	local minors = {}
	for i = 1,3 do
		for j = 1,3 do
			table.insert(minors, 
				self:at(M[i][1], M[j][1]) * self:at(M[i][2], M[j][2]) - 
				self:at(M[i][2], M[j][1]) * self:at(M[i][1], M[j][2])
			)
				
		end
	end
	return static.new(minors)
end

mt.cofactor = function (self)
	local minors = self:minors()
	local cofactor = {}
	local s = 1
	for i = 1,9 do
		table.insert(cofactor, minors[i] * s)
		s = -s
	end
	return static.new(cofactor)
end

mt.transpose = function (self)
	local transpose = {}
	for i = 1,3 do
		for j = 1,3 do
			table.insert(transpose, self:at(j,i))
		end
	end
	return static.new(transpose)
end

mt.adjugate = function (self)
	return self:cofactor():transpose()
end

mt.scale = function (self, scalar)
	local scaled = {}
	for i = 1,9 do
		table.insert(scaled, self[i] * scalar)
	end
	return static.new(scaled)
end

mt.inverse = function (self)
	-- thanks khan academy! ( https://www.khanacademy.org/math/precalculus/precalc-matrices/inverting_matrices/e/matrix_inverse_3x3 )
	return self:adjugate():scale(1 / self:determinant())
end

mt.applyfn = function (self)
	return function (v)
		return self:apply(v)
	end
end

mt.__mul = function (self, other)
	local product = {}
	for i = 1,3 do
		for j = 1,3 do
			table.insert(product, self:row(i):dot(other:col(j)))
		end
	end
	return static.new(product)
end


static.new = function (a)
	return setmetatable(a, mt)
end

static.identity = function ()
	return static.new{1,0,0, 0,1,0, 0,0,1}
end

static.rotate = function (phi)
	local c = math.cos(phi)
	local s = math.sin(phi)
	return static.new{c,s,0, -s,c,0, 0,0,1}
end

static.translate = function (t)
	return static.new{1,0,t[1], 0,1,t[2], 0,0,1}
end

static.scale = function (s)
	return static.new{s,0,0, 0,s,0, 0,0,1}
end

return setmetatable(static, {__call = function(_, ...) return static.new(...) end })

