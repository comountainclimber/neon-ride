"""Import Neon Ride's original surface paintings, glyphs and ambient recordings."""
from pathlib import Path
import os
import unreal as ue
from hologram_mesh import build_jellyfish
from instrument_material import build_instruments
from circuit_material import build_road


def build_environment(source):
    assets = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
    tools = ue.AssetToolsHelpers.get_asset_tools()
    lib = ue.MaterialEditingLibrary
    folder = "/Game/Environment"

    def import_file(relative, name):
        path = folder + "/" + name
        if assets.does_asset_exist(path):
            return assets.load_asset(path)
        task = ue.AssetImportTask()
        task.filename = str(source / relative)
        task.destination_path = folder
        task.destination_name = name
        task.automated = True
        task.save = True
        tools.import_asset_tasks([task])
        objects = task.get_objects()
        if not objects:
            raise RuntimeError("Import failed: " + str(source / relative))
        obj = objects[0]
        if obj.get_path_name().split(".")[0] != path:
            assert assets.rename_loaded_asset(obj, path), path
        return obj

    def make(name, builder):
        path = folder + "/" + name
        if assets.does_asset_exist(path):
            return assets.load_asset(path)
        mat = tools.create_asset(name, folder, ue.Material, ue.MaterialFactoryNew())
        def node(cls, x, y, **properties):
            obj = lib.create_material_expression(mat, cls, x, y)
            for key, value in properties.items():
                obj.set_editor_property(key, value)
            return obj
        builder(mat, node)
        lib.recompile_material(mat)
        assert assets.save_loaded_asset(mat, False), path
        return mat

    def wire(a, b, pin, channel=""):
        input_pin = "" if pin in ("Input", "Coordinates") else pin
        assert lib.connect_material_expressions(a, "" if channel == "RGB" else channel, b, input_pin), (a,b,pin)
    def output(a, prop, channel=""):
        assert lib.connect_material_property(a, "" if channel == "RGB" else channel, prop), prop
    mp = ue.MaterialProperty
    def named_input(name):
        pin = ue.CustomInput()
        pin.set_editor_property("input_name",name)
        return pin
    sheet = import_file("signs/glyph-sheet.png","T_DashGlyphs")
    build_road(import_file,make,wire,named_input,output)
    make("M_CircuitDisplay",lambda mat,node: build_instruments(node,wire,output,named_input,mat,sheet))

    def surface(texture, walls):
        def build(mat, node):
            world = node(ue.MaterialExpressionWorldPosition, -1000, 0)
            if walls:
                # City blocks are axis aligned. Both pairs of vertical faces get a continuous U axis.
                x = node(ue.MaterialExpressionComponentMask, -820, -80, r=True, g=False, b=False, a=False)
                y = node(ue.MaterialExpressionComponentMask, -820, 60, r=False, g=True, b=False, a=False)
                z = node(ue.MaterialExpressionComponentMask, -820, 210, r=False, g=False, b=True, a=False)
                for mask in (x,y,z): wire(world, mask, "Input")
                horizontal = node(ue.MaterialExpressionAdd, -630, 0)
                wire(x,horizontal,"A"); wire(y,horizontal,"B")
                height = node(ue.MaterialExpressionMultiply,-630,180,const_b=-1.0)
                wire(z,height,"A")
                coords = node(ue.MaterialExpressionAppendVector,-450,0)
                wire(horizontal,coords,"A"); wire(height,coords,"B")
            else:
                coords = node(ue.MaterialExpressionComponentMask, -500, 0, r=True,g=True,b=False,a=False)
                wire(world,coords,"Input")
            uv = node(ue.MaterialExpressionDivide,-270,0,const_b=1350.0 if walls else 580.0)
            wire(coords,uv,"A")
            image = node(ue.MaterialExpressionTextureSample,-70,0,texture=texture)
            wire(uv,image,"Coordinates")
            output(image,mp.MP_BASE_COLOR,"RGB")
            rough = node(ue.MaterialExpressionConstant,120,220,r=0.27 if walls else 0.18)
            metal = node(ue.MaterialExpressionConstant,120,290,r=0.18 if walls else 0.36)
            output(rough,mp.MP_ROUGHNESS); output(metal,mp.MP_METALLIC)
            if walls:
                light = node(ue.MaterialExpressionMultiply,140,70,const_b=0.78)
                wire(image,light,"A","RGB"); output(light,mp.MP_EMISSIVE_COLOR)
        return build

    deck = import_file("textures/deck.png","T_Deck")
    facade = import_file("textures/facade-tower.png","T_Facade")
    make("M_WetDeck", surface(deck, False))
    make("M_Facade", surface(facade, True))
    def asphalt(mat,node):
        world = node(ue.MaterialExpressionWorldPosition,-780,0)
        xy = node(ue.MaterialExpressionComponentMask,-620,0,r=True,g=True,b=False,a=False)
        wire(world,xy,"Input")
        uv = node(ue.MaterialExpressionDivide,-410,0,const_b=470.)
        wire(xy,uv,"A")
        tex = node(ue.MaterialExpressionTextureSample,-220,0,texture=deck)
        wire(uv,tex,"Coordinates")
        tint = node(ue.MaterialExpressionConstant3Vector,-220,180,constant=ue.LinearColor(.035,.044,.059,1))
        colour = node(ue.MaterialExpressionAdd,0,0)
        wire(tex,colour,"A"); wire(tint,colour,"B"); output(colour,mp.MP_BASE_COLOR)
        rough = node(ue.MaterialExpressionCustom,-120,300,inputs=[named_input("XY")],
                     output_type=ue.CustomMaterialOutputType.CMOT_FLOAT1,
                     code="""float2 p=XY*.0014;
float pools=sin(p.x*1.7+.65*sin(p.y*1.9))+sin(p.y*.77+p.x*.32)+.42*sin(p.x*4.8+p.y*1.2);
return lerp(.235,.53,smoothstep(-.65,.85,pools));""")
        wire(xy,rough,"XY"); output(rough,mp.MP_ROUGHNESS)
        metal = node(ue.MaterialExpressionConstant,-80,480,r=.08)
        output(metal,mp.MP_METALLIC)
    make("M_RainAsphalt",asphalt)

    def glow(mat, node):
        mat.set_editor_property("shading_model",ue.MaterialShadingModel.MSM_UNLIT)
        color = node(ue.MaterialExpressionVectorParameter,-300,0,parameter_name="Color",default_value=ue.LinearColor(0.05,0.8,0.9,1))
        value = node(ue.MaterialExpressionMultiply,0,0,const_b=4.0)
        wire(color,value,"A"); output(value,mp.MP_EMISSIVE_COLOR)
    make("M_Neon",glow)
    def glass(mat,node):
        mat.set_editor_property("blend_mode",ue.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property("two_sided",True)
        colour = node(ue.MaterialExpressionConstant3Vector,-260,0,constant=ue.LinearColor(0.045,0.20,0.22,1))
        alpha = node(ue.MaterialExpressionConstant,-260,100,r=0.095)
        output(colour,mp.MP_BASE_COLOR); output(alpha,mp.MP_OPACITY)
    make("M_Glass",glass)

    def panel(texture, tint, strength):
        def build(mat, node):
            mat.set_editor_property("shading_model",ue.MaterialShadingModel.MSM_UNLIT)
            mat.set_editor_property("blend_mode",ue.BlendMode.BLEND_ADDITIVE)
            mat.set_editor_property("two_sided",True)
            image = node(ue.MaterialExpressionTextureSample,-500,0,texture=texture)
            color = node(ue.MaterialExpressionConstant3Vector,-500,230,constant=ue.LinearColor(*tint,1))
            mix = node(ue.MaterialExpressionMultiply,-200,0)
            wire(image,mix,"A","RGB"); wire(color,mix,"B")
            level = node(ue.MaterialExpressionMultiply,0,0,const_b=strength)
            wire(mix,level,"A"); output(level,mp.MP_EMISSIVE_COLOR)
        return build

    for i in range(1,7):
        image = import_file(f"signs/word-{i}.png",f"T_Glyph{i}")
        tint = [(0.15,0.93,1.0),(1.0,0.23,0.45),(1.0,0.64,0.19)][(i-1)%3]
        make(f"M_Glyph{i}",panel(image,tint,3.2))
    for letter in ("a","b"):
        image = import_file(f"textures/skyline-{letter}.png",f"T_Skyline_{letter}")
        def skyline(mat,node):
            mat.set_editor_property("shading_model",ue.MaterialShadingModel.MSM_UNLIT)
            mat.set_editor_property("blend_mode",ue.BlendMode.BLEND_ADDITIVE)
            mat.set_editor_property("two_sided",True)
            uv = node(ue.MaterialExpressionTextureCoordinate,-680,0)
            paint = node(ue.MaterialExpressionTextureSample,-450,0,texture=image)
            feather = node(ue.MaterialExpressionCustom,-420,200,
                           inputs=[named_input("UV")],
                           output_type=ue.CustomMaterialOutputType.CMOT_FLOAT1,
                           code="return smoothstep(0.,.16,min(UV.x,1.-UV.x))*smoothstep(0.,.19,min(UV.y,1.-UV.y))*.21;")
            wire(uv,feather,"UV")
            faded = node(ue.MaterialExpressionMultiply,-80,0)
            wire(paint,faded,"A"); wire(feather,faded,"B")
            output(faded,mp.MP_EMISSIVE_COLOR)
        make(f"M_Horizon_{letter}",skyline)

    def hologram(mat,node):
        mat.set_editor_property("shading_model",ue.MaterialShadingModel.MSM_UNLIT)
        colour = node(ue.MaterialExpressionVertexColor,-450,0)
        light = node(ue.MaterialExpressionMultiply,-160,0,const_b=3.7)
        wire(colour,light,"A"); output(light,mp.MP_EMISSIVE_COLOR)
        pos = node(ue.MaterialExpressionPreSkinnedPosition,-500,200)
        clock = node(ue.MaterialExpressionTime,-500,300)
        motion = node(ue.MaterialExpressionCustom,-130,225,
                      inputs=[named_input("P"),named_input("Clock")],
                      output_type=ue.CustomMaterialOutputType.CMOT_FLOAT3, code="""
float depth=saturate(-P.z/7900.);
float pulse=.5+.5*sin(Clock*.92-depth*2.);
float rim=saturate(1.-P.z/2100.);
float2 sway=float2(sin(Clock*.51+depth*5.),cos(Clock*.41+depth*7.))*330.*depth;
return float3(-P.xy*rim*.13*pulse+sway, (1.-rim*2.)*125.*pulse+75.*sin(Clock*.43));""")
        wire(pos,motion,"P"); wire(clock,motion,"Clock"); output(motion,mp.MP_WORLD_POSITION_OFFSET)
    make("M_Hologram",hologram)
    mesh_path = folder+"/SM_Hologram"
    if not assets.does_asset_exist(mesh_path):
        intermediate = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_intermediate_dir()))
        intermediate.mkdir(parents=True,exist_ok=True)
        file = intermediate/"SM_Hologram.glb"
        build_jellyfish(file)
        task = ue.AssetImportTask()
        task.filename,task.destination_path = str(file),folder
        task.automated,task.save = True,True
        tools.import_asset_tasks([task])
        mesh, = [obj for obj in task.get_objects() if isinstance(obj,ue.StaticMesh)]
        if mesh.get_path_name().split(".")[0] != mesh_path:
            assert assets.rename_loaded_asset(mesh,mesh_path)
        mesh.set_material(0,assets.load_asset(folder+"/M_Hologram"))
        assert assets.save_loaded_asset(mesh,False)
    for file, name in (("rain","A_Rain"),("wind_woosh_loop","A_Wind"),("engineCircular_000","A_Engine")):
        sound = import_file(f"audio/wav/{file}.wav",name)
        sound.set_editor_property("looping",True)
        assets.save_loaded_asset(sound,False)
    assets.save_directory(folder,only_if_is_dirty=True,recursive=True)
    ue.log("NEON_ENVIRONMENT_COMPLETE")


if __name__ == "__main__":
    project = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir()))
    local = project / "SourceArt"
    default = local if (local / "textures").is_dir() else project.parent.parent
    build_environment(Path(os.getenv("NEON_RIDE_ART",str(default))))
