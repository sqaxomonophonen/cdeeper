-- editor built with love 0.9.1

local entities = require('d/entities')

local entity_type_map = (function ()
	local map = {}
	for _,e in ipairs(entities) do
		map[e.type] = e
	end
	return map
end)()

MAGIC = {
	zoom_speed = 1.08,
	mouse_radius = 48,
	vertex_epsilon = 0.9,
	selection_color = {255,255,0,255},
	sidedef_dist = 5
}

local lson = require('lson')
local mat33 = require('mat33')
local vec2 = require('vec2')
local rect = require('rect')
local fn = require('fn')

function render_grid(bag)
	local tx = bag.tx
	local txi = bag.txi
	local sz = bag.grid_size

	local c0 = txi:homogeneous_apply(vec2{0,0})
	local c1 = txi:homogeneous_apply(vec2{love.graphics.getWidth(),love.graphics.getHeight()})

	local x0 = c0[1] - c0[1]%sz
	local y0 = c0[2] - c0[2]%sz

	local x1 = c1[1] + sz - c1[1]%sz
	local y1 = c1[2] + sz - c1[2]%sz

	local hi = function (v)
		return ((v+sz/4) % (sz*4)) < sz/2
	end

	local zcol = function (v)
		if math.abs(v) < sz/2 then
			love.graphics.setColor(100, 255, 255, 255)
		elseif hi(v) then
			love.graphics.setColor(70, 90, 200, 255)
		else
			love.graphics.setColor(40, 40, 70, 255)
		end
	end

	love.graphics.setLineWidth(1);

	for x = x0,x1,sz do
		zcol(x)
		local v0 = tx:homogeneous_apply(vec2{x,y0})
		local v1 = tx:homogeneous_apply(vec2{x,y1})
		love.graphics.line(v0[1], v0[2], v1[1], v1[2])
	end
	for y = y0,y1,sz do
		zcol(y)
		local v0 = tx:homogeneous_apply(vec2{x0,y})
		local v1 = tx:homogeneous_apply(vec2{x1,y})
		love.graphics.line(v0[1], v0[2], v1[1], v1[2])
	end
end

function raw_snap(value, interval)
	local frac = value % interval
	if frac < interval/2 then
		return value - frac
	else
		return value - frac + interval
	end
end

function snap_to_grid(bag, v)
	local sz = bag.grid_size
	return vec2{raw_snap(v[1],sz), raw_snap(v[2],sz)}
end

function snap_to_grid_or_dont(bag, v)
	if bag.snap_to_grid then
		return snap_to_grid(bag, v)
	else
		return v
	end
end

function mouse_vec2()
	return vec2{love.mouse.getX(), love.mouse.getY()}
end

function tx_txi_mtx(nav)
	local tx = mat33.identity()
	tx = tx * mat33.homogeneous_translate{love.graphics.getWidth()/2, love.graphics.getHeight()/2}
	tx = tx * mat33.homogeneous_translate(nav.pan)
	tx = tx * mat33.homogeneous_scale(nav.scale)

	local txi = tx:inverse()
	local mousetx = txi:homogeneous_apply(mouse_vec2())

	return tx, txi, mousetx
end

function new_brick()
	return {
		vertices = {},
		linedefs = {},
		sidedefs = {},
		sectors = {},
		entities = {}
	}
end

function brick_path(name)
	return "bricks/" .. name .. ".lua"
end

function load_brick(name)
	local f = io.open(brick_path(name), "r")
	if f == nil then
		return new_brick()
	else
		local line = f:read("*l")
		if line ~= "-- lson brick" then
			error("not an lson brick")
		end
		local rest = f:read("*a")
		local brick = load(rest)()
		brick.vertices = fn.map(vec2, brick.vertices)
		for k,v in pairs(new_brick()) do
			if not brick[k] then brick[k] = v end
		end
		return brick
	end
end

function save_brick(name, brick)
	local f = io.open(brick_path(name), "w")
	f:write(lson(brick, "lson brick"))
	f:close()
end


function mouse_vertex(bag)
	local nr = nil
	local ni = nil
	for i,v in ipairs(bag.brick.vertices) do
		local vtx = bag.tx:homogeneous_apply(v)
		local r = (vtx-bag.mouse):length()
		if ni == nil or r < nr then
			ni = i
			nr = r
		end
	end
	if nr and nr < MAGIC.mouse_radius then
		return ni
	else
		return nil
	end
end

function mouse_linedef(bag)
	local nr = nil
	local nsd = nil
	local ni = nil
	for i,ld in ipairs(bag.brick.linedefs) do
		local v0 = bag.tx:homogeneous_apply(bag.brick.vertices[ld.vertex[1]])
		local v1 = bag.tx:homogeneous_apply(bag.brick.vertices[ld.vertex[2]])
		local len = (v1-v0):length()
		local vd = (v1-v0):normalize()
		local dw = bag.mouse - v0
		local u = vd:dot(dw)
		local sd = dw:dot(vd:normal())
		if u >= 0 and u <= len then
			local dabs = math.abs(sd)
			if ni == nil or dabs < nr then
				ni = i
				nr = dabs
				nsd = sd
			end
		else
			local v = u < 0 and v0 or v1
			local d = (v-bag.mouse):length()
			if ni == nil or d < nr then
				ni = i
				nr = d
				nsd = sd
			end
		end
	end
	if nr and nr < MAGIC.mouse_radius then
		return ni, nsd
	else
		return nil
	end
end

