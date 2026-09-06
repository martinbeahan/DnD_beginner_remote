#ifndef ANDROIDGLINVESTIGATIONS_RENDERER_H
#define ANDROIDGLINVESTIGATIONS_RENDERER_H

#include <EGL/egl.h>
#include <memory>
#include <vector>
#include <map>

#include "Model.h"
#include "Shader.h"
#include "Game.h"

struct android_app;

struct AnimationState {
    float xOffset = 0.0f;
    float yOffset = 0.0f;
    float scaleBoost = 0.0f;
    float flashIntensity = 0.0f;
    int framesRemaining = 0;
    int framesTotal = 0;
};

class Renderer {
public:
    Renderer(android_app *pApp, dnd::Game& game);
    virtual ~Renderer();

    void handleInput();
    void render();
    void present();

    dnd::Game& getGame() { return game_; }

private:
    void initRenderer();
    void updateRenderArea();
    void createModels();

    android_app *app_;
    dnd::Game& game_; // Reference to the persistent global game

    EGLDisplay display_;
    EGLSurface surface_;
    EGLContext context_;
    EGLint width_;
    EGLint height_;
    float totalTime_;

    bool shaderNeedsNewProjectionMatrix_;

    std::unique_ptr<Shader> shader_;
    std::vector<Model> models_;

    std::map<int, AnimationState> playerAnims_;
    std::map<int, AnimationState> enemyAnims_;

    void updateAnimations();
    void triggerAnim(bool isPlayer, int index, dnd::VisualEventType type);
};

#endif //ANDROIDGLINVESTIGATIONS_RENDERER_H
