
#include <prism/processor.h>
#include "gfx_rendering_api.h"
#include "window/gui/Gui.h"
#include "../interpreter.h"
#include <LLGL/LLGL.h>
#include "LLGL/Utils/VertexFormat.h"
#ifdef LLGL_OS_LINUX
#include <GL/glx.h>
#endif
#include "gfx_llgl.h"

#include "gfx_rendering_api.h"

#include <LLGL/Backend/OpenGL/NativeHandle.h>
#include "../../../resource/type/Shader.h"
#include <Context.h>
#include "../shader_translation.h"
#include "graphic/Fast3D/Fast3dWindow.h"

#include "utils/StringHelper.h"

LLGL::RenderSystemPtr llgl_renderer;
LLGL::SwapChain* llgl_swapChain;
LLGL::CommandBuffer* llgl_cmdBuffer;

struct {
    int current_tile;
    uint32_t current_texture_ids[SHADER_MAX_TEXTURES];
    std::vector<LLGL::Texture*> textures;
    bool srgb_mode = false;
    Fast::FilteringMode current_filter_mode = Fast::FILTER_NONE;
} llgl_state;
namespace Fast {

GfxRenderingAPILLGL::GfxRenderingAPILLGL(GfxWindowBackend* backend) {
    mWindowBackend = backend;
}

const char* Fast::GfxRenderingAPILLGL::GetName(void) {
    // renderer->GetName();
    return "LLGL";
}

int Fast::GfxRenderingAPILLGL::GetMaxTextureSize(void) {
    auto caps = llgl_renderer->GetRenderingCaps();
    return caps.limits.max2DTextureSize;
}

Fast::GfxClipParameters Fast::GfxRenderingAPILLGL::GetClipParameters(void) {
    return { false, false };
}

void Fast::GfxRenderingAPILLGL::UnloadShader(struct ShaderProgram* old_prg) {
}

void Fast::GfxRenderingAPILLGL::LoadShader(struct ShaderProgram* new_prg) {
    // llgl_cmdBuffer->SetPipelineState(new_prg->pipeline);
}

static void llgl_append_str(char* buf, size_t* len, const char* str) {
    while (*str != '\0') {
        buf[(*len)++] = *str++;
    }
}

static void llgl_append_line(char* buf, size_t* len, const char* str) {
    while (*str != '\0') {
        buf[(*len)++] = *str++;
    }
    buf[(*len)++] = '\n';
}

#define RAND_NOISE "((random(vec3(floor(gl_FragCoord.xy * noise_scale), float(frame_count))) + 1.0) / 2.0)"

static const char* llgl_shader_item_to_str(uint32_t item, bool with_alpha, bool only_alpha, bool inputs_have_alpha,
                                           bool first_cycle, bool hint_single_element) {
    if (!only_alpha) {
        switch (item) {
            case SHADER_0:
                return with_alpha ? "vec4(0.0, 0.0, 0.0, 0.0)" : "vec3(0.0, 0.0, 0.0)";
            case SHADER_1:
                return with_alpha ? "vec4(1.0, 1.0, 1.0, 1.0)" : "vec3(1.0, 1.0, 1.0)";
            case SHADER_INPUT_1:
                return with_alpha || !inputs_have_alpha ? "vInput1" : "vInput1.rgb";
            case SHADER_INPUT_2:
                return with_alpha || !inputs_have_alpha ? "vInput2" : "vInput2.rgb";
            case SHADER_INPUT_3:
                return with_alpha || !inputs_have_alpha ? "vInput3" : "vInput3.rgb";
            case SHADER_INPUT_4:
                return with_alpha || !inputs_have_alpha ? "vInput4" : "vInput4.rgb";
            case SHADER_TEXEL0:
                return first_cycle ? (with_alpha ? "texVal0" : "texVal0.rgb")
                                   : (with_alpha ? "texVal1" : "texVal1.rgb");
            case SHADER_TEXEL0A:
                return first_cycle
                           ? (hint_single_element ? "texVal0.a"
                                                  : (with_alpha ? "vec4(texVal0.a, texVal0.a, texVal0.a, texVal0.a)"
                                                                : "vec3(texVal0.a, texVal0.a, texVal0.a)"))
                           : (hint_single_element ? "texVal1.a"
                                                  : (with_alpha ? "vec4(texVal1.a, texVal1.a, texVal1.a, texVal1.a)"
                                                                : "vec3(texVal1.a, texVal1.a, texVal1.a)"));
            case SHADER_TEXEL1A:
                return first_cycle
                           ? (hint_single_element ? "texVal1.a"
                                                  : (with_alpha ? "vec4(texVal1.a, texVal1.a, texVal1.a, texVal1.a)"
                                                                : "vec3(texVal1.a, texVal1.a, texVal1.a)"))
                           : (hint_single_element ? "texVal0.a"
                                                  : (with_alpha ? "vec4(texVal0.a, texVal0.a, texVal0.a, texVal0.a)"
                                                                : "vec3(texVal0.a, texVal0.a, texVal0.a)"));
            case SHADER_TEXEL1:
                return first_cycle ? (with_alpha ? "texVal1" : "texVal1.rgb")
                                   : (with_alpha ? "texVal0" : "texVal0.rgb");
            case SHADER_COMBINED:
                return with_alpha ? "texel" : "texel.rgb";
            case SHADER_NOISE:
                return with_alpha ? "vec4(" RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ")"
                                  : "vec3(" RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ")";
        }
    } else {
        switch (item) {
            case SHADER_0:
                return "0.0";
            case SHADER_1:
                return "1.0";
            case SHADER_INPUT_1:
                return "vInput1.a";
            case SHADER_INPUT_2:
                return "vInput2.a";
            case SHADER_INPUT_3:
                return "vInput3.a";
            case SHADER_INPUT_4:
                return "vInput4.a";
            case SHADER_TEXEL0:
                return first_cycle ? "texVal0.a" : "texVal1.a";
            case SHADER_TEXEL0A:
                return first_cycle ? "texVal0.a" : "texVal1.a";
            case SHADER_TEXEL1A:
                return first_cycle ? "texVal1.a" : "texVal0.a";
            case SHADER_TEXEL1:
                return first_cycle ? "texVal1.a" : "texVal0.a";
            case SHADER_COMBINED:
                return "texel.a";
            case SHADER_NOISE:
                return RAND_NOISE;
        }
    }
    return "";
}

bool llgl_get_bool(prism::ContextTypes* value) {
     if (std::holds_alternative<int>(*value)) {
        return std::get<int>(*value) == 1;
    }
    return false;
}

prism::ContextTypes* llgl_append_formula(prism::ContextItems* _, prism::ContextTypes* a_arg, prism::ContextTypes* a_single,
                                         prism::ContextTypes* a_mult, prism::ContextTypes* a_mix,
                                         prism::ContextTypes* a_with_alpha, prism::ContextTypes* a_only_alpha,
                                         prism::ContextTypes* a_alpha, prism::ContextTypes* a_first_cycle) {
    auto c = std::get<prism::MTDArray<int>>(*a_arg);
    bool do_single = llgl_get_bool(a_single);
    bool do_multiply = llgl_get_bool(a_mult);
    bool do_mix = llgl_get_bool(a_mix);
    bool with_alpha = llgl_get_bool(a_with_alpha);
    bool only_alpha = llgl_get_bool(a_only_alpha);
    bool opt_alpha = llgl_get_bool(a_alpha);
    bool first_cycle = llgl_get_bool(a_first_cycle);
    std::string out = "";
    if (do_single) {
        out += llgl_shader_item_to_str(c.at(only_alpha, 3), with_alpha, only_alpha, opt_alpha, first_cycle, false);
    } else if (do_multiply) {
        out += llgl_shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += " * ";
        out += llgl_shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
    } else if (do_mix) {
        out += "mix(";
        out += llgl_shader_item_to_str(c.at(only_alpha, 1), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ", ";
        out += llgl_shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ", ";
        out += llgl_shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
        out += ")";
    } else {
        out += "(";
        out += llgl_shader_item_to_str(c.at(only_alpha, 0), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += " - ";
        out += llgl_shader_item_to_str(c.at(only_alpha, 1), with_alpha, only_alpha, opt_alpha, first_cycle, false);
        out += ") * ";
        out += llgl_shader_item_to_str(c.at(only_alpha, 2), with_alpha, only_alpha, opt_alpha, first_cycle, true);
        out += " + ";
        out += llgl_shader_item_to_str(c.at(only_alpha, 3), with_alpha, only_alpha, opt_alpha, first_cycle, false);
    }
    return new prism::ContextTypes{ out };
}

std::optional<std::string> llgl_opengl_include_fs(const std::string& path) {
    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Type = (uint32_t)Ship::ResourceType::Shader;
    init->ByteOrder = Ship::Endianness::Native;
    init->Format = RESOURCE_FORMAT_BINARY;
    auto res = static_pointer_cast<Ship::Shader>(
        Ship::Context::GetInstance()->GetResourceManager()->LoadResource(path, true, init));
    if (res == nullptr) {
        return std::nullopt;
    }
    auto inc = static_cast<std::string*>(res->GetRawPointer());
    return *inc;
}

std::string prism_to_string(const prism::ContextTypes* value) {
    if (std::holds_alternative<std::string>(*value)) {
        return std::get<std::string>(*value);
    } else if (std::holds_alternative<int>(*value)) {
        return std::to_string(std::get<int>(*value));
    } else if (std::holds_alternative<float>(*value)) {
        return std::to_string(std::get<float>(*value));
    }
    return "";
}

prism::ContextTypes* prism_context_to_string(prism::ContextItems* items, prism::ContextTypes* value) {
    return new prism::ContextTypes{ prism_to_string(value) };
}

prism::ContextTypes* get_input_location(prism::ContextItems* items, prism::ContextTypes* name) {
    auto name_str = std::get<std::string>(*name);

    int& input_index = std::get<int>(items->at("input_index"));
    auto input = input_index;
    input_index++;

    items->emplace(name_str, prism::ContextTypes{ input });
    return new prism::ContextTypes{ input };
}

prism::ContextTypes* get_output_location(prism::ContextItems* items, prism::ContextTypes* name) {
    auto name_str = std::get<std::string>(*name);

    int& output_index = std::get<int>(items->at("output_index"));
    auto output = output_index;
    output_index++;
    
    items->emplace(name_str, prism::ContextTypes{ output });
    return new prism::ContextTypes{ output };
}

prism::ContextTypes* get_binding_index(prism::ContextItems* items, prism::ContextTypes* name) {
    auto name_str = std::get<std::string>(*name);

    int& binding_index = std::get<int>(items->at("binding_index"));
    auto bind = binding_index;
    binding_index++;

    items->emplace(name_str, prism::ContextTypes{ bind });
    return new prism::ContextTypes{ bind };
}

static std::string llgl_build_fs_shader(const CCFeatures& cc_features) {
    prism::Processor processor;
    prism::ContextItems context = {
        { "o_c", M_ARRAY(cc_features.c, int, 2, 2, 4) },
        { "o_alpha", cc_features.opt_alpha },
        { "o_fog", cc_features.opt_fog },
        { "o_texture_edge", cc_features.opt_texture_edge },
        { "o_noise", cc_features.opt_noise },
        { "o_2cyc", cc_features.opt_2cyc },
        { "o_alpha_threshold", cc_features.opt_alpha_threshold },
        { "o_invisible", cc_features.opt_invisible },
        { "o_grayscale", cc_features.opt_grayscale },
        { "o_textures", M_ARRAY(cc_features.usedTextures, bool, 2) },
        { "o_masks", M_ARRAY(cc_features.used_masks, bool, 2) },
        { "o_blend", M_ARRAY(cc_features.used_blend, bool, 2) },
        { "o_clamp", M_ARRAY(cc_features.clamp, bool, 2, 2) },
        { "o_inputs", cc_features.numInputs },
        { "o_do_mix", M_ARRAY(cc_features.do_mix, bool, 2, 2) },
        { "o_do_single", M_ARRAY(cc_features.do_single, bool, 2, 2) },
        { "o_do_multiply", M_ARRAY(cc_features.do_multiply, bool, 2, 2) },
        { "o_color_alpha_same", M_ARRAY(cc_features.color_alpha_same, bool, 2) },
        { "o_three_point_filtering", llgl_state.current_filter_mode == FILTER_THREE_POINT },
        { "FILTER_THREE_POINT", Fast::FILTER_THREE_POINT },
        { "FILTER_LINEAR", Fast::FILTER_LINEAR },
        { "FILTER_NONE", Fast::FILTER_NONE },
        { "srgb_mode", llgl_state.srgb_mode },
        { "SHADER_0", SHADER_0 },
        { "SHADER_INPUT_1", SHADER_INPUT_1 },
        { "SHADER_INPUT_2", SHADER_INPUT_2 },
        { "SHADER_INPUT_3", SHADER_INPUT_3 },
        { "SHADER_INPUT_4", SHADER_INPUT_4 },
        { "SHADER_INPUT_5", SHADER_INPUT_5 },
        { "SHADER_INPUT_6", SHADER_INPUT_6 },
        { "SHADER_INPUT_7", SHADER_INPUT_7 },
        { "SHADER_TEXEL0", SHADER_TEXEL0 },
        { "SHADER_TEXEL0A", SHADER_TEXEL0A },
        { "SHADER_TEXEL1", SHADER_TEXEL1 },
        { "SHADER_TEXEL1A", SHADER_TEXEL1A },
        { "SHADER_1", SHADER_1 },
        { "SHADER_COMBINED", SHADER_COMBINED },
        { "SHADER_NOISE", SHADER_NOISE },
        { "append_formula", (InvokeFunc)llgl_append_formula },
        { "get_input_location", (InvokeFunc)get_input_location },
        { "get_binding_index", (InvokeFunc)get_binding_index },
        { "to_string", (InvokeFunc)prism_context_to_string },
        // local variables
        { "input_index", 0 },
        { "binding_index", 1},
        { "output_index", 0 },
    };
    processor.populate(context);
    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Type = (uint32_t)Ship::ResourceType::Shader;
    init->ByteOrder = Ship::Endianness::Native;
    init->Format = RESOURCE_FORMAT_BINARY;
    auto res = static_pointer_cast<Ship::Shader>(Ship::Context::GetInstance()->GetResourceManager()->LoadResource(
        "shaders/opengl/default.shader.fs", true, init));

    if (res == nullptr) {
        SPDLOG_ERROR("Failed to load default fragment shader, missing f3d.o2r?");
        abort();
    }

    auto shader = static_cast<std::string*>(res->GetRawPointer());
    processor.load(*shader);
    processor.bind_include_loader(llgl_opengl_include_fs);
    auto result = processor.process();
    // for (const auto& item : processor.getTypes()) {
    //     SPDLOG_INFO("{}: {}", item.first, prism_to_string((prism::ContextTypes*) &item.second));
    // }
    // SPDLOG_INFO("=========== FRAGMENT SHADER ============");
    // // print line per line with number
    // size_t line_num = 0;
    // for (const auto& line : StringHelper::Split(result, "\n")) {
    //     printf("%zu: %s\n", line_num, line.c_str());
    //     line_num++;
    // }
    // SPDLOG_INFO("========================================");
    return result;
}

static std::string llgl_build_vs_shader(const CCFeatures& cc_features) {
    prism::Processor processor;

    prism::ContextItems context = { { "o_textures", M_ARRAY(cc_features.usedTextures, bool, 2) },
                                    { "o_clamp", M_ARRAY(cc_features.clamp, bool, 2, 2) },
                                    { "o_fog", cc_features.opt_fog },
                                    { "o_grayscale", cc_features.opt_grayscale },
                                    { "o_alpha", cc_features.opt_alpha },
                                    { "o_inputs", cc_features.numInputs },
                                    { "get_input_location", (InvokeFunc)get_input_location },
                                    { "get_output_location", (InvokeFunc)get_output_location },
                                { "to_string", (InvokeFunc)prism_context_to_string },
        // local variables
        { "input_index", 0 },
        { "binding_index", 1},
        { "output_index", 0 }, };
    processor.populate(context);

    auto init = std::make_shared<Ship::ResourceInitData>();
    init->Type = (uint32_t)Ship::ResourceType::Shader;
    init->ByteOrder = Ship::Endianness::Native;
    init->Format = RESOURCE_FORMAT_BINARY;
    auto res = static_pointer_cast<Ship::Shader>(Ship::Context::GetInstance()->GetResourceManager()->LoadResource(
        "shaders/opengl/default.shader.vs", true, init));

    if (res == nullptr) {
        SPDLOG_ERROR("Failed to load default vertex shader, missing f3d.o2r?");
        abort();
    }

    auto shader = static_cast<std::string*>(res->GetRawPointer());
    processor.load(*shader);
    processor.bind_include_loader(llgl_opengl_include_fs);
    auto result = processor.process();
    // for (const auto& item : processor.getTypes()) {
    //     SPDLOG_INFO("{}: {}", item.first, prism_to_string((prism::ContextTypes*) &item.second));
    // }
    // SPDLOG_INFO("=========== VERTEX SHADER ============");
    // // print line per line with number
    // size_t line_num = 0;
    // for (const auto& line : StringHelper::Split(result, "\n")) {
    //     printf("%zu: %s\n", line_num, line.c_str());
    //     line_num++;
    // }
    // SPDLOG_INFO("========================================");
    return result;
}

LLGL::PipelineState* create_pipeline(LLGL::RenderSystemPtr& llgl_renderer, LLGL::SwapChain* llgl_swapChain,
                                     LLGL::VertexFormat& vertexFormat, std::string vertShaderSource,
                                     std::string fragShaderSource, LLGL::PipelineLayout* pipelineLayout = nullptr) {
    const auto& languages = llgl_renderer->GetRenderingCaps().shadingLanguages;
    LLGL::ShaderDescriptor vertShaderDesc, fragShaderDesc;

    std::variant<std::string, std::vector<uint32_t>> vertShaderSourceC, fragShaderSourceC;

    generate_shader_from_string(vertShaderDesc, fragShaderDesc, languages, vertexFormat, vertShaderSource,
                                fragShaderSource, vertShaderSourceC, fragShaderSourceC);

    // Specify vertex attributes for vertex shader
    vertShaderDesc.vertex.inputAttribs = vertexFormat.attributes;

    LLGL::Shader* vertShader = llgl_renderer->CreateShader(vertShaderDesc);
    LLGL::Shader* fragShader = llgl_renderer->CreateShader(fragShaderDesc);

    if (const LLGL::Report* report = vertShader->GetReport()) {
        if (std::holds_alternative<std::string>(vertShaderSourceC)) {
            int line_num = 0;
            for (const auto& line : StringHelper::Split(std::get<std::string>(vertShaderSourceC), "\n")) {
                printf("%d: %s\n", line_num, line.c_str());
                line_num++;
            }
        }
        SPDLOG_ERROR("vertShader: {}", report->GetText());
    }
    
    if (const LLGL::Report* report = fragShader->GetReport()) {
        if (std::holds_alternative<std::string>(fragShaderSourceC)) {
            int line_num = 0;
            for (const auto& line : StringHelper::Split(std::get<std::string>(fragShaderSourceC), "\n")) {
                printf("%d: %s\n", line_num, line.c_str());
                line_num++;
            }
        }
        SPDLOG_ERROR("fragShader: {}", report->GetText());
    }

    // Create graphics pipeline
    LLGL::PipelineState* pipeline = nullptr;
    LLGL::PipelineCache* pipelineCache = nullptr;

    LLGL::GraphicsPipelineDescriptor pipelineDesc;
    {
        pipelineDesc.vertexShader = vertShader;
        pipelineDesc.fragmentShader = fragShader;
        pipelineDesc.renderPass = llgl_swapChain->GetRenderPass();
        pipelineDesc.pipelineLayout = pipelineLayout;
    }

    // Create graphics PSO
    pipeline = llgl_renderer->CreatePipelineState(pipelineDesc, pipelineCache);

    // Link shader program and check for errors
    if (const LLGL::Report* report = pipeline->GetReport()) {
        if (report->HasErrors()) {
            const char* a = report->GetText();
            SPDLOG_ERROR(a);
            throw std::runtime_error("Failed to link shader program");
        }
    }
    return pipeline;
}

struct ShaderProgram* Fast::GfxRenderingAPILLGL::CreateAndLoadNewShader(uint64_t shader_id0, uint32_t shader_id1) {
    CCFeatures cc_features;
    gfx_cc_get_features(shader_id0, shader_id1, &cc_features);

    LLGL::VertexFormat vertexFormat;

    vertexFormat.AppendAttribute({ "aVtxPos", LLGL::Format::RGBA32Float });

    for (int i = 0; i < 2; i++) {
        if (cc_features.usedTextures[i]) {
            vertexFormat.AppendAttribute({ "aTexCoord" + std::to_string(i), LLGL::Format::RG32Float });
            for (int j = 0; j < 2; j++) {
                if (cc_features.clamp[i][j]) {
                    if (j == 0) {
                        vertexFormat.AppendAttribute({ "aTexClampS" + std::to_string(i), LLGL::Format::R32Float });
                    } else {
                        vertexFormat.AppendAttribute({ "aTexClampT" + std::to_string(i), LLGL::Format::R32Float });
                    }
                }
            }
        }
    }

    if (cc_features.opt_fog) {
        vertexFormat.AppendAttribute({ "aFogCoord", LLGL::Format::RGBA32Float });
    }
    if (cc_features.opt_grayscale) {
        vertexFormat.AppendAttribute({ "aGrayscaleColor", LLGL::Format::RGBA32Float });
    }

    for (int i = 0; i < cc_features.numInputs; i++) {
        if (cc_features.opt_alpha) {
            vertexFormat.AppendAttribute({ "aInput" + std::to_string(i + 1), LLGL::Format::RGBA32Float });
        } else {
            vertexFormat.AppendAttribute({ "aInput" + std::to_string(i + 1), LLGL::Format::RGB32Float });
        }
    }

    const auto vs_buf = llgl_build_vs_shader(cc_features);
    const auto fs_buf = llgl_build_fs_shader(cc_features);
    auto pipeline = create_pipeline(llgl_renderer, llgl_swapChain, vertexFormat, vs_buf, fs_buf);
    return nullptr;
}

Fast::ShaderProgram* Fast::GfxRenderingAPILLGL::LookupShader(uint64_t shader_id0, uint32_t shader_id1) {
    return nullptr;
}

void Fast::GfxRenderingAPILLGL::ShaderGetInfo(struct ShaderProgram* prg, uint8_t* num_inputs, bool used_textures[2]) {
}

uint32_t Fast::GfxRenderingAPILLGL::NewTexture(void) {
    llgl_state.textures.resize(llgl_state.textures.size() + 1);
    return (uint32_t)(llgl_state.textures.size() - 1);
}

void Fast::GfxRenderingAPILLGL::SelectTexture(int tile, uint32_t texture_id) {
    // TODO: not finish
    llgl_state.current_tile = tile;
    llgl_state.current_texture_ids[tile] = texture_id;
}

void Fast::GfxRenderingAPILLGL::UploadTexture(const uint8_t* rgba32_buf, uint32_t width, uint32_t height) {
    // TODO: not finish
    LLGL::ImageView imageView;
    imageView.format = LLGL::ImageFormat::RGBA;
    imageView.data = rgba32_buf;
    imageView.dataSize = width * height * 4;
    LLGL::TextureDescriptor texDesc;
    texDesc.type = LLGL::TextureType::Texture2D;
    texDesc.format = LLGL::Format::RGBA8UNorm;
    texDesc.extent = { width, height, 1 };
    llgl_state.textures[llgl_state.current_texture_ids[llgl_state.current_tile]] =
        llgl_renderer->CreateTexture(texDesc, &imageView);
}

void Fast::GfxRenderingAPILLGL::SetSamplerParameters(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt) {
}

void Fast::GfxRenderingAPILLGL::SetDepthTestAndMask(bool depth_test, bool z_upd) {
}

void Fast::GfxRenderingAPILLGL::SetZmodeDecal(bool zmode_decal) {
}

void Fast::GfxRenderingAPILLGL::SetViewport(int x, int y, int width, int height) {
    llgl_cmdBuffer->SetViewport(LLGL::Viewport(x, y, width, height));
}

void Fast::GfxRenderingAPILLGL::SetScissor(int x, int y, int width, int height) {
    llgl_cmdBuffer->SetScissor(LLGL::Scissor(x, y, width, height));
}

void Fast::GfxRenderingAPILLGL::SetUseAlpha(bool use_alpha) {
}

void Fast::GfxRenderingAPILLGL::DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    //llgl_cmdBuffer->SetVertexBuffer(buf_vbo)
}

void Fast::GfxRenderingAPILLGL::Init() {
    LLGL::Report report;


    llgl_renderer = LLGL::RenderSystem::Load(mWindowBackend->mInitData.LLGL.desc, &report);

    if (!llgl_renderer) {
        auto a = report.GetText();
        SPDLOG_ERROR("Failed to load \"%s\" module. Falling back to \"Null\" device.\n", "OpenGL");
        printf("Reason for failure: %s", report.HasErrors() ? report.GetText() : "Unknown\n");
        llgl_renderer = LLGL::RenderSystem::Load("Null");
        if (!llgl_renderer) {
            SPDLOG_CRITICAL("Failed to load \"Null\" module. Exiting.\n");
            exit(1);
        }
    }

    LLGL::SwapChainDescriptor swapChainDesc;
    swapChainDesc.resolution = { 800, 400 };
    swapChainDesc.resizable = true;
    llgl_swapChain = llgl_renderer->CreateSwapChain(swapChainDesc, mWindowBackend->mInitData.LLGL.Window);

    llgl_cmdBuffer = llgl_renderer->CreateCommandBuffer(LLGL::CommandBufferFlags::ImmediateSubmit);
}

void Fast::GfxRenderingAPILLGL::OnResize(void) {
}

void Fast::GfxRenderingAPILLGL::StartFrame(void) {
    llgl_cmdBuffer->Begin();
    llgl_cmdBuffer->BeginRenderPass(*llgl_swapChain);
}

void Fast::GfxRenderingAPILLGL::EndFrame(void) {
    // llgl_cmdBuffer->EndRenderPass();
    // llgl_cmdBuffer->End();
    // llgl_swapChain->Present();
}

void Fast::GfxRenderingAPILLGL::FinishRender(void) {
    llgl_cmdBuffer->EndRenderPass();
    llgl_cmdBuffer->End();
    llgl_swapChain->Present();
}

int Fast::GfxRenderingAPILLGL::CreateFramebuffer(void) {
    return 0;
}

void Fast::GfxRenderingAPILLGL::UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                            bool opengl_invert_y, bool render_target, bool has_depth_buffer,
                                            bool can_extract_depth) {
}

void Fast::GfxRenderingAPILLGL::StartDrawToFramebuffer(int fb_id, float noise_scale) {
}

void Fast::GfxRenderingAPILLGL::CopyFramebuffer(int fb_dst_id, int fb_src_id, int srcX0, int srcY0, int srcX1, int srcY1, int dstX0,
                               int dstY0, int dstX1, int dstY1) {
}

void Fast::GfxRenderingAPILLGL::ClearFramebuffer(bool color, bool depth) {
    int flags = 0 | (color ? LLGL::ClearFlags::Color : 0) | (depth ? LLGL::ClearFlags::Depth : 0);
    llgl_cmdBuffer->Clear(flags,
                          LLGL::ClearValue{ 0.0f, 0.0f, 0.0f, 1.0f });
}

void Fast::GfxRenderingAPILLGL::ReadFramebufferToCPU(int fb_id, uint32_t width, uint32_t height, uint16_t* rgba16_buf) {
}

void Fast::GfxRenderingAPILLGL::ResolveMSAAColorBuffer(int fb_id_target, int fb_id_source) {
}

std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
Fast::GfxRenderingAPILLGL::GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) {
    return {};
}

