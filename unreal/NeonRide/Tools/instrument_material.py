"""Draw the film instrument cluster in UV space, driven by the possessed motorcycle."""
import unreal as ue


def build_instruments(node, wire, output, named_input, mat, sheet):
    mat.set_editor_property("shading_model",ue.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided",True)
    uv = node(ue.MaterialExpressionTextureCoordinate,-620,0)
    telemetry = node(ue.MaterialExpressionVectorParameter,-620,120,parameter_name="Telemetry")
    traffic = node(ue.MaterialExpressionVectorParameter,-620,240,parameter_name="Traffic")
    glyphs = node(ue.MaterialExpressionTextureObject,-620,350,texture=sheet)
    display = node(ue.MaterialExpressionCustom,-100,50,
        inputs=[named_input(k) for k in ("UV","Data","Blips","Glyphs")],
        output_type=ue.CustomMaterialOutputType.CMOT_FLOAT3, code="""
float2 p=UV;
float3 teal=float3(.13,.86,.93), hot=float3(.98,.09,.42), amber=float3(1.,.48,.10);
float3 c=float3(.002,.011,.017);
float2 edge=min(p,1.-p);
c+=teal*.55*(1.-smoothstep(.003,.009,min(edge.x,edge.y*.35)));
c+=teal*.25*(1.-smoothstep(.001,.004,min(abs(p.x-.45),abs(p.x-.71))));
float2 dial=(p-float2(.225,.80))*float2(3.09,-1.);
float radius=length(dial);
float angle=atan2(dial.y,dial.x);
float arc=saturate((3.04-angle)/2.94);
if (angle>.1 && angle<3.04 && radius>.49 && radius<.63 && frac(arc*26.)<.76)
    c+=lerp(teal,hot,step(.80,arc))*(arc<=Data.r ? 2.3 : .36);
float2 digits=(p-float2(.105,.55))/float2(.084,.20);
if (all(digits>=0) && digits.x<3 && digits.y<1) {
    int kmh=(int)round(Data.r*202.);
    int col=(int)digits.x;
    int digit=(col==0 ? kmh/100 : (col==1 ? kmh/10 : kmh))%10;
    int cell=digit+(digit>=4 ? 1 : 0);
    float2 atlas=(float2(41,22)+(float2(cell%8,cell/8)+frac(digits))*float2(182,125))/float2(1536,1024);
    float glyph=Texture2DSampleLevel(Glyphs,GlyphsSampler,atlas,0).r;
    c+=glyph*float3(.91,1.,1.)*1.8;
}
float2 h=(p-float2(.58,.37))*float2(3.09,1.);
float distanceToHorizon=abs(dot(h,float2(-sin(Data.g),cos(Data.g))));
if (abs(h.x)<.29) c+=teal*(1.-smoothstep(.012,.026,distanceToHorizon));
float2 bars=p-float2(.475,.63);
if (bars.x>=0 && bars.x<.21 && bars.y>=0 && bars.y<.23) {
    float stepId=floor(bars.x*65.);
    float value=.21+.16*cos(stepId*2.2)+.46*Data.r+.14*Data.b;
    if (bars.y>(1.-value)*.23 && frac(bars.x*65.)<.68) c+=teal*.85;
}
float2 chart=(p-float2(.735,.11))/float2(.235,.78);
if (all(chart>=0) && all(chart<1)) {
    float guide=min(abs(chart.x-.32),abs(chart.x-.68));
    c+=teal*.30*(1.-smoothstep(.012,.026,guide));
    float player=length((chart-float2(.5,.78))*float2(.95,1));
    c+=teal*(1.-smoothstep(.034,.060,player));
    float dot1=length((chart-Blips.rg)*float2(.95,1));
    float dot2=length((chart-Blips.ba)*float2(.95,1));
    c+=amber*2.*(1.-smoothstep(.03,.055,min(dot1,dot2)));
}
return c;""")
    wire(uv,display,"UV")
    # Vector parameter's default pin supplies RGB. The explicit RGBA pin also carries the second blip's Y.
    wire(telemetry,display,"Data")
    wire(traffic,display,"Blips","RGBA")
    wire(glyphs,display,"Glyphs")
    output(display,ue.MaterialProperty.MP_EMISSIVE_COLOR)
