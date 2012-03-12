--[=[
	bld=buildinglist[1]
	bld.x1=10
	bld.flags.exists=true
	boolval=bld.flags.justice
	if boolval then
		bld.mat_type=bld.mat_type+1
	end
	
	type info:
	
--]=]
dofile("dfusion/xml_types.lua")

--[=[sometype={	
}
sometype.x1={INT32_T,0}
sometype.y1={INT32_T,4}
--...
sometype.flags={"building_flags",7*4}
--...

types.building=sometype
]=]

function parseTree(t)
	for k,v in ipairs(t) do
		if v.xarg~=nil and v.xarg["type-name"]~=nil and v.label=="ld:global-type" then
			local name=v.xarg["type-name"];
			if(types[name]==nil) then
				--print("Parsing:"..name)
				--for kk,vv in pairs(v.xarg) do
				--	print("\t"..kk.." "..tostring(vv))
				--end
				types[name]=makeType(v)
				--print("found "..name.." or type:"..v.xarg.meta or v.xarg.base)
			end
		end
	end
end
function parseTreeGlobals(t)
	local glob={}
	--print("Parsing global-objects")
	for k,v in ipairs(t) do
		if v.xarg~=nil and v.label=="ld:global-object" then
			local name=v.xarg["name"];
			--print("Parsing:"..name)
			local subitem=v[1]
			if subitem==nil then 
				error("Global-object subitem is nil:"..name)
			end
			local ret=makeType(subitem)
			if ret==nil then
				error("Make global returned nil!")
			end
			glob[name]=ret
		end
	end
	--print("Printing globals:")
	--for k,v in pairs(glob) do
	--	print(k)
	--end
	return glob
end
function findAndParse(tname)
	for k,v in ipairs(main_tree) do
		local name=v.xarg["type-name"];
		if v.xarg~=nil and v.xarg["type-name"]~=nil and v.label=="ld:global-type" then
			
			if(name ==tname) then
			--print("Parsing:"..name)
			--for kk,vv in pairs(v.xarg) do
			--	print("\t"..kk.." "..tostring(vv))
			--end
			types[name]=makeType(v)
			end
			--print("found "..name.." or type:"..v.xarg.meta or v.xarg.base)
		end
	end
end
df={}
df.types=rawget(df,"types") or {} --temporary measure for debug
local df_meta={}
function df_meta:__index(key)
	local addr=VersionInfo.getAddress(key)
	local vartype=rawget(df,"types")[key];
	if addr==0 then
		error("No such global address exist")
	elseif vartype==nil then
		error("No such global type exist")
	else
		return type_read(vartype,addr)
	end
end
function df_meta:__newindex(key,val)
	local addr=VersionInfo.getAddress(key)
	local vartype=rawget(df,"types")[key];
	if addr==0 then
		error("No such global address exist")
	elseif vartype==nil then
		error("No such global type exist")
	else
		return type_write(vartype,addr,val)
	end
end
setmetatable(df,df_meta)
--------------------------------
types=types or {}
dofile("dfusion/patterns/xml_angavrilov.lua")
-- [=[
main_tree=parseXmlFile("dfusion/patterns/supplementary.xml")[1]
parseTree(main_tree)
main_tree=parseXmlFile("dfusion/patterns/codegen.out.xml")[1]
parseTree(main_tree)
rawset(df,"types",parseTreeGlobals(main_tree))
--]=]
--[=[labels={}
for k,v in ipairs(t) do
	labels[v.label]=labels[v.label] or {meta={}}
	if v.label=="ld:global-type" and v.xarg~=nil and v.xarg.meta ~=nil then
		labels[v.label].meta[v.xarg.meta]=1
	end
end
for k,v in pairs(labels) do
	print(k)
	if v.meta~=nil then
		for kk,vv in pairs(v.meta) do
			print("=="..kk)
		end
	end
end--]=]
function addressOf(var)
	local addr=rawget(var,"ptr")
	return addr
end
function printGlobals()
	print("Globals:")
	for k,v in pairs(rawget(df,"types")) do
		print(k)
	end
end
function printFields(object)
	local tbl
	if getmetatable(object)==xtypes["struct-type"].wrap then
		tbl=rawget(object,"mtype")
	elseif getmetatable(object)==xtypes["struct-type"] then
		tbl=object
	else
		error("Not an class_type or a class_object")
	end
	for k,v in pairs(tbl.types) do
		print(k)
	end
end