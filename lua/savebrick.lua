return function(lson, file)
	print("\n\n" .. lson)

	local answer
	while true do
		print("do you want to save this back into " .. file .. "? [yes/no]")
		answer = io.read()
		if answer == "yes" or answer == "no" then break end
	end

	if answer == "yes" then
		print("okay, writing " .. file)
		local f = io.open(file, "w")
		f:write(lson)
		f:close()
	elseif answer == "no" then
		print("okay, leaving " .. file .. " as it was")
	end
end
