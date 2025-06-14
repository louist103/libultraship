#include <filesystem>
#include <fstream>
#include <variant>

#include <glslang/Include/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/SpvTools.h>

#include <LLGL/LLGL.h>
#include <LLGL/Utils/VertexFormat.h>
#include "spirv_cross/spirv.hpp"

#include "spirv_cross/spirv.hpp"
#include "spirv_cross/spirv_glsl.hpp"
#include "spirv_cross/spirv_hlsl.hpp"
#include "spirv_cross/spirv_msl.hpp"
#include "shader_translation.h"
#include "backends/gfx_llgl.h"

static TBuiltInResource InitResources() {
    TBuiltInResource Resources;
    auto caps = llgl_renderer->GetRenderingCaps();
    auto limits = caps.limits;

    Resources.maxLights = 32;
    Resources.maxClipPlanes = 6;
    Resources.maxTextureUnits = 32;
    Resources.maxTextureCoords = 32;
    Resources.maxVertexAttribs = 64;
    Resources.maxVertexUniformComponents = 4096;
    Resources.maxVaryingFloats = 64;
    Resources.maxVertexTextureImageUnits = 32;
    Resources.maxCombinedTextureImageUnits = 80;
    Resources.maxTextureImageUnits = 32;
    Resources.maxFragmentUniformComponents = 4096;
    Resources.maxDrawBuffers = 32;
    Resources.maxVertexUniformVectors = 128;
    Resources.maxVaryingVectors = 8;
    Resources.maxFragmentUniformVectors = 16;
    Resources.maxVertexOutputVectors = 16;
    Resources.maxFragmentInputVectors = 15;
    Resources.minProgramTexelOffset = -8;
    Resources.maxProgramTexelOffset = 7;
    Resources.maxClipDistances = 8;
    Resources.maxComputeWorkGroupCountX = limits.maxComputeShaderWorkGroups[0];
    Resources.maxComputeWorkGroupCountY = limits.maxComputeShaderWorkGroups[1];
    Resources.maxComputeWorkGroupCountZ = limits.maxComputeShaderWorkGroups[2];
    Resources.maxComputeWorkGroupSizeX = limits.maxComputeShaderWorkGroupSize[0];
    Resources.maxComputeWorkGroupSizeY = limits.maxComputeShaderWorkGroupSize[1];
    Resources.maxComputeWorkGroupSizeZ = limits.maxComputeShaderWorkGroupSize[2];
    Resources.maxComputeUniformComponents = 1024;
    Resources.maxComputeTextureImageUnits = 16;
    Resources.maxComputeImageUniforms = 8;
    Resources.maxComputeAtomicCounters = 8;
    Resources.maxComputeAtomicCounterBuffers = 1;
    Resources.maxVaryingComponents = 60;
    Resources.maxVertexOutputComponents = 64;
    Resources.maxGeometryInputComponents = 64;
    Resources.maxGeometryOutputComponents = 128;
    Resources.maxFragmentInputComponents = 128;
    Resources.maxImageUnits = 8;
    Resources.maxCombinedImageUnitsAndFragmentOutputs = 8;
    Resources.maxCombinedShaderOutputResources = 8;
    Resources.maxImageSamples = 0;
    Resources.maxVertexImageUniforms = 0;
    Resources.maxTessControlImageUniforms = 0;
    Resources.maxTessEvaluationImageUniforms = 0;
    Resources.maxGeometryImageUniforms = 0;
    Resources.maxFragmentImageUniforms = 8;
    Resources.maxCombinedImageUniforms = 8;
    Resources.maxGeometryTextureImageUnits = 16;
    Resources.maxGeometryOutputVertices = 256;
    Resources.maxGeometryTotalOutputComponents = 1024;
    Resources.maxGeometryUniformComponents = 1024;
    Resources.maxGeometryVaryingComponents = 64;
    Resources.maxTessControlInputComponents = 128;
    Resources.maxTessControlOutputComponents = 128;
    Resources.maxTessControlTextureImageUnits = 16;
    Resources.maxTessControlUniformComponents = 1024;
    Resources.maxTessControlTotalOutputComponents = 4096;
    Resources.maxTessEvaluationInputComponents = 128;
    Resources.maxTessEvaluationOutputComponents = 128;
    Resources.maxTessEvaluationTextureImageUnits = 16;
    Resources.maxTessEvaluationUniformComponents = 1024;
    Resources.maxTessPatchComponents = 120;
    Resources.maxPatchVertices = 32;
    Resources.maxTessGenLevel = 64;
    Resources.maxViewports = limits.maxViewports;
    Resources.maxVertexAtomicCounters = 0;
    Resources.maxTessControlAtomicCounters = 0;
    Resources.maxTessEvaluationAtomicCounters = 0;
    Resources.maxGeometryAtomicCounters = 0;
    Resources.maxFragmentAtomicCounters = 8;
    Resources.maxCombinedAtomicCounters = 8;
    Resources.maxAtomicCounterBindings = 1;
    Resources.maxVertexAtomicCounterBuffers = 0;
    Resources.maxTessControlAtomicCounterBuffers = 0;
    Resources.maxTessEvaluationAtomicCounterBuffers = 0;
    Resources.maxGeometryAtomicCounterBuffers = 0;
    Resources.maxFragmentAtomicCounterBuffers = 1;
    Resources.maxCombinedAtomicCounterBuffers = 1;
    Resources.maxAtomicCounterBufferSize = 16384;
    Resources.maxTransformFeedbackBuffers = 4;
    Resources.maxTransformFeedbackInterleavedComponents = 64;
    Resources.maxCullDistances = 8;
    Resources.maxCombinedClipAndCullDistances = 8;
    Resources.maxSamples = 4;
    Resources.maxMeshOutputVerticesNV = 256;
    Resources.maxMeshOutputPrimitivesNV = 512;
    Resources.maxMeshWorkGroupSizeX_NV = 32;
    Resources.maxMeshWorkGroupSizeY_NV = 1;
    Resources.maxMeshWorkGroupSizeZ_NV = 1;
    Resources.maxTaskWorkGroupSizeX_NV = 32;
    Resources.maxTaskWorkGroupSizeY_NV = 1;
    Resources.maxTaskWorkGroupSizeZ_NV = 1;
    Resources.maxMeshViewCountNV = 4;

    Resources.limits.nonInductiveForLoops = 1;
    Resources.limits.whileLoops = 1;
    Resources.limits.doWhileLoops = 1;
    Resources.limits.generalUniformIndexing = 1;
    Resources.limits.generalAttributeMatrixVectorIndexing = 1;
    Resources.limits.generalVaryingIndexing = 1;
    Resources.limits.generalSamplerIndexing = 1;
    Resources.limits.generalVariableIndexing = 1;
    Resources.limits.generalConstantMatrixVectorIndexing = 1;

    return Resources;
}

