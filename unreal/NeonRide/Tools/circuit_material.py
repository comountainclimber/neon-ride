"""Shade wet asphalt, reflective lane paint and local puddles in continuous circuit UVs."""
from pathlib import Path
import unreal as ue


def build_road(import_file, make, wire, named_input, output):
    source = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())) / "SourceArt" / "Circuit"
    assets = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
    import_file(source / "impact.wav", "A_Impact")
    for stem, asset_name in (("idle", "Idle"), ("drive", "Drive"), ("redline", "Redline"), ("nitro", "Nitro")):
        layer = import_file(source / f"engine-{stem}.wav", "A_Engine" + asset_name)
        layer.set_editor_property("looping", True)
        layer.set_editor_property("virtualization_mode", ue.VirtualizationMode.PLAY_WHEN_SILENT)
        assets.save_loaded_asset(layer, False)
    cover = import_file(source / "vesper-cover.jpg", "T_VesperCover")
    cover.set_editor_property("lod_group", ue.TextureGroup.TEXTUREGROUP_UI)
    cover.set_editor_property("never_stream", True)
    cover.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_EDITOR_ICON)
    assets.save_loaded_asset(cover, True)
    maps = [import_file(source / (key + ".png"), name) for key, name in
            (("asphalt", "T_CircuitAsphalt"), ("asphalt-normal", "T_CircuitNormal"), ("asphalt-rough", "T_CircuitRough"))]
    maps[1].set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
    maps[1].set_editor_property("srgb", False)
    maps[2].set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_MASKS)
    maps[2].set_editor_property("srgb", False)
    for texture in maps[1:]:
        assets.save_loaded_asset(texture, True)

    def asphalt(mat, node):
        uv = node(ue.MaterialExpressionTextureCoordinate, -900, 0)
        tiling = node(ue.MaterialExpressionMultiply, -720, 0, const_b=.43)
        wire(uv, tiling, "A")
        gravel = node(ue.MaterialExpressionTextureSample, -480, -150, texture=maps[0])
        normal = node(ue.MaterialExpressionTextureSample, -480, 340, texture=maps[1],
                      sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        moisture = node(ue.MaterialExpressionTextureSample, -480, 540, texture=maps[2],
                        sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_MASKS)
        for sample in (gravel, normal, moisture):
            wire(tiling, sample, "Coordinates")
        stripe = node(ue.MaterialExpressionCustom, -370, 30, inputs=[named_input("P")],
            output_type=ue.CustomMaterialOutputType.CMOT_FLOAT1, code="""
float w=max(.008,min(.09,fwidth(P.x)));
float edge=1.-smoothstep(.055-w,.055+w,abs(abs(P.x)-5.99));
float center=(1.-smoothstep(.07-w,.07+w,abs(P.x)))*step(frac((P.y+1.)/12.),.27);
return max(edge,center);""")
        wire(uv, stripe, "P")
        albedo = node(ue.MaterialExpressionCustom, -110, -150, inputs=[named_input("G"),named_input("Line")],
            output_type=ue.CustomMaterialOutputType.CMOT_FLOAT3, code="return lerp(G*.78+.018,float3(.70,.75,.72)*(.76+.24*G),Line);")
        wire(gravel, albedo, "G")
        wire(stripe, albedo, "Line")
        output(albedo, ue.MaterialProperty.MP_BASE_COLOR)
        paint = node(ue.MaterialExpressionMultiply, -100, 20, const_b=.22)
        wire(stripe, paint, "A")
        output(paint, ue.MaterialProperty.MP_EMISSIVE_COLOR)
        rough = node(ue.MaterialExpressionCustom, -80, 240, inputs=[named_input("Moisture"),named_input("P")],
            output_type=ue.CustomMaterialOutputType.CMOT_FLOAT1, code="""
float puddles=sin(P.x*.73+P.y*.023)+.48*sin(P.y*.26-P.x*.13);
return clamp(.17+Moisture*.44+.09*puddles,.11,.56);""")
        wire(moisture, rough, "Moisture", "R")
        wire(uv, rough, "P")
        output(rough, ue.MaterialProperty.MP_ROUGHNESS)
        depth = node(ue.MaterialExpressionCustom, -70, 410, inputs=[named_input("N")],
            output_type=ue.CustomMaterialOutputType.CMOT_FLOAT3, code="return normalize(float3(N.xy*.52,N.z));")
        wire(normal, depth, "N")
        output(depth, ue.MaterialProperty.MP_NORMAL)
        metal = node(ue.MaterialExpressionConstant, 0, 530, r=.16)
        output(metal, ue.MaterialProperty.MP_METALLIC)
    make("M_CircuitAsphalt", asphalt)

    def midrise(mat, node):
        mat.set_editor_property("used_with_instanced_static_meshes", True)
        world = node(ue.MaterialExpressionWorldPosition, -650, 0)
        normal = node(ue.MaterialExpressionPixelNormalWS, -650, 180)
        glass = node(ue.MaterialExpressionConstant3Vector, 0, -260, constant=ue.LinearColor(.033, .048, .066, 1.))
        output(glass, ue.MaterialProperty.MP_BASE_COLOR)
        rough = node(ue.MaterialExpressionConstant, 0, -140, r=.31)
        output(rough, ue.MaterialProperty.MP_ROUGHNESS)
        metal = node(ue.MaterialExpressionConstant, 0, -60, r=.56)
        output(metal, ue.MaterialProperty.MP_METALLIC)
        glow = node(ue.MaterialExpressionCustom, -200, 80, inputs=[named_input("P"),named_input("N")],
                    output_type=ue.CustomMaterialOutputType.CMOT_FLOAT3, code="""
float2 along=float2(-N.y,N.x);
float2 uv=float2(dot(P.xy,along)/180.,P.z/315.);
float2 grid=floor(uv), local=frac(uv);
float2 aa=min(.15,.5*fwidth(uv)+.006);
float2 frame=smoothstep(float2(.12,.19)-aa,float2(.12,.19)+aa,min(local,1.-local));
float fade=1.-smoothstep(.12,.50,max(fwidth(uv.x),fwidth(uv.y)));
float office=frac(sin(dot(float2(floor(grid.x/3.),grid.y),float2(73.1,117.3)))*15473.51);
float noise=frac(sin(dot(grid,float2(11.7,47.5)))*19517.37);
float value=lerp(.02,.29+.56*noise,step(.64,office+.12*noise));
float3 tint=lerp(float3(.25,.50,.59),float3(.95,.62,.33),step(.8,office));
return tint*lerp(.082,frame.x*frame.y*value,fade)*saturate(1.-abs(N.z));""")
        wire(world, glow, "P")
        wire(normal, glow, "N")
        output(glow, ue.MaterialProperty.MP_EMISSIVE_COLOR)
    make("M_CircuitInfill", midrise)

    def canal(mat, node):
        position = node(ue.MaterialExpressionWorldPosition, -620, 60)
        time = node(ue.MaterialExpressionTime, -620, 200)
        wave = node(ue.MaterialExpressionCustom, -300, 120, inputs=[named_input("P"), named_input("T")],
                    output_type=ue.CustomMaterialOutputType.CMOT_FLOAT3, code="""
float2 p=P.xy*.01;
float swell=sin(dot(p,float2(.38,.64))+T*1.1);
float chop=sin(dot(p,float2(.43,3.23))+T*2.+swell);
return normalize(float3(.19*(.68*swell+.32*chop),.11*cos(.77*p.x+1.3*T)+.058*sin(5.8*p.y+2.5*T),1.));""")
        wire(position, wave, "P")
        wire(time, wave, "T")
        output(wave, ue.MaterialProperty.MP_NORMAL)
        tint = node(ue.MaterialExpressionConstant3Vector, -240, -120, constant=ue.LinearColor(.012,.065,.077,1.))
        output(tint, ue.MaterialProperty.MP_BASE_COLOR)
        output(node(ue.MaterialExpressionConstant, -230, -40, r=.14), ue.MaterialProperty.MP_ROUGHNESS)
        output(node(ue.MaterialExpressionConstant, -230, 30, r=.32), ue.MaterialProperty.MP_METALLIC)
    make("M_CourierCanal", canal)


    def mirror(mat, node):
        mat.set_editor_property("shading_model", ue.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property("two_sided", True)
        rear = node(ue.MaterialExpressionTextureSampleParameter2D, -600, 0,
                    parameter_name="RearView", texture=maps[0], sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_COLOR)
        tint = node(ue.MaterialExpressionCustom, -230, 0, inputs=[named_input("Rear")],
                    output_type=ue.CustomMaterialOutputType.CMOT_FLOAT3,
                    code="return max(Rear,0.)*float3(.77,.87,.93)+float3(.016,.026,.030);")
        wire(rear, tint, "Rear")
        output(tint, ue.MaterialProperty.MP_EMISSIVE_COLOR)
    make("M_RearMirror", mirror)

