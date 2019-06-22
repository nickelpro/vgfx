#include <stdint.h>
#include "vgfx.h"
#include "logc/log.h"

void glfw_error_cb(int err, const char *desc) {
	log_error("GLFW Error: %s", desc);
}

void glfw_size_cb(GLFWwindow *window, int width, int height) {
	//Don't bother firing a resize for a minimized window
	for(; width == 0 || height == 0;) {
		glfwWaitEvents();
		glfwGetFramebufferSize(window, &width, &height);
	}
	vgfx_vk_t *vk = (vgfx_vk_t *) glfwGetWindowUserPointer(window);
	vk->resized = 1;
	log_trace("Resize Width: %d Height: %d", width, height);
}

int draw_frame(vgfx_vk_t *vk) {
	static unsigned int current_frame = 0;
	if(vk->resized) goto resize;

	vkWaitForFences(
		vk->logicdev.handle,
		1,
		&vk->syncobjects.in_flight[current_frame],
		VK_TRUE,
		UINT64_MAX
	);

	uint32_t image_idx;
	VkResult err = vkAcquireNextImageKHR(
		vk->logicdev.handle,
		vk->swapchain.handle,
		UINT64_MAX,
		vk->syncobjects.image_available[current_frame],
		VK_NULL_HANDLE,
		&image_idx
	);
	if(
		err == VK_ERROR_OUT_OF_DATE_KHR ||
		err == VK_SUBOPTIMAL_KHR
	) {
		goto resize;
	} else if(err != VK_SUCCESS) {
		goto borked;
	}

	vkResetFences(
		vk->logicdev.handle,
		1,
		&vk->syncobjects.in_flight[current_frame]
	);
	err = vkQueueSubmit(
		vk->logicdev.gfx_q,
		1,
		&(const VkSubmitInfo) {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = NULL,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &vk->syncobjects.image_available[current_frame],
			.pWaitDstStageMask = &(const VkPipelineStageFlags) {
				VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
			},
			.commandBufferCount = 1,
			.pCommandBuffers = &vk->commandbuffers.handles[image_idx],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &vk->syncobjects.render_finished[current_frame],
		},
		vk->syncobjects.in_flight[current_frame]
	);
	if(err != VK_SUCCESS) goto borked;

	err = vkQueuePresentKHR(
		vk->logicdev.gfx_q,
		&(const VkPresentInfoKHR) {
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = NULL,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &vk->syncobjects.render_finished[current_frame],
			.swapchainCount = 1,
			.pSwapchains = &vk->swapchain.handle,
			.pImageIndices = &image_idx,
			.pResults = NULL
		}
	);
	if(err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
		goto resize;
	} else if(err != VK_SUCCESS) {
		goto borked;
	}

	current_frame = (current_frame + 1) % vk->max_fif;
	return VGFX_SUCCESS;
	resize:
		return rebuild_vk_swapchain(vk);
	borked:
		log_error("Failed to draw frame: %s", vkr2str(err));
		return VGFX_FAIL;
}

int main(int argc, char const *argv[]) {
	log_set_level(LOG_DEBUG);
	log_set_simple(0);

	//Setup glfw
	glfwSetErrorCallback(glfw_error_cb);
	if(!glfwInit()) {
		log_error("Failed to init glfw");
		return VGFX_FAIL;
	}
	log_debug("Successful init of glfw");

	log_debug("Creating a window");
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow *window = glfwCreateWindow(
		800, 600, "Test Window", NULL, NULL
	);
	if(!window) {
		log_error("Window creation failed");
		return VGFX_FAIL;
	}

	vgfx_vk_t vk;
	if(init_vk(&vk, window, 2)) {
		log_error("init_vk failed");
		return VGFX_FAIL;
	}
	glfwSetWindowUserPointer(window, &vk);
	glfwSetWindowSizeCallback(window, glfw_size_cb);
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		if(draw_frame(&vk)) {
			log_error("Frame draw failed");
			break;
		};
	}
	vkDeviceWaitIdle(vk.logicdev.handle);

	destroy_vk(vk);
	log_debug("Terminating glfw");
	glfwDestroyWindow(window);
	glfwTerminate();
	log_debug("Shutting Down");
	return VGFX_SUCCESS;
}