glslang::TShader create_shader(EShLanguage type, std::filesystem::path shaderPath, char** shaderSourceC) {
    glslang::TShader shaderGlslang(type);

    shaderGlslang.setStrings(shaderSourceC, 1);

    shaderGlslang.setSourceFile(shaderPath.string().c_str());

    shaderGlslang.setEnvInput(glslang::EShSourceGlsl, type, glslang::EShClientVulkan, 100);
    shaderGlslang.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);
    shaderGlslang.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
    shaderGlslang.setTextureSamplerTransformMode(EShTextureSamplerTransformMode::EShTexSampTransKeep);

    return shaderGlslang;
}

void parse_shader(glslang::TShader& vertShaderGlslang, glslang::TShader& fragShaderGlslang) {
    TBuiltInResource builtInResources = InitResources();
    vertShaderGlslang.parse(&builtInResources, 120, true, EShMsgDefault);
    const char* vertShaderGlslangLog = vertShaderGlslang.getInfoLog();

    if (vertShaderGlslangLog != nullptr && *vertShaderGlslangLog != '\0') {
        SPDLOG_ERROR(vertShaderGlslangLog);
        throw std::runtime_error("Failed to compile vertex shader");
    }

    fragShaderGlslang.parse(&builtInResources, 120, true, EShMsgDefault);
    const char* fragShaderGlslangLog = fragShaderGlslang.getInfoLog();

    if (fragShaderGlslangLog != nullptr && *fragShaderGlslangLog != '\0') {
        SPDLOG_ERROR(fragShaderGlslangLog);
        throw std::runtime_error("Failed to compile fragment shader");
    }
}

