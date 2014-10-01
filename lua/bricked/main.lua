-- editor built with love 0.9.1

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
local fn = require('fn')

function render_grid(bag)
	local tx = bag.tx
	local txi = bag.txi
	local sz = bag.grid_size

	local c0 = txi:apply(vec2{0,0})
	local c1 = txi:apply(vec2{love.graphics.getWidth(),love.graphics.getHeight()})

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
		local v0 = tx:apply(vec2{x,y0})
		local v1 = tx:apply(vec2{x,y1})
		love.graphics.line(v0[1], v0[2], v1[1], v1[2])
	end
	for y = y0,y1,sz do
		zcol(y)
		local v0 = tx:apply(vec2{x0,y})
		local v1 = tx:apply(vec2{x1,y})
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
	tx = tx * mat33.translate{love.graphics.getWidth()/2, love.graphics.getHeight()/2}
	tx = tx * mat33.translate(nav.pan)
	tx = tx * mat33.scale(nav.scale)

	local txi = tx:inverse()
	local mousetx = txi:apply(mouse_vec2())

	return tx, txi, mousetx
end

function new_brick()
	return {
		vertices = {},
		linedefs = {},
		sidedefs = {},
		sectors = {}
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
		local vtx = bag.tx:apply(v)
		r = (vtx-bag.mouse):length()
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
		local v0 = bag.tx:apply(bag.brick.vertices[ld.vertex0])
		local v1 = bag.tx:apply(bag.brick.vertices[ld.vertex1])
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
			local sd = bag.brick.sidedefs[ld[linedefside(side)]]
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
			local v0 = bag.brick.vertices[ld.vertex0]
			local v1 = bag.brick.vertices[ld.vertex1]
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

function insert_linedef(brick, vertex0, vertex1)
	for i,ld in ipairs(brick.linedefs) do
		if (ld.vertex0 == vertex0 and ld.vertex1 == vertex1) or (ld.vertex0 == vertex1 and ld.vertex1 == vertex0) then
			return i
		end
	end
	local linedef = { vertex0 = vertex0, vertex1 = vertex1 }
	table.insert(brick.linedefs, linedef)
	return #brick.linedefs
end

function linedefside(side)
	return side > 0 and "sidedef_right" or "sidedef_left"
end

function insert_sidedef(brick, linedef, side)
	local ld = brick.linedefs[linedef]
	local lds = linedefside(side)
	local sd = ld[lds]
	if sd then return sd end
	local sidedef = {}
	table.insert(brick.sidedefs, sidedef)
	local i = #brick.sidedefs
	ld[lds] = i
	return i
end

function insert_sector(brick)
	local sector = {
		floor = {plane={0,0,1,64}},
		ceiling = {plane={0,0,-1,64}}
	}
	table.insert(brick.sectors, sector)
	return #brick.sectors
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
	if ld.sidedef_left then delete_sidedef(brick, ld.sidedef_left) end
	if ld.sidedef_right then delete_sidedef(brick, ld.sidedef_right) end
	brick.linedefs[to_delete] = false
end

function delete_vertex(brick, to_delete)
	brick.vertices[to_delete] = false
	for i = 1,#brick.linedefs do
		local ld = brick.linedefs[i]
		if ld and (not brick.vertices[ld.vertex0] or not brick.vertices[ld.vertex1]) then
			delete_linedef(brick, i)
		end
	end
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
			ld.vertex0 = vremap[ld.vertex0]
			ld.vertex1 = vremap[ld.vertex1]
			table.insert(nld, ld)
			ld.sidedef_left = sdremap[ld.sidedef_left]
			ld.sidedef_right = sdremap[ld.sidedef_right]
		end
	end
	brick.linedefs = nld
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
			local vtx = bag.tx:apply(v)
			if rect_test(vtx, p0, pdim) then
				selection:toggle_one(i)
			end
		end
	end,

	get_selected_vertices = function (self, brick)
		return selection.selected
	end,
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
			local v0 = bag.tx:apply(bag.brick.vertices[self.last_vertex])
			local v1 = bag.tx:apply(bag.highlighted_vertex and bag.brick.vertices[bag.highlighted_vertex] or snap_to_grid_or_dont(bag, bag.mousetx))
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
			local vtx = bag.tx:apply(v)
			if rect_test(vtx, p0, pdim) then
				vertices[i] = true
			end
		end
		for i,ld in ipairs(bag.brick.linedefs) do
			if not toggled[i] then
				if vertices[ld.vertex0] and vertices[ld.vertex1] then
					selection:toggle_one(i)
					toggled[i] = true
				end
			end
		end
	end,

	get_selected_vertices = function (self, brick)
		local vs = {}
		for _,i in ipairs(selection.selected) do
			local ld = brick.linedefs[i]
			vs[ld.vertex0] = true
			vs[ld.vertex1] = true
		end
		local vsi = {}
		for k,_ in pairs(vs) do
			table.insert(vsi, k)
		end
		return vsi
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
			local vtx0 = bag.tx:apply(bag.brick.vertices[ld.vertex0])
			local vtx1 = bag.tx:apply(bag.brick.vertices[ld.vertex1])
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
			if ld[lds] then
				selection:toggle_one(ld[lds])
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
			local vtx = bag.tx:apply(v)
			if rect_test(vtx, p0, pdim) then
				vertices[i] = true
			end
		end
		local toggled = {}
		for i,ld in ipairs(bag.brick.linedefs) do
			if vertices[ld.vertex0] and vertices[ld.vertex1] then
				for _,side in ipairs{-1,1} do
					local sd = ld[linedefside(side)]
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

	get_selected_vertices = function (self, brick)
		local sds = {}
		for _,i in ipairs(selection.selected) do
			sds[i] = true
		end
		local vs = {}
		for _,ld in ipairs(brick.linedefs) do
			if sds[ld.sidedef_left] or sds[ld.sidedef_right] then
				vs[ld.vertex0] = true
				vs[ld.vertex1] = true
			end
		end
		local vsi = {}
		for k,_ in pairs(vs) do
			table.insert(vsi, k)
		end
		return vsi
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
			local vtx = bag.tx:apply(v)
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
				local sdi = ld[linedefside(side)]
				if sdi then
					local sd = bag.brick.sidedefs[sdi]
					if sd.sector then
						if not vertices[ld.vertex0] or not vertices[ld.vertex1] then
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

	get_selected_vertices = function (self, brick)
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
			if sds[ld.sidedef_left] or sds[ld.sidedef_right] then
				vs[ld.vertex0] = true
				vs[ld.vertex1] = true
			end
		end
		local vsi = {}
		for k,_ in pairs(vs) do
			table.insert(vsi, k)
		end
		return vsi
	end
}

}

