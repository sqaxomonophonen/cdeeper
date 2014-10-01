local fn = {}

fn.map = function (fn, tbl)
	local result = {}
	for _,v in ipairs(tbl) do
		table.insert(result, fn(v))
	end
	return result
end

return fn