function mouse_sector(bag)
	local sector_linedefs = {}
	for i,ld in ipairs(bag.brick.linedefs) do
		for _,side in ipairs{-1,1} do
			local sd = bag.brick.sidedefs[ld.sidedef[linedefside(side)]]
			if sd and sd.sector then
				if not sector_linedefs[sd.sector] then
					sector_linedefs[sd.sector] = {}
				end
				sector_linedefs[sd.sector][ld] = true
			end
		end
	end
	local dir = vec2{1,0}
	for sector,linedefs in ipairs(sector_linedefs) do
		local intersections = 0
		for ld,_ in pairs(linedefs) do
			local v0 = bag.brick.vertices[ld.vertex[1]]
			local v1 = bag.brick.vertices[ld.vertex[2]]
			local vd = v1-v0
			local rxs = dir:cross(vd)
			if rxs ~= 0 then
				local qp = v0 - bag.mousetx
				local t = qp:cross(vd) / rxs
				if t > 0 then
					local u = qp:cross(dir) / rxs
					if u >= 0 and u <= 1 then
						intersections = intersections + 1
					end
				end
			end
		end
		if intersections % 2 == 1 then
			return sector
		end
	end
	return nil
end

function mouse_entity(bag)
	local nr = nil
	local ni = nil
	for i,e in ipairs(bag.brick.entities) do
		local r = (e.position - bag.mousetx):length()
		local et = entity_type_map[e.type]
		if r < et.radius and ( ni == nil or r < nr) then
			ni = i
			nr = r
		end
	end
	if ni then
		return ni
	else
		return nil
	end
end

selection = {
	selected = {},
	is_ordered = true,

	clear = function (self)
		self.selected = {}
		self.is_ordered = true
	end,

	toggle_one = function (self, i)
		found = false
		local ns = {}
		for n,s in ipairs(self.selected) do
			if i == s then
				if n ~= #self.selected then
					self.is_ordered = false
				end
				found = true
			else
				table.insert(ns, s)
			end
		end
		if not found then
			table.insert(ns, i)
		end
		self.selected = ns
		if self:empty() then
			self.is_ordered = true
		end
	end,

	toggle_multiple = function (self, is)
		for _,i in ipairs(is) do self:toggle_one(i) end
		self.is_ordered = false
	end,

	has = function (self, i)
		for _,s in ipairs(self.selected) do
			if i == s then return true end
		end
		return false
	end,

	empty = function (self)
		return #self.selected == 0
	end,

	as_string = function (self)
		return "S:" .. #self.selected .. " " .. (self.is_ordered and "ordered" or "unordered")
	end
}

function insert_vertex(brick, p)
	for i,v in ipairs(brick.vertices) do
		if (p-v):length() < MAGIC.vertex_epsilon then
			return i
		end
	end
	table.insert(brick.vertices, p)
	return #brick.vertices
end

function insert_linedef(brick, v1, v2)
	for i,ld in ipairs(brick.linedefs) do
		if (ld.vertex[1] == v1 and ld.vertex[2] == v2) or (ld.vertex[1] == v2 and ld.vertex[2] == v1) then
			return i
		end
	end
	local linedef = { vertex = {v1, v2}, sidedef = {} }
	table.insert(brick.linedefs, linedef)
	return #brick.linedefs
end

function linedefside(side)
	return side > 0 and 2 or 1
end

function insert_sidedef(brick, linedef, side)
	local ld = brick.linedefs[linedef]
	local lds = linedefside(side)
	local sd = ld.sidedef[lds]
	if sd then return sd end
	local sidedef = {}
	table.insert(brick.sidedefs, sidedef)
	local i = #brick.sidedefs
	ld.sidedef[lds] = i
	return i
end

function insert_sector(brick)
	local sector = {
		flat = {
			{plane={0,0,1,64}},
			{plane={0,0,-1,64}}
		}
	}
	table.insert(brick.sectors, sector)
	return #brick.sectors
end

function insert_entity(brick, type, position)
	local entity = {
		type = type,
		position = position,
		yaw = 0
	}
	table.insert(brick.entities, entity)
	return #brick.entities
end

function delete_sector(brick, to_delete)
	brick.sectors[to_delete] = false
	for i = 1,#brick.sidedefs do
		local sd = brick.sidedefs[i]
		if sd and sd.sector == to_delete then
			sd.sector = nil
		end
	end
end

function delete_sidedef(brick, to_delete)
	local sd = brick.sidedefs[to_delete]
	brick.sidedefs[to_delete] = false
	if sd.sector then
		local sector_still_exists = false
		for i = 1,#brick.sidedefs do
			local sd2 = brick.sidedefs[i]
			if sd2 and sd2.sector == sd.sector then
				sector_still_exists = true
				break
			end
		end
		if not sector_still_exists then
			delete_sector(brick, sd.sector)
		end
	end
end

function delete_linedef(brick, to_delete)
	local ld = brick.linedefs[to_delete]
	if ld.sidedef[1] then delete_sidedef(brick, ld.sidedef[1]) end
	if ld.sidedef[2] then delete_sidedef(brick, ld.sidedef[2]) end
	brick.linedefs[to_delete] = false
end

function delete_vertex(brick, to_delete)
	brick.vertices[to_delete] = false
	for i = 1,#brick.linedefs do
		local ld = brick.linedefs[i]
		if ld and (not brick.vertices[ld.vertex[1]] or not brick.vertices[ld.vertex[2]]) then
			delete_linedef(brick, i)
		end
	end
end

function delete_entity(brick, to_delete)
	brick.entities[to_delete] = false
end

