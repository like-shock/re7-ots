local layers,casts,clears,created={},0,0,0
local bad_method,cast_error,contact_override=nil,false,nil
local function object(methods)
 local o={}
 o.get_type_definition=function() return {get_method=function(_,n) return n~=bad_method and methods[n] and {} end,
     get_parent_type=function() return nil end} end
 o.call=function(self,n,...) assert(n~=bad_method and methods[n],'bad method '..n);return methods[n](self,...) end
 o.add_ref=function(self) return self end
 return o
end
local root
root=object({get_Parent=function() return nil end})
root.get_address=function() return 1 end
local player=object({get_Transform=function() return root end})
local child=object({get_Parent=function() return root end});child.get_address=function() return 2 end
local wall=object({get_Parent=function() return nil end});wall.get_address=function() return 3 end
local function hit(x,y,z,tr)
 local go=tr and object({get_Transform=function() return tr end})
 return {point={x=x,y=y,z=z},collider=object({get_GameObject=function() return go end})}
end
local manager={get_field=function(_,n) assert(n=='PlayerObj');return player end}
local sys={};local query,result,filter
local sdk_physics={}
sdk_physics.get_managed_singleton=function(n) assert(n=='app.ObjectManager');return manager end
sdk_physics.get_native_singleton=function(n) assert(n=='via.physics.System');return sys end
sdk_physics.find_type_definition=function(n)
 if n=='via.physics.ContactPoint' then return {} end
 assert(n=='via.physics.System')
 return {get_method=function(_,name)
  assert(name=='castRay(via.physics.CastRayQuery, via.physics.CastRayResult)')
  if bad_method==name then return nil end
  return {call=function(_,system,q,r)
   assert(system==sys and q==query and r==result)
   assert(q.all and q.sorted and q.ray and filter.group==0 and filter.mask==2)
   assert(not r.hits,'previous layer not cleared');casts=casts+1
   if cast_error then error('physics failed') end
   r.hits=layers[filter.layer] or {}
  end}
 end}
end
sdk_physics.get_native_field=function(p,ty,n) assert(n=='Position');return p end
sdk_physics.create_instance=function(n)
 created=created+1
 if n=='via.physics.CastRayQuery' then
  filter=object({set_Group=function(self,v) self.group=v end,set_MaskBits=function(self,v) self.mask=v end,
   set_Layer=function(self,v) self.layer=v end})
  query=object({['setRay(via.vec3, via.vec3)']=function(self,a,b) self.ray={a,b} end,
   clearOptions=function(self) self.all=nil;self.sorted=nil end,
   enableAllHits=function(self) self.all=true end,enableNearSort=function(self) self.sorted=true end,
   get_FilterInfo=function() return filter end,set_FilterInfo=function(_,f) assert(f==filter) end})
  return query
 end
 assert(n=='via.physics.CastRayResult')
 result=object({clear=function(self) clears=clears+1;self.hits=nil end,
  get_NumContactPoints=function(self) return contact_override or #self.hits end,
  ['getContactPoint(System.UInt32)']=function(self,i) return self.hits[i+1].point end,
  ['getContactCollidable(System.UInt32)']=function(self,i) return self.hits[i+1].collider end})
 return result
end
Vector3f={new=function(x,y,z) return {x=x,y=y,z=z} end}
local physics=Physics.new(sdk_physics)
