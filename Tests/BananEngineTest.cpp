//
// Created by yashr on 12/4/21.
//

#include "BananEngineTest.h"

#include "Systems/PointLightSystem.h"
#include "Systems/ResolveSystem.h"
#include "Systems/ComputeSystem.h"
#include "Systems/ProcrastinatedRenderSystem.h"

#include "Constants/AreaTex.h"
#include "Constants/SearchTex.h"

#include "KeyboardMovementController.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

#include <algorithm>
#include <chrono>

#include <banan_logger.h>

namespace Banan{

    BananEngineTest::BananEngineTest() {

        loadGameObjects();

        globalPool = BananDescriptorPool::Builder(bananDevice)
                .setMaxSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT)
                .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, BananSwapChain::MAX_FRAMES_IN_FLIGHT)
                .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, BananSwapChain::MAX_FRAMES_IN_FLIGHT)
                .build();

        texturePool = BananDescriptorPool::Builder(bananDevice)
                .setMaxSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT)
                .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT)
                .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, BananSwapChain::MAX_FRAMES_IN_FLIGHT * gameObjects.size())
                .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, BananSwapChain::MAX_FRAMES_IN_FLIGHT)
                .build();

        normalPool = BananDescriptorPool::Builder(bananDevice)
                .setMaxSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT)
                .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT)
                .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, BananSwapChain::MAX_FRAMES_IN_FLIGHT * gameObjects.size())
                .build();

        heightPool = BananDescriptorPool::Builder(bananDevice)
                .setMaxSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT)
                .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT_EXT)
                .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, BananSwapChain::MAX_FRAMES_IN_FLIGHT * gameObjects.size())
                .build();

        procrastinatedPool = BananDescriptorPool::Builder(bananDevice)
                .setMaxSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT)
                .addPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, BananSwapChain::MAX_FRAMES_IN_FLIGHT * 3)
                .build();

        edgeDetectionPool = BananDescriptorPool::Builder(bananDevice)
                .setMaxSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT)
                .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, BananSwapChain::MAX_FRAMES_IN_FLIGHT)
                .build();

        blendWeightPool = BananDescriptorPool::Builder(bananDevice)
                .setMaxSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT)
                .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, BananSwapChain::MAX_FRAMES_IN_FLIGHT * 3)
                .build();

        resolvePool = BananDescriptorPool::Builder(bananDevice)
                .setMaxSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT)
                .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, BananSwapChain::MAX_FRAMES_IN_FLIGHT * 2)
                .build();
    }

    BananEngineTest::~BananEngineTest() = default;

    void BananEngineTest::run() {

        std::vector<std::unique_ptr<BananBuffer>> uboBuffers(BananSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (auto & uboBuffer : uboBuffers) {
            uboBuffer = std::make_unique<BananBuffer>(bananDevice, sizeof(GlobalUbo), 1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
            uboBuffer->map();
        }

        std::vector<std::unique_ptr<BananBuffer>> storageBuffers(BananSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (auto & storageBuffer : storageBuffers) {
            storageBuffer = std::make_unique<BananBuffer>(bananDevice, sizeof(GameObjectData), gameObjects.size(), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, 0);
            storageBuffer->map();
        }

        auto globalSetLayout = BananDescriptorSetLayout::Builder(bananDevice)
                .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 1)
                .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS | VK_SHADER_STAGE_COMPUTE_BIT, 1)
                .build();

        auto textureSetLayout = BananDescriptorSetLayout::Builder(bananDevice)
                .addFlag(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT)
                .addFlag(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT)
                .addFlag(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT)
                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, gameObjects.size())
                //.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
                .build();

        auto normalSetLayout = BananDescriptorSetLayout::Builder(bananDevice)
                .addFlag(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT)
                .addFlag(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT)
                .addFlag(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT)
                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, gameObjects.size())
                .build();

        auto heightMapSetLayout = BananDescriptorSetLayout::Builder(bananDevice)
                .addFlag(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT)
                .addFlag(VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT)
                .addFlag(VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT)
                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, gameObjects.size())
                .build();

        auto procrastinatedSetLayout = BananDescriptorSetLayout::Builder(bananDevice)
                .addBinding(0, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
                .addBinding(1, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
                .addBinding(2, VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VK_SHADER_STAGE_FRAGMENT_BIT, 1)
                .build();

        auto edgeDetectionSetLayout = BananDescriptorSetLayout::Builder(bananDevice)
                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS, 1)
                .build();

        auto blendWeightSetLayout = BananDescriptorSetLayout::Builder(bananDevice)
                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS, 1)
                .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS, 1)
                .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS, 1)
                .build();

        auto resolveLayout = BananDescriptorSetLayout::Builder(bananDevice)
                .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS, 1)
                .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_ALL_GRAPHICS, 1)
                .build();

        ComputeSystem computeSystem{bananDevice, {globalSetLayout->getDescriptorSetLayout()}};

        PointLightSystem pointLightSystem{bananDevice, bananRenderer.getGeometryRenderPass(), {globalSetLayout->getDescriptorSetLayout()}};
        ProcrastinatedRenderSystem procrastinatedRenderSystem{bananDevice, bananRenderer.getGeometryRenderPass(), {globalSetLayout->getDescriptorSetLayout(), textureSetLayout->getDescriptorSetLayout(), normalSetLayout->getDescriptorSetLayout(), heightMapSetLayout->getDescriptorSetLayout()}, {globalSetLayout->getDescriptorSetLayout(), procrastinatedSetLayout->getDescriptorSetLayout()}};
        ResolveSystem resolveSystem{bananDevice, bananRenderer.getEdgeDetectionRenderPass(), bananRenderer.getBlendWeightRenderPass(), bananRenderer.getResolveRenderPass(), {globalSetLayout->getDescriptorSetLayout(), edgeDetectionSetLayout->getDescriptorSetLayout()}, {globalSetLayout->getDescriptorSetLayout(), blendWeightSetLayout->getDescriptorSetLayout()}, {globalSetLayout->getDescriptorSetLayout(), resolveLayout->getDescriptorSetLayout()}};

        BananCamera camera{};

        std::vector<VkDescriptorSet> globalDescriptorSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSet> textureDescriptorSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSet> normalDescriptorSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSet> heightDescriptorSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSet> procrastinatedDescriptorSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSet> edgeDetectionDescriptorSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSet> blendWeightDescriptorSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSet> resolveDescriptorSets(BananSwapChain::MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < BananSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {

            BananDescriptorWriter writer = BananDescriptorWriter(*globalSetLayout, *globalPool);
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            writer.writeBuffer(0, &bufferInfo);

            auto storageInfo = storageBuffers[i]->descriptorInfo();
            writer.writeBuffer(1, &storageInfo);

            writer.build(globalDescriptorSets[i], std::vector<uint32_t> {});

            BananDescriptorWriter textureWriter = BananDescriptorWriter(*textureSetLayout, *texturePool);
            textureWriter.writeImages(0, gameObjectsTextureInfo);

            textureWriter.build(textureDescriptorSets[i], std::vector<uint32_t> {static_cast<uint32_t>(gameObjectsTextureInfo.size())});

            BananDescriptorWriter normalWriter = BananDescriptorWriter(*normalSetLayout, *normalPool);
            normalWriter.writeImages(0, gameObjectsNormalInfo);

            normalWriter.build(normalDescriptorSets[i], std::vector<uint32_t> {static_cast<uint32_t>(gameObjectsNormalInfo.size())});

            BananDescriptorWriter heightWriter = BananDescriptorWriter(*heightMapSetLayout, *heightPool);
            heightWriter.writeImages(0, gameObjectsHeightInfo);

            heightWriter.build(heightDescriptorSets[i], std::vector<uint32_t> {static_cast<uint32_t>(gameObjectsHeightInfo.size())});

            BananDescriptorWriter procrastinatedWriter = BananDescriptorWriter(*procrastinatedSetLayout, *procrastinatedPool);

            auto normalInfo = bananRenderer.getGBufferDescriptorInfo()[0];
            auto albedoInfo = bananRenderer.getGBufferDescriptorInfo()[1];
            auto depthInfo = bananRenderer.getGBufferDescriptorInfo()[2];

            procrastinatedWriter.writeImage(0, &normalInfo);
            procrastinatedWriter.writeImage(1, &albedoInfo);
            procrastinatedWriter.writeImage(2, &depthInfo);

            procrastinatedWriter.build(procrastinatedDescriptorSets[i], std::vector<uint32_t> {});

            BananDescriptorWriter edgeDetectionWriter(*edgeDetectionSetLayout, *edgeDetectionPool);

            auto geometryInfo = bananRenderer.getGeometryDescriptorInfo();
            auto edgeInfo = bananRenderer.getEdgeDescriptorInfo();
            auto blendInfo = bananRenderer.getBlendWeightDescriptorInfo();

            edgeDetectionWriter.writeImage(0, &geometryInfo);

            edgeDetectionWriter.build(edgeDetectionDescriptorSets[i], std::vector<uint32_t> {});

            BananDescriptorWriter blendWeightWriter(*blendWeightSetLayout, *blendWeightPool);

            auto areaTexInfo = areaTex->descriptorInfo();
            auto searchTexInfo = searchTex->descriptorInfo();

            blendWeightWriter.writeImage(0, &edgeInfo);
            blendWeightWriter.writeImage(1, &areaTexInfo);
            blendWeightWriter.writeImage(2, &searchTexInfo);

            blendWeightWriter.build(blendWeightDescriptorSets[i], std::vector<uint32_t> {});

            BananDescriptorWriter resolveWriter(*resolveLayout, *resolvePool);

            resolveWriter.writeImage(0, &geometryInfo);
            resolveWriter.writeImage(1, &blendInfo);

            resolveWriter.build(resolveDescriptorSets[i], std::vector<uint32_t> {});
        }

        auto viewerObject = BananGameObject::createGameObject();
        KeyboardMovementController cameraController{};

        auto currentTime = std::chrono::high_resolution_clock::now();

        while(true)
        {

            SDL_Event event;
            SDL_PollEvent(&event);

            switch(event.type) {
                case SDL_WINDOWEVENT:
                    if(event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        bananRenderer.recreateSwapChain();

                        for (int i = 0; i < BananSwapChain::MAX_FRAMES_IN_FLIGHT; i++) {
                            BananDescriptorWriter procrastinatedWriter = BananDescriptorWriter(*procrastinatedSetLayout,*procrastinatedPool);

                            auto normalInfo = bananRenderer.getGBufferDescriptorInfo()[0];
                            auto albedoInfo = bananRenderer.getGBufferDescriptorInfo()[1];
                            auto depthInfo = bananRenderer.getGBufferDescriptorInfo()[2];

                            procrastinatedWriter.writeImage(0, &normalInfo);
                            procrastinatedWriter.writeImage(1, &albedoInfo);
                            procrastinatedWriter.writeImage(2, &depthInfo);

                            procrastinatedWriter.overwrite(procrastinatedDescriptorSets[i]);

                            BananDescriptorWriter edgeDetectionWriter(*edgeDetectionSetLayout, *edgeDetectionPool);

                            auto geometryInfo = bananRenderer.getGeometryDescriptorInfo();
                            auto edgeInfo = bananRenderer.getEdgeDescriptorInfo();
                            auto blendInfo = bananRenderer.getBlendWeightDescriptorInfo();

                            edgeDetectionWriter.writeImage(0, &geometryInfo);
                            edgeDetectionWriter.overwrite(edgeDetectionDescriptorSets[i]);

                            BananDescriptorWriter blendWeightWriter(*blendWeightSetLayout, *blendWeightPool);

                            auto areaTexInfo = areaTex->descriptorInfo();
                            auto searchTexInfo = searchTex->descriptorInfo();

                            blendWeightWriter.writeImage(0, &edgeInfo);
                            blendWeightWriter.writeImage(1, &areaTexInfo);
                            blendWeightWriter.writeImage(2, &searchTexInfo);

                            blendWeightWriter.overwrite(blendWeightDescriptorSets[i]);

                            BananDescriptorWriter resolveWriter(*resolveLayout, *resolvePool);

                            resolveWriter.writeImage(0, &geometryInfo);
                            resolveWriter.writeImage(1, &blendInfo);

                            resolveWriter.overwrite(resolveDescriptorSets[i]);
                        }

                        pointLightSystem.reconstructPipeline(bananRenderer.getGeometryRenderPass(), {globalSetLayout->getDescriptorSetLayout()});
                        computeSystem.reconstructPipeline({globalSetLayout->getDescriptorSetLayout()});
                        procrastinatedRenderSystem.reconstructPipeline(bananRenderer.getGeometryRenderPass(), {globalSetLayout->getDescriptorSetLayout(), textureSetLayout->getDescriptorSetLayout(), normalSetLayout->getDescriptorSetLayout(), heightMapSetLayout->getDescriptorSetLayout()}, {globalSetLayout->getDescriptorSetLayout(), procrastinatedSetLayout->getDescriptorSetLayout()});
                        resolveSystem.reconstructPipelines(bananRenderer.getEdgeDetectionRenderPass(), bananRenderer.getBlendWeightRenderPass(), bananRenderer.getResolveRenderPass(), {globalSetLayout->getDescriptorSetLayout(), edgeDetectionSetLayout->getDescriptorSetLayout()}, {globalSetLayout->getDescriptorSetLayout(), blendWeightSetLayout->getDescriptorSetLayout()}, {globalSetLayout->getDescriptorSetLayout(), resolveLayout->getDescriptorSetLayout()});
                    }

                    continue;

                case SDL_QUIT:
                    vkDeviceWaitIdle(bananDevice.device());
                    return;
            }

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            cameraController.moveInPlaneXZ(frameTime, viewerObject);
            camera.setViewYXZ(viewerObject.transform.translation, viewerObject.transform.rotation);

            float aspect = bananRenderer.getAspectRatio();
            camera.setOrthographicProjection(-aspect, aspect, -1, 1, -1, 1);
            camera.setPerspectiveProjection(glm::radians(90.f), aspect, 0.1f, 100.f);

            if (auto commandBuffer = bananRenderer.beginFrame()) {
                int frameIndex = bananRenderer.getFrameIndex();
                BananFrameInfo frameInfo{frameIndex, frameTime, commandBuffer, camera, globalDescriptorSets[frameIndex], textureDescriptorSets[frameIndex], normalDescriptorSets[frameIndex], heightDescriptorSets[frameIndex], procrastinatedDescriptorSets[frameIndex], edgeDetectionDescriptorSets[frameIndex], blendWeightDescriptorSets[frameIndex], resolveDescriptorSets[frameIndex], gameObjects};

                // ok heres the plan to fix this, one descriptor has the buffer as a regular storage buffer, another has the buffer as a dynamic storage buffer, we write the buffer to both descriptors
                // then we calculate the stuff in the comp shader, and finally we apply a execution to guarantee that the shader has finished execution before doing the actual rendering

                std::vector<GameObjectData> data{};
                data.reserve(gameObjects.size());
                for (auto &kv : gameObjects) {
                    if (kv.second.model != nullptr) {
                        GameObjectData objectData{glm::vec4(kv.second.transform.translation, 0),
                                                  glm::vec4(kv.second.transform.rotation,0),
                                                  glm::vec4(kv.second.transform.scale, 0),
                                                  kv.second.transform.mat4(),
                                                  kv.second.transform.normalMatrix(),
                                                  kv.second.model->isTextureLoaded() ? (int) kv.first : -1,
                                                  kv.second.model->isNormalsLoaded() ? (int) kv.first : -1,
                                                  kv.second.model->isHeightmapLoaded() ? (int) kv.first : -1,
                                                  kv.second.parallax.heightscale,
                                                  kv.second.parallax.parallaxBias,
                                                  kv.second.parallax.numLayers,
                                                  kv.second.parallax.parallaxmode,
                                                  0
                        };

                        data.push_back(objectData);
                    } else {
                        GameObjectData pointLightData{glm::vec4(kv.second.transform.translation, 0),
                                                      glm::vec4(kv.second.color, kv.second.pointLight->lightIntensity),
                                                      glm::vec4(kv.second.transform.scale.x, -1, -1, -1),
                                                      glm::mat4{1.f},
                                                      glm::mat4{1.f},
                                                      -1,
                                                      -1,
                                                      -1,
                                                      -1,
                                                      -1,
                                                      -1,
                                                      -1,
                                                      1
                        };

                        data.push_back(pointLightData);
                    }
                }

                std::reverse(data.begin(), data.end());

                GlobalUbo ubo{};
                ubo.projection = camera.getProjection();
                ubo.view = camera.getView();
                ubo.inverseView = camera.getInverseView();
                ubo.inverseProjection = camera.getInverseProjection();
                ubo.numGameObjects = gameObjects.size();

                pointLightSystem.update(frameInfo);
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                storageBuffers[frameIndex]->writeToBuffer(data.data());
                storageBuffers[frameIndex]->flush();

//                computeSystem.compute(frameInfo);

//                // ensure compute shader finishes work (it isnt working)
//                VkBufferMemoryBarrier barrier;
//                barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
//                barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
//                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
//                barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//                barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//                barrier.buffer = storageBuffers[frameIndex]->getBuffer();
//                barrier.size = storageBuffers[frameIndex]->getBufferSize();
//                barrier.offset = 0;
//
//                vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,nullptr, 1, &barrier, 0, nullptr);

                /*for (int i = 0; i < 6; i++) {
                    bananRenderer.beginShadowRenderPass(commandBuffer);
                    shadowSystem.render(frameInfo, i);
                    bananRenderer.endShadowRenderPass(commandBuffer, i);
                }*/

                bananRenderer.beginGeometryRenderPass(commandBuffer);
                procrastinatedRenderSystem.calculateGBuffer(frameInfo);
                procrastinatedRenderSystem.render(frameInfo);
                pointLightSystem.render(frameInfo);
                bananRenderer.endRenderPass(commandBuffer);

                bananRenderer.beginEdgeDetectionRenderPass(commandBuffer);
                resolveSystem.runEdgeDetection(frameInfo);
                bananRenderer.endRenderPass(commandBuffer);

                bananRenderer.beginBlendWeightRenderPass(commandBuffer);
                resolveSystem.calculateBlendWeights(frameInfo);
                bananRenderer.endRenderPass(commandBuffer);

                bananRenderer.beginResolveRenderPass(commandBuffer);
                resolveSystem.resolveImage(frameInfo);
                bananRenderer.endRenderPass(commandBuffer);

                bananRenderer.endFrame();
            }
        }
    }

    void BananEngineTest::loadGameObjects() {
        // SMAA Textures stuff
        BananBuffer areaTexStagingBuffer{bananDevice, 2, AREATEX_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
        areaTexStagingBuffer.map();
        areaTexStagingBuffer.writeToBuffer((void *) &areaTexBytes, AREATEX_SIZE);

        BananBuffer searchTexStagingBuffer{bananDevice, 1, SEARCHTEX_SIZE, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
        searchTexStagingBuffer.map();
        searchTexStagingBuffer.writeToBuffer((void *) &searchTexBytes, SEARCHTEX_SIZE);

        VkCommandBuffer commandBuffer = bananDevice.beginSingleTimeCommands();

        areaTex = std::make_unique<BananImage>(bananDevice, AREATEX_WIDTH, AREATEX_HEIGHT, 1, VK_FORMAT_R8G8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        areaTex->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        searchTex = std::make_unique<BananImage>(bananDevice, SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1, VK_FORMAT_R8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        searchTex->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        bananDevice.endSingleTimeCommands(commandBuffer);

        bananDevice.copyBufferToImage(areaTexStagingBuffer.getBuffer(), areaTex->getImageHandle(), AREATEX_WIDTH, AREATEX_HEIGHT, 1);
        bananDevice.copyBufferToImage(searchTexStagingBuffer.getBuffer(), searchTex->getImageHandle(), SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, 1);

        commandBuffer = bananDevice.beginSingleTimeCommands();

        areaTex->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        searchTex->transitionLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        bananDevice.endSingleTimeCommands(commandBuffer);

        BananModel::Builder vaseBuilder{};
        vaseBuilder.loadModel("banan_assets/ceramic_vase_01_4k.blend");
        vaseBuilder.loadTexture("banan_assets/textures/ceramic_vase_01_diff_4k.jpg");
        vaseBuilder.loadNormals("banan_assets/textures/ceramic_vase_01_nor_gl_4k.exr");

        std::shared_ptr<BananModel> vaseModel = std::make_shared<BananModel>(bananDevice, vaseBuilder);
        auto vase = BananGameObject::createGameObject();
        vase.model = vaseModel;
        vase.transform.translation = {0.f, .5f, 0.f};
        vase.transform.rotation = {-glm::pi<float>() / 2.0f, 0.f, 0.0f};
        vase.transform.scale = {3.f, 3.f, 3.f};
        vase.transform.id = (int) vase.getId();
        gameObjects.emplace(vase.getId(), std::move(vase));

        /*BananModel::Builder otherfloorBuilder{};
        otherfloorBuilder.loadModel("banan_assets/obamium.blend");
        otherfloorBuilder.loadTexture("banan_assets/textures/base.png");

        std::shared_ptr<BananModel> otherfloorModel = std::make_shared<BananModel>(bananDevice, otherfloorBuilder);
        auto otherfloor = BananGameObject::createGameObject();
        otherfloor.model = otherfloorModel;
        otherfloor.transform.translation = {0.f, .3f, 0.f};
        otherfloor.transform.rotation = {glm::pi<float>() / 2.0f, 0.f, 0.f};
        otherfloor.transform.scale = {1.f, 1.f, 1.f};
        otherfloor.transform.id = (int) otherfloor.getId();
        gameObjects.emplace(otherfloor.getId(), std::move(otherfloor));*/

        BananModel::Builder floorBuilder{};
        floorBuilder.loadModel("banan_assets/quad.obj");
        floorBuilder.loadTexture("banan_assets/textures/Tiles_046_basecolor.jpg");
        floorBuilder.loadNormals("banan_assets/textures/Tiles_046_normal.exr");
        floorBuilder.loadHeightMap("banan_assets/textures/Tiles_046_height.png");

        std::shared_ptr<BananModel> floorModel = std::make_shared<BananModel>(bananDevice, floorBuilder);
        auto floor = BananGameObject::createGameObject();
        floor.model = floorModel;
        floor.transform.translation = {0.f, .5f, 0.f};
        floor.transform.rotation = {0.f, glm::pi<float>(), 0.0f};
        floor.transform.scale = {3.f, 3.f, 3.f};
        floor.transform.id = (int) floor.getId();

        floor.parallax.heightscale = 0.1;
        floor.parallax.parallaxBias = -0.02f;
        floor.parallax.numLayers = 48.0f;
        floor.parallax.parallaxmode = 1;

        gameObjects.emplace(floor.getId(), std::move(floor));

        std::vector<glm::vec3> lightColors{
                {1.f, .1f, .1f},
                {.1f, .1f, 1.f},
                {.1f, 1.f, .1f},
                {1.f, 1.f, .1f},
                {.1f, 1.f, 1.f},
                {1.f, 1.f, 1.f}
        };

        for (size_t i = 0; i < lightColors.size(); i++) {
            auto pointLight = BananGameObject::makePointLight(0.5f);
            pointLight.color = lightColors[i];
            auto rotateLight = glm::rotate(glm::mat4(1.f), (static_cast<float>(i) * glm::two_pi<float>()) / static_cast<float>(lightColors.size()), {0.f, -1.f, 0.f});
            pointLight.transform.translation = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f));
            pointLight.transform.id = 0;

            pointLight.parallax.heightscale = -1;
            pointLight.parallax.parallaxBias = -1;
            pointLight.parallax.numLayers = -1;
            pointLight.parallax.parallaxmode = -1;

            gameObjects.emplace(pointLight.getId(), std::move(pointLight));
        }

        for (auto &kv : gameObjects)
        {
            if (kv.second.model != nullptr) {
                if (kv.second.model->isTextureLoaded()) {
                    gameObjectsTextureInfo.emplace(kv.first, kv.second.model->getDescriptorTextureImageInfo());
                }

                if (kv.second.model->isNormalsLoaded()) {
                    gameObjectsNormalInfo.emplace(kv.first, kv.second.model->getDescriptorNormalImageInfo());
                }

                if (kv.second.model->isHeightmapLoaded()) {
                    gameObjectsHeightInfo.emplace(kv.first, kv.second.model->getDescriptorHeightMapInfo());
                }
            }
        }
    }

    std::shared_ptr<BananLogger> BananEngineTest::getLogger() {
        return bananLogger;
    }
}

