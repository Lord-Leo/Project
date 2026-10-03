#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Shader.h"
#include "Camera.h"
#include "Mesh.h"
#include "Classroom.h"
#include "Computer.h"
#include "InteractionSystem.h"
#include "AudioManager.h"
#include "ClassroomAudioController.h"
#include "PostProcessor.h"

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <array>
#include <cctype>
#include <algorithm>
#include <iomanip>
#include <sstream>

// ---------------------------------------------------------------------------
// Small app-wide state passed through GLFW's user pointer so callbacks can
// reach the camera/window bookkeeping without globals.
// ---------------------------------------------------------------------------
struct AppState
{
    Camera* camera = nullptr;
    bool firstMouse = true;
    float lastX = 0.0f;
    float lastY = 0.0f;
    bool mouseCaptured = true;
};

// Phase 3 rendering toggles (F1/F2/F3), shown in the window title and HUD.
struct RenderSettings
{
    bool shadowsEnabled = true;
    bool msaaEnabled = true;
    bool gammaEnabled = true;
    bool pbrEnabled = true;
    bool bloomEnabled = true;
    bool hdrEnabled = true;
    float exposure = 1.0f;
    float bloomStrength = 0.10f;
    float bloomThreshold = 1.15f;
    PostProcessor::ToneMapping toneMapping = PostProcessor::ToneMapping::ACES;
};

static void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    (void)window;
    glViewport(0, 0, width, height);
}

static void mouseCallback(GLFWwindow* window, double xposIn, double yposIn)
{
    AppState* state = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!state || !state->mouseCaptured || !state->camera) return;

    float xpos = (float)xposIn;
    float ypos = (float)yposIn;

    if (state->firstMouse)
    {
        state->lastX = xpos;
        state->lastY = ypos;
        state->firstMouse = false;
    }

    float xoffset = xpos - state->lastX;
    float yoffset = state->lastY - ypos; // reversed: y-coordinates go from bottom to top
    state->lastX = xpos;
    state->lastY = ypos;

    state->camera->ProcessMouseMovement(xoffset, yoffset);
}

// ---------------------------------------------------------------------------
// Uniform-array upload for the classroom Phong shader's dynamic light list.
// ---------------------------------------------------------------------------
static void uploadLights(const Shader& shader, const std::vector<PointLight>& lights)
{
    int count = (int)std::min<size_t>(lights.size(), 64);
    shader.setInt("numLights", count);
    for (int i = 0; i < count; ++i)
    {
        std::string base = "[" + std::to_string(i) + "]";
        shader.setVec3("lightPositions" + base, lights[i].position);
        shader.setVec3("lightColors" + base, lights[i].color);
        shader.setFloat("lightIntensities" + base, lights[i].intensity);
    }
}

static void uploadSpotLights(const Shader& shader, const std::vector<SpotLight>& lights)
{
    const int count = static_cast<int>(std::min<size_t>(lights.size(), 4));
    shader.setInt("numSpotLights", count);
    for (int i = 0; i < count; ++i)
    {
        const std::string index = "[" + std::to_string(i) + "]";
        shader.setVec3("spotLightPositions" + index, lights[i].position);
        shader.setVec3("spotLightDirections" + index, lights[i].direction);
        shader.setVec3("spotLightColors" + index, lights[i].color);
        shader.setFloat("spotLightIntensities" + index, lights[i].intensity);
        shader.setFloat("spotLightInnerCutoffs" + index, lights[i].innerCutoffCos);
        shader.setFloat("spotLightOuterCutoffs" + index, lights[i].outerCutoffCos);
    }
}

// ---------------------------------------------------------------------------
// Shadow map (Phase 3): a depth-only framebuffer + texture for the
// directional light, plus the fixed light-space matrix used to render into
// it and to sample it back during the main color pass.
// ---------------------------------------------------------------------------
static const unsigned int SHADOW_MAP_SIZE = 2048;

static bool createShadowMap(unsigned int& outFBO, unsigned int& outDepthTexture)
{
    glGenTextures(1, &outDepthTexture);
    glBindTexture(GL_TEXTURE_2D, outDepthTexture);
    // Use an explicit sized depth format for better framebuffer
    // compatibility across Intel/NVIDIA/AMD Windows drivers.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, (GLsizei)SHADOW_MAP_SIZE, (GLsizei)SHADOW_MAP_SIZE,
                 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    // Border = white (maximum depth = 1.0), so any shadow-map sample that
    // falls outside the light's frustum reads as "far away" -> not in
    // shadow, instead of wrapping/clamping into an incorrect nearby texel.
    float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glGenFramebuffers(1, &outFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, outFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, outDepthTexture, 0);
    glDrawBuffer(GL_NONE); // depth-only: no color attachment to write
    glReadBuffer(GL_NONE);

    bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!complete)
        std::cerr << "[main] ERROR: shadow map framebuffer is incomplete (status 0x"
                   << std::hex << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::dec << ")\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return complete;
}

// ---------------------------------------------------------------------------
// MSAA (Phase 3, toggleable at runtime): an offscreen multisampled
// color+depth renderbuffer target. When MSAA is on, the scene renders into
// this framebuffer and is then resolved (blitted) into the default
// framebuffer; when off, the scene renders straight into the default
// framebuffer at 1 sample. This avoids having to recreate the GL context/
// window to change the sample count.
// ---------------------------------------------------------------------------
struct MSAATarget
{
    unsigned int fbo = 0;
    unsigned int colorRBO = 0;
    unsigned int depthRBO = 0;
    int width = 0;
    int height = 0;
    int samples = 4;
};

static void destroyMSAATarget(MSAATarget& target)
{
    if (target.fbo) { glDeleteFramebuffers(1, &target.fbo); target.fbo = 0; }
    if (target.colorRBO) { glDeleteRenderbuffers(1, &target.colorRBO); target.colorRBO = 0; }
    if (target.depthRBO) { glDeleteRenderbuffers(1, &target.depthRBO); target.depthRBO = 0; }
}

