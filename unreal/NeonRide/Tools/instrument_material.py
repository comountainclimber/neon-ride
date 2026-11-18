"""Build the physical circuit instrument cluster and its rechargeable nitro gauge."""
import unreal as ue


def build_instruments(node, wire, output, named_input, mat, sheet):
    mat.set_editor_property("shading_model", ue.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    uv = node(ue.MaterialExpressionTextureCoordinate, -600, 0)
    data = node(ue.MaterialExpressionVectorParameter, -600, 100, parameter_name="Telemetry")
    nitro = node(ue.MaterialExpressionVectorParameter, -600, 200, parameter_name="Nitro")
    glyphs = node(ue.MaterialExpressionTextureObject, -600, 300, texture=sheet)
    display = node(ue.MaterialExpressionCustom, -100, 50,
        inputs=[named_input(k) for k in ("UV", "Data", "NitroState", "Glyphs")],
        output_type=ue.CustomMaterialOutputType.CMOT_FLOAT3, code="""
float2 p=UV;
float3 teal=float3(.17,.88,.95), amber=float3(1.,.49,.08);
float3 c=float3(.003,.009,.014);
float2 edge=min(p,1.-p);
c+=teal*.48*(1.-smoothstep(.002,.01,min(edge.x,edge.y*.34)));
float2 digits=(p-float2(.041,.28))/float2(.119,.50);
if(all(digits>=0) && digits.x<3 && digits.y<1) {
    int kmh=(int)round(Data.r*300.);
    int col=int(digits.x);
    int digit=(col==0 ? kmh/100 : (col==1 ? kmh/10 : kmh))%10;
    int masks[10]={63,6,91,79,102,109,125,7,127,111};
    int mask=masks[digit];
    float2 q=frac(digits);
    float2 a[7]={float2(.19,.06),float2(.74,.13),float2(.74,.56),float2(.19,.92),float2(.13,.56),float2(.13,.13),float2(.19,.49)};
    float2 b[7]={float2(.67,.06),float2(.74,.42),float2(.74,.85),float2(.67,.92),float2(.13,.85),float2(.13,.42),float2(.67,.49)};
    float ink=0;
    for(int i=0;i<7;i++) if((mask>>i)&1) {
        float2 ab=b[i]-a[i];
        float2 delta=(q-a[i])-ab*saturate(dot(q-a[i],ab)/dot(ab,ab));
        ink=max(ink,1.-smoothstep(.041,.072,length(delta*float2(.95,1.))));
    }
    c+=ink*float3(.86,.98,1.)*1.5;
}
float2 rev=(p-float2(.044,.12))/float2(.91,.075);
if(all(rev>=0)&&all(rev<1)&&frac(rev.x*32.)<.78)
    c+=lerp(teal,amber,step(.83,rev.x))*(rev.x<Data.g?1.4:.12);
float2 tank=(p-float2(.44,.58))/float2(.51,.22);
if(all(tank>=0)&&all(tank<1)&&frac(tank.x*17.)<.82)
    c+=lerp(teal,amber,NitroState.g)*(tank.x<NitroState.r?1.85:.14);
float2 label=(p-float2(.448,.29))/float2(.016,.037);
if(all(label>=0)&&label.x<29&&label.y<7) {
    int glyph=int(label.x)/6, col=int(label.x)%6, row=int(label.y);
    int letters[35]={17,25,25,21,19,19,17,31,4,4,4,4,4,31,31,4,4,4,4,4,4,30,17,17,30,20,18,17,14,17,17,17,17,17,14};
    if(col<5 && ((letters[glyph*7+row]>>(4-col))&1)) c+=lerp(teal,amber,NitroState.g)*1.55;
}
return c;""")
    wire(uv, display, "UV")
    wire(data, display, "Data")
    wire(nitro, display, "NitroState")
    wire(glyphs, display, "Glyphs")
    output(display, ue.MaterialProperty.MP_EMISSIVE_COLOR)
