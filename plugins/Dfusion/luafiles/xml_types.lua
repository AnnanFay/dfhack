function type_read(valtype,address)
	if valtype.issimple then
		--print("Simple read:"..tostring(valtype.ctype))
		return engine.peek(address,valtype.ctype)
	else
		return valtype:makewrap(address)
	end
end
function type_write(valtype,address,val)
	if valtype.issimple then
		engine.poke(address,valtype.ctype,val)
	else
		engine.poke(address,DWORD,rawget(val,"ptr"))
	end
end
function first_of_type(node,labelname)
	for k,v in ipairs(node) do
		if type(v)=="table" and v.label==labelname then
			return v
		end
	end
end
function padAddress(curoff,sizetoadd) --return new offset to place things
	--windows -> sizeof(x)==alignof(x)
	if sizetoadd>8 then sizetoadd=8 end
	if(math.mod(curoff,sizetoadd)==0) then 
		return curoff
	else
		return curoff+(sizetoadd-math.mod(curoff,sizetoadd))
	end
end
xtypes={} -- list of all types prototypes (e.g. enum-type -> announcement_type)
-- type must have new and makewrap (that makes a wrapper around ptr)
local sarr={} 
sarr.__index=sarr
function sarr.new(node)
	local o={}
	setmetatable(o,sarr)
	--print("Making array.")
	o.count=tonumber(node.xarg.count)
	--print("got count:"..o.count)
	o.ctype=makeType(first_of_type(node,"ld:item"))
	
	o.size=o.count*o.ctype.size
	--print("got subtypesize:"..o.ctype.size)
	return o
end
function sarr:makewrap(address)
	local o={}
	o.mtype=self
	o.ptr=address
	setmetatable(o,self.wrap)
	return o
end
sarr.wrap={}
function sarr.wrap:__index(key)
	if key=="size" then
		return rawget(self,"mtype").count
	end
	local num=tonumber(key)
	local mtype=rawget(self,"mtype")
	if num~=nil and num<mtype.count then
		return type_read(mtype.ctype,num*mtype.ctype.size+rawget(self,"ptr"))
	else
		error("invalid key to static-array")
	end
end
function sarr.wrap:__newindex(key,val)
	local num=tonumber(key)
	if num~=nil and num<rawget(self,"mtype").count then
		return type_write(mtype.ctype,num*mtype.ctype.size+rawget(self,"ptr"),val)
	else
		error("invalid key to static-array")
	end
end
--]=]
xtypes["static-array"]=sarr


simpletypes={}

simpletypes["s-float"]={FLOAT,4}
simpletypes.int64_t={QWORD,8}
simpletypes.uint32_t={DWORD,4}
simpletypes.uint16_t={WORD,2}
simpletypes.uint8_t={BYTE,1}
simpletypes.int32_t={DWORD,4}
simpletypes.int16_t={WORD,2}
simpletypes.int8_t={BYTE,1}
simpletypes.bool={BYTE,1}
simpletypes["stl-string"]={STD_STRING,24}
function getSimpleType(typename)
	if simpletypes[typename] == nil then return end
	local o={}
	o.ctype=simpletypes[typename][1]
	o.size=simpletypes[typename][2]
	o.issimple=true
	return o
end
local type_enum={}
type_enum.__index=type_enum
function type_enum.new(node)
	local o={}
	setmetatable(o,type_enum)
	for k,v in pairs(node) do
		if type(v)=="table" and v.xarg~=nil then
			--print("\t"..k.." "..v.xarg.name)
			o[k-1]=v.xarg.name
		end
	end
	local btype=node.xarg["base-type"] or "uint32_t"
	--print(btype.." t="..convertType(btype))
	o.type=getSimpleType(btype) -- should be simple type
	o.size=o.type.size
	return o
end
xtypes["enum-type"]=type_enum

local type_bitfield={} --bitfield can be accessed by number (bf[1]=true) or by name bf.DO_MEGA=true
type_bitfield.__index=type_bitfield
function type_bitfield.new(node)
	local o={}
	setmetatable(o,type_bitfield)
	o.size=0
	o.fields={}
	for k,v in pairs(node) do
		if type(v)=="table" and v.xarg~=nil then
			
			local name=v.xarg.name
			if name==nil then
				name="anon_"..tostring(k)
			end
			--print("\t"..k.." "..name)
			o.fields[k]=name
			o.fields[name]=k
			o.size=o.size+1
		end
	end
	o.size=o.size/8 -- size in bytes, not bits.
	return o
