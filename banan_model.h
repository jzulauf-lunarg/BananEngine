//
// Created by yashr on 12/30/21.
//

#pragma once

#include "banan_device.h"
#include "banan_buffer.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace Banan {
    class BananModel {
        public:

            struct Vertex {
                glm::vec3 position;
            };

            struct LessImportantStuff {
                glm::vec3 color;
                glm::vec3 normal;
                glm::vec2 uv;
            };

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();

            struct Builder {
                std::vector<Vertex> vertices{};
                std::vector<LessImportantStuff> lessImportantStuff{};
                std::vector<uint32_t> indices{};

                void loadModel(const std::string &filepath);
            };

            BananModel(BananDevice &device, const Builder &builder);
            ~BananModel();

            BananModel(const BananModel &) = delete;
            BananModel &operator=(const BananModel &) = delete;

            static std::unique_ptr<BananModel> createModelFromFile(BananDevice &device, const std::string &filepath);

            void bind(VkCommandBuffer commandBuffer);
            void draw(VkCommandBuffer commandBuffer);

        private:
            void createVertexBuffers(const std::vector<Vertex> &vertices, const std::vector<LessImportantStuff> &lessImportantStuff);
            void createIndexBuffers(const std::vector<uint32_t> &indices);

            bool hasIndexBuffer;
            BananDevice &bananDevice;

            std::unique_ptr<BananBuffer> vertexBuffer;
            std::unique_ptr<BananBuffer> otherBufferForLessImportantStuff;
            uint32_t vertexCount;

            std::unique_ptr<BananBuffer> indexBuffer;
            uint32_t indexCount;
    };
}
