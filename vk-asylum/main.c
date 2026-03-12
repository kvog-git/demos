// Vulkan demo
//
// SPDX-License-Identifier: 0BSD
//

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>

#define COUNTOF(_Arr) (sizeof(_Arr) / sizeof((_Arr)[0]))
#define DIE(...) do { printf(__VA_ARGS__); printf("\n"); exit(1); } while (0);
#define ASSERT(_Expr) do { if (!(_Expr)) { DIE("%s:%d: " #_Expr, __FILE__, __LINE__); } } while (0);

int main(int argc, char* argv[])
{
	(void)argc; (void)argv;
	SDL_Init(SDL_INIT_VIDEO);

	SDL_Window* wnd = SDL_CreateWindow("vk-asylum", 1024, 768, SDL_WINDOW_VULKAN);
	ASSERT(wnd);

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
}
