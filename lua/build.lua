local vec2 = require('vec2')
local mat33 = require('mat33')

function shuffle(list)
	local copy = {}
	for _,e in ipairs(list) do
		table.insert(copy, e)
	end
	local n = #copy
	for i = 1,n do
		local j = math.random(i,n)
		local tmp = copy[j]
		copy[j] = copy[i]
		copy[i] = tmp
	end
	return copy
end

function firstn(n, list)
	if n > #list then error("firstn() out of bounds; n=" .. n .. " for table of length " .. #list) end
	local result = {}
	for i = 1,n do table.insert(result, list[n]) end
	return result
end

function pickn(n, list)
	return firstn(n, shuffle(list))
end

function pick1(list)
	return pickn(1, list)[1]
end


return function (plan_id)
	local load_plan = function (id)
		local list = {}
		local flatten
		flatten = function (subtree)
			for _,x in ipairs(subtree) do
				if type(x) == "table" then
					flatten(x)
				else
					table.insert(list, x)
				end
			end
		end
		flatten(require('plans/' .. id)())
		return list
	end

	local pick_portal = function(target, n)
		local portals = {}
		local match_tag = "portal_" .. n
		for ldi,ld in ipairs(target.linedefs) do
			for _,tag in ipairs(ld.tags or {}) do
				if tag == match_tag then
					if ld.sidedef[1] and ld.sidedef[2] then
						error("two-sided linedef cannot be a portal")
					end
					table.insert(portals, ldi)
					break
				end
			end
		end
		if #portals == 0 then error("no portals in target") end
		local i = pick1(portals)
		local new_tags = {}
		for _,tag in ipairs(target.linedefs[i].tags) do
			if tag ~= match_tag then
				table.insert(new_tags, tag)
			end
		end
		target.linedefs[i].tags = new_tags
		return i
	end

	local lvl = {
		vertices = {},
		linedefs = {},
		sidedefs = {},
		sectors = {},
		entities = {},

		add_initial_brick = function (self, brick)
			self.vertices = brick.vertices
			self.linedefs = brick.linedefs
			self.sidedefs = brick.sidedefs
			self.sectors = brick.sectors
			self.entities = brick.entities
		end,

		add_brick = function (self, brick)
			if #self.vertices == 0 then
				return self:add_initial_brick(brick)
			else
				local t = 0

				local pick = function(target, side)
					local portal = pick_portal(target, t)
					local ld = target.linedefs[portal]
					local s = 0
					if ld.sidedef[2] then s=1 end
					local i0 = (side+s)%2
					local i1 = (side+s+1)%2
					local v0i = ld.vertex[i0+1]
					local v1i = ld.vertex[i1+1]
					return v0i, v1i, portal
				end

				local sv0i, sv1i, sportal = pick(self, 0)
				local bv0i, bv1i, bportal = pick(brick, 1)

				local sv0 = self.vertices[sv0i]
				local sv1 = self.vertices[sv1i]
				local bv0 = brick.vertices[bv0i]
				local bv1 = brick.vertices[bv1i]

				local tx = mat33.map33(bv0, bv1, sv0, sv1)

				local sidedef_offset = #self.sidedefs
				local sector_offset = #self.sectors

				local vertex_map = {[bv0i]=sv0i,[bv1i]=sv1i}
				for i = 1,#brick.vertices do
					if i ~= bv0i and i ~= bv1i then
						table.insert(self.vertices, tx:apply(brick.vertices[i]))
						vertex_map[i] = #self.vertices
					end
				end

				for ldi,ld in ipairs(brick.linedefs) do
					if ldi == bportal then
						local sld = self.linedefs[sportal]
						local c = 0
						for a=1,2 do
							for b=1,2 do
								if not sld.sidedef[a] and ld.sidedef[b] then
									sld.sidedef[a] = ld.sidedef[b] + sidedef_offset
									c = c + 1
								end
							end
						end
						if c ~= 1 then error("expected exactly one replacement") end
					else
						for i = 1,2 do
							ld.vertex[i] = vertex_map[ld.vertex[i]]
							if not ld.vertex[i] then error("vertex_map contains nil") end
							if ld.sidedef[i] then
								ld.sidedef[i] = ld.sidedef[i] + sidedef_offset
							end
						end
						table.insert(self.linedefs, ld)
					end
				end
				for _,sd in ipairs(brick.sidedefs) do
					sd.sector = sd.sector + sector_offset
					table.insert(self.sidedefs, sd)
				end
				for _,sector in ipairs(brick.sectors) do
					table.insert(self.sectors, sector)
				end
				for _,entity in ipairs(brick.entities) do
					entity.position = tx:apply(entity.position)
					table.insert(self.entities, entity)
				end
				-- (NOTE first sidedef is left, second is right)
			end
		end
	}

	local handle_brick = function (name)
		local brick = dofile("lua/bricks/" .. name .. ".lua")

		-- bless vertices
		for i = 1,#brick.vertices do brick.vertices[i] = vec2(brick.vertices[i]) end

		lvl:add_brick(brick)

	end

	for _,thing in ipairs(load_plan(plan_id)) do
		local handlers = {
			["$"] = handle_brick
		}
		handlers[string.sub(thing, 1, 1)](string.sub(thing, 2))
	end

	--[[
	print("vertices: " .. #lvl.vertices)
	print("linedefs: " .. #lvl.linedefs)
	print("sidedefs: " .. #lvl.sidedefs)
	print("sectors: " .. #lvl.sectors)
	]]

	return lvl
end
