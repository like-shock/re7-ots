import unittest
import test_head_integration as integration
import test_ethan_model as backend
import test_rt_models as models


class HeadMaterialBackendTests(unittest.TestCase):
    check = backend.EthanBackendTests.check

    def test_material_is_selected_by_name_after_reordering_and_guarded_before_write(self):
        self.check('''
local name='pl0050_eye';local s=b:read_material(name);assert(s and s.visible)
head.materials[1],head.materials[2]=head.materials[2],head.materials[1]
assert(b:write_material(s.id,name,true,false))
assert(head.material_enabled[name]==false and head.material_enabled.pl0050_face==nil)
assert(not b:write_material(s.id,name,true,false),'compare before write')
assert(not b:write_material(s.id,'pl0000_body',true,false),'body is not a supported head material')
attach(rig,node(head_path))
assert(not b:write_material(s.id,name,false,true),'new duplicate blocks mutation')
assert(#player.material_writes==0 and #npc.material_writes==0)
''')

    def test_replacement_and_epoch_block_stale_material_writes(self):
        for change in ["b:set_epoch('native:2')", "rig.children={};attach(npc,head)",
                       "rig.children={};head=node(head_path);attach(rig,head)"]:
            with self.subTest(change=change):
                self.check("local s=b:read_material('pl0050_eye');"+change+'''
assert(not b:write_material(s.id,'pl0050_eye',true,false))
assert(#head.material_writes==0)
''')


class HeadMaterialIntegrationTests(unittest.TestCase):
    adapter = integration.HeadIntegrationTests.adapter
    head = integration.HeadIntegrationTests.head

    def test_first_person_disables_every_head_material_even_if_draw_flag_is_late(self):
        self.head('''
start();assert(head.visible)
step(function(s) s.aim.raw=1 end)
assert(not head.visible and near(position[3],3))
-- A DrawSelf update may precede the effective Draw update. Every material
-- must still be disabled; equating DrawSelf with rendered visibility misses this.
for _,name in ipairs(head.materials) do
 assert(head.material_enabled[name]==false,'FP must disable each head material: '..name)
end
for i=1,6 do step(function(s) s.aim.raw=1 end) end
local observed=false
for _,r in ipairs(records) do
 local part=r.model and r.model.parts.ethan
 if part and part.draw and part.draw.draw_self==false and part.draw.effective_draw==true then observed=true end
end
assert(observed,'log distinguishes DrawSelf from effective Draw')
assert(#player.material_writes==0 and #npc.material_writes==0)
for i=1,16 do step() end
for _,name in ipairs(head.materials) do assert(head.material_enabled[name]==true) end
''', '''
head.go.methods.get_Draw=function() return true end
local restore_camera=tr.set_position
tr.set_position=function(self,p,dirty)
 if p.z==3 then for _,name in ipairs(head.materials) do
  assert(head.material_enabled[name]==false,'all materials hidden before FP restore')
 end end
 return restore_camera(self,p,dirty)
end
''')

    def test_original_disabled_material_survives_third_person_and_explicit_disable(self):
        self.head('''
start();assert(head.visible and head.material_enabled.pl0050_eye_shadow==false)
key=true;step();key=false
for _,name in ipairs(head.materials) do assert(head.material_enabled[name]==false) end
ui_head=false;ui();step()
assert(head.visible and head.material_enabled.pl0050_eye_shadow==false)
for _,name in ipairs(head.materials) do
 if name~='pl0050_eye_shadow' then assert(head.material_enabled[name]==true) end
end
''', "head.material_enabled.pl0050_eye_shadow=false;tr.set_position=camera_set;model_hook=nil")

    def test_partial_material_write_failure_hides_other_materials_and_restores_camera(self):
        self.head('''
start();head.throw_material=true
key=true;step();key=false
assert(near(position[3],3),'material fault cannot block camera cleanup')
for _,name in ipairs(head.materials) do assert(head.material_enabled[name]==false) end
assert(ui():find('MATERIAL_',1,true),'fault must identify material control')
''')

    def test_reset_and_stale_native_keep_owned_materials_hidden(self):
        self.head('''
start();fail_json=true;step()
for _,name in ipairs(head.materials) do assert(head.material_enabled[name]==false) end
phase='reset';callbacks.reset()
for _,name in ipairs(head.materials) do assert(head.material_enabled[name]==false) end
assert(near(position[3],3))
''')

    def test_missing_material_api_does_not_claim_hidden_or_stop_camera_fallback(self):
        self.head('''
start();key=true;step();key=false
assert(near(position[3],3))
assert(ui():find('MATERIAL_',1,true),'missing material API must be visible')
''', "head.mesh.missing.setMaterialsEnable=true;tr.set_position=camera_set;model_hook=nil")

    def test_ignored_setter_and_external_material_change_are_not_reported_hidden(self):
        self.head('''
start();head.ignore_material=true;key=true;step();key=false
assert(near(position[3],3) and ui():find('MATERIAL_',1,true))
assert(head.material_enabled.pl0050_eye==true,'ignored write is still observable')
''')
        self.head('''
start();head.material_enabled.pl0050_eye=false
key=true;step();key=false
assert(near(position[3],3) and ui():find('OWNERSHIP_LOST',1,true))
ui_head=false;ui();step()
assert(head.material_enabled.pl0050_eye==false,'release cannot overwrite external material value')
''', "tr.set_position=camera_set;model_hook=nil")

    def test_failed_partial_show_is_cleaned_up_for_every_material(self):
        self.head('''
start();key=true;step();key=false;step()
head.material_hook=function(name,v)
 if name=='pl0050_eye' and v then head.material_enabled[name]=true;error('partial show') end
end
key=true;step();key=false;for i=1,16 do step() end
for _,name in ipairs(head.materials) do assert(head.material_enabled[name]==false) end
assert(not head.visible and ui():find('MATERIAL_',1,true))
''')

    def test_invalid_native_cannot_acquire_a_previously_unreadable_material(self):
        self.head('''
start();material_readable=true;fail_json=true;step()
assert(head.material_enabled.pl0050_eye==nil,'invalid native cannot acquire another channel')
assert(near(position[3],3))
''', '''
local material_readable=false
local read_material=head.mesh.methods.getMaterialsEnable
head.mesh.methods.getMaterialsEnable=function(self,i)
 if head.materials[i+1]=='pl0050_eye' and not material_readable then return nil end
 return read_material(self,i)
end
''')


