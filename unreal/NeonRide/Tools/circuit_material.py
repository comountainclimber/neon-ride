"""Shade wet asphalt, reflective lane paint and local puddles in continuous circuit UVs."""
from pathlib import Path
import unreal as ue


def build_road(import_file, make, wire, named_input, output):
    source = Path(ue.Paths.convert_relative_path_to_full(ue.Paths.project_dir())) / "SourceArt" / "Circuit"
    assets = ue.get_editor_subsystem(ue.EditorAssetSubsystem)
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