end
function type_bitfield:bitread(addr,nbit)
	local byte=engine.peekb(addr+nbit/8)
	if bit.band(byte,bit.lshift(1,nbit%8))~=0 then
		return true
	else
		return false
	end
end
function type_bitfield:bitwrite(addr,nbit,value)
	local byte=engine.peekb(addr+nbit/8)
	if self:bitread(addr,nbit)~= value then
		local byte=bit.bxor(byte,bit.lshift(1,nbit%8))
		engine.pokeb(addr+nbit/8,byte)
	end
end
type_bitfield.wrap={}

function type_bitfield:makewrap(address)
	local o={}
	o.mtype=self
	o.ptr=address
	setmetatable(o,self.wrap)
	return o
end
function type_bitfield.wrap:__index(key)
	local myptr=rawget(self,"ptr")
	local mytype=rawget(self,"mtype")
	local num=tonumber(key)
	if num~=nil then
		if mytype.fields[num]~=nil then
			return mytype:bitread(myptr,num-1)
		else
			error("No bit with index:"..tostring(num))
		end
	elseif mytype.fields[key]~= nil then
		return mytype:bitread(myptr,mytype.fields[key]-1)
	else
		error("No such field exists")
	end
end
function type_bitfield.wrap:__newindex(key,value)
	local myptr=rawget(self,"ptr")
	local mytype=rawget(self,"mtype")
	local num=tonumber(key)
	if num~=nil then
		if mytype.fields[num]~=nil then
			return mytype:bitwrite(myptr,num-1,value)
		else
			error("No bit with index:"..tostring(num))
		end
	elseif mytype.fields[key]~= nil then
		return mytype:bitwrite(myptr,mytype.fields[key]-1,value)
	else
		error("No such field exists")
	end
end
xtypes["bitfield-type"]=type_bitfield


local type_class={}
type_class.__index=type_class
function type_class.new(node)
	local o={}
	setmetatable(o,type_class)
	o.types={}
	o.base={}
	o.size=0
	if node.xarg["inherits-from"]~=nil then
		table.insert(o.base,getGlobal(node.xarg["inherits-from"]))
		--error("Base class:"..node.xarg["inherits-from"])
	end
	for k,v in ipairs(o.base) do
		for kk,vv in pairs(v.types) do
			o.types[kk]={vv[1],vv[2]+o.size}
		end
		o.size=o.size+v.size
	end
	
	for k,v in pairs(node) do
		if type(v)=="table" and v.label=="ld:field" and v.xarg~=nil then
			local t_name=""
			local name=v.xarg.name or v.xarg["anon-name"] or ("unk_"..k)
				
			--print("\t"..k.." "..name.."->"..v.xarg.meta.." ttype:"..v.label)
			local ttype=makeType(v)
			
			--for k,v in pairs(ttype) do
			--	print(k..tostring(v))
			--end
			local off=padAddress(o.size,ttype.size)
			o.size=off
			o.types[name]={ttype,o.size}
			o.size=o.size+ttype.size
		end
	end
	return o
end

type_class.wrap={}
function type_class.wrap:__index(key)
	local myptr=rawget(self,"ptr")
	local mytype=rawget(self,"mtype")
	if mytype.types[key] ~= nil then
		return type_read(mytype.types[key][1],myptr+mytype.types[key][2])
	else
		error("No such field exists")
	end
end
function type_class.wrap:__newindex(key,value)
	local myptr=rawget(self,"ptr")
	local mytype=rawget(self,"mtype")
	if mytype.types[key] ~= nil then
		return type_write(mytype.types[key][1],myptr+mytype.types[key][2],value)
	else
		error("No such field exists")
	end
end
function type_class:makewrap(ptr)
	local o={}
	o.ptr=ptr
	o.mtype=self
	setmetatable(o,self.wrap)
	return o
end
xtypes["struct-type"]=type_class
xtypes["class-type"]=type_class
local type_pointer={}
type_pointer.__index=type_pointer
function type_pointer.new(node)
	local o={}
	setmetatable(o,type_pointer)
	o.ptype=makeType(first_of_type(node,"ld:item"))
	o.size=4
	return o
end
type_pointer.wrap={}
function type_pointer.wrap:tonumber()
	local myptr=rawget(self,"ptr")
	return type_read(myptr,DWORD)
end
function type_pointer.wrap:fromnumber(num)
	local myptr=rawget(self,"ptr")
	return type_write(myptr,DWORD,num)
