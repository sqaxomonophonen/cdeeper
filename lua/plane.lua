return {
	floor = function (z)
		return {0,0,1,-z}
	end,
	ceiling = function (z)
		return {0,0,-1,z}
	end
}
