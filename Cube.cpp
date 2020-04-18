#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#define VK_USE_PLATFORM_WIN32_KHR 
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <windows.h>

const int WIDTH = 800;
const int HEIGHT = 600;

const std::vector<const char*> validationLayers{
	"VK_LAYER_LUNARG_standard_validation"
};

#if NDEBUG
constexpr bool isEnableValidationLayers = true;
#else
constexpr bool isEnableValidationLayers = false;
#endif

#define ASSERT_VK(res) if (vk_res != VK_SUCCESS){return EXIT_FAILURE;}
#define ASSERT(res) if (result != EXIT_SUCCESS){ return EXIT_FAILURE;}
static VkResult vk_res = VK_SUCCESS;

#define APP_NAME "Vulkan Cube";

struct HWND_INFO
{
	HINSTANCE hInstance;
	HWND hWnd;
};

static HWND_INFO s_hInfos;

static VkInstance s_instance;

static std::vector<const char*> s_instanceExtensionNames;
static std::vector<const char*> s_deviceExtensionNames;

static uint32_t s_presentQueueFamilyIndex;
static uint32_t s_graphicQueueFamilyIndex;

static std::vector<VkPhysicalDevice> s_physicalDevices;
static VkPhysicalDevice s_physicalDevice;
static VkDevice s_logicalDevice;

static VkCommandPool s_commandPool;
static std::vector<VkCommandBuffer> s_commandBuffers;

static VkSurfaceKHR s_surfaceKHR;

static std::vector<VkImage> s_images;
static std::vector<VkImageView> s_imageViews;

static VkSwapchainKHR s_swapChain;
static VkExtent2D s_swapChainExtent;
static VkSurfaceFormatKHR s_swapChainFormat;
static std::vector<VkFramebuffer> s_swapChainBuffers;

static VkQueue s_graphicsQueue;
static VkRenderPass s_renderPass;
static VkPipeline s_graphicsPipeline;
static VkPipelineLayout s_pipelineLayout;

static VkSemaphore s_imageAvailableSemaphore;
static VkSemaphore s_renderFinishedSemaphore;

static std::vector<char> readFile(const std::string& filename)
{
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		throw std::runtime_error(std::string{ "failed to open file" } +filename + "!");
	}

	const size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

static VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& presentModes)
{
	for (const auto& mode : presentModes)
	{
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			return mode;
	}

	return VK_PRESENT_MODE_FIFO_KHR;
}

static VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	for (const auto& format : formats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
			return format;
	}

	return formats[0];
}

static VkShaderModule createShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo shader_module_create_info{};

	shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shader_module_create_info.pNext = nullptr;
	shader_module_create_info.codeSize = code.size();
	shader_module_create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;

	if (vkCreateShaderModule(s_logicalDevice, &shader_module_create_info, nullptr, &shaderModule) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create shader module");
	}

	return shaderModule;
}

void initAppExtensions()
{
	uint32_t glfwExtensionCount;
	const char** glfwRequiredExtensions;
	glfwRequiredExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	for (int i = 0; i < glfwExtensionCount; ++i)
	{
		s_instanceExtensionNames.push_back(glfwRequiredExtensions[i]);
	}

	if (isEnableValidationLayers) {
		s_instanceExtensionNames.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}
}

void initDeviceExtension()
{
	s_deviceExtensionNames.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
}

int initSurface()
{
	VkWin32SurfaceCreateInfoKHR surfaceInfo = { };
	surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceInfo.pNext = nullptr;
	surfaceInfo.hinstance = s_hInfos.hInstance;
	surfaceInfo.hwnd = s_hInfos.hWnd;

	vk_res = vkCreateWin32SurfaceKHR(s_instance, &surfaceInfo, nullptr, &s_surfaceKHR);
	ASSERT_VK(vk_res);

	return EXIT_SUCCESS;
}