function remap(brick)
	local sectorremap = {}
	local nsec = {}
	for i = 1,#brick.sectors do
		local sector = brick.sectors[i]
		if sector then
			table.insert(nsec, sector)
			sectorremap[i] = #nsec
		end
	end
	brick.sectors = nsec

	local vremap = {}
	local nv = {}
	for i = 1,#brick.vertices do
		local v = brick.vertices[i]
		if v then
			table.insert(nv, v)
			vremap[i] = #nv
		end
	end
	brick.vertices = nv

	local sdremap = {}
	local nsd = {}
	for i = 1,#brick.sidedefs do
		local sd = brick.sidedefs[i]
		if sd then
			sd.sector = sectorremap[sd.sector]
			table.insert(nsd, sd)
			sdremap[i] = #nsd
		end
	end
	brick.sidedefs = nsd

	local nld = {}
	for i = 1,#brick.linedefs do
		local ld = brick.linedefs[i]
		if ld then
			ld.vertex[1] = vremap[ld.vertex[1]]
			ld.vertex[2] = vremap[ld.vertex[2]]
			table.insert(nld, ld)
			ld.sidedef[1] = sdremap[ld.sidedef[1]]
			ld.sidedef[2] = sdremap[ld.sidedef[2]]
		end
	end
	brick.linedefs = nld

	local nent = {}
	for i = 1,#brick.entities do
		local e = brick.entities[i]
		if e then
			table.insert(nent, e)
		end
	end
	brick.entities = nent
end

function rect_test(p, p0, pdim)
	if pdim[1] < 0 then
		pdim[1] = -pdim[1]
		p0[1] = p0[1] - pdim[1]
	end
	if pdim[2] < 0 then
		pdim[2] = -pdim[2]
		p0[2] = p0[2] - pdim[2]
	end
	local p1 = p0 + pdim
	return p[1] >= p0[1] and p[1] <= p1[1] and p[2] >= p0[2] and p[2] <= p1[2]

end

