#include "Renderer.h"

#include <game-activity/native_app_glue/android_native_app_glue.h>
#include <GLES3/gl3.h>
#include <memory>
#include <vector>
#include <android/imagedecoder.h>
#include <cmath>

#include "AndroidOut.h"
#include "Shader.h"
#include "Utility.h"
#include "TextureAsset.h"
#include "GameLogic.h"

#define CORNFLOWER_BLUE 100 / 255.f, 149 / 255.f, 237 / 255.f, 1

static const char *vertex = R"vertex(#version 300 es
in vec3 inPosition;
in vec2 inUV;
out vec2 fragUV;
uniform mat4 uProjection;
uniform vec3 uOffset;
uniform vec3 uScale;
void main() {
    fragUV = inUV;
    gl_Position = uProjection * vec4((inPosition * uScale) + uOffset, 1.0);
}
)vertex";

static const char *fragment = R"fragment(#version 300 es
precision mediump float;
in vec2 fragUV;
uniform sampler2D uTexture;
uniform vec3 uTint;
uniform float uFlash;
uniform bool uUseTexture;
out vec4 outColor;
void main() {
    vec4 texColor = uUseTexture ? texture(uTexture, fragUV) : vec4(1.0);
    vec3 flashColor = vec3(1.0, 1.0, 1.0);
    outColor = vec4(mix(texColor.rgb * uTint, flashColor, uFlash), texColor.a);
}
)fragment";

static constexpr float kProjectionHalfHeight = 2.f;
static constexpr float kProjectionNearPlane = -1.f;
static constexpr float kProjectionFarPlane = 1.f;

Renderer::Renderer(android_app *pApp, dnd::Game& game) :
        app_(pApp),
        game_(game),
        display_(EGL_NO_DISPLAY),
        surface_(EGL_NO_SURFACE),
        context_(EGL_NO_CONTEXT),
        width_(0),
        height_(0),
        totalTime_(0.0f),
        shaderNeedsNewProjectionMatrix_(true) {
    initRenderer();
}

Renderer::~Renderer() {
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
    }
}

void Renderer::updateAnimations() {
    while (game_.hasVisualEvent()) {
        dnd::VisualEvent ev = game_.popVisualEvent();
        bool isPlayerSide = (ev.type == dnd::VisualEventType::PLAYER_ATTACK || ev.type == dnd::VisualEventType::PLAYER_DAMAGE);
        triggerAnim(isPlayerSide, ev.targetIndex, ev.type);
    }

    auto updateMap = [](std::map<int, AnimationState>& anims) {
        for (auto it = anims.begin(); it != anims.end(); ) {
            if (it->second.framesRemaining > 0) {
                it->second.framesRemaining--;
                if (it->second.framesRemaining == 0) {
                    it->second.xOffset = 0;
                    it->second.flashIntensity = 0;
                } else {
                    it->second.flashIntensity *= 0.85f;
                    if (it->second.flashIntensity > 0) it->second.xOffset = (rand() % 10 - 5) * 0.015f;
                }
                ++it;
            } else {
                it = anims.erase(it);
            }
        }
    };
    updateMap(playerAnims_);
    updateMap(enemyAnims_);
}

void Renderer::triggerAnim(bool isPlayer, int index, dnd::VisualEventType type) {
    AnimationState& state = isPlayer ? playerAnims_[index] : enemyAnims_[index];
    if (type == dnd::VisualEventType::PLAYER_ATTACK || type == dnd::VisualEventType::ENEMY_ATTACK) {
        state.framesRemaining = 12;
        state.xOffset = isPlayer ? 0.4f : -0.4f;
        state.flashIntensity = 0;
    } else {
        state.framesRemaining = 15;
        state.flashIntensity = 1.0f;
    }
}

