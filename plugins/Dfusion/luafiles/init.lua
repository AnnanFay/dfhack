function print(msg)
	Console.print(msg.."\n")
end
function err(msg) --make local maybe...
	print(msg)
	print(debug.traceback())
end
function dofile(filename) --safer dofile, with traceback (very usefull)
	f,perr=loadfile(filename)
	if f~=nil then
		return xpcall(f,err)
	else
		print(perr)
	end
end
function dofile_silent(filename) --safer dofile, with traceback, no file not found error
	f,perr=loadfile(filename)
	if f~=nil then
		return xpcall(f,err)
	end
end
function loadall(t1) --loads all non interactive plugin parts, so that later they could be used
	for k,v in pairs(t1) do
		dofile_silent("dfusion/"..v[1].."/init.lua")
	end
end
function mainmenu(t1)
	--Console.clear()
	while true do
		print("No.	Name           Desc")
		for k,v in pairs(t1) do
			print(string.format("%3d %15s %s",k,v[1],v[2]))
		end
		local q=Console.lineedit("Select plugin to run (q to quit):")
		if q=='q' then return end
		q=tonumber(q)
		if q~=nil then
			if q>=1 and q<=#t1 then
				dofile("dfusion/"..t1[q][1].."/plugin.lua")
				
			end
		end
	end
end
dofile("dfusion/common.lua")
dofile("dfusion/utils.lua")
unlockDF()
plugins={}
table.insert(plugins,{"simple_embark","A simple embark dwarf count editor"})
table.insert(plugins,{"items","A collection of item hacking tools"})
table.insert(plugins,{"offsets","Find all offsets"})
table.insert(plugins,{"friendship","Multi race fort enabler"})
table.insert(plugins,{"friendship_civ","Multi civ fort enabler"})
table.insert(plugins,{"embark","Multi race embark"})
table.insert(plugins,{"adv_tools","some tools for (mainly) advneturer hacking"})
table.insert(plugins,{"tools","some misc tools"})
table.insert(plugins,{"triggers","a function calling plug (discontinued...)"})
table.insert(plugins,{"migrants","multi race imigrations"})
table.insert(plugins,{"onfunction","run lua on some df function"})
loadall(plugins)
dofile_silent("dfusion/initcustom.lua")

print("Locating saves...")
local str=engine.peekstr(0x1447A40+offsets.base())
print("Current region:"..str)
str="data/save/"..str.."/dfusion/init.lua"
dofile_silent(str)

if not INIT then
mainmenu(plugins)
end

