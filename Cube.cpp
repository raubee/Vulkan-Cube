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

#define ASSERT_VK(res) if (vk_res != VK_SUCCESS){return -1;}
#define ASSERT(res) if (result != 0){ exit(-1);}

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
	std::vector<VkImage> images;
	std::vector<VkImageView> image_views;
	std::vector<VkFramebuffer> image_buffers;
	uint32_t width;
	uint32_t height;
};

static VkResult vk_res;
static ApplicationInfo infos;
static VkInstance instance;
static VkDevice device;
static VkCommandPool command_pool;
static VkCommandBuffer command_buffer;
static VkSurfaceKHR surface;
static VkSwapchainKHR swap_chain;

static VkFormat swap_chain_format;
static VkPipelineLayout pipeline_layout;
static VkRenderPass render_pass;
static VkPipeline graphics_pipeline;

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
	ASSERT_VK(vk_res);

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
	ASSERT_VK(vk_res);

	uint32_t devicesCount;

	vk_res = vkEnumeratePhysicalDevices(instance, &devicesCount, nullptr);
	ASSERT_VK(vk_res);

	infos.gpus.resize(devicesCount);

	vk_res = vkEnumeratePhysicalDevices(instance, &devicesCount, infos.gpus.data());
	ASSERT_VK(vk_res);

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
	ASSERT_VK(vk_res);

	return  0;
}

int createCommandBuffer()
{
	VkCommandPoolCreateInfo pool_info = {};
	pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	pool_info.queueFamilyIndex = infos.graphic_queue_family_index;
	pool_info.pNext = nullptr;
	pool_info.flags = 0;

	vk_res = vkCreateCommandPool(device, &pool_info, nullptr, &command_pool);
	ASSERT_VK(vk_res);

	VkCommandBufferAllocateInfo command_buffer_info = { };
	command_buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	command_buffer_info.pNext = nullptr;
	command_buffer_info.commandBufferCount = 1;
	command_buffer_info.commandPool = command_pool;
	command_buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	vk_res = vkAllocateCommandBuffers(device, &command_buffer_info, &command_buffer);
	ASSERT_VK(vk_res);

	return 0;
}

int createSwapChain()
{
	uint32_t formatCount;
	vk_res = vkGetPhysicalDeviceSurfaceFormatsKHR(infos.gpus[0], surface, &formatCount, nullptr);
	ASSERT_VK(vk_res);

	VkSurfaceFormatKHR* surfFormats = static_cast<VkSurfaceFormatKHR*>(malloc(formatCount * sizeof(VkSurfaceFormatKHR)));
	vk_res = vkGetPhysicalDeviceSurfaceFormatsKHR(infos.gpus[0], surface, &formatCount, surfFormats);
	ASSERT_VK(vk_res);

	VkSwapchainCreateInfoKHR swap_chain_info = {};
	swap_chain_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swap_chain_info.pNext = nullptr;
	swap_chain_info.flags = 0;
	swap_chain_info.surface = surface;
	swap_chain_format = swap_chain_info.imageFormat = surfFormats[0].format;

	VkSurfaceCapabilitiesKHR capabilities;

	vk_res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(infos.gpus[0], surface, &capabilities);
	ASSERT_VK(vk_res);

	swap_chain_info.minImageCount = capabilities.minImageCount;
	infos.width = swap_chain_info.imageExtent.width = capabilities.currentExtent.width;
	infos.height = swap_chain_info.imageExtent.height = capabilities.currentExtent.height;
	swap_chain_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	swap_chain_info.preTransform = capabilities.currentTransform;
	swap_chain_info.imageArrayLayers = 1;
	swap_chain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swap_chain_info.queueFamilyIndexCount = 0;
	swap_chain_info.pQueueFamilyIndices = nullptr;
	swap_chain_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swap_chain_info.imageColorSpace = surfFormats[0].colorSpace;
	swap_chain_info.clipped = true;
	swap_chain_info.oldSwapchain = VK_NULL_HANDLE;

	vk_res = vkCreateSwapchainKHR(device, &swap_chain_info, nullptr, &swap_chain);
	ASSERT_VK(vk_res);

	uint32_t swapImageCount;

	vk_res = vkGetSwapchainImagesKHR(device, swap_chain, &swapImageCount, nullptr);
	ASSERT_VK(vk_res);

	infos.images.resize(swapImageCount);

	vk_res = vkGetSwapchainImagesKHR(device, swap_chain, &swapImageCount, infos.images.data());
	ASSERT_VK(vk_res);

	infos.image_views.resize(swapImageCount);
	infos.image_buffers.resize(swapImageCount);

	VkImageViewCreateInfo i_info = {};
	i_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	i_info.pNext = nullptr;
	i_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	i_info.format = swap_chain_info.imageFormat;
	i_info.flags = 0;

	VkFramebufferCreateInfo buf_info = { };
	buf_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	buf_info.pNext = nullptr;
	buf_info.flags = 0;
	buf_info.attachmentCount = 1;
	buf_info.width = infos.width;
	buf_info.height = infos.height;
	buf_info.layers = 1;

	for (int i = 0; i < swapImageCount; i++)
	{
		i_info.image = infos.images[i];

		vk_res = vkCreateImageView(device, &i_info, nullptr, &infos.image_views[i]);
		ASSERT_VK(vk_res);

		buf_info.pAttachments = &infos.image_views[i];

		vk_res = vkCreateFramebuffer(device, &buf_info, nullptr, &infos.image_buffers[i]);
		ASSERT_VK(vk_res);
	}

	return 0;
}

