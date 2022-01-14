//
// Created by yashr on 12/4/21.
//
#pragma once

#include "../banan_camera.h"
#include "../banan_pipeline.h"
#include "../banan_device.h"
#include "../banan_game_object.h"

#include <memory>
#include <vector>

namespace Banan{
    class SimpleRenderSystem {
        public:
            SimpleRenderSystem(const SimpleRenderSystem &) = delete;
            SimpleRenderSystem &operator=(const SimpleRenderSystem &) = delete;

            SimpleRenderSystem(BananDevice &device, VkRenderPass renderPass);
            ~SimpleRenderSystem();

            void renderGameObjects(VkCommandBuffer commandBuffer, std::vector<BananGameObject> &gameObjects, const BananCamera &camera);

        private:
            void createPipelineLayout();
            void createPipeline(VkRenderPass renderPass);

            BananDevice &bananDevice;
            std::unique_ptr<BananPipeline> bananPipeline;
            VkPipelineLayout pipelineLayout;
    };
}