void glslang_spirv_cross_test() {
    glslang::InitializeProcess();

    std::filesystem::path shaderPath = "../shader";
    std::filesystem::path vertShaderPath = shaderPath / "test.vert";
    std::filesystem::path fragShaderPath = shaderPath / "test.frag";

    std::string vertShaderSource;
    std::string fragShaderSource;
    std::ifstream shaderVertFile(shaderPath);
    if (!shaderVertFile.is_open()) {
        LLGL::Log::Printf("Failed to open shader file");
        throw std::runtime_error("Failed to open shader file");
    }
    vertShaderSource = std::string((std::istreambuf_iterator<char>(shaderVertFile)), std::istreambuf_iterator<char>());
    char* vertShaderSourceC = vertShaderSource.data();
    auto vertShaderGlslang = create_shader(EShLangVertex, vertShaderPath, &vertShaderSourceC);
    std::ifstream shaderFragFile(fragShaderPath);
    if (!shaderFragFile.is_open()) {
        LLGL::Log::Printf("Failed to open shader file");
        throw std::runtime_error("Failed to open shader file");
    }
    fragShaderSource = std::string((std::istreambuf_iterator<char>(shaderFragFile)), std::istreambuf_iterator<char>());
    char* fragShaderSourceC = fragShaderSource.data();
    auto fragShaderGlslang = create_shader(EShLangFragment, fragShaderPath, &fragShaderSourceC);

    glslang::TProgram program;
    parse_shader(vertShaderGlslang, fragShaderGlslang);

    auto vertShaderGlslangIntermediate = vertShaderGlslang.getIntermediate();
    auto fragShaderGlslangIntermediate = fragShaderGlslang.getIntermediate();

    if (vertShaderGlslangIntermediate == nullptr || fragShaderGlslangIntermediate == nullptr) {
        LLGL::Log::Printf("Failed to link shaders");
        throw std::runtime_error("Failed to link shaders");
    }

    spv::SpvBuildLogger logger;
    std::vector<uint32_t> spirvSourceVert;
    std::vector<uint32_t> spirvSourceFrag;

    glslang::SpvOptions spvOptions;
    spvOptions.validate = false;
    spvOptions.disableOptimizer = true;
    spvOptions.optimizeSize = false;

    glslang::GlslangToSpv(*vertShaderGlslangIntermediate, spirvSourceVert, &logger, &spvOptions);
    glslang::GlslangToSpv(*fragShaderGlslangIntermediate, spirvSourceFrag, &logger, &spvOptions);

    glslang::FinalizeProcess();

    spirv_cross::CompilerGLSL::Options scoptions;
    scoptions.version = 120;
    scoptions.es = false;
    spirv_cross::CompilerGLSL glslVert(spirvSourceVert);
    glslVert.set_common_options(scoptions);
    LLGL::Log::Printf("GLSL:\n%s\n", glslVert.compile().c_str());
    spirv_cross::CompilerGLSL glslFrag(spirvSourceFrag);
    glslFrag.set_common_options(scoptions);
    LLGL::Log::Printf("GLSL:\n%s\n", glslFrag.compile().c_str());

    spirv_cross::CompilerHLSL::Options hlslOptions;
    hlslOptions.shader_model = 500;
    spirv_cross::CompilerHLSL hlslVert(spirvSourceVert);
    hlslVert.set_hlsl_options(hlslOptions);
    LLGL::Log::Printf("HLSL:\n%s\n", hlslVert.compile().c_str());
    spirv_cross::CompilerHLSL hlslFrag(spirvSourceFrag);
    hlslFrag.set_hlsl_options(hlslOptions);
    LLGL::Log::Printf("HLSL:\n%s\n", hlslFrag.compile().c_str());

    spirv_cross::CompilerMSL mslVert(spirvSourceVert);
    LLGL::Log::Printf("MSL:\n%s\n", mslVert.compile().c_str());
    spirv_cross::CompilerMSL mslFrag(spirvSourceFrag);
    LLGL::Log::Printf("MSL:\n%s\n", mslFrag.compile().c_str());
}