static bool createMSAATarget(MSAATarget& target, int width, int height)
{
    destroyMSAATarget(target);
    if (width <= 0 || height <= 0) return false;

    target.width = width;
    target.height = height;

    glGenFramebuffers(1, &target.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);

    glGenRenderbuffers(1, &target.colorRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, target.colorRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, target.samples, GL_RGB8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, target.colorRBO);

    glGenRenderbuffers(1, &target.depthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, target.depthRBO);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, target.samples, GL_DEPTH_COMPONENT24, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, target.depthRBO);

    bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    if (!complete)
        std::cerr << "[main] ERROR: MSAA framebuffer is incomplete (status 0x"
                   << std::hex << glCheckFramebufferStatus(GL_FRAMEBUFFER) << std::dec << ")\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return complete;
}

// ---------------------------------------------------------------------------
// Minimal built-in 5x7 bitmap font (only the glyphs the HUD ever needs:
// the letters actually used by the interaction prompts and the Phase 3
// settings readout, plus space). Each entry is 7 rows; each row's low 5
// bits are the pixels for that row (bit 4 = leftmost column .. bit 0 =
// rightmost column).
// ---------------------------------------------------------------------------
static const std::map<char, std::array<unsigned char, 7>>& GetFont()
{
    static const std::map<char, std::array<unsigned char, 7>> font = {
        {'A',{14,17,17,31,17,17,17}}, {'B',{30,17,17,30,17,17,30}},
        {'C',{14,17,16,16,16,17,14}}, {'D',{30,17,17,17,17,17,30}},
        {'E',{31,16,16,30,16,16,31}}, {'F',{31,16,16,30,16,16,16}},
        {'G',{14,17,16,23,17,17,14}}, {'H',{17,17,17,31,17,17,17}},
        {'I',{31,4,4,4,4,4,31}}, {'J',{3,1,1,1,17,17,14}},
        {'K',{17,18,20,24,20,18,17}}, {'L',{16,16,16,16,16,16,31}},
        {'M',{17,27,21,21,17,17,17}}, {'N',{17,25,21,19,17,17,17}},
        {'O',{14,17,17,17,17,17,14}}, {'P',{30,17,17,30,16,16,16}},
        {'Q',{14,17,17,17,21,18,13}}, {'R',{30,17,17,30,20,18,17}},
        {'S',{15,16,16,14,1,1,30}}, {'T',{31,4,4,4,4,4,4}},
        {'U',{17,17,17,17,17,17,14}}, {'V',{17,17,17,17,17,10,4}},
        {'W',{17,17,17,21,21,21,10}}, {'X',{17,17,10,4,10,17,17}},
        {'Y',{17,17,10,4,4,4,4}}, {'Z',{31,1,2,4,8,16,31}},
        {'0',{14,17,19,21,25,17,14}}, {'1',{4,12,4,4,4,4,14}},
        {'2',{14,17,1,2,4,8,31}}, {'3',{30,1,1,14,1,1,30}},
        {'4',{2,6,10,18,31,2,2}}, {'5',{31,16,16,30,1,1,30}},
        {'6',{14,16,16,30,17,17,14}}, {'7',{31,1,2,4,8,8,8}},
        {'8',{14,17,17,14,17,17,14}}, {'9',{14,17,17,15,1,1,14}},
        {' ',{0,0,0,0,0,0,0}}, {'.',{0,0,0,0,0,12,12}},
        {':',{0,12,12,0,12,12,0}}, {'-',{0,0,0,31,0,0,0}},
        {'/',{1,2,4,8,16,0,0}}, {'[',{14,8,8,8,8,8,14}},
        {']',{14,2,2,2,2,2,14}}, {'+',{0,4,4,31,4,4,0}}
    };    return font;
}

static std::string toUpper(const std::string& s)
{
    std::string out = s;
    for (char& c : out) c = (char)std::toupper((unsigned char)c);
    return out;
}

// Builds an NDC-space triangle list drawing `text` centered horizontally at
// (centerXPixels, yPixels), each font pixel `pixelSize` screen pixels wide,
// and uploads/draws it immediately with the given screen shader.
static void drawText(const Shader& screenShader, unsigned int vao, unsigned int vbo,
                      const std::string& textIn, float centerXPixels, float yPixels,
                      float pixelSize, int windowWidth, int windowHeight, glm::vec4 color)
{
    std::string text = toUpper(textIn);
    const auto& font = GetFont();

    const int glyphCols = 5;
    const int glyphRows = 7;
    const float glyphWidthPx = glyphCols * pixelSize;
    const float glyphSpacingPx = pixelSize; // gap between characters
    const float totalWidthPx = text.size() * (glyphWidthPx + glyphSpacingPx);

    float startX = centerXPixels - totalWidthPx * 0.5f;

    std::vector<float> verts;
    verts.reserve(text.size() * glyphRows * glyphCols * 12);

    float penX = startX;
    for (char ch : text)
    {
        auto it = font.find(ch);
        if (it != font.end())
        {
            const auto& rows = it->second;
            for (int row = 0; row < glyphRows; ++row)
            {
                unsigned char bits = rows[row];
                for (int col = 0; col < glyphCols; ++col)
                {
                    bool on = (bits >> (glyphCols - 1 - col)) & 0x1;
                    if (!on) continue;

                    float pxX0 = penX + col * pixelSize;
                    float pxY0 = yPixels + row * pixelSize;
                    float pxX1 = pxX0 + pixelSize;
                    float pxY1 = pxY0 + pixelSize;

                    auto toNDC = [&](float px, float py, float& nx, float& ny)
                    {
                        nx = (px / (float)windowWidth) * 2.0f - 1.0f;
                        ny = 1.0f - (py / (float)windowHeight) * 2.0f;
                    };

                    float x0, y0, x1, y1;
                    toNDC(pxX0, pxY0, x0, y0);
                    toNDC(pxX1, pxY1, x1, y1);

                    float quad[12] = {
                        x0, y0,  x1, y0,  x1, y1,
                        x1, y1,  x0, y1,  x0, y0
                    };
                    for (float v : quad) verts.push_back(v);
                }
            }
        }
        penX += glyphWidthPx + glyphSpacingPx;
    }

    if (verts.empty()) return;

    screenShader.use();
    screenShader.setVec4("uColor", color);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 2));
    glBindVertexArray(0);
}

