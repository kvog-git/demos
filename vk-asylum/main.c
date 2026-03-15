// Vulkan demo
//
// SPDX-License-Identifier: 0BSD
//

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdlib.h>
#include <stdio.h>
#include <vulkan/vulkan.h>
#include "vkutil.h"

#define COUNTOF(_Arr) (sizeof(_Arr) / sizeof((_Arr)[0]))
#define DIE(...) do { printf(__VA_ARGS__); printf("\n"); exit(1); } while (0);
#define ASSERT(_Expr) do { if (!(_Expr)) { DIE("%s:%d: " #_Expr, __FILE__, __LINE__); } } while (0);

#define WND_W 1024
#define WND_H 768

int main(int argc, char* argv[])
{
	(void)argc; (void)argv;
	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window* wnd = SDL_CreateWindow("vk-asylum", WND_W, WND_H, SDL_WINDOW_VULKAN);
	ASSERT(wnd);

	VkResult r = VK_SUCCESS;

	VkInstance vk = 0;
	{
		VkApplicationInfo vkai = (VkApplicationInfo){
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.apiVersion = VK_API_VERSION_1_4,
		};
		VkInstanceCreateInfo vkici = (VkInstanceCreateInfo){
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &vkai,
		};

		vkici.ppEnabledExtensionNames = SDL_Vulkan_GetInstanceExtensions(&vkici.enabledExtensionCount);
		printf("Instance extensions requested by SDL:\n");
		for (uint32_t i = 0; i < vkici.enabledExtensionCount; ++i) {
			printf("	%s\n", vkici.ppEnabledExtensionNames[i]);
		}

#ifdef __APPLE__
		// Required for instance extension VK_KHR_portability_enumeration (MoltenVK)
		vkici.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

		if ((r = vkCreateInstance(&vkici, 0, &vk)) != VK_SUCCESS) {
			DIE("Failed to create Vulkan instance: %s", VkResultToString(r));
		}
	}

	VkSurfaceKHR vk_surf = 0;
	if (!SDL_Vulkan_CreateSurface(wnd, vk, 0, &vk_surf)) {
		DIE("Failed to create Vulkan surface: %s", SDL_GetError());
	}

	VkPhysicalDevice vk_pdev = 0;
	{
		uint32_t vk_pdev_count = 0;
		vkEnumeratePhysicalDevices(vk, &vk_pdev_count, 0);
		if (vk_pdev_count < 1) {
			DIE("No Vulkan-capable devices available");
		}

		VkPhysicalDevice* vk_pdevs = calloc(vk_pdev_count, sizeof(VkPhysicalDevice));
		vkEnumeratePhysicalDevices(vk, &vk_pdev_count, vk_pdevs);

		// Just pick the first available physical device
		vk_pdev = vk_pdevs[0];

		printf("Vulkan devices:\n");
		for (uint32_t i = 0; i < vk_pdev_count; ++i) {
			VkPhysicalDeviceProperties vk_pdev_props = { 0 };
			vkGetPhysicalDeviceProperties(vk_pdev, &vk_pdev_props);
			printf("  %s\n", vk_pdev_props.deviceName);
		}

		free(vk_pdevs);
	}

	uint32_t vk_qf_index = (uint32_t)-1;
	{
		uint32_t vk_qf_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(vk_pdev, &vk_qf_count, 0);
		ASSERT(vk_qf_count > 0);

		VkQueueFamilyProperties* vk_qfs = calloc(vk_qf_count, sizeof(VkQueueFamilyProperties));
		vkGetPhysicalDeviceQueueFamilyProperties(vk_pdev, &vk_qf_count, vk_qfs);

		// Select queue family with graphics capabilities
		for (uint32_t i = 0; i < vk_qf_count; ++i) {
			if (vk_qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				vk_qf_index = i;
				break;
			}
		}

		free(vk_qfs);
	}
	ASSERT(vk_qf_index != (uint32_t)-1);

	VkDevice vk_dev = 0;
	{
		float priority = 1.0f;
		VkDeviceQueueCreateInfo queue_create = (VkDeviceQueueCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex = vk_qf_index,
			.queueCount = 1,
			.pQueuePriorities = &priority,
		};

		const char* device_extensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, };
		VkDeviceCreateInfo device_create = (VkDeviceCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &queue_create,
			.enabledExtensionCount = COUNTOF(device_extensions),
			.ppEnabledExtensionNames = device_extensions,
		};

		if ((r = vkCreateDevice(vk_pdev, &device_create, 0, &vk_dev)) != VK_SUCCESS) {
			DIE("Failed to create logical device: %s", VkResultToString(r));
		}
	}

	VkQueue vk_graphics_queue = 0;
	vkGetDeviceQueue(vk_dev, vk_qf_index, 0, &vk_graphics_queue);

	VkSwapchainKHR vk_swp = 0;
	VkExtent2D vk_extent;
	{
		VkSurfaceCapabilitiesKHR surf_caps = { 0 };
		if ((r = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk_pdev, vk_surf, &surf_caps)) != VK_SUCCESS) {
			DIE("Failed to get device surface capabilities: %s", VkResultToString(r));
		}

		vk_extent = surf_caps.currentExtent;
		if (vk_extent.width == UINT32_MAX || vk_extent.height == UINT32_MAX) {
			vk_extent.width = WND_W;
			vk_extent.height = WND_H;
		}

		VkSwapchainCreateInfoKHR swapchain_create = (VkSwapchainCreateInfoKHR){
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = vk_surf,
			.minImageCount = 3,
			.imageFormat = VK_FORMAT_B8G8R8A8_SRGB,
			.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
			.imageExtent = vk_extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.preTransform = surf_caps.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = VK_PRESENT_MODE_FIFO_KHR,
			.clipped = VK_TRUE,
			.oldSwapchain = VK_NULL_HANDLE,
		};
		if ((r = vkCreateSwapchainKHR(vk_dev, &swapchain_create, 0, &vk_swp)) != VK_SUCCESS) {
			DIE("Failed to create swapchain: %s", VkResultToString(r));
		}
	}

	uint32_t vk_swap_image_count = 0;
	VkImage* vk_swap_images = 0;
	VkImageView* vk_swap_image_views = 0;
	{
		vkGetSwapchainImagesKHR(vk_dev, vk_swp, &vk_swap_image_count, 0);
		ASSERT(vk_swap_image_count > 0);
		vk_swap_images = calloc(vk_swap_image_count, sizeof(VkImage));
		vkGetSwapchainImagesKHR(vk_dev, vk_swp, &vk_swap_image_count, vk_swap_images);

		vk_swap_image_views = calloc(vk_swap_image_count, sizeof(VkImageView));
		for (uint32_t i = 0; i < vk_swap_image_count; ++i) {
			VkImageViewCreateInfo image_view_create = (VkImageViewCreateInfo){
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.image = vk_swap_images[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = VK_FORMAT_B8G8R8A8_SRGB,
				.components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
				.components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
				.components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
				.components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
				.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.subresourceRange.baseMipLevel = 0,
				.subresourceRange.levelCount = 1,
				.subresourceRange.baseArrayLayer = 0,
				.subresourceRange.layerCount = 1,
			};
			if ((r = vkCreateImageView(vk_dev, &image_view_create, 0, &vk_swap_image_views[i])) != VK_SUCCESS) {
				DIE("Failed to create image view #%u: %s", i, VkResultToString(r));
			}
		}
	}

	VkRenderPass vk_pass = 0;
	{
		VkAttachmentDescription attachment = (VkAttachmentDescription){
			.format = VK_FORMAT_B8G8R8A8_SRGB,
			.samples = VK_SAMPLE_COUNT_1_BIT,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		};
		VkAttachmentReference color_ref = (VkAttachmentReference){
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};
		VkSubpassDescription subpass = (VkSubpassDescription){
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount = 1,
			.pColorAttachments = &color_ref,
		};
		VkSubpassDependency dep = (VkSubpassDependency){
			.srcSubpass = VK_SUBPASS_EXTERNAL,
			.dstSubpass = 0,
			.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
			.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
		};
		VkRenderPassCreateInfo render_pass_create = (VkRenderPassCreateInfo){
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.attachmentCount = 1,
			.pAttachments = &attachment,
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = 1,
			.pDependencies = &dep,
		};
		if ((r = vkCreateRenderPass(vk_dev, &render_pass_create, 0, &vk_pass)) != VK_SUCCESS) {
			DIE("Failed to create render pass: %s", VkResultToString(r));
		}
	}

	VkFramebuffer* vk_framebuffers = calloc(vk_swap_image_count, sizeof(VkFramebuffer));
	{
		for (uint32_t i = 0; i < vk_swap_image_count; ++i) {
			VkImageView attachments[] = { vk_swap_image_views[i] };
			VkFramebufferCreateInfo framebuffer_create = (VkFramebufferCreateInfo){
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass = vk_pass,
				.attachmentCount = 1,
				.pAttachments = attachments,
				.width = vk_extent.width,
				.height = vk_extent.height,
				.layers = 1,
			};
			if ((r = vkCreateFramebuffer(vk_dev, &framebuffer_create, 0, &vk_framebuffers[i])) != VK_SUCCESS) {
				DIE("Failed to create render pass #%u: %s", i, VkResultToString(r));
			}
		}
	}

	VkPipeline vk_pipeline = 0;
	{
		VkPipelineLayoutCreateInfo pipeline_layout_create = (VkPipelineLayoutCreateInfo){
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		};
		VkPipelineLayout pipeline_layout = 0;
		if ((r = vkCreatePipelineLayout(vk_dev, &pipeline_layout_create, 0, &pipeline_layout)) != VK_SUCCESS) {
			DIE("Failed to create pipeline layout: %s", VkResultToString(r));
		}
	}

	VkCommandPool vk_cmd_pool = 0;
	{
		VkCommandPoolCreateInfo cmd_pool_create = (VkCommandPoolCreateInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.queueFamilyIndex = vk_qf_index,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		};
		if ((r = vkCreateCommandPool(vk_dev, &cmd_pool_create, 0, &vk_cmd_pool)) != VK_SUCCESS) {
			DIE("Failed to create command pool: %s", VkResultToString(r));
		}
	}

	VkCommandBuffer* vk_cmd_buffers = calloc(vk_swap_image_count, sizeof(VkCommandBuffer));
	{
		VkCommandBufferAllocateInfo cmd_buffer_info = (VkCommandBufferAllocateInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.commandPool = vk_cmd_pool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = vk_swap_image_count,
		};
		if ((r = vkAllocateCommandBuffers(vk_dev, &cmd_buffer_info, vk_cmd_buffers)) != VK_SUCCESS) {
			DIE("Failed to allocate command buffers: %s", VkResultToString(r));
		}
	}

	for (uint32_t i = 0; i < vk_swap_image_count; ++i) {
		VkCommandBufferBeginInfo cmd_buffer_begin_info = (VkCommandBufferBeginInfo){
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		};
		if ((r = vkBeginCommandBuffer(vk_cmd_buffers[i], &cmd_buffer_begin_info)) != VK_SUCCESS) {
			DIE("Failed to begin command buffer for swapchain image #%u: %s", i, VkResultToString(r));
		}

		VkClearValue clear_val = (VkClearValue){
			.color.float32[0] = 1.0f,
			.color.float32[1] = 0.0f,
			.color.float32[2] = 1.0f,
			.color.float32[3] = 1.0f,
		};
		VkRenderPassBeginInfo render_pass_begin_info = (VkRenderPassBeginInfo){
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass = vk_pass,
			.framebuffer = vk_framebuffers[i],
			.renderArea.extent = vk_extent,
			.clearValueCount = 1,
			.pClearValues = &clear_val
		};
		vkCmdBeginRenderPass(vk_cmd_buffers[i], &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);

		//

		vkCmdEndRenderPass(vk_cmd_buffers[i]);
		vkEndCommandBuffer(vk_cmd_buffers[i]);
	}

	VkSemaphore vk_image_available = 0;
	VkSemaphore vk_render_finished = 0;
	{
		VkSemaphoreCreateInfo semaphore_create = (VkSemaphoreCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};
		vkCreateSemaphore(vk_dev, &semaphore_create, 0, &vk_image_available);
		vkCreateSemaphore(vk_dev, &semaphore_create, 0, &vk_render_finished);
	}

	bool running = true;
	while (running) {
		SDL_Event evt;
		while (SDL_PollEvent(&evt)) {
			switch (evt.type) {
				case SDL_EVENT_QUIT: {
					running = false;
				} break;
			}
		}

		uint32_t next_image_idx = 0;
		vkAcquireNextImageKHR(vk_dev, vk_swp, UINT64_MAX, vk_image_available, VK_NULL_HANDLE, &next_image_idx);

		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSubmitInfo submit_info = (VkSubmitInfo){
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &vk_image_available,
			.pWaitDstStageMask = wait_stages,
			.commandBufferCount = 1,
			.pCommandBuffers = &vk_cmd_buffers[next_image_idx],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &vk_render_finished,
		};
		if ((r = vkQueueSubmit(vk_graphics_queue, 1, &submit_info, VK_NULL_HANDLE)) != VK_SUCCESS) {
			DIE("Failed to submit graphics queue: %s", VkResultToString(r));
		}

		VkPresentInfoKHR present_info = (VkPresentInfoKHR){
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &vk_render_finished,
			.swapchainCount = 1,
			.pSwapchains = &vk_swp,
			.pImageIndices = &next_image_idx,
		};
		if ((r = vkQueuePresentKHR(vk_graphics_queue, &present_info)) != VK_SUCCESS) {
			DIE("Failed to submit present queue: %s", VkResultToString(r));
		}
	}
}

