#ifndef UNICODE
#define UNICODE
#endif

#define VK_USE_PLATFORM_WIN32_KHR

#include <vulkan/vulkan.h>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <string>
#include <windows.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

#define APP_NAME "Vulkan Cube";

struct ApplicationInfo
{
	HINSTANCE connection;
	HWND window;
	std::vector<VkPhysicalDevice> gpus;
	std::vector<const char*> instance_extension_names;
	std::vector<const char*> device_extension_names;
	std::vector<VkQueueFamilyProperties> queue_props;
	uint32_t queue_family_index;
	uint32_t queue_family_count;
	uint32_t graphic_queue_family_index;
	uint32_t present_queue_family_index;
	std::vector<VkImage> buffers;
	std::vector<VkImageView> image_views;
};

static VkResult vk_res;
static ApplicationInfo infos;
static VkInstance instance;
static VkDevice device;
static VkCommandPool command_pool;
static VkCommandBuffer command_buffer;
static VkSurfaceKHR surface;
static VkSwapchainKHR swap_chain;

void init_app_extensions()
{
	infos.instance_extension_names.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
	infos.instance_extension_names.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
}

void init_device_extension()
{
	infos.device_extension_names.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

int init_surface()
{
	VkWin32SurfaceCreateInfoKHR surface_info = { };
	surface_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surface_info.pNext = nullptr;
	surface_info.hinstance = infos.connection;
	surface_info.hwnd = infos.window;

	vk_res = vkCreateWin32SurfaceKHR(instance, &surface_info, nullptr, &surface);

	if (vk_res != VK_SUCCESS)
	{
		return -1;
	}

	return 0;
}

int init_instance()
{
	VkApplicationInfo application_info = {};
	application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	application_info.pApplicationName = APP_NAME;
	application_info.applicationVersion = 1;
	application_info.apiVersion = VK_API_VERSION_1_0;
	application_info.pEngineName = APP_NAME;
	application_info.engineVersion = 1;

	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pNext = nullptr;
	create_info.flags = 0;
	create_info.pApplicationInfo = &application_info;
	create_info.enabledLayerCount = 0;
	create_info.ppEnabledLayerNames = nullptr;
	create_info.enabledExtensionCount = infos.instance_extension_names.size();
	create_info.ppEnabledExtensionNames = infos.instance_extension_names.data();

	vk_res = vkCreateInstance(&create_info, nullptr, &instance);

	if (vk_res != VK_SUCCESS)
	{
		return -1;
	}

	uint32_t devicesCount;

	vk_res = vkEnumeratePhysicalDevices(instance, &devicesCount, nullptr);

	if (vk_res != VK_SUCCESS)
	{
		return -1;
	}

	infos.gpus.resize(devicesCount);

	vk_res = vkEnumeratePhysicalDevices(instance, &devicesCount, infos.gpus.data());

	if (vk_res != VK_SUCCESS)
	{
		return -1;
	}

	for (int i = 0; i < infos.gpus.size(); i++)
	{
		VkPhysicalDeviceProperties prop;
		vkGetPhysicalDeviceProperties(infos.gpus[i], &prop);
	}

	return 0;
}

int initlogicalDevice()
{
	vkGetPhysicalDeviceQueueFamilyProperties(infos.gpus[0], &infos.queue_family_count, nullptr);

	infos.queue_props.resize(infos.queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(infos.gpus[0], &infos.queue_family_count, infos.queue_props.data());

	VkBool32* pSupport = (VkBool32*)malloc(infos.queue_family_count * sizeof(VkBool32));
	for (uint32_t i = 0; i < infos.queue_family_count; i++) {
		vkGetPhysicalDeviceSurfaceSupportKHR(infos.gpus[0], i, surface,
			&pSupport[i]);
	}

	infos.graphic_queue_family_index = UINT32_MAX;
	infos.present_queue_family_index = UINT32_MAX;

	for (uint32_t i = 0; i < infos.queue_family_count; i++)
	{
		if (infos.queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			if (infos.graphic_queue_family_index == UINT32_MAX)
			{
				infos.graphic_queue_family_index = i;
			}

			if (pSupport[i] == VK_TRUE)
			{
				infos.graphic_queue_family_index = i;
				infos.present_queue_family_index = i;
				break;
			}
		}
	}

	if (infos.present_queue_family_index == UINT32_MAX) {
		// If didn't find a queue that supports both graphics and present, then
		// find a separate present queue.
		for (size_t i = 0; i < infos.queue_family_count; ++i)
			if (pSupport[i] == VK_TRUE) {
				infos.present_queue_family_index = i;
				break;
			}
	}

	free(pSupport);

	VkDeviceQueueCreateInfo queue_info = {};
	queue_info.flags = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info.pNext = nullptr;
	queue_info.pQueuePriorities = new float[1]{ 0.0 };
	queue_info.queueCount = 1;

	VkDeviceCreateInfo device_info = {};

	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.pNext = nullptr;
	device_info.queueCreateInfoCount = 1;
	device_info.pQueueCreateInfos = &queue_info;
	device_info.enabledExtensionCount = infos.device_extension_names.size();
	device_info.ppEnabledExtensionNames = infos.device_extension_names.data();
	device_info.enabledLayerCount = 0;
	device_info.ppEnabledLayerNames = nullptr;

	vk_res = vkCreateDevice(infos.gpus[0], &device_info, nullptr, &device);

	if (vk_res != VK_SUCCESS)
	{
		return -1;
	}

	return  0;
}

int CreateCommandBuffer()
{
	VkCommandPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = infos.graphic_queue_family_index;
	pool_info.pNext = nullptr;
	pool_info.flags = 0;

	vk_res = vkCreateCommandPool(device, &pool_info, nullptr, &command_pool);

	if (vk_res != VK_SUCCESS)
	{
		return -1;
	}

	VkCommandBufferAllocateInfo command_buffer_info = { };
	command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_info.pNext = nullptr;
	command_buffer_info.commandBufferCount = 1;
	command_buffer_info.commandPool = command_pool;
	command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	vk_res = vkAllocateCommandBuffers(device, &command_buffer_info, &command_buffer);

	if (vk_res != VK_SUCCESS)
	{
		return -1;
	}

	return 0;
}

int CreateSwapChain()
{
	VkSwapchainCreateInfoKHR swap_chain_info = {};
	swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swap_chain_info.pNext = nullptr;
	swap_chain_info.surface = surface;
	swap_chain_info.imageFormat = VK_FORMAT_D24_UNORM_S8_UINT;

	VkSurfaceCapabilitiesKHR capabilities;

	vk_res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(infos.gpus[0], surface, &capabilities);

	if (vk_res != VK_SUCCESS)
	{
		return -1;
	}

	swap_chain_info.minImageCount = capabilities.minImageCount;
	swap_chain_info.imageExtent.width = 200;
	swap_chain_info.imageExtent.height = 200;
	swap_chain_info.preTransform = capabilities.currentTransform;

	vk_res = vkCreateSwapchainKHR(device, &swap_chain_info, nullptr, &swap_chain);

	if (vk_res != VK_SUCCESS)
	{
		return -1;
	}

	uint32_t swapImageCount;

	vk_res = vkGetSwapchainImagesKHR(device, swap_chain, &swapImageCount, nullptr);

	if (vk_res != VK_SUCCESS)
	{
		return -1;
	}

	infos.buffers.resize(swapImageCount);

	vk_res = vkGetSwapchainImagesKHR(device, swap_chain, &swapImageCount, infos.buffers.data());

	if (vk_res != VK_SUCCESS)
	{
		return -1;
	}

	infos.image_views.resize(swapImageCount);

	VkImageViewCreateInfo i_info = {};
	i_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	i_info.pNext = nullptr;
	i_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	i_info.format = swap_chain_info.imageFormat;
	i_info.flags = 0;
		
	for (int i = 0; i < swapImageCount; i++)
	{
		i_info.image = infos.buffers[i];
		
		vk_res = vkCreateImageView(device, &i_info, nullptr, &infos.image_views[i]);

		if (vk_res != VK_SUCCESS)
		{
			return -1;
		}
	}

	return 0;
}

void draw()
{
	VkCommandBufferBeginInfo cmd_begin_info = {};
	cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmd_begin_info.pNext = nullptr;
	cmd_begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
	cmd_begin_info.pInheritanceInfo = nullptr;

	vkBeginCommandBuffer(command_buffer, &cmd_begin_info);

	vkEndCommandBuffer(command_buffer);

	//vkQueueSubmit()
}

void setup_vulkan(HINSTANCE hInstance, HWND hwnd)
{
	OutputDebugString(L"[CubeVulkan] Starting Application...");

	infos.connection = hInstance;
	infos.window = hwnd;

	init_app_extensions();
	init_device_extension();

	int result = init_instance();

	if (result != 0)
	{
		exit(-1);
	}

	result = init_surface();

	if (result != 0)
	{
		exit(-1);
	}

	result = initlogicalDevice();

	if (result != 0)
	{
		exit(-1);
	}

	result = CreateSwapChain();

	if (result != 0)
	{
		exit(-1);
	}

	CreateCommandBuffer();

	if (result != 0)
	{
		exit(-1);
	}

	draw();
}

void ShutdownVulkan()
{
	vkDestroyCommandPool(device, command_pool, nullptr);
	vkDestroyDevice(device, nullptr);
	vkDestroyInstance(instance, nullptr);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR pCmdLine, int nCmdShow)
{
	// Register the window class.
	const wchar_t CLASS_NAME[] = L"Vulkan Cube";

	WNDCLASS wc = { };

	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	// Create the window.

	HWND hwnd = CreateWindowEx(
		0,                              // Optional window styles.
		CLASS_NAME,                     // Window class
		CLASS_NAME,    // Window text
		WS_OVERLAPPEDWINDOW,            // Window style

		// Size and position
		CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

		NULL,       // Parent window    
		NULL,       // Menu
		hInstance,  // Instance handle
		NULL        // Additional application data
	);

	if (hwnd == NULL)
	{
		exit(0);
	}

	ShowWindow(hwnd, nCmdShow);

	setup_vulkan(hInstance, hwnd);

	MSG msg = { };
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	exit(0);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_DESTROY:
		ShutdownVulkan();
		PostQuitMessage(0);
		return 0;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));

		EndPaint(hwnd, &ps);
	}
	return 0;

	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}