#include "ReShade.fxh"

texture RLStreams_Main { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
texture RLStreams_Depth { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };
texture RLStreams_Normal { Width = BUFFER_WIDTH; Height = BUFFER_HEIGHT; Format = RGBA8; };

float3 GetScreenSpaceNormal(float2 texcoord) {
    float3 offset = float3(BUFFER_PIXEL_SIZE, 0.0);
    float2 posCenter = texcoord.xy;
    float2 posNorth  = posCenter - offset.zy;
    float2 posEast   = posCenter + offset.xz;

    float3 vertCenter = float3(posCenter - 0.5, 1) * ReShade::GetLinearizedDepth(posCenter);
    float3 vertNorth  = float3(posNorth - 0.5,  1) * ReShade::GetLinearizedDepth(posNorth);
    float3 vertEast   = float3(posEast - 0.5,   1) * ReShade::GetLinearizedDepth(posEast);

    return normalize(cross(vertCenter - vertNorth, vertCenter - vertEast)) * 0.5 + 0.5;
}

void PS_Depth(in float4 vpos : SV_Position, in float2 texcoord : TexCoord, out float4 color : SV_Target0) {
    color = float4(ReShade::GetLinearizedDepth(float2(texcoord.x, 1 - texcoord.y)).xxx, 1.0);
}

void PS_Normal(in float4 vpos : SV_Position, in float2 texcoord : TexCoord, out float4 color : SV_Target0) {
    color = float4(GetScreenSpaceNormal(float2(texcoord.x, 1 - texcoord.y)).zyx, 1.0);
}

void PS_Main(in float4 vpos : SV_Position, in float2 texcoord : TexCoord, out float4 color : SV_Target0) {
    color = float4(tex2D(ReShade::BackBuffer, float2(texcoord.x, 1 - texcoord.y)).bgr, 1.0);
}

technique RLStreams2 {
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_Depth;
        RenderTarget0 = RLStreams_Depth;
    }
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_Normal;
        RenderTarget0 = RLStreams_Normal;
    }
    pass {
        VertexShader = PostProcessVS;
        PixelShader = PS_Main;
        RenderTarget0 = RLStreams_Main;
    }
}
