#include "simple_render_system.hpp"

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>
#include <gtc/constants.hpp>

#include <array>
#include <cassert>
#include <stdexcept>

namespace Biosim::Engine {

	struct SimplePushConstantData {
		glm::mat4 transform{ 1.f };
		alignas(16) glm::vec3 rotation{};
		alignas(16) glm::vec3 color;
	};

	SimpleRenderSystem::SimpleRenderSystem(Device& device, VkRenderPass render_pass) : device{device} {
		createPipelineLayout();
		createPipeline(render_pass);
	}

	SimpleRenderSystem::~SimpleRenderSystem() {
		vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
	}

	void SimpleRenderSystem::createPipelineLayout() {
		VkPushConstantRange push_constant_range{};
		push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		push_constant_range.offset = 0;
		push_constant_range.size = sizeof(SimplePushConstantData);

		VkPipelineLayoutCreateInfo pipeline_layout_info{};
		pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipeline_layout_info.setLayoutCount = 0;
		pipeline_layout_info.pSetLayouts = nullptr;
		pipeline_layout_info.pushConstantRangeCount = 1;
		pipeline_layout_info.pPushConstantRanges = &push_constant_range;

		if (vkCreatePipelineLayout(device.device(), &pipeline_layout_info, nullptr, &pipelineLayout) != VK_SUCCESS) {
			throw std::runtime_error("failed to create pipeline layout!");
		}
	}

	void SimpleRenderSystem::createPipeline(VkRenderPass render_pass) {
		assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

		Engine::PipelineConfig pipeline_config{};
		Engine::Pipeline::defaultCfg(pipeline_config);
		pipeline_config.renderPass = render_pass;
		pipeline_config.pipelineLayout = pipelineLayout;
		pipeline = std::make_unique<Engine::Pipeline>(device, pipeline_config, "shaders/simple_shader.vert.spv", "shaders/simple_shader.frag.spv");
	}

	void SimpleRenderSystem::renderObjects(
		const std::vector<GameObject>& objects,
		VkCommandBuffer cmd_buffer,
		const Camera& camera) {

		pipeline->bind(cmd_buffer);

		auto projection_view = camera.getProjection() * camera.getView();

		for (auto& obj : objects) {
			SimplePushConstantData push{};
			push.color = obj.color;
			push.transform = projection_view * obj.transform.mat4();
			push.rotation = obj.transform.rotation;

			vkCmdPushConstants(cmd_buffer,
				pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
				0,
				sizeof(SimplePushConstantData),
				&push);
			obj.model->bind(cmd_buffer);
			obj.model->draw(cmd_buffer);
		}
	}
}