bool is_glsl(const std::vector<LLGL::ShadingLanguage>& languages, int& version) {
    bool isGLSL = false;

    version = 0;

    for (const auto& language : languages) {
        if (language == LLGL::ShadingLanguage::GLSL) {
            isGLSL = true;
        } else if (static_cast<int>(language) & static_cast<int>(LLGL::ShadingLanguage::GLSL)) {
            int v = static_cast<int>(language) & static_cast<int>(LLGL::ShadingLanguage::VersionBitmask);
            if (v > version) {
                version = v;
            }
        }
    }
    return isGLSL;
}

bool is_glsles(const std::vector<LLGL::ShadingLanguage>& languages, int& version) {
    bool isGLSLES = false;

    version = 0;

    for (const auto& language : languages) {
        if (language == LLGL::ShadingLanguage::ESSL) {
            isGLSLES = true;
        } else if (static_cast<int>(language) & static_cast<int>(LLGL::ShadingLanguage::ESSL)) {
            int v = static_cast<int>(language) & static_cast<int>(LLGL::ShadingLanguage::VersionBitmask);
            if (v > version) {
                version = v;
            }
        }
    }
    return isGLSLES;
}

bool is_hlsl(const std::vector<LLGL::ShadingLanguage>& languages, int& version) {
    bool isHLSL = false;

    version = 0;

    for (const auto& language : languages) {
        if (language == LLGL::ShadingLanguage::HLSL) {
            isHLSL = true;
        } else if (static_cast<int>(language) & static_cast<int>(LLGL::ShadingLanguage::HLSL)) {
            int v = static_cast<int>(language) & static_cast<int>(LLGL::ShadingLanguage::VersionBitmask);
            if (v > version) {
                version = v;
            }
        }
    }
    return isHLSL;
}

bool is_metal(const std::vector<LLGL::ShadingLanguage>& languages, int& version) {
    bool isMetal = false;

    version = 0;

    for (const auto& language : languages) {
        if (language == LLGL::ShadingLanguage::Metal) {
            isMetal = true;
        } else if (static_cast<int>(language) & static_cast<int>(LLGL::ShadingLanguage::Metal)) {
            int v = static_cast<int>(language) & static_cast<int>(LLGL::ShadingLanguage::VersionBitmask);
            if (v > version) {
                version = v;
            }
        }
    }
    return isMetal;
}

bool is_spirv(const std::vector<LLGL::ShadingLanguage>& languages, int& version) {
    bool isSPIRV = false;

    version = 0;

    for (const auto& language : languages) {
        if (language == LLGL::ShadingLanguage::SPIRV) {
            isSPIRV = true;
        } else if (static_cast<int>(language) & static_cast<int>(LLGL::ShadingLanguage::SPIRV)) {
            int v = static_cast<int>(language) & static_cast<int>(LLGL::ShadingLanguage::VersionBitmask);
            if (v > version) {
                version = v;
            }
        }
    }
    return isSPIRV;
}