void* Fast::GfxRenderingAPILLGL::GetFramebufferTextureId(int fb_id) {
    return nullptr;
}

void Fast::GfxRenderingAPILLGL::SelectTextureFb(int fb_id) {
}

void Fast::GfxRenderingAPILLGL::DeleteTexture(uint32_t texture_id) {
}

void Fast::GfxRenderingAPILLGL::SetTextureFilter(Fast::FilteringMode mode) {
    llgl_state.current_filter_mode = mode;
}

Fast::FilteringMode Fast::GfxRenderingAPILLGL::GetTextureFilter(void) {
    return llgl_state.current_filter_mode;
}

void Fast::GfxRenderingAPILLGL::SetSrgbMode(void) {
    llgl_state.srgb_mode = true;
}

ImTextureID Fast::GfxRenderingAPILLGL::GetTextureById(int id) {
    return nullptr; //TODO fix me
}

}
#if 0
struct GfxRenderingAPI gfx_llgl_api = { GetName,
                                        GetMaxTextureSize,
                                        GetClipParameters,
                                        UnloadShader,
                                        LoadShader,
                                        CreateAndLoadNewShader,
                                        LookupShader,
                                        ShaderGetInfo,
                                        NewTexture,
                                        SelectTexture,
                                        UploadTexture,
                                        SetSamplerParameters,
                                        SetDepthTestAndMask,
                                        SetZmodeDecal,
                                        SetViewport,
                                        SetScissor,
                                        SetUsaAlpha,
                                        DrawTriangles,
                                        Init,
                                        OnResize,
                                        StartFrame,
                                        EndFrame,
                                        FinishRender,
                                        CreateFramebuffer,
                                        UpdateFramebufferParameters,
                                        StartDrawToFramebuffer,
                                        CopyFramebuffer,
                                        ClearFramebuffer,
                                        ReadFramebufferToCPU,
                                        ResolveMSAAColorBuffer,
                                        GetPixelDepth,
                                        GetFramebufferTextureId,
                                        SelectTextureFb,
                                        DeleteTexture,
                                        SetTextureFilter,
                                        GetTextureFilter,
                                        SetSrgbMode };
#endif