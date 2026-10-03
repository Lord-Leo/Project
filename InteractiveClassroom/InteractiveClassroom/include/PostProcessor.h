#ifndef POST_PROCESSOR_H
#define POST_PROCESSOR_H

#include "Shader.h"
#include <string>

class PostProcessor
{
public:
    enum class ToneMapping
    {
        Reinhard = 0,
        ACES = 1,
        None = 2
    };

    PostProcessor(const std::string& fullscreenVertexPath,
                  const std::string& blurFragmentPath,
                  const std::string& compositeFragmentPath);
    ~PostProcessor();

    PostProcessor(const PostProcessor&) = delete;
    PostProcessor& operator=(const PostProcessor&) = delete;

    bool Initialize(int width, int height, int samples = 4);
    bool Resize(int width, int height);
    bool IsValid() const { return m_valid; }

    // Binds a complete HDR target. If useMSAA is true, the multisampled HDR
    // target is used and later resolved into the non-multisampled textures.
    void BeginScene(bool useMSAA);
    void EndScene(bool useMSAA);

    // Runs bounded ping-pong bloom blur and composites to framebuffer 0.
    void Composite(bool bloomEnabled, float bloomStrength, float exposure,
                   ToneMapping toneMapping, bool gammaEnabled,
                   int blurPasses = 8);

    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }

private:
    bool CreateTargets();
    bool CheckFramebuffer(const char* label) const;
    void DestroyTargets();
    void CreateFullscreenQuad();
    void DrawFullscreenQuad() const;
    unsigned int BlurBrightTexture(int passes);

    Shader m_blurShader;
    Shader m_compositeShader;

    unsigned int m_hdrFBO = 0;
    unsigned int m_hdrColour[2] = {0, 0};
    unsigned int m_hdrDepthRBO = 0;

    unsigned int m_msaaFBO = 0;
    unsigned int m_msaaColour[2] = {0, 0};
    unsigned int m_msaaDepthRBO = 0;

    unsigned int m_pingPongFBO[2] = {0, 0};
    unsigned int m_pingPongColour[2] = {0, 0};

    unsigned int m_quadVAO = 0;
    unsigned int m_quadVBO = 0;

    int m_width = 0;
    int m_height = 0;
    int m_samples = 4;
    bool m_valid = false;
};

#endif // POST_PROCESSOR_H
