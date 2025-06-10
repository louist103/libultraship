@prism(type='fragment', name='Fast3D Fragment Shader', version='1.0.0', description='Ported shader to prism', author='Emill & Prism Team')

#version 450 core

// should stay position
layout(location = @{get_vs_input_location("position", "RGBA32Float")}) in vec4 position;

layout(location = @{get_vs_input_location("aColor", "RGBA32Float")}) in vec4 aColor;
layout(location = @{get_output_location()}) out vec4 vColor;

layout(location = @{get_vs_input_location("aTexCoord", "RG32Float")}) in vec2 aTexCoord;

@for(i in 0..2)
    @if(o_textures[i])
        layout(location = @{get_output_location()}) out vec2 vTexCoord@{i};
        layout(std140, binding = @{get_binding_index("texData" + to_string(i), "Buffer", "ConstantBuffer")}) uniform texData@{i} {
            ivec2 texShift@{i};
            vec2 texUl@{i};
            bool texIsRect@{i};
            vec2 texSize@{i};
        };
    @end
@end

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    @for(i in 0..2)
        @if(o_textures[i])
            vec2 uv@{i} = aTexCoord;

            // Mask for conditions
            bvec2 hasShift@{i} = notEqual(texShift@{i}, ivec2(0));
            bvec2 isSmallShift@{i} = lessThanEqual(texShift@{i}, ivec2(10));
            bvec2 isLargeShift@{i} = greaterThan(texShift@{i}, ivec2(10));

            // Vectorized application of shifts
            vec2 smallShiftDiv@{i} = uv@{i} / vec2(1 << texShift@{i}.x, 1 << texShift@{i}.y);
            vec2 largeShiftMul@{i} = uv@{i} * vec2(1 << (16 - texShift@{i}.x), 1 << (16 - texShift@{i}.y));

            // Vectorized conditional selection
            uv@{i} = mix(uv@{i}, smallShiftDiv@{i}, vec2(hasShift@{i}) * vec2(isSmallShift@{i}));
            uv@{i} = mix(uv@{i}, largeShiftMul@{i}, vec2(hasShift@{i}) * vec2(isLargeShift@{i}));
            // if (shifts != 0) {
            //     if (shifts <= 10) {
            //         u /= 1 << shifts;
            //     } else {
            //         u *= 1 << (16 - shifts);
            //     }
            // }
            // if (shiftt != 0) {
            //     if (shiftt <= 10) {
            //         v /= 1 << shiftt;
            //     } else {
            //         v *= 1 << (16 - shiftt);
            //     }
            // }
            uv@{i} -= texUl@{i} / 4.0f;

            if (texIsRect@{i}) {
                uv@{i} += 0.5f;
            }
            vTexCoord@{i} = uv@{i} / texSize@{i};
        @end
    @end

    vColor = aColor;
    gl_Position = position;
}
