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

typedef struct Vec3 Vec3;
struct Vec3
{
    float x;
    float y;
    float z;
};

typedef struct Vertex Vertex;
struct Vertex
{
    Vec3 p;
    Vec3 c;
};

int main(int argc, char* argv[])
{
	(void)argc; (void)argv;
	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window* wnd = SDL_CreateWindow("vk-asylum", WND_W, WND_H, SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
	ASSERT(wnd);

	VkResult r = VK_SUCCESS;

	VkInstance vk = 0;
	{
		VkApplicationInfo vkai = (VkApplicationInfo){
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.apiVersion = VK_API_VERSION_1_4,
		};
        const char* layers[] = { "VK_LAYER_KHRONOS_validation" };
		VkInstanceCreateInfo vkici = (VkInstanceCreateInfo){
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &vkai,
            .enabledLayerCount = 1,
            .ppEnabledLayerNames = layers,
		};

		// Extensions required for the SDL window backend
		uint32_t num_sdl_extensions = 0;
		const char* const* sdl_extensions = SDL_Vulkan_GetInstanceExtensions(&num_sdl_extensions);

		const char* extensions[64] = { 0 };
		for (; vkici.enabledExtensionCount < num_sdl_extensions; ++vkici.enabledExtensionCount) {
			extensions[vkici.enabledExtensionCount] = sdl_extensions[vkici.enabledExtensionCount];
		}

#ifdef __APPLE__
		// Required for instance extension VK_KHR_portability_enumeration (MoltenVK)
		vkici.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		extensions[vkici.enabledExtensionCount++] = VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME;
#endif

		vkici.ppEnabledExtensionNames = extensions;
		printf("Instance extensions requested by SDL:\n");
		for (uint32_t i = 0; i < vkici.enabledExtensionCount; ++i) {
			printf("	%s\n", vkici.ppEnabledExtensionNames[i]);
		}

		if ((r = vkCreateInstance(&vkici, 0, &vk)) != VK_SUCCESS) {
			DIE("Failed to create Vulkan instance: %s", VkResultToString(r));
		}
	}

	VkSurfaceKHR vk_surf = 0;
	if (!SDL_Vulkan_CreateSurface(wnd, vk, 0, &vk_surf)) {
		DIE("Failed to create Vulkan surface: %s", SDL_GetError());
	}

	VkPhysicalDevice vk_pdev = VK_NULL_HANDLE;
	{
		uint32_t vk_pdev_count = 0;
		vkEnumeratePhysicalDevices(vk, &vk_pdev_count, 0);
		if (vk_pdev_count < 1) {
			DIE("No Vulkan-capable devices available");
		}

		VkPhysicalDevice* vk_pdevs = calloc(vk_pdev_count, sizeof(VkPhysicalDevice));
		vkEnumeratePhysicalDevices(vk, &vk_pdev_count, vk_pdevs);
        
        // Select the first device with dynamic rendering support

		// Just pick the first available physical device
		vk_pdev = vk_pdevs[0];

		printf("Vulkan devices:\n");
		for (uint32_t i = 0; i < vk_pdev_count; ++i) {
            VkPhysicalDeviceProperties vk_pdev_props = { 0 };
            vkGetPhysicalDeviceProperties(vk_pdevs[i], &vk_pdev_props);
            VkPhysicalDeviceDynamicRenderingFeatures vk_pdev_dr = (VkPhysicalDeviceDynamicRenderingFeatures){
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            };
            VkPhysicalDeviceFeatures2 vk_pdev_features = (VkPhysicalDeviceFeatures2){
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = &vk_pdev_dr,
            };
            vkGetPhysicalDeviceFeatures2(vk_pdevs[i], &vk_pdev_features);
			printf("  %s\n", vk_pdev_props.deviceName);
            printf("    dynamic rendering? %s\n", vk_pdev_dr.dynamicRendering ? "YES" : "NO");
		}

		free(vk_pdevs);
	}
    ASSERT(vk_pdev != VK_NULL_HANDLE);

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
        
        VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering = (VkPhysicalDeviceDynamicRenderingFeatures){
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
            .dynamicRendering = VK_TRUE,
        };

        const char* device_extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
#ifdef __APPLE__
            "VK_KHR_portability_subset",
#endif
        };
		VkDeviceCreateInfo device_create = (VkDeviceCreateInfo){
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.queueCreateInfoCount = 1,
			.pQueueCreateInfos = &queue_create,
			.enabledExtensionCount = COUNTOF(device_extensions),
			.ppEnabledExtensionNames = device_extensions,
            .pNext = &dynamic_rendering,
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
    VkFence* vk_swap_fences = 0;
	{
		vkGetSwapchainImagesKHR(vk_dev, vk_swp, &vk_swap_image_count, 0);
		ASSERT(vk_swap_image_count > 0);
		vk_swap_images = calloc(vk_swap_image_count, sizeof(VkImage));
		vkGetSwapchainImagesKHR(vk_dev, vk_swp, &vk_swap_image_count, vk_swap_images);
        vk_swap_fences = calloc(vk_swap_image_count, sizeof(VkFence));

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
            
            VkFenceCreateInfo fence_create = (VkFenceCreateInfo){
                .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                .flags = VK_FENCE_CREATE_SIGNALED_BIT, // start in signaled state
            };
            if ((r = vkCreateFence(vk_dev, &fence_create, 0, &vk_swap_fences[i])) != VK_SUCCESS) {
                DIE("Failed to create swap image fence #%u: %s", i, VkResultToString(r));
            }
		}
	}
    
    VkShaderModule vk_vshader = VK_NULL_HANDLE;
    VkShaderModule vk_fshader = VK_NULL_HANDLE;
    {
#include "shaders.h"
        VkShaderModuleCreateInfo shader_create = (VkShaderModuleCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        };
        shader_create.codeSize = sizeof(triangle_vert_spv);
        shader_create.pCode = triangle_vert_spv;
        if ((r = vkCreateShaderModule(vk_dev, &shader_create, 0, &vk_vshader)) != VK_SUCCESS) {
            DIE("Failed to create vertex shader module: %s", VkResultToString(r));
        }
        shader_create.codeSize = sizeof(triangle_frag_spv);
        shader_create.pCode = triangle_frag_spv;
        if ((r = vkCreateShaderModule(vk_dev, &shader_create, 0, &vk_fshader)) != VK_SUCCESS) {
            DIE("Failed to create fragment shader module: %s", VkResultToString(r));
        }
    }
    
    VkBuffer vk_vbuf = VK_NULL_HANDLE;
    {
        Vertex vdata[] = {
            (Vertex){ .p = (Vec3){  0.0f, -0.5f, 0.0f, }, .c = (Vec3){ 1.0f, 0.0f, 0.0f } },
            (Vertex){ .p = (Vec3){  0.5f,  0.5f, 0.0f, }, .c = (Vec3){ 0.0f, 1.0f, 0.0f } },
            (Vertex){ .p = (Vec3){ -0.5f,  0.5f, 0.0f, }, .c = (Vec3){ 0.0f, 0.0f, 1.0f } },
        };
        VkBufferCreateInfo buffer_create = (VkBufferCreateInfo){
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = sizeof(vdata),
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        };
        if ((r = vkCreateBuffer(vk_dev, &buffer_create, 0, &vk_vbuf)) != VK_SUCCESS) {
            DIE("Failed to create vertex buffer: %s", VkResultToString(r));
        }
        
        VkMemoryRequirements memreq;
        vkGetBufferMemoryRequirements(vk_dev, vk_vbuf, &memreq);
        
        VkMemoryAllocateInfo alloc_info = (VkMemoryAllocateInfo){
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memreq.size,
            .memoryTypeIndex = UINT32_MAX,
        };
        
        VkPhysicalDeviceMemoryProperties memprops;
        vkGetPhysicalDeviceMemoryProperties(vk_pdev, &memprops);
        for (uint32_t i = 0; i < memprops.memoryTypeCount; ++i) {
            if (memreq.memoryTypeBits & (1 << i)) {
                const uint32_t needed = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                if ((memprops.memoryTypes[i].propertyFlags & needed) == needed) {
                    alloc_info.memoryTypeIndex = i;
                    break;
                }
            }
        }
        ASSERT(alloc_info.memoryTypeIndex != UINT32_MAX);
        
        VkDeviceMemory vkmem;
        if ((r = vkAllocateMemory(vk_dev, &alloc_info, 0, &vkmem)) != VK_SUCCESS) {
            DIE("Failed to allocate GPU memory for vertex buffer: %s", VkResultToString(r));
        }
        
        vkBindBufferMemory(vk_dev, vk_vbuf, vkmem, 0);
        
        void* data = 0;
        vkMapMemory(vk_dev, vkmem, 0, sizeof(vdata), 0, &data);
        memcpy(data, vdata, sizeof(vdata));
        vkUnmapMemory(vk_dev, vkmem);
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
        
        VkPipelineShaderStageCreateInfo stages[] = {
            (VkPipelineShaderStageCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_VERTEX_BIT,
                .module = vk_vshader,
                .pName = "main",
            },
            (VkPipelineShaderStageCreateInfo){
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                .module = vk_fshader,
                .pName = "main",
            }
        };
        
        VkPipelineInputAssemblyStateCreateInfo input_assembly = (VkPipelineInputAssemblyStateCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        };
        
        VkViewport viewport = (VkViewport){
            .width = vk_extent.width,
            .height = vk_extent.height,
            .maxDepth = 1.0f,
        };
        
        VkRect2D scissor = (VkRect2D){
            .extent = vk_extent,
        };
        
        VkPipelineViewportStateCreateInfo viewport_state_create = (VkPipelineViewportStateCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor,
        };
        
        VkPipelineRasterizationStateCreateInfo raster_state_create = (VkPipelineRasterizationStateCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_NONE,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .lineWidth = 1.0,
        };
        
        VkPipelineMultisampleStateCreateInfo multisample_state = (VkPipelineMultisampleStateCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        };
        
        VkPipelineColorBlendAttachmentState blending_attach = (VkPipelineColorBlendAttachmentState){
            .colorWriteMask = 0x0F,
        };
        
        VkPipelineColorBlendStateCreateInfo blending = (VkPipelineColorBlendStateCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .attachmentCount = 1,
            .pAttachments = &blending_attach,
        };
        
        VkFormat color_format = VK_FORMAT_B8G8R8A8_SRGB;
        VkPipelineRenderingCreateInfo rendering = (VkPipelineRenderingCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
            .colorAttachmentCount = 1,
            .pColorAttachmentFormats = &color_format,
        };
        
        VkPipelineDepthStencilStateCreateInfo depth_stencil = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
            .depthTestEnable = VK_FALSE,
            .depthWriteEnable = VK_FALSE,
            .depthCompareOp = VK_COMPARE_OP_ALWAYS,
        };
        
        VkVertexInputBindingDescription input_binding = (VkVertexInputBindingDescription){
            .binding = 0,
            .stride = sizeof(Vertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
        
        VkVertexInputAttributeDescription input_attrs[] = {
            (VkVertexInputAttributeDescription){
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, p),
            },
            (VkVertexInputAttributeDescription){
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, c),
            },
        };
        
        VkPipelineVertexInputStateCreateInfo vertex_input = (VkPipelineVertexInputStateCreateInfo){
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &input_binding,
            .vertexAttributeDescriptionCount = COUNTOF(input_attrs),
            .pVertexAttributeDescriptions = input_attrs,
        };
        
        VkGraphicsPipelineCreateInfo pipeline_create = (VkGraphicsPipelineCreateInfo){
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .pNext = &rendering,
            .stageCount = COUNTOF(stages),
            .pStages = stages,
            .pInputAssemblyState = &input_assembly,
            .pViewportState = &viewport_state_create,
            .pRasterizationState = &raster_state_create,
            .pMultisampleState = &multisample_state,
            .pColorBlendState = &blending,
            .layout = pipeline_layout,
            .pVertexInputState = &vertex_input,
            .pDepthStencilState = &depth_stencil,
        };
        if ((r = vkCreateGraphicsPipelines(vk_dev, VK_NULL_HANDLE, 1, &pipeline_create, 0, &vk_pipeline)) != VK_SUCCESS) {
            DIE("Failed to create pipeline: %s", VkResultToString(r));
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
        
        VkImageMemoryBarrier vk_barrier = (VkImageMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .image = vk_swap_images[i],
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.levelCount = 1,
            .subresourceRange.layerCount = 1,
        };
        vkCmdPipelineBarrier(vk_cmd_buffers[i], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, 0, 0, 0, 1, &vk_barrier);
        
        VkRenderingAttachmentInfo vk_rendering_attachment = (VkRenderingAttachmentInfo){
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = vk_swap_image_views[i],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue.color = {{1.0f, 0.0f, 1.0f, 1.0f}},
        };
        VkRenderingInfo vk_rendering_info = (VkRenderingInfo){
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea.extent = vk_extent,
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &vk_rendering_attachment,
        };
        vkCmdBeginRendering(vk_cmd_buffers[i], &vk_rendering_info);
        
        vkCmdBindPipeline(vk_cmd_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);
        VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(vk_cmd_buffers[i], 0, 1, &vk_vbuf, &offset);
        vkCmdDraw(vk_cmd_buffers[i], 3, 1, 0, 0);
        
        vkCmdEndRendering(vk_cmd_buffers[i]);
        
        vk_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        vk_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        vk_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        vk_barrier.dstAccessMask = 0;
        vkCmdPipelineBarrier(vk_cmd_buffers[i], VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, 0, 0, 0, 1, &vk_barrier);

		if ((r = vkEndCommandBuffer(vk_cmd_buffers[i])) != VK_SUCCESS) {
			DIE("Failed to end command buffer for swapchain image #%u: %s", i, VkResultToString(r));
		}
	}

	VkSemaphore* vk_image_available = calloc(vk_swap_image_count, sizeof(VkSemaphore));
    VkSemaphore* vk_render_finished = calloc(vk_swap_image_count, sizeof(VkSemaphore));
	{
		VkSemaphoreCreateInfo semaphore_create = (VkSemaphoreCreateInfo){
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		};
        for (uint32_t i = 0; i < vk_swap_image_count; ++i) {
            vkCreateSemaphore(vk_dev, &semaphore_create, 0, &vk_image_available[i]);
            vkCreateSemaphore(vk_dev, &semaphore_create, 0, &vk_render_finished[i]);
        }
	}
    
#define MAX_FRAMES_IN_FLIGHT 2
    VkFence vk_frame_fences[MAX_FRAMES_IN_FLIGHT];
    {
        VkFenceCreateInfo create = (VkFenceCreateInfo){
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            if ((r = vkCreateFence(vk_dev, &create, 0, &vk_frame_fences[i])) != VK_SUCCESS) {
                DIE("Failed to create frame fence #%u: %s", i, VkResultToString(r));
            }
        }
    }
    
    uint32_t frame_idx = 0;

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
		vkAcquireNextImageKHR(vk_dev, vk_swp, UINT64_MAX, vk_image_available[frame_idx], VK_NULL_HANDLE, &next_image_idx);

        // Wait for GPU to finish using this swapchain image
        vkWaitForFences(vk_dev, 1, &vk_frame_fences[frame_idx], VK_TRUE, UINT64_MAX);
        vkResetFences(vk_dev, 1, &vk_frame_fences[frame_idx]);
        
		VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSubmitInfo submit_info = (VkSubmitInfo){
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &vk_image_available[frame_idx],
			.pWaitDstStageMask = wait_stages,
			.commandBufferCount = 1,
			.pCommandBuffers = &vk_cmd_buffers[next_image_idx],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &vk_render_finished[next_image_idx],
		};
		if ((r = vkQueueSubmit(vk_graphics_queue, 1, &submit_info, vk_frame_fences[frame_idx])) != VK_SUCCESS) {
			DIE("Failed to submit graphics queue: %s", VkResultToString(r));
		}

		VkPresentInfoKHR present_info = (VkPresentInfoKHR){
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &vk_render_finished[next_image_idx],
			.swapchainCount = 1,
			.pSwapchains = &vk_swp,
			.pImageIndices = &next_image_idx,
		};
		if ((r = vkQueuePresentKHR(vk_graphics_queue, &present_info)) != VK_SUCCESS) {
			DIE("Failed to submit present queue: %s", VkResultToString(r));
		}
        
        frame_idx = (frame_idx + 1) % MAX_FRAMES_IN_FLIGHT;
	}
    
    vkDeviceWaitIdle(vk_dev);
}