function render_brick(bag)
	love.graphics.setLineWidth(2)
	for i,ld in ipairs(bag.brick.linedefs) do
		if (bag.mode == modes.linedef or bag.mode.previous_mode == modes.linedef) and selection:has(i) then
			love.graphics.setColor(MAGIC.selection_color)
		else
			love.graphics.setColor{70,70,70,255}
		end
		local vtx0 = bag.tx:apply(bag.brick.vertices[ld.vertex0])
		local vtx1 = bag.tx:apply(bag.brick.vertices[ld.vertex1])
		love.graphics.line(vtx0[1], vtx0[2], vtx1[1], vtx1[2])

		for _,side in ipairs{-1,1} do
			local lds = linedefside(side)
			if ld[lds] then
				local sd = bag.brick.sidedefs[ld[lds]]
				if
					((bag.mode == modes.sidedef or bag.mode.previous_mode == modes.sidedef) and selection:has(ld[lds]))
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
		local vtx = bag.tx:apply(v)
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

	local vis = bag.mode:get_selected_vertices(bag.brick)

	local center = vec2{0,0}
	local n = 0
	for _,i in ipairs(vis) do
		center = center + bag.brick.vertices[i]
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
			for _,i in ipairs(vis) do
				local v = bag.brick.vertices[i]
				local vtx = bag.tx:apply(impl:transform(self, bag, v))
				vz[i] = vtx
				love.graphics.circle('fill', vtx[1], vtx[2], 4, 6)
			end
			love.graphics.setLineWidth(1)
			love.graphics.setColor(15,30,100,255)
			for _,ld in ipairs(bag.brick.linedefs) do
				local p0 = vz[ld.vertex0]
				local p1 = vz[ld.vertex1]
				if p0 and p1 then
					love.graphics.line(p0[1], p0[2], p1[1], p1[2])
				end
			end
		end,
		insert = function (self, bag)
			self:_set_p1(bag)
			for _,i in ipairs(vis) do
				bag.brick.vertices[i] = impl:transform(self, bag, bag.brick.vertices[i])
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
			op = op * mat33.translate(txmode.center)
			op = op * mat33.scale(dscale)
			op = op * mat33.translate(-txmode.center)

			return op:apply(v)
		end,
		render_gui = function (self, txmode, bag)
			local p0 = bag.tx:apply(txmode.center)
			local p1 = bag.tx:apply(txmode.p1)
			love.graphics.setColor(0,255,255,255)
			love.graphics.line(p0[1], p0[2], p1[1], p1[2])
		end
	})
end

function rotate_mode(bag)
	return tx_mode(bag, {
		name = "rotate",
		transform = function (self, txmode, bag, v)
			local d1 = txmode.center - txmode.p0
			local d = txmode.center - txmode.p1
			phi = math.atan2(d[1],d[2]) - math.atan2(d1[1],d1[2])

			if bag.snap_to_grid then
				phi = raw_snap(phi, math.pi / 20)
			end

			local op = mat33.identity()
			op = op * mat33.translate(txmode.center)
			op = op * mat33.rotate(phi)
			op = op * mat33.translate(-txmode.center)

			return op:apply(v)
		end,
		render_gui = function (self, txmode, bag)
			local p0 = bag.tx:apply(txmode.center)
			local p1 = bag.tx:apply(txmode.p1)
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
					["4"] = "sector"
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

				if t == "keypressed" and a == "z" and not selection:empty() and bag.mode.get_selected_vertices then
					local svs = bag.mode:get_selected_vertices(bag.brick)
					for _,i in ipairs(svs) do
						bag.brick.vertices[i] = snap_to_grid(bag, bag.brick.vertices[i])
					end
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

		love.graphics.setColor(255,255,0,255)
		love.graphics.print(bag.mode.name .. " mode", 10, 10)

		if show_grid then
			love.graphics.setColor(255,200,0,255)
		else
			love.graphics.setColor(155,100,0,255)
		end
		love.graphics.print("grid size: " .. bag.grid_size .. (bag.snap_to_grid and " (snap)" or ""), 10, 30)

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