class FullPackMaterialTests(unittest.TestCase):
    check = models.RTModelTests.check
    adapter = integration.HeadIntegrationTests.adapter

    def test_full_pack_material_diagnostics_fit_the_actual_journal(self):
        encode=(backend.ROOT/'tests/json_encoder.lua').read_text()
        self.adapter('''
start();for i=1,20 do step() end
step(function(s) s.aim.raw=1 end);for i=1,8 do step() end
assert(ui():find('Automatic play log: RECORDING',1,true),'material evidence must fit bounded journal records')
assert(max_line<=4096 and detail_seen and phase_seen,'retain full material and phase evidence within the journal limit')
''', backend.HARNESS+models.MIA+'''
config.ethan_head=true;config.full_model_pack=true
local p={12.345678329467773,23.456789016723633,34.567890167236328}
position={table.unpack(p)};head.position={table.unpack(p)};hair.position={table.unpack(p)}
local old_snapshot=snapshot
snapshot=function(n) local s=old_snapshot(n);s.player.xyz={table.unpack(p)};return s end
tr.call=function(_,name)
 assert(name=='getJointByName');return {call=function() return {x=p[1],y=p[2],z=p[3]} end}
end
sdk.typeof=function(name) return name end
local target={get_type_definition=function() return {is_a=function() return true end} end,
 get_position=function() return {x=p[1],y=p[2],z=p[3]} end}
player.go.methods['getComponent(System.Type)']=function(_,name)
 return {call=function() return player.go end,get_field=function(_,field)
  if field=='HandIK' then return {get_size=function() return 2 end,
   get_element=function() return {get_field=function() return target end} end} end
  return false
 end}
end
local max_line,detail_seen,phase_seen=0,false,false
local encode=(function()
'''+encode+'''
end)()
json.dump_string=function(r)
 local line=encode(r);max_line=math.max(max_line,#line+1)
 if r.kind=='scene_detail' and r.model and r.model.parts.mia2100 then
  detail_seen=r.model.parts.mia2100.materials.em2050_Skin_Mia~=nil
  local p=r.phase_probe
  if p and p.player and p.player.hands and p.player.hands.status=='OK'
   and p.phases.render and p.phases.render.joint.status=='OK' then phase_seen=true end
 end
 return line
end
''')

    def test_mia_materials_keep_independent_original_values(self):
        self.check(models.MIA+'''
hair.material_enabled.em2070_Hair3=false
g:configure(true,'s:1',true);assert(g:before_restore())
for _,n in ipairs({head,hair}) do
 for _,name in ipairs(n.materials) do assert(n.material_enabled[name]==false) end
end
assert(g:after_apply({1,3,5}));assert(hair.material_enabled.em2070_Hair3==false)
g:configure(false,'s:1',true);assert(g:before_restore())
assert(head.material_enabled.em2050_Skin_Mia==true and hair.material_enabled.em2070_Hair3==false)
''')
