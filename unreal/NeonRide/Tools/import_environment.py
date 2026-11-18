"""Import Neon Ride's original surface paintings, glyphs and ambient recordings."""
from pathlib import Path
import os
import unreal as ue


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
        assert lib.connect_material_expressions(a, channel, b, pin)
    def output(a, prop, channel=""):
        assert lib.connect_material_property(a, channel, prop)
    mp = ue.MaterialProperty

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
        make(f"M_Skyline_{letter}",panel(image,(0.68,0.75,0.95),0.65))
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
