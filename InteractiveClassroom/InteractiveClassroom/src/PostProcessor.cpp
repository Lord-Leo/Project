#include "PostProcessor.h"
#include <glad/glad.h>
#include <algorithm>
#include <iostream>

PostProcessor::PostProcessor(const std::string& fullscreenVertexPath,
                             const std::string& blurFragmentPath,
                             const std::string& compositeFragmentPath)
    : m_blurShader(fullscreenVertexPath, blurFragmentPath),
      m_compositeShader(fullscreenVertexPath, compositeFragmentPath)
{
    CreateFullscreenQuad();
}

PostProcessor::~PostProcessor()
{
    DestroyTargets();
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
}

void PostProcessor::CreateFullscreenQuad()
{
    const float vertices[] = {
        // position       // uv
        -1.0f, -1.0f,     0.0f, 0.0f,
         1.0f, -1.0f,     1.0f, 0.0f,
         1.0f,  1.0f,     1.0f, 1.0f,
        -1.0f, -1.0f,     0.0f, 0.0f,
         1.0f,  1.0f,     1.0f, 1.0f,
        -1.0f,  1.0f,     0.0f, 1.0f
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glBindVertexArray(0);
}

void PostProcessor::DestroyTargets()
{
    if (m_hdrFBO) glDeleteFramebuffers(1, &m_hdrFBO);
    if (m_msaaFBO) glDeleteFramebuffers(1, &m_msaaFBO);
    glDeleteFramebuffers(2, m_pingPongFBO);
    glDeleteTextures(2, m_hdrColour);
    glDeleteTextures(2, m_msaaColour);
    glDeleteTextures(2, m_pingPongColour);
    if (m_hdrDepthRBO) glDeleteRenderbuffers(1, &m_hdrDepthRBO);
    if (m_msaaDepthRBO) glDeleteRenderbuffers(1, &m_msaaDepthRBO);

    m_hdrFBO = 0;
    m_msaaFBO = 0;
    m_pingPongFBO[0] = m_pingPongFBO[1] = 0;
    m_hdrColour[0] = m_hdrColour[1] = 0;
    m_msaaColour[0] = m_msaaColour[1] = 0;
    m_pingPongColour[0] = m_pingPongColour[1] = 0;
    m_hdrDepthRBO = 0;
    m_msaaDepthRBO = 0;
    m_valid = false;
}

bool PostProcessor::CheckFramebuffer(const char* label) const
{
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "[PostProcessor] " << label << " framebuffer incomplete (status 0x"
                  << std::hex << status << std::dec << "). HDR disabled; legacy fallback remains available.\n";
        return false;
    }
    return true;
}

bool PostProcessor::CreateTargets()
{
    if (m_width <= 0 || m_height <= 0)
        return false;

    // Non-multisampled HDR resolve target: scene colour + bright pass.
    glGenFramebuffers(1, &m_hdrFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_hdrFBO);
    glGenTextures(2, m_hdrColour);
    for (int i = 0; i < 2; ++i)
    {
        glBindTexture(GL_TEXTURE_2D, m_hdrColour[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0,
                     GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                               GL_TEXTURE_2D, m_hdrColour[i], 0);
    }
    const GLenum attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);

    glGenRenderbuffers(1, &m_hdrDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_hdrDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_width, m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, m_hdrDepthRBO);
    if (!CheckFramebuffer("HDR resolve"))
        return false;

    // Multisampled HDR target. Both floating-point colour attachments are
    // resolved explicitly; they are never sampled as sampler2DMS.
    glGenFramebuffers(1, &m_msaaFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_msaaFBO);
    glGenTextures(2, m_msaaColour);
    for (int i = 0; i < 2; ++i)
    {
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, m_msaaColour[i]);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, m_samples,
                                GL_RGBA16F, m_width, m_height, GL_TRUE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i,
                               GL_TEXTURE_2D_MULTISAMPLE, m_msaaColour[i], 0);
    }
    glDrawBuffers(2, attachments);

    glGenRenderbuffers(1, &m_msaaDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, m_msaaDepthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, m_samples,
                                     GL_DEPTH_COMPONENT24, m_width, m_height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, m_msaaDepthRBO);
    if (!CheckFramebuffer("multisampled HDR"))
        return false;

    // Ping-pong blur targets.
    glGenFramebuffers(2, m_pingPongFBO);
    glGenTextures(2, m_pingPongColour);
    for (int i = 0; i < 2; ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_pingPongFBO[i]);
        glBindTexture(GL_TEXTURE_2D, m_pingPongColour[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, m_width, m_height, 0,
                     GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, m_pingPongColour[i], 0);
        if (!CheckFramebuffer(i == 0 ? "bloom ping" : "bloom pong"))
            return false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return true;
}

