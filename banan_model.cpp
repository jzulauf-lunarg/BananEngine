//
// Created by yashr on 12/30/21.
//

#include "banan_model.h"
#include "banan_logger.h"

#include <cassert>
#include <cstring>
#include <iterator>
#include <thread>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <stb_image.h>

#include <openexr.h>

#include <ImathBox.h>
#include <ImfRgbaFile.h>
#include <ImfArray.h>

using namespace Imf;
using namespace Imath;

namespace Banan {
    BananModel::BananModel(BananDevice &device, const Builder &builder) : bananDevice{device} {
        createTextureImage(builder.texture);
        createNormalImage(builder.normals);
        createHeightmap(builder.heights);
        createVertexBuffers(builder.positions, builder.misc);
        createIndexBuffers(builder.indices);
    }

    BananModel::~BananModel() {
    }

    void BananModel::bindPosition(VkCommandBuffer commandBuffer) {
        VkBuffer buffers[] = {vertexBuffer->getBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

        if (hasIndexBuffer) {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        }
    }

    void BananModel::bindAll(VkCommandBuffer commandBuffer) {
        VkBuffer buffers[] = {vertexBuffer->getBuffer(), miscBuffer->getBuffer()};
        VkDeviceSize offsets[] = {0, 0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, buffers, offsets);

        if (hasIndexBuffer) {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        }
    }

    void BananModel::draw(VkCommandBuffer commandBuffer) {

        if (hasIndexBuffer) {
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        } else {
            vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
        }
    }

    void BananModel::createVertexBuffers(const std::vector<glm::vec3> &vertices, const std::vector<Vertex> &misc) {
        vertexCount = static_cast<uint32_t>(vertices.size());
        assert(vertexCount >= 3 && "Vertex count must be atleast 3");
        assert(vertexCount == misc.size() && "Vertex count must be same as misc count");
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
        uint32_t vertexSize = sizeof(vertices[0]);

        BananBuffer stagingBuffer{bananDevice, vertexSize, vertexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void *)vertices.data());

        vertexBuffer = std::make_unique<BananBuffer>(bananDevice, vertexSize, vertexCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        bananDevice.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer->getBuffer(), bufferSize);

        bufferSize = sizeof(misc[0]) * vertexCount;
        uint32_t miscSize = sizeof(misc[0]);

        BananBuffer miscStagingBuffer{bananDevice, miscSize, vertexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
        miscStagingBuffer.map();
        miscStagingBuffer.writeToBuffer((void *)misc.data());

        miscBuffer = std::make_unique<BananBuffer>(bananDevice, miscSize, vertexCount, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        bananDevice.copyBuffer(miscStagingBuffer.getBuffer(), miscBuffer->getBuffer(), bufferSize);
    }

    void BananModel::createIndexBuffers(const std::vector<uint32_t> &indices) {
        indexCount = static_cast<uint32_t>(indices.size());
        hasIndexBuffer = indexCount > 0;

        if (!hasIndexBuffer) {
            return;
        }

        VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
        uint32_t indexSize = sizeof(indices[0]);

        BananBuffer stagingBuffer{bananDevice, indexSize, indexCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void *)indices.data());

        indexBuffer = std::make_unique<BananBuffer>(bananDevice, indexSize, indexCount, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        bananDevice.copyBuffer(stagingBuffer.getBuffer(), indexBuffer->getBuffer(), bufferSize);
    }

    std::unique_ptr<BananModel> BananModel::createModelFromFile(BananDevice &device, const std::string &filepath) {
        Builder builder{};
        builder.loadModel(filepath);
        std::cout << "Model Vertex Count: " + std::to_string(builder.misc.size());
        return std::make_unique<BananModel>(device, builder);
    }

    void BananModel::createTextureImage(const Texture &image) {
        if (image.width > 0 && image.height > 0) {
            texturepixelCount = image.width * image.height;
            assert(texturepixelCount >= 0 && "Failed to load image: 0 pixels");
            uint32_t pixelSize = image.stride / 2; // stride is in bits, pixel size should be in bytes

            VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
            if (image.stride == 16) format = VK_FORMAT_R16G16B16A16_SFLOAT;

            BananBuffer stagingBuffer{bananDevice, pixelSize, texturepixelCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
            stagingBuffer.map();
            stagingBuffer.writeToBuffer((void *)image.data);

            VkCommandBuffer commandBuffer = bananDevice.beginSingleTimeCommands();
            textureImage = std::make_unique<BananImage>(bananDevice, image.width, image.height, image.mipLevels, format, VK_IMAGE_TILING_OPTIMAL, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            textureImage->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            bananDevice.endSingleTimeCommands(commandBuffer);
            bananDevice.copyBufferToImage(stagingBuffer.getBuffer(), textureImage->getImageHandle(), image.width, image.height, 1);
            textureImage->generateMipMaps(image.mipLevels);

            hasTexture = true;
        } else {
            hasTexture = false;
        }

        if (image.data != nullptr) {
            free(image.data);
        }
    }

    void BananModel::createNormalImage(const Texture &image) {
        if (image.height > 0 && image.width > 0) {
            normalpixelCount = image.height * image.width;
            assert(normalpixelCount >= 0 && "Failed to load image: 0 pixels");
            uint32_t pixelSize = image.stride / 2; // stride is in bits, pixel size should be in bytes

            VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
            if (image.stride == 16) format = VK_FORMAT_R16G16B16A16_SFLOAT;

            BananBuffer stagingBuffer{bananDevice, pixelSize, normalpixelCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
            stagingBuffer.map();
            stagingBuffer.writeToBuffer((void *)image.data);

            VkCommandBuffer commandBuffer = bananDevice.beginSingleTimeCommands();
            normalImage = std::make_unique<BananImage>(bananDevice, image.width, image.height, image.mipLevels, format, VK_IMAGE_TILING_OPTIMAL, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            normalImage->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            bananDevice.endSingleTimeCommands(commandBuffer);
            bananDevice.copyBufferToImage(stagingBuffer.getBuffer(), normalImage->getImageHandle(), image.width, image.height, 1);
            bananDevice.generateMipMaps(normalImage->getImageHandle(), image.width, image.height, image.mipLevels);

            hasNormal = true;
        } else {
            hasNormal = false;
        }

        if (image.data != nullptr) {
            free(image.data);
        }
    }

    void BananModel::createHeightmap(const Texture &image) {
        if (image.height > 0 && image.width > 0) {
            heightMapPixelCount = image.height * image.width;
            assert(heightMapPixelCount >= 0 && "Failed to load image: 0 pixels");
            uint32_t pixelSize = image.stride / 2; // stride is in bits, pixel size should be in bytes

            VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
            if (image.stride == 16) format = VK_FORMAT_R16G16B16A16_SFLOAT;

            BananBuffer stagingBuffer{bananDevice, pixelSize, heightMapPixelCount, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
            stagingBuffer.map();
            stagingBuffer.writeToBuffer((void *)image.data);

            VkCommandBuffer commandBuffer = bananDevice.beginSingleTimeCommands();
            heightMap = std::make_unique<BananImage>(bananDevice, image.width, image.height, image.mipLevels, format, VK_IMAGE_TILING_OPTIMAL, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            heightMap->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            bananDevice.endSingleTimeCommands(commandBuffer);
            bananDevice.copyBufferToImage(stagingBuffer.getBuffer(), heightMap->getImageHandle(), image.width, image.height, 1);
            bananDevice.generateMipMaps(heightMap->getImageHandle(), image.width, image.height, image.mipLevels);

            hasHeightmap = true;
        } else {
            hasHeightmap = false;
        }

        if (image.data != nullptr) {
            free(image.data);
        }
    }

    bool BananModel::isTextureLoaded() {
        return hasTexture && textureImage != nullptr;
    }

    bool BananModel::isNormalsLoaded() {
        return hasNormal && normalImage != nullptr;
    }

    bool BananModel::isHeightmapLoaded() {
        return hasHeightmap && normalImage != nullptr;
    }

    VkDescriptorImageInfo BananModel::getDescriptorTextureImageInfo() {
        return textureImage->descriptorInfo();
    }

    VkDescriptorImageInfo BananModel::getDescriptorNormalImageInfo() {
        return normalImage->descriptorInfo();
    }

    VkDescriptorImageInfo BananModel::getDescriptorHeightMapInfo() {
        return heightMap->descriptorInfo();
    }

    std::vector<VkVertexInputBindingDescription> BananModel::Vertex::getBindingDescriptions() {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(2);
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(glm::vec3);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        bindingDescriptions[1].binding = 1;
        bindingDescriptions[1].stride = sizeof(Vertex);
        bindingDescriptions[1].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescriptions;
    }

    std::vector<VkVertexInputAttributeDescription> BananModel::Vertex::getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

        attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0});
        attributeDescriptions.push_back({1, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
        attributeDescriptions.push_back({2, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
        attributeDescriptions.push_back({3, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, tangent)});
        attributeDescriptions.push_back({4, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});

        return attributeDescriptions;
    }

    std::vector<VkVertexInputBindingDescription> BananModel::Vertex::getPositionOnlyBindingDescriptions() {
        std::vector<VkVertexInputBindingDescription> bindingDescriptions(1);
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = sizeof(glm::vec3);
        bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescriptions;
    }

    std::vector<VkVertexInputAttributeDescription> BananModel::Vertex::getPositionOnlyAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0});
        return attributeDescriptions;
    }

    void BananModel::Builder::loadModel(const std::string &filepath) {
        Assimp::Importer importer;
        const aiScene *scene = importer.ReadFile(filepath,
                                                 aiProcess_Triangulate |
                                                 aiProcess_GenSmoothNormals |
                                                 aiProcess_FlipUVs |
                                                 aiProcess_JoinIdenticalVertices |
                                                 aiProcess_GenUVCoords |
                                                 aiProcess_CalcTangentSpace |
                                                 aiProcess_MakeLeftHanded);

        positions.clear();
        misc.clear();
        indices.clear();

        if (scene) {
            for (uint32_t i = 0; i < scene->mNumMeshes; i++) {
                const aiMesh *mesh = scene->mMeshes[i];

                positions.reserve(positions.size() + mesh->mNumVertices);
                misc.reserve(positions.size() + mesh->mNumVertices);
                indices.reserve(positions.size() + mesh->mNumFaces * 3);

                for (uint32_t j = 0; j < mesh->mNumVertices; j++) {
                    Vertex v{};

                    v.color =  mesh->HasVertexColors(j) ? glm::vec3{mesh->mColors[j]->r, mesh->mColors[j]->g, mesh->mColors[j]->b} : glm::vec3{1.0f, 1.0f, 1.0f};
                    v.normal = glm::vec3{mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z};
                    v.tangent = glm::vec3{mesh->mTangents[j].x, mesh->mTangents[j].y, mesh->mTangents[j].z};
                    v.uv = glm::vec2{mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y};

                    misc.push_back(v);
                    positions.emplace_back( mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z);
                }

                for (uint32_t k = 0; k < mesh->mNumFaces; k++) {
                    indices.push_back(mesh->mFaces[k].mIndices[0]);
                    indices.push_back(mesh->mFaces[k].mIndices[1]);
                    indices.push_back(mesh->mFaces[k].mIndices[2]);
                }
            }

            float max_uv_x = 0.0;
            float min_uv_x = 0.0;
            float max_uv_y = 0.0;
            float min_uv_y = 0.0;

            for (Vertex v : misc) {
                if (v.uv.x > max_uv_x) {
                    max_uv_x = v.uv.x;
                }

                if (v.uv.y > max_uv_y) {
                    max_uv_y = v.uv.y;
                }

                if (v.uv.x < min_uv_x) {
                    min_uv_x = v.uv.x;
                }

                if (v.uv.y < min_uv_y) {
                    min_uv_y = v.uv.y;
                }
            }

            if (min_uv_x < 0.0f || min_uv_y < 0.0f || max_uv_x > 1.0f || max_uv_y > 1.0f) {
                if (min_uv_x < 0.0f) {
                    for (Vertex &v : misc) {
                        v.uv.x += min_uv_x * -1.0f;
                    }

                    max_uv_x += min_uv_x * -1.0f;
                    min_uv_x = 0.0f;
                }

                if (min_uv_y < 0.0f) {
                    for (Vertex &v : misc) {
                        v.uv.y += min_uv_y * -1.0f;
                    }

                    max_uv_y += min_uv_y * -1.0f;
                    min_uv_y = 0.0f;
                }

                for (Vertex &v : misc) {
                    v.uv.x /= max_uv_x;
                    v.uv.y /= max_uv_y;
                }
            }

            if (scene->HasTextures()) {
                if (scene->mNumTextures == 1) {
                    const aiTexture *sceneTexture = scene->mTextures[0];

                    int width = 0;
                    int height = 0;
                    int texChannels = 0;

                    if (sceneTexture->mHeight == 0)
                    {
                        texture.data = stbi_load_from_memory(reinterpret_cast<uint8_t *>(sceneTexture->pcData), static_cast<int>(sceneTexture->mWidth), &width, &height, &texChannels, STBI_rgb_alpha);
                    }
                    else
                    {
                        texture.data = stbi_load_from_memory(reinterpret_cast<uint8_t *>(sceneTexture->pcData), static_cast<int>(sceneTexture->mWidth * sceneTexture->mHeight), &width, &height, &texChannels, STBI_rgb_alpha);
                    }

                    texture.width = width;
                    texture.height = height;
                    texture.stride = 8;
                }
            }
        } else {
            throw std::runtime_error(importer.GetErrorString());
        }
    }

    void BananModel::Builder::loadTexture(const std::string &filepath) {
        char *fileName = const_cast<char *>(filepath.c_str());
        size_t len = strlen(fileName);
        size_t idx = len-1;
        for(size_t i = 0; *(fileName+i); i++) {
            if (*(fileName+i) == '.') {
                idx = i;
            } else if (*(fileName + i) == '/' || *(fileName + i) == '\\') {
                idx = len - 1;
            }
        }

        std::string extension = std::string(fileName).substr(idx+1);

        if (extension == "exr") {
            loadHDR(filepath, texture);
        } else if (extension == "jpg" || extension == "jpeg" || extension == "png") {
            loadRGB(filepath, texture);
        } else {
            throw std::runtime_error("unsupported texture file format");
        }
    }

    void BananModel::Builder::loadNormals(const std::string &filepath) {
        char *fileName = const_cast<char *>(filepath.c_str());
        size_t len = strlen(fileName);
        size_t idx = len-1;
        for(size_t i = 0; *(fileName+i); i++) {
            if (*(fileName+i) == '.') {
                idx = i;
            } else if (*(fileName + i) == '/' || *(fileName + i) == '\\') {
                idx = len - 1;
            }
        }

        std::string extension = std::string(fileName).substr(idx+1);

        if (extension == "exr") {
            loadHDR(filepath, normals);
        } else if (extension == "jpg" || extension == "jpeg" || extension == "png") {
            loadRGB(filepath, normals);
        } else {
            throw std::runtime_error("unsupported texture file format");
        }
    }

    void BananModel::Builder::loadHeightMap(const std::string &filepath) {
        char *fileName = const_cast<char *>(filepath.c_str());
        size_t len = strlen(fileName);
        size_t idx = len-1;
        for(size_t i = 0; *(fileName+i); i++) {
            if (*(fileName+i) == '.') {
                idx = i;
            } else if (*(fileName + i) == '/' || *(fileName + i) == '\\') {
                idx = len - 1;
            }
        }

        std::string extension = std::string(fileName).substr(idx+1);

        if (extension == "exr") {
            loadHDR(filepath, heights);
        } else if (extension == "jpg" || extension == "jpeg" || extension == "png") {
            loadRGB(filepath, heights);
        } else {
            throw std::runtime_error("unsupported texture file format");
        }
    }

    void BananModel::Builder::loadHDR(const string &filepath, Texture &target) {
        Imf::setGlobalThreadCount((int) std::thread::hardware_concurrency());
        Imf::Array2D<Rgba> pixelBuffer = Imf::Array2D<Rgba>();
        Imf::Array2D<Rgba> &pixelBufferRef = pixelBuffer;

        Imf::RgbaInputFile in(filepath.c_str());
        Imath::Box2i win = in.dataWindow();
        Imath::V2i dim(win.max.x - win.min.x + 1, win.max.y - win.min.y + 1);

        int dx = win.min.x;
        int dy = win.min.y;

        pixelBufferRef.resizeErase(dim.x, dim.y);

        in.setFrameBuffer(&pixelBufferRef[0][0] - dx - dy * dim.x, 1, dim.x);
        in.readPixels(win.min.y, win.max.y);

        target.data = (uint16_t *) malloc(dim.y * dim.y * 8);
        target.width = dim.x;
        target.height = dim.y;
        target.stride = 16;
        target.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(target.width, target.width)))) + 1;

        auto *singleChannelPixelBuffer = (uint16_t *) malloc(dim.y * dim.y * 8);
        int index = 0;

        for (int y1 = 0; y1 < dim.y; y1++) {
            for (int x1 = 0; x1 < dim.x; x1++) {
                singleChannelPixelBuffer[index++] = pixelBufferRef[y1][x1].r.bits();
                singleChannelPixelBuffer[index++] = pixelBufferRef[y1][x1].g.bits();
                singleChannelPixelBuffer[index++] = pixelBufferRef[y1][x1].b.bits();
                singleChannelPixelBuffer[index++] = pixelBufferRef[y1][x1].a.bits();
            }
        }

        memcpy(target.data, singleChannelPixelBuffer, dim.x * dim.y * 8);
        free(singleChannelPixelBuffer);
    }

    void BananModel::Builder::loadRGB(const string &filepath, Texture &target) {
        uint8_t *data = stbi_load(filepath.c_str(), (int *) &target.width, (int *) &target.height, nullptr, STBI_rgb_alpha);

        target.data = (uint8_t *) malloc(target.width * target.height * 4);
        target.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(target.width, target.width)))) + 1;
        target.stride = 8;

        memcpy(target.data, data, target.width * target.height * 4);
        stbi_image_free(data);
    }
}