void generate_shader_from_string(LLGL::ShaderDescriptor& vertShaderDesc, LLGL::ShaderDescriptor& fragShaderDesc,
                                 const std::vector<LLGL::ShadingLanguage>& languages, LLGL::VertexFormat& vertexFormat,
                                 std::string vertShaderSource, std::string fragShaderSource,
                                 std::variant<std::string, std::vector<uint32_t>>& vertShader,
                                 std::variant<std::string, std::vector<uint32_t>>& fragShader) {
    glslang::InitializeProcess();
    char* vertShaderSourceC = vertShaderSource.data();
    auto vertShaderGlslang = create_shader(EShLangVertex, "", &vertShaderSourceC);
    char* fragShaderSourceC = fragShaderSource.data();
    auto fragShaderGlslang = create_shader(EShLangFragment, "", &fragShaderSourceC);

    parse_shader(vertShaderGlslang, fragShaderGlslang);

    auto vertShaderGlslangIntermediate = vertShaderGlslang.getIntermediate();
    auto fragShaderGlslangIntermediate = fragShaderGlslang.getIntermediate();

    if (vertShaderGlslangIntermediate == nullptr || fragShaderGlslangIntermediate == nullptr) {
        throw std::runtime_error("Failed to get intermediate");
    }

    spv::SpvBuildLogger logger;
    std::vector<uint32_t> spirvSourceVert;
    std::vector<uint32_t> spirvSourceFrag;

    glslang::SpvOptions spvOptions;
    spvOptions.validate = false;
    spvOptions.disableOptimizer = true;
    spvOptions.optimizeSize = false;

    glslang::GlslangToSpv(*vertShaderGlslangIntermediate, spirvSourceVert, &logger, &spvOptions);
    glslang::GlslangToSpv(*fragShaderGlslangIntermediate, spirvSourceFrag, &logger, &spvOptions);

    glslang::FinalizeProcess();
    int version = 0;
    if (is_glsl(languages, version)) {
        spirv_cross::CompilerGLSL::Options scoptions;
        scoptions.version = version;
        scoptions.es = false;
#ifdef __APPLE__
        scoptions.enable_420pack_extension = false;
#endif

        spirv_cross::CompilerGLSL glslVert(spirvSourceVert);
        glslVert.set_common_options(scoptions);
        vertShader = glslVert.compile();
        vertShaderDesc = { LLGL::ShaderType::Vertex, std::get<std::string>(vertShader).c_str() };
        vertShaderDesc.sourceType = LLGL::ShaderSourceType::CodeString;
        printf("GLSL:\n%s\n", std::get<std::string>(vertShader).c_str());

        spirv_cross::CompilerGLSL glslFrag(spirvSourceFrag);
        glslFrag.set_common_options(scoptions);
        glslFrag.build_combined_image_samplers();
        auto &samplers = glslFrag.get_combined_image_samplers();
        for (const auto& sampler : samplers) {
            glslFrag.set_name(sampler.combined_id, glslFrag.get_name(sampler.image_id));

            if (glslFrag.has_decoration(sampler.image_id, spv::DecorationDescriptorSet)) {
                uint32_t set = glslFrag.get_decoration(sampler.image_id, spv::DecorationDescriptorSet);
                glslFrag.set_decoration(sampler.combined_id, spv::DecorationDescriptorSet, set);
            }

            if (glslFrag.has_decoration(sampler.image_id, spv::DecorationBinding)) {
                uint32_t binding = glslFrag.get_decoration(sampler.image_id, spv::DecorationBinding);
                glslFrag.set_decoration(sampler.combined_id, spv::DecorationBinding, binding);
            }
        }
        fragShader = glslFrag.compile();
        fragShaderDesc = { LLGL::ShaderType::Fragment, std::get<std::string>(fragShader).c_str() };
        fragShaderDesc.sourceType = LLGL::ShaderSourceType::CodeString;
        printf("GLSL:\n%s\n", std::get<std::string>(fragShader).c_str());
    } else if (is_glsles(languages, version)) {
        spirv_cross::CompilerGLSL::Options scoptions;
        scoptions.version = version;
        scoptions.es = true;
#ifdef __APPLE__
        scoptions.enable_420pack_extension = false;
#endif

        spirv_cross::CompilerGLSL glslVert(spirvSourceVert);
        glslVert.set_common_options(scoptions);
        vertShader = glslVert.compile();
        vertShaderDesc = { LLGL::ShaderType::Vertex, std::get<std::string>(vertShader).c_str() };
        vertShaderDesc.sourceType = LLGL::ShaderSourceType::CodeString;
        printf("GLSL ES:\n%s\n", std::get<std::string>(vertShader).c_str());

        spirv_cross::CompilerGLSL glslFrag(spirvSourceFrag);
        glslFrag.set_common_options(scoptions);
        fragShader = glslFrag.compile();
        fragShaderDesc = { LLGL::ShaderType::Fragment, std::get<std::string>(fragShader).c_str() };
        fragShaderDesc.sourceType = LLGL::ShaderSourceType::CodeString;
        printf("GLSL ES:\n%s\n", std::get<std::string>(fragShader).c_str());
    } else if (is_spirv(languages, version)) {
        vertShader = spirvSourceVert;
        char* vertShaderSourceC = (char*)malloc(spirvSourceVert.size() * sizeof(uint32_t));
        memcpy(vertShaderSourceC, &(*spirvSourceVert.begin()), spirvSourceVert.size() * sizeof(uint32_t));
        vertShaderDesc = { LLGL::ShaderType::Vertex, vertShaderSourceC };
        vertShaderDesc.sourceType = LLGL::ShaderSourceType::BinaryBuffer;
        vertShaderDesc.sourceSize = spirvSourceVert.size() * sizeof(uint32_t);

        fragShader = spirvSourceFrag;
        char* fragShaderSourceC = (char*)malloc(spirvSourceFrag.size() * sizeof(uint32_t));
        memcpy(fragShaderSourceC, &(*spirvSourceFrag.begin()), spirvSourceFrag.size() * sizeof(uint32_t));
        fragShaderDesc = { LLGL::ShaderType::Fragment, (char*)fragShaderSourceC };
        fragShaderDesc.sourceType = LLGL::ShaderSourceType::BinaryBuffer;
        fragShaderDesc.sourceSize = spirvSourceFrag.size() * sizeof(uint32_t);

        printf("SPIRV:\n");
    } else if (is_hlsl(languages, version)) {
        spirv_cross::CompilerHLSL::Options hlslOptions;
        hlslOptions.shader_model = version / 10;
        int semanticIndex = 0;
        for (auto& attribute : vertexFormat.attributes) {
            if (attribute.name.compare("position") != 0) {
                attribute.name = "TEXCOORD";
                attribute.semanticIndex = semanticIndex++;
            }
        }

        spirv_cross::CompilerHLSL hlslVert(spirvSourceVert);
        hlslVert.set_hlsl_options(hlslOptions);
        for (unsigned int i = 0; i < vertexFormat.attributes.size(); i++) {
            std::string semanticName = vertexFormat.attributes[i].name.c_str();
            if (semanticName != "position") {
                semanticName += std::to_string(vertexFormat.attributes[i].semanticIndex);
            }
            hlslVert.add_vertex_attribute_remap({ i, semanticName });
        }
        vertShader = hlslVert.compile();
        vertShaderDesc = { LLGL::ShaderType::Vertex, std::get<std::string>(vertShader).c_str() };
        vertShaderDesc.sourceType = LLGL::ShaderSourceType::CodeString;
        vertShaderDesc.entryPoint = "main";
        vertShaderDesc.profile = "vs_5_0";
        printf("HLSL:\n%s\n", std::get<std::string>(vertShader).c_str());

        spirv_cross::CompilerHLSL hlslFrag(spirvSourceFrag);
        hlslFrag.set_hlsl_options(hlslOptions);
        fragShader = hlslFrag.compile();
        fragShaderDesc = { LLGL::ShaderType::Fragment, std::get<std::string>(fragShader).c_str() };
        fragShaderDesc.sourceType = LLGL::ShaderSourceType::CodeString;
        fragShaderDesc.entryPoint = "main";
        fragShaderDesc.profile = "ps_5_0";
        printf("HLSL:\n%s\n", std::get<std::string>(fragShader).c_str());
    } else if (is_metal(languages, version)) {
        spirv_cross::CompilerMSL::Options options;
        options.enable_decoration_binding = true;
        spirv_cross::CompilerMSL mslVert(spirvSourceVert);
        mslVert.set_msl_options(options);
        vertShader = mslVert.compile();
        vertShaderDesc = { LLGL::ShaderType::Vertex, std::get<std::string>(vertShader).c_str() };
        vertShaderDesc.sourceType = LLGL::ShaderSourceType::CodeString;
        // vertShaderDesc.flags |= LLGL::ShaderCompileFlags::DefaultLibrary;
        vertShaderDesc.entryPoint = "main0";
        vertShaderDesc.profile = "2.1";
        printf("MSL:\n%s\n", std::get<std::string>(vertShader).c_str());

        spirv_cross::CompilerMSL mslFrag(spirvSourceFrag);
        mslFrag.set_msl_options(options);
        fragShader = mslFrag.compile();
        fragShaderDesc = { LLGL::ShaderType::Fragment, std::get<std::string>(fragShader).c_str() };
        fragShaderDesc.sourceType = LLGL::ShaderSourceType::CodeString;
        // fragShaderDesc.flags |= LLGL::ShaderCompileFlags::DefaultLibrary;
        fragShaderDesc.entryPoint = "main0";
        fragShaderDesc.profile = "2.1";
        printf("MSL:\n%s\n", std::get<std::string>(fragShader).c_str());
    } else {
        throw std::runtime_error("Unknown shader language");        
    }
}