int createGraphicsPipeline()
{
	VkPipelineLayoutCreateInfo pipe_info = { };
	pipe_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipe_info.flags = 0;
	pipe_info.pNext = nullptr;
	pipe_info.setLayoutCount = 0;
	pipe_info.pSetLayouts = nullptr;
	pipe_info.pushConstantRangeCount = 0;
	pipe_info.pPushConstantRanges = nullptr;

	vkCreatePipelineLayout(device, &pipe_info, nullptr, &pipeline_layout);

	VkAttachmentDescription attachment = {};

	attachment.flags = 0;
	attachment.format = swap_chain_format;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;

	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_info.flags = 0;
	render_pass_info.pNext = nullptr;
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &attachment;
	render_pass_info.subpassCount = 0;
	render_pass_info.pSubpasses = nullptr;
	render_pass_info.dependencyCount = 0;

	vk_res = vkCreateRenderPass(device, &render_pass_info, nullptr, &render_pass);
	ASSERT_VK(vk_res);

	VkGraphicsPipelineCreateInfo graph_info = {};
	graph_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graph_info.flags = 0;
	graph_info.pNext = nullptr;
	graph_info.layout = pipeline_layout;
	graph_info.renderPass = render_pass;

	vk_res = vkCreateGraphicsPipelines(device, nullptr, 1, &graph_info, nullptr, &graphics_pipeline);
	ASSERT_VK(vk_res);

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
	ASSERT(result);

	result = init_surface();
	ASSERT(result);

	result = initlogicalDevice();
	ASSERT(result);

	result = createSwapChain();
	ASSERT(result);

	result = createCommandBuffer();
	ASSERT(result);

	result = createGraphicsPipeline();
	ASSERT(result);

	draw();
}

void ShutdownVulkan()
{
	vkDestroyPipeline(device, graphics_pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
	vkDestroyRenderPass(device, render_pass, nullptr);
	vkDestroyCommandPool(device, command_pool, nullptr);

	for (auto image_view : infos.image_views)
	{
		vkDestroyImageView(device, image_view, nullptr);
	}

	for (auto fb : infos.image_buffers)
	{
		vkDestroyFramebuffer(device, fb, nullptr);
	}

	vkDestroySwapchainKHR(device, swap_chain, nullptr);
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