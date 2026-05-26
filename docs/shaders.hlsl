struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

VS_OUTPUT VSMain(uint vertexID : SV_VertexID)
{
    VS_OUTPUT output;

    // Define a simple triangle
    float3 vertices[3] = {
        float3(0.0f, 0.5f, 0.0f),   // Top vertex
        float3(0.5f, -0.5f, 0.0f),  // Bottom right vertex
        float3(-0.5f, -0.5f, 0.0f)  // Bottom left vertex
    };

    // Define colors for each vertex
    float4 colors[3] = {
        float4(1.0f, 0.0f, 0.0f, 1.0f), // Red
        float4(0.0f, 1.0f, 0.0f, 1.0f), // Green
        float4(0.0f, 0.0f, 1.0f, 1.0f)  // Blue
    };

    output.position = float4(vertices[vertexID], 1.0f);
    output.color = colors[vertexID];

    return output;
}

float4 PSMain(VS_OUTPUT input) : SV_TARGET
{
    return input.color; // Output the color from the vertex shader
}