// Draws a simple "+" crosshair centered on the screen.
static void drawCrosshair(const Shader& screenShader, unsigned int vao, unsigned int vbo,
                           int windowWidth, int windowHeight)
{
    float halfLen = 9.0f;   // pixels
    float thickness = 2.0f; // pixels

    auto toNDC = [&](float px, float py, float& nx, float& ny)
    {
        nx = (px / (float)windowWidth) * 2.0f - 1.0f;
        ny = 1.0f - (py / (float)windowHeight) * 2.0f;
    };

    float cx = windowWidth * 0.5f;
    float cy = windowHeight * 0.5f;

    std::vector<float> verts;
    auto addRect = [&](float x0, float y0, float x1, float y1)
    {
        float nx0, ny0, nx1, ny1;
        toNDC(x0, y0, nx0, ny0);
        toNDC(x1, y1, nx1, ny1);
        float quad[12] = {
            nx0, ny0,  nx1, ny0,  nx1, ny1,
            nx1, ny1,  nx0, ny1,  nx0, ny0
        };
        for (float v : quad) verts.push_back(v);
    };

    addRect(cx - halfLen, cy - thickness * 0.5f, cx + halfLen, cy + thickness * 0.5f);
    addRect(cx - thickness * 0.5f, cy - halfLen, cx + thickness * 0.5f, cy + halfLen);

    screenShader.use();
    screenShader.setVec4("uColor", glm::vec4(1.0f, 1.0f, 1.0f, 0.9f));

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 2));
    glBindVertexArray(0);
}

// Builds the "SHADOWS ON  MSAA ON  GAMMA ON" style status string shown in
// both the window title and the HUD (requirement 12). Takes the *effective*
// state of each feature (already combined with its validity flag by the
// caller), so the label never claims shadows/MSAA are on when their
// framebuffer actually failed to create.
static std::string toneMappingLabel(PostProcessor::ToneMapping mode)
{
    switch (mode)
    {
        case PostProcessor::ToneMapping::Reinhard: return "REINHARD";
        case PostProcessor::ToneMapping::ACES:     return "ACES";
        case PostProcessor::ToneMapping::None:     return "NONE";
    }
    return "UNKNOWN";
}

static std::string formatFloat(float value, int precision = 2)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

static std::string buildSettingsLabel(bool effectiveShadows, bool effectiveMSAA,
                                      const RenderSettings& settings, bool effectiveHDR)
{
    return std::string("SHADOWS ") + (effectiveShadows ? "ON" : "OFF") +
           "   MSAA " + (effectiveMSAA ? "ON" : "OFF") +
           "   GAMMA " + (settings.gammaEnabled ? "ON" : "OFF") +
           "   PBR " + (settings.pbrEnabled ? "ON" : "LEGACY") +
           "   HDR " + (effectiveHDR ? "ON" : "FALLBACK") +
           "   BLOOM " + ((effectiveHDR && settings.bloomEnabled) ? "ON" : "OFF") +
           "   EXP " + formatFloat(settings.exposure, 1);
}

static std::string buildProjectorLabel(const Projector* projector)
{
    if (!projector) return "PROJECTOR UNKNOWN";
    return std::string("PROJECTOR ") + projector->GetStateName() +
           "   MODE " + projector->GetProjectionModeName();
}

static void updateWindowTitle(GLFWwindow* window, bool effectiveShadows, bool effectiveMSAA,
                              const RenderSettings& settings, bool effectiveHDR,
                              const Projector* projector, const AudioManager& audio,
                              float fps)
{
    std::string title = "Interactive Classroom - Phase 9 [" +
                        buildSettingsLabel(effectiveShadows, effectiveMSAA, settings, effectiveHDR) +
                        "] [TONE " + toneMappingLabel(settings.toneMapping) +
                        "] [FPS " + formatFloat(fps, 0) +
                        "] [" + buildProjectorLabel(projector) +
                        "] [" + audio.GetStatusLabel() + "]";
    glfwSetWindowTitle(window, title.c_str());
}

