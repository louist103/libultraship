@prism(type='fragment', name='Fast3D Fragment Shader', version='1.0.0', description='Ported shader to prism', author='Emill & Prism Team')

#version 450 core

// should stay position
layout(location = @{get_vs_input_location("position", "RGBA32Float")}) in vec4 position;

layout(location = @{get_vs_input_location("aColor", "RGBA32Float")}) in vec4 aColor;
layout(location = @{get_output_location()}) out vec4 vColor;

@for(i in 0..2)
    @if(o_textures[i])
        layout(location = @{get_vs_input_location("aTexCoord" + to_string(i), "RG32Float")}) in vec2 aTexCoord@{i};
        layout(location = @{get_output_location()}) out vec2 vTexCoord@{i};
    @end
@end

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    @for(i in 0..2)
        @if(o_textures[i])
            vTexCoord@{i} = aTexCoord@{i};
        @end
    @end
    
    vColor = aColor;
    gl_Position = position;
}