end
function type_pointer.wrap:deref()
	local myptr=rawget(self,"ptr")
	local mytype=rawget(self,"ptype")
	return type_read(myptr,mytype)
end
function type_pointer:makewrap(ptr)
	local o={}
	o.ptr=ptr
	o.mtype=self
	setmetatable(o,self.wrap)
	return o
end
xtypes["pointer"]=type_class
--------------------------------------------
--stl-vector (beginptr,endptr,allocptr)
--df-flagarray (ptr,size)
xtypes.containers={}
local dfarr={} 
dfarr.__index=dfarr
function dfarr.new(node)
	local o={}
	setmetatable(o,dfarr)
	o.size=8
	return o
end
function dfarr:makewrap(address)
	local o={}
	o.mtype=self
	o.ptr=address
	setmetatable(o,self.wrap)
	return o
end
dfarr.wrap={}
function dfarr.wrap:__index(key)
	local num=tonumber(key)
	local mtype=rawget(self,"mtype")
	local size=type_read(rawget(self,"ptr")+4,DWORD)
	error("TODO make __index for dfarray")
	if num~=nil and num<sizethen then
		return type_read(mtype.ctype,num*mtype.ctype.size+rawget(self,"ptr"))
	else
		error("invalid key to df-flagarray")
	end
end
function dfarr.wrap:__newindex(key,val)
	local num=tonumber(key)
	error("TODO make __index for dfarray")
	if num~=nil and num<rawget(self,"mtype").count then
		return type_write(mtype.ctype,num*mtype.ctype.size+rawget(self,"ptr"),val)
	else
		error("invalid key to static-array")
	end
end

xtypes.containers["df-array"]=dfarr
local farr={} 
farr.__index=farr
function farr.new(node)
	local o={}
	setmetatable(o,farr)
	o.size=8
	return o
end
function farr:makewrap(address)
	local o={}
	o.mtype=self
	o.ptr=address
	setmetatable(o,self.wrap)
	return o
end
farr.wrap={}
function farr.wrap:__index(key)
	
	local num=tonumber(key)
	local mtype=rawget(self,"mtype")
	local size=type_read(rawget(self,"ptr")+4,DWORD)
	if key=="size" then
		return size/mtype.ctype.size;
	end
	error("TODO make __index for df-flagarray")
	if num~=nil and num<sizethen then
		return type_read(mtype.ctype,num*mtype.ctype.size+rawget(self,"ptr"))
	else
		error("invalid key to df-flagarray")
	end
end
function farr.wrap:__newindex(key,val)
	local num=tonumber(key)
	error("TODO make __index for df-flagarray")
	if num~=nil and num<rawget(self,"mtype").count then
		return type_write(mtype.ctype,num*mtype.ctype.size+rawget(self,"ptr"),val)
	else
		error("invalid key to static-array")
	end
end

xtypes.containers["df-flagarray"]=farr

local stl_vec={} 
stl_vec.__index=stl_vec
function stl_vec.new(node)
	local o={}
	
	o.size=16
	local titem=first_of_type(node,"ld:item")
	if titem~=nil then
		o.item_type=makeType(titem)
	else
		o.item_type=getSimpleType("uint32_t")
	end
	setmetatable(o,stl_vec)
	return o
end
function stl_vec:makewrap(address)
	local o={}
	o.mtype=self
	o.ptr=address
	setmetatable(o,self.wrap)
	return o
end
stl_vec.wrap={}
function stl_vec.wrap:__index(key)
	local num=tonumber(key)
	local mtype=rawget(self,"mtype")
	local ptr=rawget(self,"ptr")
	local p_begin=engine.peek(ptr,DWORD)
	local p_end=engine.peek(ptr+4,DWORD)
	if key=="size" then
		return (p_end-p_begin)/mtype.item_type.size;
	end
	--allocend=type_read(ptr+8,DWORD)
	error("TODO make __index for stl_vec")
	if num~=nil and num<sizethen then
		return type_read(mtype.ctype,num*mtype.ctype.size+rawget(self,"ptr"))
	else
		error("invalid key to df-flagarray")
	end
end
function stl_vec.wrap:__newindex(key,val)
	local num=tonumber(key)
	error("TODO make __index for stl_vec")
	if num~=nil and num<rawget(self,"mtype").count then
		return type_write(mtype.ctype,num*mtype.ctype.size+rawget(self,"ptr"),val)
	else
		error("invalid key to static-array")
	end
end
xtypes.containers["stl-vector"]=stl_vec

