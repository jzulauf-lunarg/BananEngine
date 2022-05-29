//
// Created by yashr on 12/4/21.
//

#include "BananEngineTest.h"
#include "Systems/PointLightSystem.h"
#include "Systems/SimpleRenderSystem.h"
#include "KeyboardMovementController.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <chrono>

#include "../banan_logger.h"

namespace Banan{

    BananEngineTest::BananEngineTest() {

        loadGameObjects();

        globalPool = BananDescriptorPool::Builder(bananDevice)
                .setMaxSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT * gameObjects.size())
                .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT)
                .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, BananSwapChain::MAX_FRAMES_IN_FLIGHT)
                .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, BananSwapChain::MAX_FRAMES_IN_FLIGHT * gameObjects.size())
                .build();
    }

    BananEngineTest::~BananEngineTest() = default;

    void BananEngineTest::run() {

        std::vector<std::unique_ptr<BananBuffer>> uboBuffers(BananSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (auto & uboBuffer : uboBuffers) {
            uboBuffer = std::make_unique<BananBuffer>(bananDevice, sizeof(GlobalUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            uboBuffer->map();
        }

        auto globalSetLayout = BananDescriptorSetLayout::Builder(bananDevice)
                .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS, 1)
                .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, gameObjects.size())
                .build();

        std::vector<VkDescriptorSet> globalDescriptorSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < globalDescriptorSets.size(); i++) {

            BananDescriptorWriter writer = BananDescriptorWriter(*globalSetLayout, *globalPool);
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            writer.writeBuffer(0, &bufferInfo);

            std::unordered_map<uint32_t, VkDescriptorImageInfo> gameObjectsTextureInfo{};
            for (auto &kv : gameObjects)
            {
                if (kv.second.model != nullptr) {
                    if (kv.second.model->isTextureLoaded()) {
                        gameObjectsTextureInfo.emplace(kv.first, kv.second.model->getDescriptorImageInfo());
                    }
                }
            }

            writer.writeImages(1, gameObjectsTextureInfo);
            writer.build(globalDescriptorSets[i], gameObjects.size() - 1);
        }

        SimpleRenderSystem renderSystem{bananDevice, bananRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
        PointLightSystem pointLightSystem{bananDevice, bananRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
        BananCamera camera{};

        auto viewerObject = BananGameObject::createGameObject();
        KeyboardMovementController cameraController{};

        auto currentTime = std::chrono::high_resolution_clock::now();

        while(!bananWindow.windowShouldClose())
        {
            glfwPollEvents();

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            cameraController.moveInPlaneXZ(bananWindow.getGLFWwindow(), frameTime, viewerObject);
            camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

            float aspect = bananRenderer.getAspectRatio();
            camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);
            camera.setPerspectiveProjection(glm::radians(90.f), aspect, 0.1f, 10.f);

            if (auto commandBuffer = bananRenderer.beginFrame()) {
                int frameIndex = bananRenderer.getFrameIndex();
                BananFrameInfo frameInfo{frameIndex, frameTime, commandBuffer, camera, globalDescriptorSets[frameIndex], gameObjects};

                GlobalUbo ubo{};
                ubo.projection = camera.getProjection();
                ubo.view = camera.getView();
                ubo.inverseView = camera.getInverseView();

                pointLightSystem.update(frameInfo, ubo);
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                bananRenderer.beginSwapChainRenderPass(commandBuffer);
                renderSystem.render(frameInfo);
                pointLightSystem.render(frameInfo);
                bananRenderer.endSwapChainRenderPass(commandBuffer);
                bananRenderer.endFrame();
            }
        }

        vkDeviceWaitIdle(bananDevice.device());
    }

    void BananEngineTest::loadGameObjects() {
        BananModel::Builder vaseBuilder{};
        vaseBuilder.loadModel("banan_assets/ceramic_vase_01_4k.blend");
        vaseBuilder.loadTexture("banan_assets/textures/ceramic_vase_01_diff_4k.jpg");

        std::shared_ptr<BananModel> vaseModel = std::make_shared<BananModel>(bananDevice, vaseBuilder);
        auto vase = BananGameObject::createGameObject();
        vase.model = vaseModel;
        vase.transform.translation = {0.f, 1.f, 0.f};
        vase.transform.rotation = {glm::pi<float>() / 2.0f, 0.f, 0.0f};
        vase.transform.scale = {3.f, 3.f, 3.f};
        gameObjects.emplace(vase.getId(), std::move(vase));

        BananModel::Builder floorBuilder{};
        floorBuilder.loadModel("banan_assets/quad.obj");
        floorBuilder.loadTexture("banan_assets/textures/pepe.jpg");

        std::shared_ptr<BananModel> floorModel = std::make_shared<BananModel>(bananDevice, floorBuilder);
        auto floor = BananGameObject::createGameObject();
        floor.model = floorModel;
        floor.transform.translation = {0.f, .5f, 0.f};
        floor.transform.rotation = {0.f, 0.f, 0.f};
        floor.transform.scale = {3.f, 1.f, 3.f};
        gameObjects.emplace(floor.getId(), std::move(floor));

        std::vector<glm::vec3> lightColors{
                {1.f, .1f, .1f},
                {.1f, .1f, 1.f},
                {.1f, 1.f, .1f},
                {1.f, 1.f, .1f},
                {.1f, 1.f, 1.f},
                {1.f, 1.f, 1.f}
        };

        for (int i = 0; i < lightColors.size(); i++) {
            auto pointLight = BananGameObject::makePointLight(0.5f);
            pointLight.color = lightColors[i];
            auto rotateLight = glm::rotate(glm::mat4(1.f), (static_cast<float>(i) * glm::two_pi<float>()) / static_cast<float>(lightColors.size()), {0.f, -1.f, 0.f});
            pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f));
            gameObjects.emplace(pointLight.getId(), std::move(pointLight));
        }
    }

    std::shared_ptr<BananLogger> BananEngineTest::getLogger() {
        return bananLogger;
    }
}

