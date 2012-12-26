-- example BOOT32.LUA file

print("- start OF BOOT32.LUA file")

function hidden()
	local i

	print("- inside hidden() function in BOOT32.LUA file")
	
	for i=1,3 do
		print("  blabla "..i)
	end

end

function BOOT32()
	print("- inside BOOT32() function in BOOT32.LUA file")
	hidden()
end

print("- end of BOOT32.LUA file")