bool checkValidationLayersSupport()
{
	uint32_t numLayers;
	vk_res = vkEnumerateInstanceLayerProperties(&numLayers, nullptr);
	ASSERT_VK(vk_res);

	std::vector<VkLayerProperties> supportedLayers;
	supportedLayers.resize(numLayers);
	vk_res = vkEnumerateInstanceLayerProperties(&numLayers, supportedLayers.data());

	for (const char* layerName : validationLayers)
	{
		bool foundLayer = false;
		for (VkLayerProperties layer_supported : supportedLayers)
		{
			if (strcmp(layerName, layer_supported.layerName) == 0)
			{
				foundLayer = true;
				break;
			}
		}

		if (foundLayer)
		{
			std::cout << "Requested validation layer " << layerName << " is supported." << std::endl;
		}
		else {
			std::wcerr << "Requested validation layer " << layerName << " is not supported." << std::endl;
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

int setupCallbacks()
{
	return EXIT_SUCCESS;
}

int initInstance()
{
	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pApplicationName = APP_NAME;
	applicationInfo.applicationVersion = 1;
	applicationInfo.apiVersion = VK_API_VERSION_1_0;
	applicationInfo.pEngineName = APP_NAME;
	applicationInfo.engineVersion = 1;

	VkInstanceCreateInfo instanceInfo = {};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pNext = nullptr;
	instanceInfo.flags = 0;
	instanceInfo.pApplicationInfo = &applicationInfo;

	if (isEnableValidationLayers)
	{
		if (checkValidationLayersSupport() != EXIT_SUCCESS)
		{
			return EXIT_FAILURE;
		}
		instanceInfo.enabledLayerCount = validationLayers.size();
		instanceInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else {
		instanceInfo.enabledLayerCount = 0;
		instanceInfo.ppEnabledLayerNames = nullptr;
	}

	instanceInfo.enabledExtensionCount = s_instanceExtensionNames.size();
	instanceInfo.ppEnabledExtensionNames = s_instanceExtensionNames.data();

	vk_res = vkCreateInstance(&instanceInfo, nullptr, &s_instance);
	ASSERT_VK(vk_res);

	uint32_t devicesCount;

	vk_res = vkEnumeratePhysicalDevices(s_instance, &devicesCount, nullptr);
	ASSERT_VK(vk_res);

	s_physicalDevices.resize(devicesCount);

	vk_res = vkEnumeratePhysicalDevices(s_instance, &devicesCount, s_physicalDevices.data());
	ASSERT_VK(vk_res);

	for (int i = 0; i < s_physicalDevices.size(); i++)
	{
		VkPhysicalDeviceProperties prop;
		vkGetPhysicalDeviceProperties(s_physicalDevices[i], &prop);

		std::cout << "Device " << i << " : " << prop.deviceName << std::endl;
	}

	return EXIT_SUCCESS;
}

int initLogicalDevice()
{
	// Select first device
	//
	s_physicalDevice = s_physicalDevices[0];

	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(s_physicalDevice, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties>queueFamilyProperties;
	queueFamilyProperties.resize(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(s_physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

	const auto pSupport = static_cast<VkBool32*>(malloc(queueFamilyCount * sizeof(VkBool32)));
	for (uint32_t i = 0; i < queueFamilyCount; i++) {
		vkGetPhysicalDeviceSurfaceSupportKHR(s_physicalDevice, i, s_surfaceKHR,
			&pSupport[i]);
	}

	s_graphicQueueFamilyIndex = UINT32_MAX;
	s_presentQueueFamilyIndex = UINT32_MAX;

	for (uint32_t i = 0; i < queueFamilyCount; i++)
	{
		if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			if (s_graphicQueueFamilyIndex == UINT32_MAX)
			{
				s_graphicQueueFamilyIndex = i;
			}

			if (pSupport[i] == VK_TRUE)
			{
				s_graphicQueueFamilyIndex = i;
				s_presentQueueFamilyIndex = i;
				break;
			}
		}
	}

	if (s_presentQueueFamilyIndex == UINT32_MAX) {
		// If didn't find a queue that supports both graphics and present, then
		// find a separate present queue.
		for (size_t i = 0; i < queueFamilyCount; ++i)
			if (pSupport[i] == VK_TRUE) {
				s_presentQueueFamilyIndex = i;
				break;
			}
	}

	free(pSupport);

	VkDeviceQueueCreateInfo deviceQueueInfo = {};
	deviceQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueInfo.pNext = nullptr;
	deviceQueueInfo.pQueuePriorities = new float[1]{ 0.0 };
	deviceQueueInfo.queueCount = 1;
	deviceQueueInfo.queueFamilyIndex = s_graphicQueueFamilyIndex;

	VkDeviceCreateInfo deviceInfo = {};

	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext = nullptr;
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &deviceQueueInfo;
	deviceInfo.enabledExtensionCount = s_deviceExtensionNames.size();
	deviceInfo.ppEnabledExtensionNames = s_deviceExtensionNames.data();
	deviceInfo.enabledLayerCount = 0;
	deviceInfo.ppEnabledLayerNames = nullptr;

	vk_res = vkCreateDevice(s_physicalDevice, &deviceInfo, nullptr, &s_logicalDevice);
	ASSERT_VK(vk_res);

	vkGetDeviceQueue(s_logicalDevice, deviceQueueInfo.queueFamilyIndex, 0, &s_graphicsQueue);

	return EXIT_SUCCESS;
}

int createSwapChain()
{
	// Image formats
	//
	uint32_t formatCount;
	std::vector<VkSurfaceFormatKHR> formats;

	vk_res = vkGetPhysicalDeviceSurfaceFormatsKHR(s_physicalDevice, s_surfaceKHR, &formatCount, nullptr);
	ASSERT_VK(vk_res);

	if (formatCount != 0)
	{
		formats.resize(formatCount);
		vk_res = vkGetPhysicalDeviceSurfaceFormatsKHR(s_physicalDevice, s_surfaceKHR, &formatCount, formats.data());
		ASSERT_VK(vk_res);
	}
	else { return EXIT_FAILURE; }

	const VkSurfaceFormatKHR format = chooseSurfaceFormat(formats);
	s_swapChainFormat = format;

	// Present mode
	//
	uint32_t presentModeCount;
	std::vector<VkPresentModeKHR> presentModes;

	vkGetPhysicalDeviceSurfacePresentModesKHR(s_physicalDevice, s_surfaceKHR, &presentModeCount, nullptr);

	if (presentModeCount != 0) {
		presentModes.resize(presentModeCount);
		vk_res = vkGetPhysicalDeviceSurfacePresentModesKHR(s_physicalDevice, s_surfaceKHR, &presentModeCount, presentModes.data());
		ASSERT_VK(vk_res);
	}
	else { return EXIT_FAILURE; }

	const VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes);

	// Get capabilities
	//
	VkSurfaceCapabilitiesKHR capabilities;

	vk_res = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(s_physicalDevice, s_surfaceKHR, &capabilities);
	ASSERT_VK(vk_res);

	s_swapChainExtent = capabilities.currentExtent;

	// Create swap chain
	VkSwapchainCreateInfoKHR swapChainInfo = {};
	swapChainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapChainInfo.pNext = nullptr;
	swapChainInfo.flags = 0;
	swapChainInfo.surface = s_surfaceKHR;
	swapChainInfo.imageFormat = format.format;
	swapChainInfo.imageColorSpace = format.colorSpace;
	swapChainInfo.presentMode = presentMode;
	swapChainInfo.minImageCount = capabilities.minImageCount + 1;
	swapChainInfo.imageExtent = capabilities.currentExtent;
	swapChainInfo.preTransform = capabilities.currentTransform;
	swapChainInfo.imageArrayLayers = 1;
	swapChainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapChainInfo.queueFamilyIndexCount = 0;
	swapChainInfo.pQueueFamilyIndices = nullptr;
	swapChainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapChainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapChainInfo.clipped = VK_TRUE;
	swapChainInfo.oldSwapchain = nullptr;

	vk_res = vkCreateSwapchainKHR(s_logicalDevice, &swapChainInfo, nullptr, &s_swapChain);
	ASSERT_VK(vk_res);

	uint32_t swapImageCount;

	vk_res = vkGetSwapchainImagesKHR(s_logicalDevice, s_swapChain, &swapImageCount, nullptr);
	ASSERT_VK(vk_res);

	s_images.resize(swapImageCount);

	vk_res = vkGetSwapchainImagesKHR(s_logicalDevice, s_swapChain, &swapImageCount, s_images.data());
	ASSERT_VK(vk_res);

	s_imageViews.resize(swapImageCount);

	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.pNext = nullptr;
	imageViewInfo.flags = 0;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.format = swapChainInfo.imageFormat;
	imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.layerCount = 1;

	for (int i = 0; i < swapImageCount; i++)
	{
		imageViewInfo.image = s_images[i];

		vk_res = vkCreateImageView(s_logicalDevice, &imageViewInfo, nullptr, &s_imageViews[i]);
		ASSERT_VK(vk_res);
	}

	return EXIT_SUCCESS;
}

int createRenderPass()
{
	// Color Attachment
	//
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = s_swapChainFormat.format;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	// SubPass
	//
	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subPassDesc = {};
	subPassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subPassDesc.colorAttachmentCount = 1;
	subPassDesc.pColorAttachments = &colorAttachmentRef;

	// SubPass dependency
	//
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;

	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;

	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	// RenderPass
	//
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext = nullptr;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subPassDesc;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	vk_res = vkCreateRenderPass(s_logicalDevice, &renderPassInfo, nullptr, &s_renderPass);
	ASSERT_VK(vk_res);

	return EXIT_SUCCESS;
}

int createGraphicsPipeline()
{
	const auto vertShaderCode = readFile("shaders/vert.spv");
	const auto fragShaderCode = readFile("shaders/frag.spv");

	auto shaderModuleVert = createShaderModule(vertShaderCode);
	auto shaderModuleFrag = createShaderModule(fragShaderCode);

	// Create vertex shader stage infos
	//
	VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
	vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStageInfo.pNext = nullptr;
	vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStageInfo.module = shaderModuleVert;
	vertShaderStageInfo.pName = "main";
	vertShaderStageInfo.pSpecializationInfo = nullptr;

	// Create fragment shader stage infos
	//
	VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
	fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStageInfo.pNext = nullptr;
	fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStageInfo.module = shaderModuleFrag;
	fragShaderStageInfo.pName = "main";
	fragShaderStageInfo.pSpecializationInfo = nullptr;

	VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

	// Input shader stage
	//
	VkPipelineVertexInputStateCreateInfo vertInputInfo = {};
	vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertInputInfo.pNext = nullptr;
	vertInputInfo.vertexBindingDescriptionCount = 0;
	vertInputInfo.pVertexBindingDescriptions = nullptr;
	vertInputInfo.vertexAttributeDescriptionCount = 0;
	vertInputInfo.pVertexAttributeDescriptions = nullptr;

	// Input assembly
	//
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.pNext = nullptr;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// Viewport State
	//
	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(s_swapChainExtent.width);
	viewport.height = static_cast<float>(s_swapChainExtent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0,0 };
	scissor.extent = s_swapChainExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.pNext = nullptr;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	// Rasterization
	//
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.pNext = nullptr;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	// Multi-sampling
	//
	VkPipelineMultisampleStateCreateInfo multiSample = {};
	multiSample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multiSample.pNext = nullptr;
	multiSample.sampleShadingEnable = VK_FALSE;
	multiSample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// Stencil/Depth state
	//
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlendState = {};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.pNext = nullptr;
	colorBlendState.logicOpEnable = VK_FALSE;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &colorBlendAttachment;

	// DynamicStates
	//

	// Pipeline layout
	//
	VkPipelineLayoutCreateInfo pipeLineLayoutInfo = { };
	pipeLineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLineLayoutInfo.flags = 0;
	pipeLineLayoutInfo.pNext = nullptr;
	pipeLineLayoutInfo.setLayoutCount = 0;
	pipeLineLayoutInfo.pSetLayouts = nullptr;
	pipeLineLayoutInfo.pushConstantRangeCount = 0;
	pipeLineLayoutInfo.pPushConstantRanges = nullptr;

	vk_res = vkCreatePipelineLayout(s_logicalDevice, &pipeLineLayoutInfo, nullptr, &s_pipelineLayout);
	ASSERT_VK(vk_res);

	// Pipeline
	//
	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.pNext = nullptr;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;

	pipelineInfo.pVertexInputState = &vertInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multiSample;
	pipelineInfo.pColorBlendState = &colorBlendState;
	pipelineInfo.pDynamicState = nullptr;

	pipelineInfo.layout = s_pipelineLayout;
	pipelineInfo.renderPass = s_renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = nullptr;
	pipelineInfo.basePipelineIndex = -1;

	vk_res = vkCreateGraphicsPipelines(s_logicalDevice, nullptr, 1, &pipelineInfo, nullptr, &s_graphicsPipeline);
	ASSERT_VK(vk_rest);

	vkDestroyShaderModule(s_logicalDevice, shaderModuleVert, nullptr);
	vkDestroyShaderModule(s_logicalDevice, shaderModuleFrag, nullptr);

	return 0;
}

int createFrameBuffers()
{
	s_swapChainBuffers.resize(s_imageViews.size());

	for (size_t i = 0; i < s_swapChainBuffers.size(); i++)
	{
		VkImageView attachments[] = {
			s_imageViews[i]
		};

		VkFramebufferCreateInfo frameBufferInfo = {};

		frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferInfo.pNext = nullptr;
		frameBufferInfo.renderPass = s_renderPass;
		frameBufferInfo.attachmentCount = 1;
		frameBufferInfo.pAttachments = attachments;
		frameBufferInfo.width = s_swapChainExtent.width;
		frameBufferInfo.height = s_swapChainExtent.height;
		frameBufferInfo.layers = 1;

		vk_res = vkCreateFramebuffer(s_logicalDevice, &frameBufferInfo, nullptr, &s_swapChainBuffers[i]);
		ASSERT_VK(vk_res);
	}

	return EXIT_SUCCESS;
}

int createCommandPool()
{
	VkCommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.pNext = nullptr;
	commandPoolInfo.queueFamilyIndex = s_graphicQueueFamilyIndex;

	vk_res = vkCreateCommandPool(s_logicalDevice, &commandPoolInfo, nullptr, &s_commandPool);
	ASSERT_VK(vk_res);

	return EXIT_SUCCESS;
}

int createCommandBuffers()
{
	s_commandBuffers.resize(s_swapChainBuffers.size());

	VkCommandBufferAllocateInfo commandBufferInfo = { };
	commandBufferInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferInfo.pNext = nullptr;
	commandBufferInfo.commandPool = s_commandPool;
	commandBufferInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferInfo.commandBufferCount = static_cast<uint32_t>(s_swapChainBuffers.size());

	vk_res = vkAllocateCommandBuffers(s_logicalDevice, &commandBufferInfo, s_commandBuffers.data());
	ASSERT_VK(vk_res);

	for (size_t i = 0; i < s_commandBuffers.size(); i++)
	{
		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.pNext = nullptr;
		commandBufferBeginInfo.flags = 0;
		commandBufferBeginInfo.pInheritanceInfo = nullptr;

		// Begin Command Buffer
		//
		vk_res = vkBeginCommandBuffer(s_commandBuffers[i], &commandBufferBeginInfo);
		ASSERT_VK(vk_res);

		// Begin Render Pass
		//
		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.pNext = nullptr;
		renderPassInfo.renderPass = s_renderPass;
		renderPassInfo.framebuffer = s_swapChainBuffers[i];

		renderPassInfo.renderArea.offset = { 0,0 };
		renderPassInfo.renderArea.extent = s_swapChainExtent;

		VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColor;

		vkCmdBeginRenderPass(s_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Activate pipeline
		vkCmdBindPipeline(s_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, s_graphicsPipeline);

		// Draw
		vkCmdDraw(s_commandBuffers[i], 3, 1, 0, 0);

		// End RenderPass
		vkCmdEndRenderPass(s_commandBuffers[i]);

		// Close Command Buffer
		vk_res = vkEndCommandBuffer(s_commandBuffers[i]);
		ASSERT_VK(vk_res);
	}

	return EXIT_SUCCESS;
}

int createSemaphores()
{
	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = nullptr;
	semaphoreInfo.flags = 0;

	vk_res = vkCreateSemaphore(s_logicalDevice, &semaphoreInfo, nullptr, &s_imageAvailableSemaphore);
	ASSERT_VK(vk_res);

	vk_res = vkCreateSemaphore(s_logicalDevice, &semaphoreInfo, nullptr, &s_renderFinishedSemaphore);
	ASSERT_VK(vk_res);

	return EXIT_SUCCESS;
}

int drawFrame()
{
	// 1 - Get image from the swapchain
	//
	uint32_t imageIndex;
	vk_res = vkAcquireNextImageKHR(s_logicalDevice, s_swapChain, UINT16_MAX, s_imageAvailableSemaphore, nullptr, &imageIndex);
	ASSERT_VK(vk_res);

	// 2 - Execute Command Buffer
	//
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;

	// Wait semaphores
	VkSemaphore waitSemaphores[] = { s_imageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	// Signal semaphores
	VkSemaphore signalSemaphores[] = { s_renderFinishedSemaphore };

	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	// Attach command buffer
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &s_commandBuffers[imageIndex];

	vk_res = vkQueueSubmit(s_graphicsQueue, 1, &submitInfo, nullptr);
	ASSERT_VK(vk_res);

	// 3 - Present Frame
	//
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = nullptr;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapchain[] = { s_swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapchain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = nullptr;

	vk_res = vkQueuePresentKHR(s_graphicsQueue, &presentInfo);
	ASSERT_VK(vk_res);

	vkQueueWaitIdle(s_graphicsQueue);

	return EXIT_SUCCESS;
}

int setupVulkan(HINSTANCE hInstance, HWND hwnd)
{
	s_hInfos.hInstance = hInstance;
	s_hInfos.hWnd = hwnd;

	initAppExtensions();
	initDeviceExtension();

	int result = initInstance();
	ASSERT(result);

	result = setupCallbacks();
	ASSERT(result);

	result = initSurface();
	ASSERT(result);

	result = initLogicalDevice();
	ASSERT(result);

	result = createSwapChain();
	ASSERT(result);

	result = createRenderPass();
	ASSERT(result);

	result = createGraphicsPipeline();
	ASSERT(result);

	result = createFrameBuffers();
	ASSERT(result);

	result = createCommandPool();
	ASSERT(result);

	result = createCommandBuffers();
	ASSERT(result);

	result = createSemaphores();
	ASSERT(result);

	return EXIT_SUCCESS;
}

void shutdownVulkan()
{
	vkDestroyPipeline(s_logicalDevice, s_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(s_logicalDevice, s_pipelineLayout, nullptr);
	vkDestroyRenderPass(s_logicalDevice, s_renderPass, nullptr);
	vkFreeCommandBuffers(s_logicalDevice, s_commandPool, s_commandBuffers.size(), s_commandBuffers.data());
	vkDestroyCommandPool(s_logicalDevice, s_commandPool, nullptr);

	for (auto imageView : s_imageViews)
	{
		vkDestroyImageView(s_logicalDevice, imageView, nullptr);
	}

	for (auto frameBuffer : s_swapChainBuffers)
	{
		vkDestroyFramebuffer(s_logicalDevice, frameBuffer, nullptr);
	}

	vkDestroySwapchainKHR(s_logicalDevice, s_swapChain, nullptr);
	vkDestroyDevice(s_logicalDevice, nullptr);
	vkDestroyInstance(s_instance, nullptr);

	vkDestroySemaphore(s_logicalDevice, s_imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(s_logicalDevice, s_renderFinishedSemaphore, nullptr);
}

int main()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	int result = setupVulkan(GetModuleHandle(nullptr), glfwGetWin32Window(window));
	ASSERT(result);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		result = drawFrame();
		ASSERT(result);
	}

	vkDeviceWaitIdle(s_logicalDevice);

	glfwDestroyWindow(window);

	glfwTerminate();

	return EXIT_SUCCESS;
}