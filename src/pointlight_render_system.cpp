#include "pointlight_render_system.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <cassert>
#include <stdexcept>
#include <map>

namespace SJFGame::Engine {

	/*
	We could render our lights by indexing into the global ubo information, which has the
	advantage of only requiring a single draw call.
	However by using push constants it's easier to add extra information per light, the shader
	code will be simpler, but we require a draw call per light object.
	*/
	struct PointLightPushConstants {
		glm::vec4 position{};
		glm::vec4 color{};
		float radius;
	};

	PointLightRenderSystem::PointLightRenderSystem(Device& device, VkRenderPass render_pass, VkDescriptorSetLayout global_set_layout) : device{ device } {
		createPipelineLayout(global_set_layout);
		createPipeline(render_pass);
	}

	PointLightRenderSystem::~PointLightRenderSystem() {
		vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
	}

	void PointLightRenderSystem::createPipelineLayout(VkDescriptorSetLayout global_set_layout) {
		VkPushConstantRange push_constant_range{};
		push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		push_constant_range.offset = 0;
		push_constant_range.size = sizeof(PointLightPushConstants);

		// the pipeline can have multiple descriptor set layouts
		std::vector<VkDescriptorSetLayout> descriptor_set_layouts{ global_set_layout };

		VkPipelineLayoutCreateInfo pipeline_layout_info{};
		pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size());
		pipeline_layout_info.pSetLayouts = descriptor_set_layouts.data();
		pipeline_layout_info.pushConstantRangeCount = 1;
		pipeline_layout_info.pPushConstantRanges = &push_constant_range;

		if (vkCreatePipelineLayout(device.device(), &pipeline_layout_info, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}
	}

	void PointLightRenderSystem::createPipeline(VkRenderPass render_pass) {
		assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

		Engine::PipelineConfig pipeline_config{};
		Engine::Pipeline::defaultCfg(pipeline_config);
		Engine::Pipeline::enableAlphaBlending(pipeline_config);

		pipeline_config.attributeDescriptions.clear();
		pipeline_config.attributeDescriptions.clear();
		pipeline_config.renderPass = render_pass;
		pipeline_config.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<Engine::Pipeline>(device, pipeline_config, "shaders/point_light.vert.spv", "shaders/point_light.frag.spv");
	}

	void PointLightRenderSystem::update(Frame& frame, GlobalUniformBufferOutput& ubo) {
		auto rotate_transform_matrix = glm::rotate(
			glm::mat4{ 1.f },
			frame.delta,
			{ 0.f, -1.f, 0.f });

		int light_index{};
		for (auto& kv : frame.gameObjects) {
			auto& obj = kv.second;
			if (obj.pointLight == nullptr) continue;
			assert(light_index < MAX_LIGHTS && "Point lights exceed maximum specified");

			// animate light pos
			obj.transform.translation = glm::vec3(rotate_transform_matrix * glm::vec4(obj.transform.translation, 1.f));

			// copy light to ubo
			ubo.pointLights[light_index].position = glm::vec4(obj.transform.translation, 0.f);
			ubo.pointLights[light_index].color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
			
			light_index += 1;
		}
		ubo.numLights = light_index;
	}

	void PointLightRenderSystem::render(const Frame& frame) {
		// sort ligghts
		std::map<float, GameObject::id_t>sorted{};
		for (auto& kv : frame.gameObjects) {
			auto& obj = kv.second;
			if (obj.pointLight == nullptr) continue;

			auto offset = frame.camera.getPosition() - obj.transform.translation;
			float distance_squared = glm::dot(offset, offset);
			sorted[distance_squared] = obj.getId();
		}

		pipeline->bind(frame.cmdBuffer);

		vkCmdBindDescriptorSets(frame.cmdBuffer,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			0, 1,
			&frame.globalDescriptorSet,
			0, nullptr
		);

		// iterate through sorted lights in reverse order
		for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
			auto& obj = frame.gameObjects.at(it->second);

			// copy light to push constants
			PointLightPushConstants push{};
			push.position = glm::vec4(obj.transform.translation, 1.f);
			push.color = glm::vec4(obj.color, obj.pointLight->lightIntensity);
			push.radius = obj.transform.scale.x;

			vkCmdPushConstants(frame.cmdBuffer,
				pipelineLayout,
				VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(PointLightPushConstants),
				&push
			);

			vkCmdDraw(frame.cmdBuffer, 6, 1, 0, 0);
		}
	}
}