void Renderer::render() {
    updateRenderArea();
    updateAnimations();
    totalTime_ += 0.016f;

    if (shaderNeedsNewProjectionMatrix_) {
        float projectionMatrix[16] = {0};
        Utility::buildOrthographicMatrix(
                projectionMatrix,
                kProjectionHalfHeight,
                float(width_) / height_,
                kProjectionNearPlane,
                kProjectionFarPlane);
        shader_->setProjectionMatrix(projectionMatrix);
        shaderNeedsNewProjectionMatrix_ = false;
    }

    glClear(GL_COLOR_BUFFER_BIT);

    if (!models_.empty()) {
        auto program = shader_->getProgram();
        auto uOffset = glGetUniformLocation(program, "uOffset");
        auto uTint = glGetUniformLocation(program, "uTint");
        auto uScale = glGetUniformLocation(program, "uScale");
        auto uFlash = glGetUniformLocation(program, "uFlash");
        auto uUseTexture = glGetUniformLocation(program, "uUseTexture");

        auto drawHealthBar = [&](float x, float y, float healthPerc) {
            glUniform1i(uUseTexture, 0);
            glUniform3f(uScale, 0.4f, 0.05f, 1.0f);

            glUniform3f(uOffset, x, y + 0.5f, 0.0f);
            glUniform3f(uTint, 0.2f, 0.0f, 0.0f);
            shader_->drawModel(models_[0]);

            glUniform3f(uScale, 0.4f * healthPerc, 0.05f, 1.0f);
            glUniform3f(uOffset, x - (0.4f * (1.0f - healthPerc)), y + 0.5f, 0.0f);
            glUniform3f(uTint, 0.2f, 1.0f, 0.2f);
            shader_->drawModel(models_[0]);

            glUniform1i(uUseTexture, 1);
            glUniform3f(uScale, 1.0f, 1.0f, 1.0f);
        };

        dnd::Character* activeActor = game_.getCurrentActor();

        const auto& players = game_.getPlayers();
        float pyStart = (players.size() - 1) * 0.6f;
        for (size_t i = 0; i < players.size(); ++i) {
            float animX = playerAnims_[(int)i].xOffset;
            float flash = playerAnims_[(int)i].flashIntensity;
            float xPos = -1.2f + animX;
            float yPos = pyStart - (i * 1.2f);

            float pulse = 1.0f;
            if (activeActor == players[i].get()) {
                pulse = 1.15f + 0.15f * sinf(totalTime_ * 10.0f);
            }

            glUniform3f(uOffset, xPos, yPos, 0.0f);
            glUniform3f(uScale, pulse, pulse, 1.0f);

            switch(players[i]->characterClass) {
                case dnd::CharacterClass::FIGHTER: glUniform3f(uTint, 0.4f, 0.6f, 1.0f); break;
                case dnd::CharacterClass::WIZARD:  glUniform3f(uTint, 0.7f, 0.3f, 1.0f); break;
                case dnd::CharacterClass::ROGUE:   glUniform3f(uTint, 1.0f, 0.9f, 0.3f); break;
                case dnd::CharacterClass::CLERIC:  glUniform3f(uTint, 1.0f, 1.0f, 1.0f); break;
            }
            glUniform1f(uFlash, flash);

            glUniform1i(uUseTexture, models_[0].hasTexture() ? 1 : 0);
            shader_->drawModel(models_[0]);

            drawHealthBar(xPos, yPos, (float)players[i]->currentHp / (float)std::max(1, players[i]->maxHp));
        }

        const auto& enemies = game_.getEnemies();
        float eyStart = (enemies.size() - 1) * 0.6f;
        for (size_t i = 0; i < enemies.size(); ++i) {
            float animX = enemyAnims_[(int)i].xOffset;
            float flash = enemyAnims_[(int)i].flashIntensity;
            float xPos = 1.2f + animX;
            float yPos = eyStart - (i * 1.2f);

            float pulse = 1.0f;
            if (activeActor == enemies[i].get()) {
                pulse = 1.15f + 0.15f * sinf(totalTime_ * 10.0f);
            }

            glUniform3f(uOffset, xPos, yPos, 0.0f);
            glUniform3f(uScale, pulse, pulse, 1.0f);
            glUniform3f(uTint, 1.0f, 0.2f, 0.2f);
            glUniform1f(uFlash, flash);

            glUniform1i(uUseTexture, models_[0].hasTexture() ? 1 : 0);
            shader_->drawModel(models_[0]);

            drawHealthBar(xPos, yPos, (float)enemies[i]->currentHp / (float)std::max(1, enemies[i]->maxHp));
        }
        glUniform3f(uScale, 1.0f, 1.0f, 1.0f);
    }
}

void Renderer::present() {
    if (display_ != EGL_NO_DISPLAY && surface_ != EGL_NO_SURFACE) {
        eglSwapBuffers(display_, surface_);
    }
}

void Renderer::initRenderer() {
    constexpr EGLint attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8,
            EGL_DEPTH_SIZE, 24, EGL_NONE
    };

    auto display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    EGLint numConfigs;
    eglChooseConfig(display, attribs, nullptr, 0, &numConfigs);

    std::unique_ptr<EGLConfig[]> supportedConfigs(new EGLConfig[numConfigs]);
    eglChooseConfig(display, attribs, supportedConfigs.get(), numConfigs, &numConfigs);

    auto config = *std::find_if(
            supportedConfigs.get(),
            supportedConfigs.get() + numConfigs,
            [&display](const EGLConfig &config) {
                EGLint red, green, blue, depth;
                if (eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red)
                    && eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &green)
                    && eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blue)
                    && eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &depth)) {
                    return red == 8 && green == 8 && blue == 8 && depth == 24;
                }
                return false;
            });

    EGLSurface surface = eglCreateWindowSurface(display, config, app_->window, nullptr);
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttribs);

    auto madeCurrent = eglMakeCurrent(display, surface, surface, context);
    assert(madeCurrent);

    display_ = display;
    surface_ = surface;
    context_ = context;
    width_ = -1;
    height_ = -1;

    shader_ = std::unique_ptr<Shader>(
            Shader::loadShader(vertex, fragment, "inPosition", "inUV", "uProjection"));
    assert(shader_);
    shader_->activate();

    glClearColor(CORNFLOWER_BLUE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    createModels();
}

void Renderer::updateRenderArea() {
    EGLint width, height;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height);

    if (width != width_ || height != height_) {
        width_ = width;
        height_ = height;
        glViewport(0, 0, width, height);
        shaderNeedsNewProjectionMatrix_ = true;
    }
}

void Renderer::createModels() {
    std::vector<Vertex> vertices = {
            Vertex(Vector3{0.35f, 0.35f, 0}, Vector2{0, 0}),
            Vertex(Vector3{-0.35f, 0.35f, 0}, Vector2{1, 0}),
            Vertex(Vector3{-0.35f, -0.35f, 0}, Vector2{1, 1}),
            Vertex(Vector3{0.35f, -0.35f, 0}, Vector2{0, 1})
    };
    std::vector<Index> indices = {0, 1, 2, 0, 2, 3};

    auto assetManager = app_->activity->assetManager;
    auto spAndroidRobotTexture = TextureAsset::loadAsset(assetManager, "android_robot.png");
    models_.emplace_back(vertices, indices, spAndroidRobotTexture);
}

void Renderer::handleInput() {
    auto *inputBuffer = android_app_swap_input_buffers(app_);
    if (!inputBuffer) return;
    android_app_clear_motion_events(inputBuffer);
    android_app_clear_key_events(inputBuffer);
}