void generate_shader(LLGL::ShaderDescriptor& vertShaderDesc, LLGL::ShaderDescriptor& fragShaderDesc,
                     const std::vector<LLGL::ShadingLanguage>& languages, LLGL::VertexFormat& vertexFormat,
                     std::string name_shader, std::variant<std::string, std::vector<uint32_t>>& vertShader,
                     std::variant<std::string, std::vector<uint32_t>>& fragShader) {
#ifdef WIN32
    std::filesystem::path shaderPath = "../../shader";
#else
    std::filesystem::path shaderPath = "../shader";
#endif
    std::filesystem::path vertShaderPath = shaderPath / (name_shader + ".vert");
    std::filesystem::path fragShaderPath = shaderPath / (name_shader + ".frag");

    std::string vertShaderSource;
    std::string fragShaderSource;
    std::ifstream shaderVertFile(shaderPath);
    if (!shaderVertFile.is_open()) {
        LLGL::Log::Printf("Failed to open shader file");
        throw std::runtime_error("Failed to open shader file");
    }
    vertShaderSource = std::string((std::istreambuf_iterator<char>(shaderVertFile)), std::istreambuf_iterator<char>());
    std::ifstream shaderFragFile(fragShaderPath);
    if (!shaderFragFile.is_open()) {
        LLGL::Log::Printf("Failed to open shader file");
        throw std::runtime_error("Failed to open shader file");
    }
    fragShaderSource = std::string((std::istreambuf_iterator<char>(shaderFragFile)), std::istreambuf_iterator<char>());

    generate_shader_from_string(vertShaderDesc, fragShaderDesc, languages, vertexFormat, vertShaderSource,
                                fragShaderSource, vertShader, fragShader);
}