local stl_vec_bit={} 
stl_vec_bit.__index=stl_vec_bit
function stl_vec_bit.new(node)
	local o={}
	setmetatable(o,stl_vec_bit)
	o.size=20
	return o
end
function stl_vec_bit:makewrap(address)
	local o={}
	o.mtype=self
	o.ptr=address
	setmetatable(o,self.wrap)
	return o
end
stl_vec_bit.wrap={}
function stl_vec_bit.wrap:__index(key)
	local num=tonumber(key)
	local mtype=rawget(self,"item_type")
	local ptr=rawget(self,"ptr")
	local p_begin=type_read(ptr,DWORD)
	local p_end=type_read(ptr+4,DWORD)
	--allocend=type_read(ptr+8,DWORD)
	error("TODO make __index for stl_vec_bit")
	if num~=nil and num<sizethen then
		return type_read(mtype.ctype,num*mtype.ctype.size+rawget(self,"ptr"))
	else
		error("invalid key to df-flagarray")
	end
end
function stl_vec_bit.wrap:__newindex(key,val)
	local num=tonumber(key)
	error("TODO make __index for stl_vec_bit")
	if num~=nil and num<rawget(self,"mtype").count then
		return type_write(mtype.ctype,num*mtype.ctype.size+rawget(self,"ptr"),val)
	else
		error("invalid key to static-array")
	end
end
xtypes.containers["stl-bit-vector"]=stl_vec_bit
--------------------------------------------
local bytes_pad={} 
bytes_pad.__index=bytes_pad
function bytes_pad.new(node)
	local o={}
	setmetatable(o,bytes_pad)
	o.size=tonumber(node.xarg.size)
	return o
end
xtypes["bytes"]=bytes_pad
--------------------------------------------
function getGlobal(name)
	if types[name]== nil then
			findAndParse(name)
			if types[name]== nil then
				error("type:"..name.." failed find-and-parse")
			end
			--error("type:"..node.xarg["type-name"].." should already be ready")
		end
		return types[name]
end
parser={}
parser["ld:global-type"]=function  (node)
	return xtypes[node.xarg.meta].new(node)
end
parser["ld:global-object"]=function  (node)
	
end

parser["ld:field"]=function (node)
	local meta=node.xarg.meta
	if meta=="number" or (meta=="primitive" and node.xarg.subtype=="stl-string") then
		return getSimpleType(node.xarg.subtype)
	elseif meta=="static-array" then
		return xtypes["static-array"].new(node)
	elseif meta=="global" then
		return getGlobal(node.xarg["type-name"])
	elseif meta=="compound" then
		if node.xarg.subtype==nil then
			return xtypes["struct-type"].new(node)
		else
			return xtypes[node.xarg.subtype.."-type"].new(node)
		end
	elseif meta=="container" then
		local subtype=node.xarg.subtype
		
		if xtypes.containers[subtype]==nil then
			error(subtype.." not implemented... (container)")
		else
			return xtypes.containers[subtype].new(node)
		end
	elseif meta=="pointer" then
		return xtypes["pointer"].new(node)
	elseif meta=="bytes" then
		return xtypes["bytes"].new(node)
	else
		error("Unknown meta:"..meta)
	end
end
parser["ld:item"]=parser["ld:field"]
function makeType(node,overwrite)
	local label=overwrite or node.label
	if parser[label] ~=nil then
		--print("Make Type with:"..label)
		local ret=parser[label](node)
		if ret==nil then
			error("Error parsing:"..label.." nil returned!")
		else
			return ret
		end
	else
		for k,v in pairs(node) do
			print(k.."->"..tostring(v))
		end
		error("Node parser not found: "..label)
	end
	--[=[
	if getSimpleType(node)~=nil then
		return getSimpleType(node)
	end
	print("Trying to make:"..node.xarg.meta)
	if xtypes[node.xarg.meta]~=nil then
		return xtypes[node.xarg.meta].new(node)
	end
	
	if node.xarg.meta=="global" then
		--print(node.xarg["type-name"])
		
		if types[node.xarg["type-name"]]== nil then
			error("type:"..node.xarg["type-name"].." should already be ready")
		end
		return types[node.xarg["type-name"]]
	end
	]=]
	--[=[for k,v in pairs(node) do
		print(k.."=>"..tostring(v))
		if type(v)=="table" then
			for kk,vv in pairs(v) do 
				print("\t"..kk.."=>"..tostring(vv))
			end
		end
	end]=]
	
end