bool PostProcessor::Initialize(int width, int height, int samples)
{
    DestroyTargets();
    m_width = width;
    m_height = height;
    m_samples = std::max(samples, 1);
    m_valid = CreateTargets();
    if (!m_valid)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        DestroyTargets();
    }
    return m_valid;
}

bool PostProcessor::Resize(int width, int height)
{
    if (width <= 0 || height <= 0)
        return false;
    if (m_valid && width == m_width && height == m_height)
        return true;
    return Initialize(width, height, m_samples);
}

void PostProcessor::BeginScene(bool useMSAA)
{
    if (!m_valid)
        return;
    glBindFramebuffer(GL_FRAMEBUFFER, useMSAA ? m_msaaFBO : m_hdrFBO);
    const GLenum attachments[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);
    glViewport(0, 0, m_width, m_height);
}

void PostProcessor::EndScene(bool useMSAA)
{
    if (!m_valid)
        return;

    if (useMSAA)
    {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, m_msaaFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_hdrFBO);
        for (int i = 0; i < 2; ++i)
        {
            glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
            glDrawBuffer(GL_COLOR_ATTACHMENT0 + i);
            glBlitFramebuffer(0, 0, m_width, m_height,
                              0, 0, m_width, m_height,
                              GL_COLOR_BUFFER_BIT, GL_NEAREST);
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void PostProcessor::DrawFullscreenQuad() const
{
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

unsigned int PostProcessor::BlurBrightTexture(int passes)
{
    const int boundedPasses = std::clamp(passes, 1, 16);
    bool horizontal = true;
    bool first = true;

    m_blurShader.use();
    m_blurShader.setInt("image", 0);
    for (int i = 0; i < boundedPasses; ++i)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, m_pingPongFBO[horizontal ? 1 : 0]);
        m_blurShader.setBool("horizontal", horizontal);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D,
                      first ? m_hdrColour[1] : m_pingPongColour[horizontal ? 0 : 1]);
        DrawFullscreenQuad();
        horizontal = !horizontal;
        if (first) first = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return m_pingPongColour[horizontal ? 0 : 1];
}

void PostProcessor::Composite(bool bloomEnabled, float bloomStrength,
                              float exposure, ToneMapping toneMapping,
                              bool gammaEnabled, int blurPasses)
{
    if (!m_valid)
        return;

    const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean blendWasEnabled = glIsEnabled(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);

    unsigned int blurred = m_hdrColour[1];
    if (bloomEnabled)
        blurred = BlurBrightTexture(blurPasses);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    glClear(GL_COLOR_BUFFER_BIT);

    m_compositeShader.use();
    m_compositeShader.setInt("sceneTexture", 0);
    m_compositeShader.setInt("bloomTexture", 1);
    m_compositeShader.setBool("bloomEnabled", bloomEnabled);
    m_compositeShader.setFloat("bloomStrength", std::clamp(bloomStrength, 0.0f, 2.0f));
    m_compositeShader.setFloat("exposure", std::clamp(exposure, 0.1f, 5.0f));
    m_compositeShader.setInt("toneMappingMode", static_cast<int>(toneMapping));
    m_compositeShader.setBool("gammaEnabled", gammaEnabled);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdrColour[0]);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, blurred);
    DrawFullscreenQuad();

    if (depthWasEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (cullWasEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (blendWasEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
}
