-- SDK boundary fake: real Lua backend runs unchanged against this object tree.
local model_reads,model_writes,model_component_reads=0,{},0
local model_hook=nil
local next_id=1000
local function managed(kind,methods)
 next_id=next_id+1
 local o={id=next_id,methods=methods or {},missing={}}
 o.get_address=function(self) return self.id end
 o.get_type_definition=function(self) return {
  is_a=function(_,n) return n==kind end,
  get_method=function(_,n) return not self.missing[n] and self.methods[n] and {} or nil end,
  get_parent_type=function() return nil end} end
 o.call=function(self,n,...)
  assert(not self.missing[n] and self.methods[n], 'unsupported method: '..n)
  return self.methods[n](self,...)
 end
 return o
end
local materials={'pl0050_face','pl0050_eye','pl0050_hair','pl0050_eye_tear','pl0050_eye_shadow'}
local function node(path)
 local n={children={},components={},visible=true,position={1,3,3},material_enabled={},material_writes={}}
 n.go=managed('via.GameObject',{
  get_Transform=function() return n.tr end,
  get_Components=function() model_component_reads=model_component_reads+1;return {
   get_size=function() return #n.components end,
   get_element=function(_,i) assert(i>=0 and i<#n.components);return n.components[i+1] end,
   get_elements=function() error('SystemArray vector must not be assumed to be a Lua table') end} end,
  get_DrawSelf=function() return n.visible end,
  get_Draw=function() return n.visible end,
  set_DrawSelf=function(_,v)
   if model_hook then model_hook(n,v) end
   model_writes[#model_writes+1]={node=n,value=v}
   if not n.ignore then n.visible=v end
   if n.throw then error('model setter exception') end
  end})
 n.tr=managed('via.Transform',{
  get_GameObject=function() return n.go end,
  get_Parent=function() return n.parent and n.parent.tr end,
  get_Child=function() return n.children[1] and n.children[1].tr end,
  get_Next=function()
   if n.next_override then return n.next_override.tr end
   if n.parent then for i,c in ipairs(n.parent.children) do if c==n then
    return n.parent.children[i+1] and n.parent.children[i+1].tr end end end
  end,
  getJointByName=function(_,name) assert(name=='Head');return n.joint end})
 n.joint=managed('via.Joint',{
  get_Owner=function() return n.joint_owner or n.tr end,
  get_Position=function() return {x=n.position[1],y=n.position[2],z=n.position[3]} end})
 if path then
  n.path=path;n.materials={table.unpack(materials)}
  n.resource=managed('via.Resource', {['ToString()']=function() return 'via.render.MeshResource[@'..n.path..']' end})
  n.mesh=managed('via.render.Mesh',{
   getMesh=function() return n.resource end,
   get_MaterialNum=function() return #n.materials end,
   getMaterialName=function(_,i) return n.materials[i+1] end,
   getMaterialsEnable=function(_,i)
    assert(i>=0 and i<#n.materials)
    return n.material_enabled[n.materials[i+1]]~=false
   end,
   setMaterialsEnable=function(_,i,v)
    assert(i>=0 and i<#n.materials and type(v)=='boolean')
    local name=n.materials[i+1]
    if n.material_hook then n.material_hook(name,v) end
    n.material_writes[#n.material_writes+1]={name=name,value=v}
    if not n.ignore_material then n.material_enabled[name]=v end
    if n.throw_material then error('material setter exception') end
   end})
  n.components={n.mesh}
 end
 return n
end
local function attach(parent,child)
 child.parent=parent;parent.children[#parent.children+1]=child
end
local head_path='character/player/pl0000/pl0002/pl0002.mesh'
local player=node('character/player/pl0000/pl0001/pl0001.mesh')
player.materials={'pl0000_body','pl0000_L_Leg','pl0000_VR'}
local rig=node();attach(player,rig)
local head=node(head_path);attach(rig,head)
local npc=node(head_path) -- exact decoy outside the local player's hierarchy
local manager=managed('app.ObjectManager')
manager.get_field=function(_,name) assert(name=='PlayerObj');return player and player.go end
sdk=sdk or {}
sdk.get_managed_singleton=function(name)
 model_reads=model_reads+1
 assert(name=='app.ObjectManager')
 return manager
end