int main()
{
    if (!glfwInit())
    {
        std::cerr << "[main] ERROR: glfwInit failed\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    int windowWidth = 1280;
    int windowHeight = 720;
    GLFWwindow* window = glfwCreateWindow(windowWidth, windowHeight, "Interactive Classroom - Phase 9", nullptr, nullptr);
    if (!window)
    {
        std::cerr << "[main] ERROR: glfwCreateWindow failed\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glfwSwapInterval(1); // vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cerr << "[main] ERROR: failed to load OpenGL function pointers\n";
        glfwTerminate();
        return -1;
    }

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glEnable(GL_MULTISAMPLE);

    // Keep every OpenGL-owning C++ object inside this scope so its
    // destructor runs while the GLFW context is still alive. This includes
    // the Phase 5 projector beam mesh as well as the pre-existing classroom
    // meshes and shader programs.
    {
    Classroom classroom;
    Camera camera(classroom.GetSpawnPosition(), classroom.GetSpawnYaw(), 0.0f);

    AudioManager audioManager;
    audioManager.Initialize("sounds"); // failure is non-fatal; every audio call becomes a safe no-op
    ClassroomAudioController classroomAudio(audioManager, classroom);
    classroomAudio.Initialize();

    InteractionSystem interactionSystem;
    interactionSystem.SetInteractables(classroom.GetInteractables());

    AppState appState;
    appState.camera = &camera;
    appState.lastX = windowWidth * 0.5f;
    appState.lastY = windowHeight * 0.5f;
    glfwSetWindowUserPointer(window, &appState);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    Shader classroomShader("shaders/classroom.vert", "shaders/classroom.frag");
    Shader pbrShader("shaders/pbr.vert", "shaders/pbr.frag");
    Shader screenShader("shaders/screen.vert", "shaders/screen.frag");
    Shader depthShader("shaders/shadow_depth.vert", "shaders/shadow_depth.frag");
    PostProcessor postProcessor("shaders/postprocess.vert",
                                "shaders/blur.frag",
                                "shaders/postprocess.frag");

    unsigned int uiVAO, uiVBO;
    glGenVertexArrays(1, &uiVAO);
    glGenBuffers(1, &uiVBO);
    glBindVertexArray(uiVAO);
    glBindBuffer(GL_ARRAY_BUFFER, uiVBO);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // ---- Shadow map (Phase 3) ----
    unsigned int shadowFBO = 0, shadowDepthTexture = 0;
    bool shadowMapValid = createShadowMap(shadowFBO, shadowDepthTexture);
    if (!shadowMapValid)
        std::cerr << "[main] Shadow map creation failed; the scene will render without shadows.\n";

    // ---- Window sunlight / directional light. ----
    // The classroom windows are on the side walls.  This direction models
    // warm daylight entering from the +X window wall and travelling inward
    // across the room, with a downward component so desks/chairs cast clear
    // classroom-style shadows.  It is intentionally independent of the
    // electric ceiling-light circuit: switching the room lights off must not
    // switch the sun off.
    glm::vec3 dirLightDirection = glm::normalize(glm::vec3(-1.0f, -0.72f, 0.20f));
    glm::vec3 dirLightColor(1.0f, 0.91f, 0.76f);

    // Static light-space matrix covering the whole room (the room never
    // moves, so this only needs to be computed once).
    float halfW = Classroom::kRoomWidth * 0.5f;
    float halfD = Classroom::kRoomDepth * 0.5f;
    float sceneRadius = std::sqrt(halfW * halfW + halfD * halfD) + 2.0f;
    glm::vec3 sceneCenter(0.0f, Classroom::kCeilingHeight * 0.5f, 0.0f);
    glm::vec3 lightEye = sceneCenter - dirLightDirection * (sceneRadius + 10.0f);
    glm::mat4 lightView = glm::lookAt(lightEye, sceneCenter, glm::vec3(0.0f, 1.0f, 0.0f));
    float orthoHalfSize = sceneRadius + 2.0f;
    glm::mat4 lightProjection = glm::ortho(-orthoHalfSize, orthoHalfSize, -orthoHalfSize, orthoHalfSize,
                                            0.1f, (sceneRadius + 10.0f) * 2.0f + 5.0f);
    glm::mat4 lightSpaceMatrix = lightProjection * lightView;

    // ---- MSAA offscreen target (Phase 3, toggleable via F2) ----
    RenderSettings settings;
    MSAATarget msaaTarget;
    msaaTarget.samples = 4;
    bool msaaTargetValid = createMSAATarget(msaaTarget, windowWidth, windowHeight);
    if (!msaaTargetValid)
        std::cerr << "[main] Legacy fallback MSAA framebuffer creation failed; direct rendering remains available.\n";

    bool hdrTargetValid = postProcessor.Initialize(windowWidth, windowHeight, 4);
    if (!hdrTargetValid)
        std::cerr << "[main] HDR framebuffer creation failed; using the validated legacy rendering fallback.\n";

    int lastFbWidth = windowWidth;
    int lastFbHeight = windowHeight;
    float displayedFPS = 0.0f;
    float fpsAccumulation = 0.0f;
    int fpsFrames = 0;

    updateWindowTitle(window, settings.shadowsEnabled && shadowMapValid,
                      settings.msaaEnabled && (hdrTargetValid || msaaTargetValid),
                      settings, settings.hdrEnabled && hdrTargetValid,
                      classroom.GetProjector(), audioManager, displayedFPS);

    bool ePressedLast = false;
    bool escPressedLast = false;
    bool f1PressedLast = false;
    bool f2PressedLast = false;
    bool f3PressedLast = false;
    bool f4PressedLast = false;
    bool f5PressedLast = false;
    bool f6PressedLast = false;
    bool f7PressedLast = false;
    bool f8PressedLast = false;
    bool f9PressedLast = false;
    bool pPressedLast = false;
    bool hPressedLast = false;
    bool lPressedLast = false;
    bool oPressedLast = false;
    bool f10PressedLast = false;
    bool minusPressedLast = false;
    bool equalPressedLast = false;
    bool mPressedLast = false;
    bool f11PressedLast = false;
    bool f12PressedLast = false;
    bool bPressedLast = false;
    bool tPressedLast = false;
    bool leftBracketPressedLast = false;
    bool rightBracketPressedLast = false;

    // Transient HUD message for the F4/F5/F6 debug/demo bulk controls
    // (requirement 16's "show the effective status ... if practical").
    std::string bulkActionMessage;
    float bulkActionMessageTimer = 0.0f;

    float lastFrameTime = (float)glfwGetTime();

    const float playerRadius = 0.35f;
    const float eyeHeight = 1.7f;
    const float interactRange = 3.0f;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrameTime = (float)glfwGetTime();
        float deltaTime = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;
        if (deltaTime > 0.1f) deltaTime = 0.1f;
        fpsAccumulation += deltaTime;
        ++fpsFrames;
        if (fpsAccumulation >= 0.5f)
        {
            displayedFPS = fpsFrames / std::max(fpsAccumulation, 0.0001f);
            fpsAccumulation = 0.0f;
            fpsFrames = 0;
        }

        glfwPollEvents();

        const glm::vec3 previousListenerPosition = camera.GetPosition();
        bool playerRunning = false;

        int fbWidth, fbHeight;
        glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
        if (fbWidth > 0 && fbHeight > 0)
        {
            windowWidth = fbWidth;
            windowHeight = fbHeight;
        }

        // Recreate the MSAA offscreen target whenever the framebuffer size
        // actually changes, so it never renders at a stale resolution.
        // The validity flag is refreshed here too, so a resize that fails
        // to allocate (e.g. out of video memory) safely falls back to
        // direct-to-default-framebuffer rendering instead of using a
        // stale or incomplete target.
        if ((fbWidth != lastFbWidth || fbHeight != lastFbHeight) && fbWidth > 0 && fbHeight > 0)
        {
            lastFbWidth = fbWidth;
            lastFbHeight = fbHeight;
            msaaTargetValid = createMSAATarget(msaaTarget, fbWidth, fbHeight);
            if (!msaaTargetValid)
                std::cerr << "[main] Legacy MSAA target recreation failed after resize; direct fallback remains available.\n";
            hdrTargetValid = postProcessor.Resize(fbWidth, fbHeight);
            if (!hdrTargetValid)
                std::cerr << "[main] HDR target recreation failed after resize; using the legacy fallback path.\n";
        }

        // ---- ESC: toggle mouse capture ----
        bool escPressed = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
        if (escPressed && !escPressedLast)
        {
            appState.mouseCaptured = !appState.mouseCaptured;
            glfwSetInputMode(window, GLFW_CURSOR, appState.mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
            appState.firstMouse = true;
        }
        escPressedLast = escPressed;

        // ---- F1/F2/F3: rendering toggles. H is an additional shadow
        // shortcut for keyboards/laptops where function keys are captured
        // by Windows or require Fn. ----
        bool f1Pressed = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
        bool hPressed = glfwGetKey(window, GLFW_KEY_H) == GLFW_PRESS;
        bool shadowTogglePressed = f1Pressed || (appState.mouseCaptured && hPressed);
        bool shadowTogglePressedLast = f1PressedLast || hPressedLast;
        if (shadowTogglePressed && !shadowTogglePressedLast)
        {
            settings.shadowsEnabled = !settings.shadowsEnabled;
            bulkActionMessage = std::string("SHADOWS ") +
                ((settings.shadowsEnabled && shadowMapValid) ? "ON" : "OFF");
            bulkActionMessageTimer = 2.0f;
        }
        f1PressedLast = f1Pressed;
        hPressedLast = hPressed;

        bool f2Pressed = glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS;
        if (f2Pressed && !f2PressedLast)
        {
            settings.msaaEnabled = !settings.msaaEnabled;
        }
        f2PressedLast = f2Pressed;

        bool f3Pressed = glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS;
        if (f3Pressed && !f3PressedLast)
        {
            settings.gammaEnabled = !settings.gammaEnabled;
        }
        f3PressedLast = f3Pressed;

        // L toggles the exact same shared circuit as both physical wall
        // switches. It is intentionally a silent debug shortcut; physical E
        // interactions remain the positional switch-audio path.
        bool lPressed = glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS;
        if (appState.mouseCaptured && lPressed && !lPressedLast)
        {
            classroom.ToggleLights();
            bulkActionMessage = std::string("CLASSROOM LIGHTS ") +
                                (classroom.AreLightsOn() ? "ON" : "OFF");
            bulkActionMessageTimer = 2.0f;
        }
        lPressedLast = lPressed;

        // ---- F4/F5/F6: debug/demo bulk power controls (Phase 4, requirement
        // 16). These only ever call the same Interact()/ForceOff() entry
        // points a player pressing E would use (or the dedicated instant
        // reset for F6), so they can never desync from or corrupt an
        // individual workstation's own state machine. ----
        bool f4Pressed = glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS;
        if (f4Pressed && !f4PressedLast)
        {
            for (Computer* c : classroom.GetComputers())
                if (c->GetPowerState() == PowerState::Off)
                    c->Interact();
            bulkActionMessage = "ALL COMPUTERS POWERING ON";
            bulkActionMessageTimer = 2.0f;
        }
        f4PressedLast = f4Pressed;

        bool f5Pressed = glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS;
        if (f5Pressed && !f5PressedLast)
        {
            // RequestShutdown() is a no-op for computers already Off or
            // already ShuttingDown, and safely cancels a Booting computer
            // straight into ShuttingDown (no corrupted timer/brightness),
            // so it's always safe to call on every computer unconditionally.
            for (Computer* c : classroom.GetComputers())
                c->RequestShutdown();
            bulkActionMessage = "ALL COMPUTERS SHUTTING DOWN";
            bulkActionMessageTimer = 2.0f;
        }
        f5PressedLast = f5Pressed;

        bool f6Pressed = glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS;
        if (f6Pressed && !f6PressedLast)
        {
            for (Computer* c : classroom.GetComputers())
                c->ForceOff();
            classroomAudio.StopComputerLoops();
            bulkActionMessage = "ALL COMPUTERS RESET";
            bulkActionMessageTimer = 2.0f;
        }
        f6PressedLast = f6Pressed;

        // ---- Phase 5 projector controls. All paths call the same Projector
        // object/state machine used by direct E interaction and the teacher
        // desk panel; there is no duplicated power or mode state. ----
        Projector* projector = classroom.GetProjector();

        bool f7Pressed = glfwGetKey(window, GLFW_KEY_F7) == GLFW_PRESS;
        bool oPressed = glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS;
        bool projectorTogglePressed = f7Pressed || (appState.mouseCaptured && oPressed);
        bool projectorTogglePressedLast = f7PressedLast || oPressedLast;
        if (projectorTogglePressed && !projectorTogglePressedLast && projector)
        {
            projector->TogglePower();
            bulkActionMessage = std::string("PROJECTOR ") + projector->GetStateName();
            bulkActionMessageTimer = 2.0f;
        }
        f7PressedLast = f7Pressed;
        oPressedLast = oPressed;

        bool f8Pressed = glfwGetKey(window, GLFW_KEY_F8) == GLFW_PRESS;
        if (f8Pressed && !f8PressedLast && projector)
        {
            projector->CycleProjectionMode();
            bulkActionMessage = projector->IsActive()
                ? std::string("PROJECTOR MODE: ") + projector->GetProjectionModeName()
                : "PROJECTOR MODE CHANGES REQUIRE ACTIVE STATE";
            bulkActionMessageTimer = 2.0f;
        }
        f8PressedLast = f8Pressed;

        bool f9Pressed = glfwGetKey(window, GLFW_KEY_F9) == GLFW_PRESS;
        if (f9Pressed && !f9PressedLast && projector)
        {
            projector->ForceOff();
            classroomAudio.ResetProjectorAudio();
            bulkActionMessage = "PROJECTOR SILENT DEBUG RESET TO OFF";
            bulkActionMessageTimer = 2.0f;
        }
        f9PressedLast = f9Pressed;

        bool pPressed = glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS;
        if (appState.mouseCaptured && pPressed && !pPressedLast && projector)
        {
            projector->CycleProjectionMode();
            if (projector->IsActive())
            {
                bulkActionMessage = std::string("PROJECTOR MODE: ") + projector->GetProjectionModeName();
                bulkActionMessageTimer = 2.0f;
            }
        }
        pPressedLast = pPressed;

        // ---- Phase 6 master audio controls. Edge-triggered so holding a key
        // never changes volume every frame. These do not replace any Phase 1-5
        // controls or remove rendering status from the title. ----
        bool f10Pressed = glfwGetKey(window, GLFW_KEY_F10) == GLFW_PRESS;
        if (f10Pressed && !f10PressedLast)
        {
            audioManager.ToggleMute();
            bulkActionMessage = audioManager.GetStatusLabel();
            bulkActionMessageTimer = 2.0f;
        }
        f10PressedLast = f10Pressed;

        bool minusPressed = glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS ||
                            glfwGetKey(window, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS;
        if (minusPressed && !minusPressedLast)
        {
            audioManager.SetMasterVolume(audioManager.GetMasterVolume() - 0.1f);
            bulkActionMessage = audioManager.GetStatusLabel();
            bulkActionMessageTimer = 2.0f;
        }
        minusPressedLast = minusPressed;

        bool equalPressed = glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS ||
                            glfwGetKey(window, GLFW_KEY_KP_ADD) == GLFW_PRESS;
        if (equalPressed && !equalPressedLast)
        {
            audioManager.SetMasterVolume(audioManager.GetMasterVolume() + 0.1f);
            bulkActionMessage = audioManager.GetStatusLabel();
            bulkActionMessageTimer = 2.0f;
        }
        equalPressedLast = equalPressed;

        bool mPressed = glfwGetKey(window, GLFW_KEY_M) == GLFW_PRESS;
        if (appState.mouseCaptured && mPressed && !mPressedLast)
        {
            audioManager.ToggleAmbientMute();
            bulkActionMessage = audioManager.GetStatusLabel();
            bulkActionMessageTimer = 2.0f;
        }
        mPressedLast = mPressed;

        // Phase 8/9 rendering controls. All are edge-triggered and affect only
        // the renderer; interaction, collision and state machines are shared.
        bool f11Pressed = glfwGetKey(window, GLFW_KEY_F11) == GLFW_PRESS;
        if (f11Pressed && !f11PressedLast)
        {
            settings.pbrEnabled = !settings.pbrEnabled;
            bulkActionMessage = settings.pbrEnabled ? "PBR ON" : "PBR OFF - LEGACY";
            bulkActionMessageTimer = 2.0f;
        }
        f11PressedLast = f11Pressed;

        bool f12Pressed = glfwGetKey(window, GLFW_KEY_F12) == GLFW_PRESS;
        if (f12Pressed && !f12PressedLast)
        {
            settings.hdrEnabled = !settings.hdrEnabled;
            bulkActionMessage = (settings.hdrEnabled && hdrTargetValid) ? "HDR ON" : "HDR FALLBACK";
            bulkActionMessageTimer = 2.0f;
        }
        f12PressedLast = f12Pressed;

        bool bPressed = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
        if (appState.mouseCaptured && bPressed && !bPressedLast)
        {
            settings.bloomEnabled = !settings.bloomEnabled;
            bulkActionMessage = settings.bloomEnabled ? "BLOOM ON" : "BLOOM OFF";
            bulkActionMessageTimer = 2.0f;
        }
        bPressedLast = bPressed;

        bool tPressed = glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS;
        if (appState.mouseCaptured && tPressed && !tPressedLast)
        {
            int next = (static_cast<int>(settings.toneMapping) + 1) % 3;
            settings.toneMapping = static_cast<PostProcessor::ToneMapping>(next);
            bulkActionMessage = std::string("TONE MAPPING ") + toneMappingLabel(settings.toneMapping);
            bulkActionMessageTimer = 2.0f;
        }
        tPressedLast = tPressed;

        bool leftBracketPressed = glfwGetKey(window, GLFW_KEY_LEFT_BRACKET) == GLFW_PRESS;
        if (leftBracketPressed && !leftBracketPressedLast)
        {
            settings.exposure = std::max(0.1f, settings.exposure - 0.1f);
            bulkActionMessage = "EXPOSURE " + formatFloat(settings.exposure, 1);
            bulkActionMessageTimer = 2.0f;
        }
        leftBracketPressedLast = leftBracketPressed;

        bool rightBracketPressed = glfwGetKey(window, GLFW_KEY_RIGHT_BRACKET) == GLFW_PRESS;
        if (rightBracketPressed && !rightBracketPressedLast)
        {
            settings.exposure = std::min(5.0f, settings.exposure + 0.1f);
            bulkActionMessage = "EXPOSURE " + formatFloat(settings.exposure, 1);
            bulkActionMessageTimer = 2.0f;
        }
        rightBracketPressedLast = rightBracketPressed;

        if (bulkActionMessageTimer > 0.0f)
            bulkActionMessageTimer -= deltaTime;

        // ---- WASD movement (only while mouse is captured, i.e. not paused) ----
        if (appState.mouseCaptured)
        {
            bool w = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
            bool s = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
            bool a = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
            bool d = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
            bool running = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;
            playerRunning = running;

            glm::vec3 moveDir(0.0f);
            glm::vec3 flatFront = camera.GetFlatFront();
            glm::vec3 flatRight = camera.GetFlatRight();
            if (w) moveDir += flatFront;
            if (s) moveDir -= flatFront;
            if (d) moveDir += flatRight;
            if (a) moveDir -= flatRight;

            if (glm::length(moveDir) > 1e-5f)
            {
                moveDir = glm::normalize(moveDir);
                float speed = camera.MovementSpeed * (running ? camera.RunMultiplier : 1.0f);
                glm::vec3 desired = camera.GetPosition() + moveDir * speed * deltaTime;
                glm::vec3 resolved = classroom.ResolveCollision(camera.GetPosition(), desired, playerRadius);
                resolved.y = eyeHeight;
                camera.SetPosition(resolved);
            }
        }

        // Classroom::Update is the single update owner for every interactive
        // object it contains (light circuit, door, projector, computers).
        // InteractionSystem only raycasts - it does not update anything, so
        // objects are never advanced twice in the same frame.
        classroom.Update(deltaTime);

        RaycastHit hit = interactionSystem.Raycast(camera.GetPosition(), camera.GetFront(), interactRange);

        // Discard the hit if solid classroom geometry (a wall, a different
        // table, ...) blocks the line of sight before reaching it, so
        // interaction can never reach through walls or large objects.
        // Target-aware occlusion (Phase 4 correction, requirement 3): the
        // full hit distance is used with no margin/shrinkage at all, since
        // IsRayOccluded is told exactly which AABB is being targeted and
        // excludes only that specific workstation's own supporting desk -
        // every wall, window segment, and unrelated desk still blocks
        // normally, and a thin obstacle right next to a distant target is
        // no longer hidden inside a percentage-scaled gap.
        if (hit.hit)
        {
            glm::vec3 targetMin = hit.object->GetAABBMin();
            glm::vec3 targetMax = hit.object->GetAABBMax();

            // A light switch is a thin wall-mounted target and is already the
            // nearest interactable hit. Do not let the wall it is mounted on
            // invalidate that hit by a few millimetres. All other objects keep
            // the full target-aware occlusion test.
            const bool mountedLightSwitch = dynamic_cast<LightSwitch*>(hit.object) != nullptr;
            if (!mountedLightSwitch &&
                classroom.IsRayOccluded(camera.GetPosition(), camera.GetFront(), hit.distance, targetMin, targetMax))
            {
                hit.hit = false;
                hit.object = nullptr;
            }
        }

        bool ePressed = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
        if (appState.mouseCaptured && ePressed && !ePressedLast && hit.hit && hit.object)
        {
            bool accepted = hit.object->InteractWithResult();
            classroomAudio.OnInteraction(hit.object, hit.partIndex, accepted);

            if (accepted && dynamic_cast<LightSwitch*>(hit.object))
            {
                bulkActionMessage = std::string("CLASSROOM LIGHTS ") +
                                    (classroom.AreLightsOn() ? "ON" : "OFF");
                bulkActionMessageTimer = 2.0f;
            }
        }
        ePressedLast = ePressed;

        // Listener orientation follows the first-person camera every frame.
        // Footsteps use confirmed post-collision displacement, while all
        // state-machine one-shots are guarded by actual state transitions.
        classroomAudio.Update(deltaTime, previousListenerPosition, camera.GetPosition(),
                              camera.GetFront(), camera.GetUp(), playerRunning, true);

        glm::mat4 projection = glm::perspective(glm::radians(70.0f),
                                                 (float)windowWidth / (float)std::max(windowHeight, 1),
                                                 0.05f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();

        // Effective state combines user toggles with successfully-created resources.
        const bool useShadows = settings.shadowsEnabled && shadowMapValid;
        const bool useHDR = settings.hdrEnabled && hdrTargetValid;
        const bool useMSAA = settings.msaaEnabled && (useHDR || msaaTargetValid);

        // ---- Pass 1: directional-light shadow depth ----
        if (useShadows)
        {
            glViewport(0, 0, static_cast<GLsizei>(SHADOW_MAP_SIZE),
                       static_cast<GLsizei>(SHADOW_MAP_SIZE));
            glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
            glClear(GL_DEPTH_BUFFER_BIT);
            glEnable(GL_POLYGON_OFFSET_FILL);
            glPolygonOffset(1.5f, 2.0f);

            depthShader.use();
            depthShader.setBool("useInstancing", false);
            depthShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
            classroom.Draw(depthShader, false);

            glDisable(GL_POLYGON_OFFSET_FILL);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // ---- Pass 2: linear main scene into HDR, or validated legacy fallback ----
        if (useHDR)
            postProcessor.BeginScene(settings.msaaEnabled);
        else
            glBindFramebuffer(GL_FRAMEBUFFER, useMSAA ? msaaTarget.fbo : 0);

        glViewport(0, 0, windowWidth, windowHeight);
        glClearColor(0.006f, 0.007f, 0.012f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Shader& sceneShader = settings.pbrEnabled ? pbrShader : classroomShader;
        sceneShader.use();
        sceneShader.setBool("useInstancing", false);
        sceneShader.setMat4("projection", projection);
        sceneShader.setMat4("view", view);
        sceneShader.setVec3("viewPos", camera.GetPosition());
        uploadLights(sceneShader, classroom.GetLights(camera.GetPosition(), !settings.pbrEnabled));
        uploadSpotLights(sceneShader, classroom.GetSpotLights());

        sceneShader.setVec3("dirLightDirection", dirLightDirection);
        sceneShader.setVec3("dirLightColor", dirLightColor);
        const float ceilingLightFactor = classroom.GetCeilingLightIntensity();
        // Sunlight remains visible even when the classroom ceiling lights are off.
        // Ceiling fixtures only change the indoor ambient contribution.
        const float dirLightIntensity = 1.15f;
        sceneShader.setFloat("dirLightIntensity", dirLightIntensity);
        sceneShader.setFloat("roomAmbientFactor", 0.18f + 0.62f * ceilingLightFactor);
        sceneShader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
        sceneShader.setBool("shadowsEnabled", useShadows);
        // HDR targets stay linear. Gamma is applied exactly once by the final
        // composite. The direct fallback retains the original F3 comparison.
        sceneShader.setBool("gammaEnabled", useHDR ? false : settings.gammaEnabled);
        sceneShader.setFloat("bloomThreshold", settings.bloomThreshold);
        sceneShader.setFloat("objectAlpha", 1.0f);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadowDepthTexture);
        sceneShader.setInt("shadowMap", 1);
        classroom.Draw(sceneShader, true);

        if (useHDR)
        {
            postProcessor.EndScene(settings.msaaEnabled);
            postProcessor.Composite(settings.bloomEnabled, settings.bloomStrength,
                                    settings.exposure, settings.toneMapping,
                                    settings.gammaEnabled, 8);
        }
        else if (useMSAA)
        {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, msaaTarget.fbo);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
            glBlitFramebuffer(0, 0, windowWidth, windowHeight,
                              0, 0, windowWidth, windowHeight,
                              GL_COLOR_BUFFER_BIT, GL_LINEAR);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }

        // ---- HUD: crosshair, interaction prompt, pause label, and the
        // Phase 3 settings readout (requirement 12) - always drawn directly
        // to the default framebuffer, on top of the (possibly resolved) scene ----
        glViewport(0, 0, windowWidth, windowHeight);
        glDisable(GL_DEPTH_TEST);
        drawCrosshair(screenShader, uiVAO, uiVBO, windowWidth, windowHeight);
        if (hit.hit && hit.object && appState.mouseCaptured)
        {
            drawText(screenShader, uiVAO, uiVBO, hit.object->GetPrompt(),
                     windowWidth * 0.5f, windowHeight * 0.5f + 40.0f, 3.0f,
                     windowWidth, windowHeight, glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
        }
        if (!appState.mouseCaptured)
        {
            drawText(screenShader, uiVAO, uiVBO, "PAUSED",
                     windowWidth * 0.5f, windowHeight * 0.5f - 60.0f, 4.0f,
                     windowWidth, windowHeight, glm::vec4(1.0f, 0.9f, 0.4f, 1.0f));
        }
        drawText(screenShader, uiVAO, uiVBO,
                 buildSettingsLabel(useShadows, useMSAA, settings, useHDR),
                 windowWidth * 0.5f, 14.0f, 1.45f,
                 windowWidth, windowHeight, glm::vec4(0.8f, 0.9f, 1.0f, 0.88f));
        drawText(screenShader, uiVAO, uiVBO,
                 std::string("TONE ") + toneMappingLabel(settings.toneMapping) +
                 "   FPS " + formatFloat(displayedFPS, 0) +
                 "   MODELS " + (classroom.HasImportedDeskModel() &&
                                   classroom.HasImportedChairModel() &&
                                   classroom.HasImportedMonitorModel() ? "LOADED" : "FALLBACK"),
                 windowWidth * 0.5f, 34.0f, 1.55f,
                 windowWidth, windowHeight, glm::vec4(0.72f, 0.86f, 1.0f, 0.88f));
        drawText(screenShader, uiVAO, uiVBO, buildProjectorLabel(classroom.GetProjector()),
                 windowWidth * 0.5f, 54.0f, 1.7f,
                 windowWidth, windowHeight, glm::vec4(0.72f, 0.86f, 1.0f, 0.88f));
        drawText(screenShader, uiVAO, uiVBO,
                 std::string("LIGHTS ") + (classroom.AreLightsOn() ? "ON" : "OFF") +
                 "   F11 PBR   B BLOOM   T TONE   [ ] EXPOSURE",
                 windowWidth * 0.5f, 74.0f, 1.55f,
                 windowWidth, windowHeight, glm::vec4(0.85f, 0.88f, 0.74f, 0.88f));
        if (bulkActionMessageTimer > 0.0f && !bulkActionMessage.empty())
        {
            drawText(screenShader, uiVAO, uiVBO, bulkActionMessage,
                     windowWidth * 0.5f, 96.0f, 2.0f,
                     windowWidth, windowHeight, glm::vec4(0.6f, 1.0f, 0.7f, 0.9f));
        }
        glEnable(GL_DEPTH_TEST);

        updateWindowTitle(window, useShadows, useMSAA, settings, useHDR,
                          classroom.GetProjector(), audioManager, displayedFPS);
        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &uiVAO);
    glDeleteBuffers(1, &uiVBO);
    glDeleteFramebuffers(1, &shadowFBO);
    glDeleteTextures(1, &shadowDepthTexture);
    destroyMSAATarget(msaaTarget);
    } // Classroom, projector beam mesh, and shaders are destroyed here.

    glfwTerminate();
    return 0;
}
