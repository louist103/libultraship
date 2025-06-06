#ifndef GFX_LLGL_H
#define GFX_LLGL_H

#include "gfx_rendering_api.h"
#include "graphic/Fast3D/interpreter.h"
#include <LLGL/LLGL.h>
#include "LLGL/Utils/VertexFormat.h"

extern LLGL::RenderSystemPtr llgl_renderer;
extern LLGL::SwapChain* llgl_swapChain;
extern LLGL::CommandBuffer* llgl_cmdBuffer;

namespace Fast {

struct ShaderProgramLLGL {
    int numInputs;
    int samplerStateBinding;
    int frameCountBinding;
    int noiseScaleBinding;
    std::optional<int> bindingTexture[2];
    std::optional<int> bindingMask[2];
    std::optional<int> bindingBlend[2];
    LLGL::PipelineState* pipeline;
    LLGL::VertexFormat vertexFormat;
};
class GfxRenderingAPILLGL : public GfxRenderingAPI {
  public:
    GfxRenderingAPILLGL(GfxWindowBackend* backend);
    ~GfxRenderingAPILLGL() override = default;
    const char* GetName() override;
    int GetMaxTextureSize() override;
    GfxClipParameters GetClipParameters() override;
    void UnloadShader(ShaderProgram* oldPrg) override;
    void LoadShader(ShaderProgram* newPrg) override;
    ShaderProgram* CreateAndLoadNewShader(uint64_t shaderId0, uint32_t shaderId1) override;
    ShaderProgram* LookupShader(uint64_t shaderId0, uint32_t shaderId1) override;
    void ShaderGetInfo(ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) override;
    uint32_t NewTexture() override;
    void SelectTexture(int tile, uint32_t textureId) override;
    void UploadTexture(const uint8_t* rgba32Buf, uint32_t width, uint32_t height) override;
    void SetSamplerParameters(int sampler, bool linear_filter, uint32_t cms, uint32_t cmt) override;
    void SetDepthTestAndMask(bool depth_test, bool z_upd) override;
    void SetZmodeDecal(bool decal) override;
    void SetViewport(int x, int y, int width, int height) override;
    void SetScissor(int x, int y, int width, int height) override;
    void SetUseAlpha(bool useAlpha) override;
    void DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) override;
    void Init() override;
    void OnResize() override;
    void StartFrame() override;
    void EndFrame() override;
    void FinishRender() override;
    int CreateFramebuffer() override;
    void UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height, uint32_t msaa_level,
                                     bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                     bool can_extract_depth) override;
    void StartDrawToFramebuffer(int fbId, float noiseScale) override;
    void CopyFramebuffer(int fbDstId, int fbSrcId, int srcX0, int srcY0, int srcX1, int srcY1, int dstX0, int dstY0,
                         int dstX1, int dstY1) override;
    void ClearFramebuffer(bool color, bool depth) override;
    void ReadFramebufferToCPU(int fbId, uint32_t width, uint32_t height, uint16_t* rgba16Buf) override;
    void ResolveMSAAColorBuffer(int fbIdTarger, int fbIdSrc) override;
    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
    GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) override;
    void* GetFramebufferTextureId(int fbId) override;
    void SelectTextureFb(int fbId) override;
    void DeleteTexture(uint32_t texId) override;
    void SetTextureFilter(FilteringMode mode) override;
    FilteringMode GetTextureFilter() override;
    void SetSrgbMode() override;
    ImTextureID GetTextureById(int id) override;

  private:
    int current_tile;
    uint32_t current_texture_ids[6];
    std::vector<LLGL::Texture*> textures;
    std::vector<std::pair<LLGL::RenderTarget*, int>> framebuffers;
    bool srgb_mode = false;
    Fast::FilteringMode current_filter_mode = Fast::FILTER_NONE;
    std::string llgl_build_fs_shader(const CCFeatures& cc_features, LLGL::PipelineLayoutDescriptor& layoutDesc);
    std::string llgl_build_vs_shader(const CCFeatures& cc_features, LLGL::PipelineLayoutDescriptor& layoutDesc,
                                     LLGL::VertexFormat& vertexFormat);
    std::map<std::pair<uint64_t, uint32_t>, struct ShaderProgramLLGL> mShaderProgramPool;
    struct ShaderProgramLLGL* mCurrentShaderProgram = nullptr;
    std::vector<LLGL::Buffer*> garbage_collection_buffers;
    int frame_count = 0;
    float noise_scale = 0.0f;
};
} // namespace Fast

#endif