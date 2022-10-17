//
// Created by yashr on 12/30/21.
//

#pragma once

#include "banan_device.h"
#include "banan_buffer.h"
#include "banan_image.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace Banan {
    class BananModel {
    public:

        struct RGB {
            uint8_t *data = nullptr;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t mipLevels = 1;
        };

        struct HDR {
            uint16_t *data = nullptr;
            uint32_t width = 0;
            uint32_t height = 0;
        };

        struct Vertex {
            glm::vec3 color;
            glm::vec3 normal;
            glm::vec3 tangent;
            glm::vec2 uv;

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
            static std::vector<VkVertexInputBindingDescription> getPositionOnlyBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getPositionOnlyAttributeDescriptions();
        };

        struct Builder {
            std::vector<glm::vec3> positions{};
            std::vector<Vertex> misc{};

            std::vector<uint32_t> indices{};
            RGB texture{};
            HDR normals{};
            RGB heights{};

            void loadModel(const std::string &filepath);
            void loadTexture(const std::string &filepath);
            void loadNormals(const std::string &filepath);
            void loadHeightMap(const std::string &filepath);

            void loadHDR(const std::string &filepath, HDR &target);
            void loadRGB(const std::string &filepath, RGB &target);
        };

        BananModel(BananDevice &device, const Builder &builder);
        ~BananModel();

        BananModel(const BananModel &) = delete;
        BananModel &operator=(const BananModel &) = delete;

        static std::unique_ptr<BananModel> createModelFromFile(BananDevice &device, const std::string &filepath);

        void bindPosition(VkCommandBuffer commandBuffer);
        void bindAll(VkCommandBuffer commandBuffer);
        void draw(VkCommandBuffer commandBuffer);

        bool isTextureLoaded();
        bool isNormalsLoaded();
        bool isHeightmapLoaded();

        VkDescriptorImageInfo getDescriptorTextureImageInfo();
        VkDescriptorImageInfo getDescriptorNormalImageInfo();
        VkDescriptorImageInfo getDescriptorHeightMapInfo();

    private:
        void createVertexBuffers(const std::vector<glm::vec3> &vertices, const std::vector<Vertex> &misc);
        void createIndexBuffers(const std::vector<uint32_t> &indices);
        void createTextureImage(const RGB &image);
        void createNormalImage(const HDR &image);
        void createHeightmap(const RGB &image);

        bool hasIndexBuffer;
        bool hasTexture;
        bool hasNormal;
        bool hasHeightmap;

        BananDevice &bananDevice;

        std::unique_ptr<BananBuffer> vertexBuffer;
        std::unique_ptr<BananBuffer> miscBuffer;
        uint32_t vertexCount;

        std::unique_ptr<BananBuffer> indexBuffer;
        uint32_t indexCount;

        std::unique_ptr<BananImage> textureImage;
        uint32_t texturepixelCount;

        std::unique_ptr<BananImage> normalImage;
        uint32_t normalpixelCount;

        std::unique_ptr<BananImage> heightMap;
        uint32_t heightMapPixelCount;
    };
}