modes = {

-- VERTEX MODE
vertex = {
	name = 'vertex',
	is_primary_mode = true,

	insert = function (self, bag)
		local i = insert_vertex(bag.brick, snap_to_grid_or_dont(bag, bag.mousetx))
		selection:toggle_one(i)
	end,

	select = function (self, bag)
		local i = mouse_vertex(bag)
		if i then selection:toggle_one(i) end
	end,

	select_all = function (self, bag)
		local is = {}
		for i = 1,#bag.brick.vertices do
			table.insert(is, i)
		end
		selection:toggle_multiple(is)
	end,

	--[[
	pre_render = function (self, bag)
		bag.highlighted_vertex = mouse_vertex(bag)
	end,
	]]

	create = function (self, bag)
		if #selection.selected < 2 or not selection.is_ordered then
			return
		end
		for i = 1,#selection.selected-1 do
			insert_linedef(bag.brick, selection.selected[i], selection.selected[i+1])
		end
		selection:clear()
	end,

	delete = function (self, bag)
		for _,i in ipairs(selection.selected) do
			delete_vertex(bag.brick, i)
		end
		remap(bag.brick)
		selection:clear()
	end,

	block_select = function (self, bag, p0, pdim)
		for i,v in ipairs(bag.brick.vertices) do
			local vtx = bag.tx:homogeneous_apply(v)
			if rect_test(vtx, p0, pdim) then
				selection:toggle_one(i)
			end
		end
	end,

	get_selected_positions = function (self, brick)
		local positions = {}
		for _,i in ipairs(selection.selected) do
			positions[i] = brick.vertices[i]
		end
		return positions
	end
},

-- LINEDEF MODE
linedef = {
	name = 'linedef',
	is_primary_mode = true,

	last_vertex = nil,

	can_cancel = function (self)
		return self.last_vertex ~= nil
	end,

	cancel = function (self)
		self.last_vertex = nil
	end,

	_mouse_vertex = function (self, bag)
		return not bag.shifted and mouse_vertex(bag) or nil
	end,

	insert = function (self, bag)
		if self:handle_tag_click(bag) then return end
		local v = self:_mouse_vertex(bag)
		if not v then
			v = insert_vertex(bag.brick, snap_to_grid_or_dont(bag, bag.mousetx))
		end
		if self.last_vertex and v ~= self.last_vertex then
			local i = insert_linedef(bag.brick, self.last_vertex, v)
			selection:toggle_one(i)

		end
		self.last_vertex = v
	end,

	select = function (self, bag)
		local i = mouse_linedef(bag)
		if i then selection:toggle_one(i) end
	end,

	select_all = function (self, bag)
		local is = {}
		for i = 1,#bag.brick.linedefs do
			table.insert(is, i)
		end
		selection:toggle_multiple(is)
	end,

	pre_render = function (self, bag)
		bag.highlighted_vertex = self:_mouse_vertex(bag)
		if self.last_vertex then
			local v0 = bag.tx:homogeneous_apply(bag.brick.vertices[self.last_vertex])
			local v1 = bag.tx:homogeneous_apply(bag.highlighted_vertex and bag.brick.vertices[bag.highlighted_vertex] or snap_to_grid_or_dont(bag, bag.mousetx))
			love.graphics.setColor{70,70,255,255}
			love.graphics.line(v0[1], v0[2], v1[1], v1[2])
		end
	end,

	delete = function (self, bag)
		for _,i in ipairs(selection.selected) do
			delete_linedef(bag.brick, i)
		end
		remap(bag.brick)
		selection:clear()
	end,

	block_select = function (self, bag, p0, pdim)
		local vertices = {}
		local toggled = {}
		for i,v in ipairs(bag.brick.vertices) do
			local vtx = bag.tx:homogeneous_apply(v)
			if rect_test(vtx, p0, pdim) then
				vertices[i] = true
			end
		end
		for i,ld in ipairs(bag.brick.linedefs) do
			if not toggled[i] then
				if vertices[ld.vertex[1]] and vertices[ld.vertex[2]] then
					selection:toggle_one(i)
					toggled[i] = true
				end
			end
		end
	end,

	get_selected_positions = function (self, brick)
		local vs = {}
		for _,i in ipairs(selection.selected) do
			local ld = brick.linedefs[i]
			vs[ld.vertex[1]] = true
			vs[ld.vertex[2]] = true
		end

		local positions = {}
		for i,_ in pairs(vs) do
			positions[i] = brick.vertices[i]
		end
		return positions
	end,

	tags = function (self)
		return {
			'portal_0',
			'portal_1'
		}
	end,

	tag_rect = function (self, i)
		return rect{10, 60 + i * 18, 100, 18}
	end,

	handle_tag_click = function (self, bag)
		local i = 0
		for _,tag in ipairs(self:tags()) do
			local r = self:tag_rect(i)
			if r:contains(bag.mouse) then
				local found = false
				for _,i in ipairs(selection.selected) do
					local ld = bag.brick.linedefs[i]
					for _,t in ipairs(ld.tags or {}) do
						if t == tag then
							found = true
						end
					end
				end
				if found then
					-- remove
					for _,i in ipairs(selection.selected) do
						local ld = bag.brick.linedefs[i]
						local new_tags = {}
						for _,t in ipairs(ld.tags or {}) do
							if t ~= tag then
								table.insert(new_tags, t)
							end
						end
						ld.tags = new_tags
					end
				else
					-- add
					for _,i in ipairs(selection.selected) do
						local ld = bag.brick.linedefs[i]
						if not ld.tags then ld.tags = {} end
						local already_there = false
						for _,t in ipairs(ld.tags) do
							if t == tag then
								already_there = true
							end
						end
						if not already_there then
							table.insert(ld.tags, tag)
						end
					end
				end
				return true
			end
			i = i + 1
		end
		return false
	end,

	render_overlay = function (self, bag)
		local i = 0
		for _,tag in ipairs(self:tags()) do
			local count = 0
			for _,i in ipairs(selection.selected) do
				local ld = bag.brick.linedefs[i]
				for _,t in ipairs(ld.tags or {}) do
					if t == tag then
						count = count + 1
					end
				end
			end
			local color
			local pre
			if count == 0 then
				color = {155,155,155,200}
				pre = "-"
			elseif count == #selection.selected then
				color = {255,0,255,200}
				pre = "+"
			else
				color = {0,128,128,200}
				pre = "="
			end

			local r = self:tag_rect(i)
			if r:contains(bag.mouse) and #selection.selected > 0 then
				color[4] = 255
			end
			love.graphics.setColor(color)

			love.graphics.print(pre .. " " .. tag, r[1] + 10, r[2])
			i = i + 1
		end
	end

},

-- SIDEDEF MODE
sidedef = {
	name = "sidedef",
	is_primary_mode = true,
	insert = function (self, bag)
		local i, d = mouse_linedef(bag)
		if i then
			local i = insert_sidedef(bag.brick, i, d)
			selection:toggle_one(i)
		end
	end,

	pre_render = function (self, bag)
		local i, d = mouse_linedef(bag)
		if i then
			local ld = bag.brick.linedefs[i]
			local vtx0 = bag.tx:homogeneous_apply(bag.brick.vertices[ld.vertex[1]])
			local vtx1 = bag.tx:homogeneous_apply(bag.brick.vertices[ld.vertex[2]])
			local v = (vtx1-vtx0):normalize()
			local n = v:normal()
			local sgn = d>0 and 1 or -1
			local dist = MAGIC.sidedef_dist
			local p0 = vtx0 + v:scale(dist) + n:scale(sgn*dist)
			local p1 = vtx1 - v:scale(dist) + n:scale(sgn*dist)
			love.graphics.setLineWidth(3)
			love.graphics.setColor(0,50,120,255)
			love.graphics.line(p0[1], p0[2], p1[1], p1[2])
		end
	end,

	select = function (self, bag)
		local i, d = mouse_linedef(bag)
		if i then
			local ld = bag.brick.linedefs[i]
			local lds = linedefside(d)
			if ld.sidedef[lds] then
				selection:toggle_one(ld.sidedef[lds])
			end
		end
	end,

	select_all = function (self, bag)
		local is = {}
		for i = 1,#bag.brick.sidedefs do
			table.insert(is, i)
		end
		selection:toggle_multiple(is)
	end,

	block_select = function (self, bag, p0, pdim)
		local vertices = {}
		for i,v in ipairs(bag.brick.vertices) do
			local vtx = bag.tx:homogeneous_apply(v)
			if rect_test(vtx, p0, pdim) then
				vertices[i] = true
			end
		end
		local toggled = {}
		for i,ld in ipairs(bag.brick.linedefs) do
			if vertices[ld.vertex[1]] and vertices[ld.vertex[2]] then
				for _,side in ipairs{-1,1} do
					local sd = ld.sidedef[linedefside(side)]
					if sd and not toggled[sd] then
						selection:toggle_one(sd)
					end
				end
			end
		end
	end,

	delete = function (self, bag)
		for _,i in ipairs(selection.selected) do
			delete_sidedef(bag.brick, i)
		end
		remap(bag.brick)
		selection:clear()
	end,

	create = function (self, bag)
		if selection:empty() then
			return
		end

		local sector = nil
		for _,i in ipairs(selection.selected) do
			local sd = bag.brick.sidedefs[i]
			if sd.sector then
				sector = sd.sector
				break
			end
		end
		if not sector then
			sector = insert_sector(bag.brick)
		end
		for _,i in ipairs(selection.selected) do
			local sd = bag.brick.sidedefs[i]
			sd.sector = sector
		end
		selection:clear()
	end,

	get_selected_positions = function (self, brick)
		local sds = {}
		for _,i in ipairs(selection.selected) do
			sds[i] = true
		end
		local vs = {}
		for _,ld in ipairs(brick.linedefs) do
			if sds[ld.sidedef[1] ] or sds[ld.sidedef[2] ] then
				vs[ld.vertex[1] ] = true
				vs[ld.vertex[2] ] = true
			end
		end
		local positions = {}
		for i,_ in pairs(vs) do
			positions[i] = brick.vertices[i]
		end
		return positions
	end
},

-- SECTOR MODE
sector = {
	name = "sector",
	is_primary_mode = true,

	select = function (self, bag)
		local i = mouse_sector(bag)
		if i then selection:toggle_one(i) end
	end,

	select_all = function (self, bag)
		local is = {}
		for i = 1,#bag.brick.sectors do
			table.insert(is, i)
		end
		selection:toggle_multiple(is)
	end,

	block_select = function (self, bag, p0, pdim)
		local vertices = {}
		for i,v in ipairs(bag.brick.vertices) do
			local vtx = bag.tx:homogeneous_apply(v)
			if rect_test(vtx, p0, pdim) then
				vertices[i] = true
			end
		end
		local sectors = {}
		for i,_ in ipairs(bag.brick.sectors) do
			sectors[i] = true
		end
		for i,ld in ipairs(bag.brick.linedefs) do
			for _,side in ipairs{-1,1} do
				local sdi = ld.sidedef[linedefside(side)]
				if sdi then
					local sd = bag.brick.sidedefs[sdi]
					if sd.sector then
						if not vertices[ld.vertex[1]] or not vertices[ld.vertex[2]] then
							sectors[sd.sector] = false
						end
					end
				end
			end
		end
		for i,b in ipairs(sectors) do
			if b then
				selection:toggle_one(i)
			end
		end
	end,

	delete = function (self, bag)
		for _,i in ipairs(selection.selected) do
			delete_sector(bag.brick, i)
		end
		remap(bag.brick)
		selection:clear()
	end,

	get_selected_positions = function (self, brick)
		local secs = {}
		for _,i in ipairs(selection.selected) do
			secs[i] = true
		end
		local sds = {}
		for i,sd in ipairs(brick.sidedefs) do
			if secs[sd.sector] then
				sds[i] = true
			end
		end
		local vs = {}
		for _,ld in ipairs(brick.linedefs) do
			if sds[ld.sidedef[1] ] or sds[ld.sidedef[2] ] then
				vs[ld.vertex[1] ] = true
				vs[ld.vertex[2] ] = true
			end
		end
		local positions = {}
		for i,_ in pairs(vs) do
			positions[i] = brick.vertices[i]
		end
		return positions
	end
},

-- ENTITY MODE
entity = {
	name = "entity",
	is_primary_mode = true,

	page = 1,
	chosen_entity = nil,
	pages = (function()
		local pages = {}
		local pmap = {}
		for _,entity in ipairs(entities) do
			if not pmap[entity.group] then
				local page = {group = entity.group, entities = {}}
				table.insert(pages, page)
				pmap[entity.group] = page.entities
			end
			table.insert(pmap[entity.group], entity)
		end
		table.sort(pages, function (a,b) return a.group < b.group end)
		return pages
	end)(),

	page_items = function (self)
		local items = {}
		local y = 40
		local w = 100
		local x0 = love.graphics.getWidth() - 100 - 10
		for _,entity in ipairs(self.pages[self.page].entities) do
			table.insert(items, {entity=entity,rect=rect{x0, y, w, 18}})
			y = y + 18
		end
		table.sort(items, function (a,b) return a.entity.type < b.entity.type end)
		return items
	end,

	insert = function (self, bag)
		for _,item in ipairs(self:page_items()) do
			if item.rect:contains(bag.mouse) then
				self.chosen_entity = item.entity.type
				return
			end
		end

		if not self.chosen_entity then return end
		local i = insert_entity(bag.brick, self.chosen_entity, snap_to_grid_or_dont(bag, bag.mousetx))
		selection:toggle_one(i)
	end,

	render_overlay = function (self, bag)
		local limit = 250
		local x = love.graphics.getWidth() - limit - 10
		local y = 10
		love.graphics.setColor{255,0,0,255}
		local page = self.pages[self.page]
		love.graphics.printf(page.group .. " " .. self.page .. "/" .. #self.pages, x, y, limit, "right")

		for _,item in ipairs(self:page_items()) do
			local color
			if self.chosen_entity == item.entity.type then
				color = {255,255,0,255}
			else
				color = {100,150,100,150}
			end
			if item.rect:contains(bag.mouse) then color[4] = 255 end
			love.graphics.setColor(color)
			love.graphics.printf(item.entity.type, x, item.rect[2], limit, "right")
		end

		local i = mouse_entity(bag)
		if i then
			local e = bag.brick.entities[i]
			love.graphics.setColor(255,255,255,255)
			love.graphics.print(e.type, bag.mouse[1] + 10, bag.mouse[2])
		end
	end,

	pg = function (self, dpage)
		self.page = self.page + dpage
		if self.page < 1 then
			self.page = #self.pages
		elseif self.page > #self.pages then
			self.page = 1
		end
	end,

	select = function (self, bag)
		local i, e = mouse_entity(bag)
		if i then selection:toggle_one(i) end
	end,

	select_all = function (self, bag)
		local is = {}
		for i = 1,#bag.brick.entities do
			table.insert(is, i)
		end
		selection:toggle_multiple(is)
	end,

	delete = function (self, bag)
		for _,i in ipairs(selection.selected) do
			delete_entity(bag.brick, i)
		end
		remap(bag.brick)
		selection:clear()
	end,

	block_select = function (self, bag, p0, pdim)
		for i,e in ipairs(bag.brick.entities) do
			local vtx = bag.tx:homogeneous_apply(e.position)
			if rect_test(vtx, p0, pdim) then
				selection:toggle_one(i)
			end
		end
	end,

	get_selected_positions = function (self, brick)
		local positions = {}
		for _,i in ipairs(selection.selected) do
			positions[i] = brick.entities[i].position
		end
		return positions
	end,

	post_tx = function (self, bag, tx_info, positions)
		if not tx_info.rotation then return end
		local deg = tx_info.rotation / math.pi * 180
		for i,_ in pairs(positions) do
			local e = bag.brick.entities[i]
			e.yaw = e.yaw - deg
		end
	end
}

}

function render_brick(bag)
	love.graphics.setLineWidth(2)
	for i,ld in ipairs(bag.brick.linedefs) do
		if (bag.mode == modes.linedef or bag.mode.previous_mode == modes.linedef) and selection:has(i) then
			love.graphics.setColor(MAGIC.selection_color)
		elseif ld.tags and #ld.tags > 0 then
			love.graphics.setColor{255,0,255,255}
		else
			love.graphics.setColor{70,70,70,255}
		end
		local vtx0 = bag.tx:homogeneous_apply(bag.brick.vertices[ld.vertex[1]])
		local vtx1 = bag.tx:homogeneous_apply(bag.brick.vertices[ld.vertex[2]])
		love.graphics.line(vtx0[1], vtx0[2], vtx1[1], vtx1[2])

		for _,side in ipairs{-1,1} do
			local lds = linedefside(side)
			if ld.sidedef[lds] then
				local sd = bag.brick.sidedefs[ld.sidedef[lds]]
				if
					((bag.mode == modes.sidedef or bag.mode.previous_mode == modes.sidedef) and selection:has(ld.sidedef[lds]))
					or ((bag.mode == modes.sector or bag.mode.previous_mode == modes.sector) and sd.sector and selection:has(sd.sector))
				then
					love.graphics.setColor(MAGIC.selection_color)
				elseif not sd.sector then
					love.graphics.setColor(90,0,0,255)
				else
					love.graphics.setColor(0,90,0,255)
				end

				local v = (vtx1-vtx0):normalize()
				local n = v:normal()
				local dist = MAGIC.sidedef_dist
				local p0 = vtx0 + v:scale(dist) + n:scale(side*dist)
				local p1 = vtx1 - v:scale(dist) + n:scale(side*dist)
				love.graphics.line(p0[1], p0[2], p1[1], p1[2])
			end
		end
	end

	for i,v in ipairs(bag.brick.vertices) do
		local vtx = bag.tx:homogeneous_apply(v)
		if (bag.mode == modes.vertex or bag.mode.previous_mode == modes.vertex) and selection:has(i) then
			love.graphics.setColor(MAGIC.selection_color)
		else
			if i == bag.highlighted_vertex then
				love.graphics.setColor{200,200,200,255}
			else
				love.graphics.setColor{100,100,100,255}
			end
		end
		love.graphics.circle('fill', vtx[1], vtx[2], 3, 6)
	end

	for i,e in ipairs(bag.brick.entities) do
		local et = entity_type_map[e.type]
		local vtx = bag.tx:homogeneous_apply(e.position)
		if (bag.mode == modes.entity or bag.mode.previous_mode == modes.entity) and selection:has(i) then
			love.graphics.setColor(MAGIC.selection_color)
		else
			love.graphics.setColor{100,120,120,200}
		end
		love.graphics.circle('line', vtx[1], vtx[2], et.radius)
		local deg2rad = function (deg)
			return deg / 180 * math.pi
		end
		love.graphics.line(vtx[1], vtx[2], vtx[1] - math.sin(deg2rad(e.yaw)) * et.radius, vtx[2] + math.cos(deg2rad(e.yaw)) * et.radius)

	end
end

function block_mode(bag)
	if not bag.mode.is_primary_mode or not bag.mode.block_select then
		return bag.mode
	end

	return {
		name = bag.mode.name .. " block select",
		previous_mode = bag.mode,
		pre_render = function (self, bag)
			love.graphics.setLineWidth(1)
			local m = mouse_vec2()
			if self.p0 then
				local d = m - self.p0
				love.graphics.setColor{0,50,0,255}
				love.graphics.rectangle('fill', self.p0[1], self.p0[2], d[1], d[2])
				love.graphics.setColor{255,255,255,255}
				love.graphics.line(self.p0[1], 0, self.p0[1], love.graphics.getHeight())
				love.graphics.line(0, self.p0[2], love.graphics.getWidth(), self.p0[2])
			end
			love.graphics.setColor{255,255,255,255}
			love.graphics.line(m[1], 0, m[1], love.graphics.getHeight())
			love.graphics.line(0, m[2], love.graphics.getWidth(), m[2])
		end,
		can_cancel = function (self)
			return true
		end,
		cancel = function (self, bag)
			bag.mode = self.previous_mode
		end,
		insert = function (self, bag)
			local m = mouse_vec2()
			if not self.p0 then
				self.p0 = m
			else
				local d = m - self.p0
				self.previous_mode:block_select(bag, self.p0, d)
				bag.mode = self.previous_mode
			end
		end
	}
end

function tx_mode(bag, impl)
	if selection:empty() or not bag.mode.is_primary_mode then
		return bag.mode
	end

	local positions = bag.mode:get_selected_positions(bag.brick)

	local center = vec2{0,0}
	local n = 0
	for _,p in pairs(positions) do
		center = center + p
		n = n + 1
	end
	center = center:scale(1/n)

	return {
		name = bag.mode.name .. " " .. impl.name,
		previous_mode = bag.mode,
		center = center,
		p0 = bag.mousetx,
		can_cancel = function (self)
			return true
		end,
		cancel = function (self, bag)
			bag.mode = self.previous_mode
		end,
		_set_p1 = function (self, bag)
			self.p1 = bag.mousetx
		end,
		pre_render = function (self, bag)
			self:_set_p1(bag)
			if impl.render_gui then
				impl:render_gui(self, bag)
			end
			love.graphics.setColor(30,100,230,255)
			local vz = {}
			for _,p in pairs(positions) do
				local ptx = bag.tx:homogeneous_apply(impl:transform(self, bag, p))
				love.graphics.circle('fill', ptx[1], ptx[2], 7, 6)
			end
		end,
		insert = function (self, bag)
			self:_set_p1(bag)
			for _,p0 in pairs(positions) do
				local p1 = impl:transform(self, bag, p0)
				for i=1,2 do p0[i] = p1[i] end
			end
			if self.previous_mode.post_tx and impl.tx_info then
				self.previous_mode:post_tx(bag, impl:tx_info(self, bag), positions)
			end
			bag.mode = self.previous_mode
		end
	}
end

function grab_mode(bag)
	return tx_mode(bag, {
		name = "grab",
		transform = function (self, txmode, bag, v)
			return v + snap_to_grid_or_dont(bag, txmode.p1 - txmode.p0)
		end
	})
end

function scale_mode(bag)
	return tx_mode(bag, {
		name = "scale",
		transform = function (self, txmode, bag, v)
			local d1 = (txmode.center - txmode.p0):length()
			local d = (txmode.center - txmode.p1):length()
			local dscale = d/d1

			if bag.snap_to_grid then
				dscale = raw_snap(dscale, 0.1)
			end

			local op = mat33.identity()
			op = op * mat33.homogeneous_translate(txmode.center)
			op = op * mat33.homogeneous_scale(dscale)
			op = op * mat33.homogeneous_translate(-txmode.center)

			return op:homogeneous_apply(v)
		end,
		render_gui = function (self, txmode, bag)
			local p0 = bag.tx:homogeneous_apply(txmode.center)
			local p1 = bag.tx:homogeneous_apply(txmode.p1)
			love.graphics.setColor(0,255,255,255)
			love.graphics.line(p0[1], p0[2], p1[1], p1[2])
		end
	})
end

function rotate_mode(bag)
	return tx_mode(bag, {
		name = "rotate",
		_phi = function (txmode, bag)
			local d1 = txmode.center - txmode.p0
			local d = txmode.center - txmode.p1
			phi = math.atan2(d[1],d[2]) - math.atan2(d1[1],d1[2])
			if bag.snap_to_grid then
				phi = raw_snap(phi, math.pi / 20)
			end
			return phi
		end,
		transform = function (self, txmode, bag, v)
			local op = mat33.identity()
			op = op * mat33.homogeneous_translate(txmode.center)
			op = op * mat33.homogeneous_rotate(self._phi(txmode, bag))
			op = op * mat33.homogeneous_translate(-txmode.center)

			return op:homogeneous_apply(v)
		end,
		tx_info = function (self, txmode, bag)
			return {rotation = self._phi(txmode, bag)}
		end,
		render_gui = function (self, txmode, bag)
			local p0 = bag.tx:homogeneous_apply(txmode.center)
			local p1 = bag.tx:homogeneous_apply(txmode.p1)
			love.graphics.setColor(0,255,255,255)
			love.graphics.line(p0[1], p0[2], p1[1], p1[2])
		end
	})
end

function love.run()
	local brick_name = arg[2]
	if not brick_name then error("no brick name given") end

	--love.filesystem.setIdentity("bricked")
	--print(love.filesystem.load("settings.lua"))

	local panning = false
	local show_grid = true
	local last_mouse = mouse_vec2()
	local escaping = false

	local bag = {
		brick = load_brick(brick_name),
		mode = modes.vertex,
		shifted = false,
		grid_size = 64,
		nav = {pan = vec2{0,0}, scale = 1},
		snap_to_grid = true
	}

	while true do
		love.event.pump()

		local mouse_delta = mouse_vec2() - last_mouse

		if panning then
			bag.nav.pan = bag.nav.pan + mouse_delta
		end

		bag.mouse = mouse_vec2()

		bag.tx, bag.txi, bag.mousetx = tx_txi_mtx(bag.nav)

		for t,a,b,c,d in love.event.poll() do
			--print(t,a,b,c,d)

			if t == "keypressed" and a == 'escape' then
				if bag.mode.can_cancel and bag.mode:can_cancel() then
					bag.mode:cancel(bag)
				else
					escaping = not escaping
				end
			end

			if escaping then
				if t == "keypressed" and a == "q" then
					save_brick(brick_name, bag.brick)
					return
				end
				if t == "keypressed" and a == "s" then
					save_brick(brick_name, bag.brick)
					escaping = false
				end
				if t == "keypressed" and a == "x" then
					return
				end
				if t == "keypressed" and a == "l" then
					bag.brick = load_brick(brick_name)
					selection:clear()
					escaping = false
				end
			else
				if t == "keypressed" and (a == "lshift" or a == "rshift") then
					bag.shifted = true
				end
				if t == "keyreleased" and (a == "lshift" or a == "rshift") then
					bag.shifted = false
				end

				local mode_keys = {
					["1"] = "vertex",
					["2"] = "linedef",
					["3"] = "sidedef",
					["4"] = "sector",
					["5"] = "entity"
				}
				if t == "keypressed" and mode_keys[a] and bag.mode ~= modes[mode_keys[a]] then
					selection:clear() -- XXX would be very nice to have selection migration, no?
					bag.mode = modes[mode_keys[a]]
				end

				if t == "keypressed" and a == "x" and bag.mode.delete then
					bag.mode:delete(bag)
				end

				if t == "keypressed" then
					if a == "b" then
						bag.mode = block_mode(bag)
					end
					if a == "g" then
						bag.mode = grab_mode(bag)
					end
					if a == "s" then
						bag.mode = scale_mode(bag)
					end
					if a == "r" then
						bag.mode = rotate_mode(bag)
					end
				end

				if t == "mousepressed" and (c == "wd" or c == "wu") then
					local tx0, txi0, mtx0 = tx_txi_mtx(bag.nav)
					if c == "wu" then bag.nav.scale = bag.nav.scale * MAGIC.zoom_speed end
					if c == "wd" then bag.nav.scale = bag.nav.scale / MAGIC.zoom_speed end
					local tx1, txi1, mtx1 = tx_txi_mtx(bag.nav)

					local mtxd = mtx1 - mtx0
					bag.nav.pan = bag.nav.pan + mtxd:scale(bag.nav.scale)

				end
				if t == "mousepressed" and c == "m" then
					panning = true
				end
				if t == "mousereleased" and c == "m" then
					panning = false
				end

				if t == "mousepressed" and c == "l" and bag.mode.insert then
					bag.mode:insert(bag)
				end

				if t == "mousepressed" and c == "r" then
					if bag.mode.can_cancel and bag.mode:can_cancel() then
						bag.mode:cancel(bag)
					elseif bag.mode.select then
						bag.mode:select(bag)
					end
				end

				if t == "keypressed" and a == "return" and bag.mode.create then
					bag.mode:create(bag)
				end

				if t == "keypressed" and a == "[" then
					bag.grid_size = bag.grid_size / 2
				end

				if t == "keypressed" and a == "]" then
					bag.grid_size = bag.grid_size * 2
				end

				if t == "keypressed" and a == "'" then
					bag.snap_to_grid = not bag.snap_to_grid
				end

				if t == "keypressed" and a == "\\" then
					show_grid = not show_grid
				end

				if t == "keypressed" and a == "a" and bag.mode.select_all then
					if selection:empty() then
						bag.mode:select_all(bag)
					else
						selection:clear()
					end
				end

				if t == "keypressed" and a == "z" and not selection:empty() and bag.mode.get_selected_positions then
					local positions = bag.mode:get_selected_positions(bag.brick)
					for _,p0 in pairs(positions) do
						local p1 = snap_to_grid(bag, p0)
						for i=1,2 do p0[i] = p1[i] end
					end
				end

				if t == "keypressed" and bag.mode.pg and (a == "pagedown" or a == "pageup") then
					local m = {pagedown = 1, pageup = -1}
					bag.mode:pg(m[a])
				end
			end
		end

		love.graphics.clear()
		love.graphics.origin()

		love.graphics.setBlendMode('additive')

		if show_grid then render_grid(bag) end

		if bag.mode.pre_render then bag.mode:pre_render(bag) end

		render_brick(bag)

		love.graphics.setBlendMode('alpha')

		local y = 10

		love.graphics.setColor(255,255,0,255)
		love.graphics.print(bag.mode.name .. " mode", 10, y)
		y = y + 20

		if show_grid then
			love.graphics.setColor(255,200,0,255)
		else
			love.graphics.setColor(155,100,0,255)
		end
		love.graphics.print("grid size: " .. bag.grid_size .. (bag.snap_to_grid and " (snap)" or ""), 10, y)
		y = y + 20

		if bag.mode.render_overlay then
			bag.mode:render_overlay(bag)
		end

		local shade_height = 30

		love.graphics.setColor(0,0,0,180)
		love.graphics.rectangle('fill', 0, love.graphics.getHeight() - shade_height, love.graphics.getWidth(), shade_height)

		love.graphics.setColor(255,255,255,255)
		love.graphics.print(
			"\"" ..
			brick_name ..
			"\" (" ..
			#bag.brick.vertices ..
			" vertices, " ..
			#bag.brick.linedefs ..
			" linedefs, " ..
			#bag.brick.sidedefs ..
			" sidedefs, " ..
			#bag.brick.sectors ..
			" sectors)",
			10,
			love.graphics.getHeight() - 20
		)

		if selection.is_ordered then
			love.graphics.setColor(0,255,0,255)
		else
			love.graphics.setColor(255,80,0,255)
		end
		love.graphics.printf(
			selection:as_string(),
			10,
			love.graphics.getHeight() - 20,
			love.graphics.getWidth() - 150,
			'right'
		)

		love.graphics.setColor(255,255,0,255)
		love.graphics.printf(
			"(" .. string.format("%.2f", bag.mousetx[1]) .. "," .. string.format("%.2f", bag.mousetx[2]) .. ")",
			10,
			love.graphics.getHeight() - 20,
			love.graphics.getWidth() - 20,
			'right'
		)

		if escaping then
			love.graphics.setBlendMode('alpha')
			love.graphics.setColor(0,0,0,200)
			love.graphics.rectangle('fill', 0, 0, love.graphics.getWidth(), love.graphics.getHeight())
			love.graphics.setColor(255,255,255,255)
			love.graphics.print("THERE IS NO ESCAPE", 10, 10)
			love.graphics.print("q: save and quit", 15, 30)
			love.graphics.print("x: quit without saving", 15, 50)
			love.graphics.print("s: save", 15, 70)
			love.graphics.print("l: reload", 15, 90)
		end

		love.graphics.present()

		last_mouse = mouse_vec2()
	end
end

