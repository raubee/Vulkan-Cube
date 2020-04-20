#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#define VK_USE_PLATFORM_WIN32_KHR 
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdlib>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <array>
#include <chrono>
#include <windows.h>

#define STB_IMAGE_IMPLEMENTATION
#include "include/stb_image.h"

struct Vertex
{
	glm::vec3 position;
	glm::vec3 color;

	static VkVertexInputBindingDescription getBindingDescription()
	{
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions = {};

		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, position);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		return attributeDescriptions;
	}
};

#define S_CUBE 0.5

const std::vector<Vertex> vertices = {
	// -Z
	{{-S_CUBE, -S_CUBE, -S_CUBE}, {1.0f, 0.0f, 0.0f}}, // Red 0
	{{S_CUBE, -S_CUBE, -S_CUBE}, {0.0f, 1.0f, 0.0f}}, // Green 1
	{{S_CUBE, S_CUBE, -S_CUBE}, {0.0f, 0.0f, 1.0f}}, // Blue 2
	{{-S_CUBE, S_CUBE, -S_CUBE}, {1.0f, 0.0f, 1.0f}}, // Purple 3

	// +Z
	{{-S_CUBE, -S_CUBE, S_CUBE}, {0.0f, 1.0f, 1.0f}}, // Cyan 4
	{{S_CUBE, -S_CUBE, S_CUBE}, {1.0f, 0.0f, 1.0f}}, // Magenta 5
	{{S_CUBE, S_CUBE, S_CUBE}, {1.0f, 1.0f, 0.0f}}, // Yellow 6
	{{-S_CUBE, S_CUBE, S_CUBE}, {1.0f, 1.0f, 1.0f}} // White 7
};

const std::vector<uint16_t> indices = {
	 0, 1, 3, 3, 1, 2,
	1, 5, 2, 2, 5, 6,
	5, 4, 6, 6, 4, 7,
	4, 0, 7, 7, 0, 3,
	3, 2, 7, 7, 2, 6,
	4, 5, 0, 0, 5, 1
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

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
static VkCommandPool s_commandTransferPool;
static VkBuffer s_vertexBuffer;
static VkDeviceMemory s_vertexBufferMemory;
static VkBuffer s_indexBuffer;
static VkDeviceMemory s_indexBufferMemory;
static std::vector<VkBuffer> s_uniformBuffers;
static std::vector<VkDeviceMemory> s_uniformBuffersMemory;
static VkDescriptorPool s_descriptorPool;
static std::vector<VkDescriptorSet> s_descriptorSets;
static std::vector<VkCommandBuffer> s_commandBuffers; // Rendering command buffers, one for each swap image

static VkSurfaceKHR s_surfaceKHR;

static VkSwapchainKHR s_swapChain;
static VkExtent2D s_swapChainExtent;
static VkSurfaceFormatKHR s_swapChainFormat;
static std::vector<VkImageView> s_swapChainImagesViews;
static std::vector<VkFramebuffer> s_swapChainBuffers;

static VkImage s_depthImage;
static VkDeviceMemory s_depthImageMemory;
static VkImageView s_depthImageView;

// Texture
static VkImage s_textureImage;
static VkImageView s_textureImageView;
static VkDeviceMemory s_textureImageMemory;

static VkQueue s_graphicsQueue;
static VkRenderPass s_renderPass;
static VkPipeline s_graphicsPipeline;
static VkDescriptorSetLayout s_descriptorLayout;
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

static uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(s_physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if (typeFilter & (1 << i) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
		{
			return i;
		}
	}

	throw std::runtime_error("Failed to get a memory type for the buffer !");
}

static int createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
	VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.pNext = nullptr;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	vk_res = vkCreateBuffer(s_logicalDevice, &bufferInfo, nullptr, &buffer);
	ASSERT_VK(vk_res);

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(s_logicalDevice, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};

	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	vk_res = vkAllocateMemory(s_logicalDevice, &allocInfo, nullptr, &bufferMemory);
	ASSERT_VK(vk_res);

	vk_res = vkBindBufferMemory(s_logicalDevice, buffer, bufferMemory, 0);
	ASSERT_VK(vk_res);

	return EXIT_SUCCESS;
}

static VkCommandBuffer beginSingleTransferCommands()
{
	VkCommandBufferAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;
	allocInfo.commandPool = s_commandTransferPool;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(s_logicalDevice, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	beginInfo.pNext = nullptr;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

static void endSingleTransferCommands(VkCommandBuffer commandBuffer)
{
	vkEndCommandBuffer(commandBuffer);

	// Submit one time
	//
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(s_graphicsQueue, 1, &submitInfo, nullptr);
	vkQueueWaitIdle(s_graphicsQueue);

	vkFreeCommandBuffers(s_logicalDevice, s_commandTransferPool, 1, &commandBuffer);
}

static int transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	const VkCommandBuffer commandBuffer = beginSingleTransferCommands();

	VkImageMemoryBarrier barrier = {};

	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;

	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.image = image;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	VkPipelineStageFlags sourceStage;
	VkPipelineStageFlags destinationStage;

	if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

		sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	}
	else
	{
		throw std::invalid_argument("Unsupported layout transition!");
	}

	vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	endSingleTransferCommands(commandBuffer);

	return EXIT_SUCCESS;
}

static int transferBuffer(const VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size)
{
	const VkCommandBuffer commandBuffer = beginSingleTransferCommands();

	VkBufferCopy copyRegion = {};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTransferCommands(commandBuffer);

	return EXIT_SUCCESS;
}

static int transferBufferToImage(const VkBuffer& srcBuffer, VkImage& dstImage, uint32_t width, uint32_t height)
{
	const VkCommandBuffer commandBuffer = beginSingleTransferCommands();

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;

	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {
		width,
		height,
		1
	};

	vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endSingleTransferCommands(commandBuffer);

	return EXIT_SUCCESS;
}

template<class T>
static int createBufferWithStaging(T* data, size_t size, size_t stride, VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& bufferMemory)
{
	const VkDeviceSize bufferSize = size * stride;
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* pData;
	vkMapMemory(s_logicalDevice, stagingBufferMemory, 0, bufferSize, 0, &pData);
	memcpy(pData, data, static_cast<size_t>(bufferSize));
	vkUnmapMemory(s_logicalDevice, stagingBufferMemory);

	createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer, bufferMemory);

	transferBuffer(stagingBuffer, buffer, bufferSize);

	vkDestroyBuffer(s_logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(s_logicalDevice, stagingBufferMemory, nullptr);

	return EXIT_SUCCESS;
}

static VkFormat findSupportedFormats(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
	for (VkFormat format : candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(s_physicalDevice, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
		{
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
		{
			return format;
		}
	}

	throw std::runtime_error("No formats supported!");
}

static bool hasStencilComponent(VkFormat format)
{
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

static VkFormat findDepthFormat()
{
	return findSupportedFormats({ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

static int createImage2D(const VkExtent2D extent, const VkFormat format, const VkImageTiling tiling,
	const VkBufferUsageFlags usage, const VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory)
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.pNext = nullptr;
	imageInfo.usage = usage;
	imageInfo.extent.width = extent.width;
	imageInfo.extent.height = extent.height;
	imageInfo.extent.depth = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.mipLevels = 1;
	imageInfo.format = format;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	vk_res = vkCreateImage(s_logicalDevice, &imageInfo, nullptr, &image);
	ASSERT_VK(vk_res);

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(s_logicalDevice, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

	vk_res = vkAllocateMemory(s_logicalDevice, &allocInfo, nullptr, &memory);
	ASSERT_VK(vk_res);

	vkBindImageMemory(s_logicalDevice, image, memory, 0);

	return EXIT_SUCCESS;
}

static VkImageView& createImageView(const VkImage& image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageView imageView;

	VkImageViewCreateInfo imageViewInfo = {};
	imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewInfo.flags = 0;
	imageViewInfo.pNext = nullptr;
	imageViewInfo.format = format;
	imageViewInfo.image = image;
	imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewInfo.subresourceRange.aspectMask = aspectFlags;
	imageViewInfo.subresourceRange.levelCount = 1;
	imageViewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(s_logicalDevice, &imageViewInfo, nullptr, &imageView) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create image view!");
	}

	return imageView;
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

	std::vector<VkImage> images;
	images.resize(swapImageCount);

	vk_res = vkGetSwapchainImagesKHR(s_logicalDevice, s_swapChain, &swapImageCount, images.data());
	ASSERT_VK(vk_res);

	s_swapChainImagesViews.resize(swapImageCount);

	for (int i = 0; i < swapImageCount; i++)
	{
		s_swapChainImagesViews[i] = createImageView(images[i], swapChainInfo.imageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
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

	// Depth
	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = findDepthFormat();
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subPassDesc = {};
	subPassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subPassDesc.colorAttachmentCount = 1;
	subPassDesc.pColorAttachments = &colorAttachmentRef;
	subPassDesc.pDepthStencilAttachment = &depthAttachmentRef;

	// SubPass dependency
	//
	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;

	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;

	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	const std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };

	// RenderPass
	//
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext = nullptr;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subPassDesc;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	vk_res = vkCreateRenderPass(s_logicalDevice, &renderPassInfo, nullptr, &s_renderPass);
	ASSERT_VK(vk_res);

	return EXIT_SUCCESS;
}

int createDescriptorSetLayout()
{
	VkDescriptorSetLayoutBinding uboLayoutBinding = {};
	uboLayoutBinding.binding = 0;
	uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	uboLayoutBinding.descriptorCount = 1;
	uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	uboLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo layoutInfo = {};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = nullptr;
	layoutInfo.bindingCount = 1;
	layoutInfo.pBindings = &uboLayoutBinding;

	vk_res = vkCreateDescriptorSetLayout(s_logicalDevice, &layoutInfo, nullptr, &s_descriptorLayout);
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
	auto bindingDescription = Vertex::getBindingDescription();
	auto attributeDescriptions = Vertex::getAttributeDescriptions();

	VkPipelineVertexInputStateCreateInfo vertInputInfo = {};
	vertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertInputInfo.pNext = nullptr;
	vertInputInfo.vertexBindingDescriptionCount = 1;
	vertInputInfo.pVertexBindingDescriptions = &bindingDescription;
	vertInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
	vertInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

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

	VkPipelineDepthStencilStateCreateInfo depthState = {};
	depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthState.pNext = nullptr;
	depthState.depthTestEnable = VK_TRUE;
	depthState.depthWriteEnable = VK_TRUE;
	depthState.depthCompareOp = VK_COMPARE_OP_LESS;
	depthState.depthBoundsTestEnable = VK_FALSE;
	depthState.minDepthBounds = 0.0f;
	depthState.maxDepthBounds = 1.0f;
	depthState.stencilTestEnable = VK_FALSE;
	depthState.front = {};
	depthState.back = {};

	// DynamicStates
	//

	// Pipeline layout
	//
	VkPipelineLayoutCreateInfo pipeLineLayoutInfo = { };
	pipeLineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeLineLayoutInfo.flags = 0;
	pipeLineLayoutInfo.pNext = nullptr;
	pipeLineLayoutInfo.setLayoutCount = 1;
	pipeLineLayoutInfo.pSetLayouts = &s_descriptorLayout;
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
	pipelineInfo.pDepthStencilState = &depthState;
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

	return EXIT_SUCCESS;
}

int createFrameBuffers()
{
	s_swapChainBuffers.resize(s_swapChainImagesViews.size());

	for (size_t i = 0; i < s_swapChainBuffers.size(); i++)
	{
		std::array<VkImageView, 2> attachments = {
			s_swapChainImagesViews[i],
			s_depthImageView
		};

		VkFramebufferCreateInfo frameBufferInfo = {};

		frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frameBufferInfo.pNext = nullptr;
		frameBufferInfo.renderPass = s_renderPass;
		frameBufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		frameBufferInfo.pAttachments = attachments.data();
		frameBufferInfo.width = s_swapChainExtent.width;
		frameBufferInfo.height = s_swapChainExtent.height;
		frameBufferInfo.layers = 1;

		vk_res = vkCreateFramebuffer(s_logicalDevice, &frameBufferInfo, nullptr, &s_swapChainBuffers[i]);
		ASSERT_VK(vk_res);
	}

	return EXIT_SUCCESS;
}

int createUniformBuffers()
{
	const VkDeviceSize bufferSize = sizeof UniformBufferObject;

	s_uniformBuffers.resize(s_swapChainImagesViews.size());
	s_uniformBuffersMemory.resize(s_swapChainImagesViews.size());

	for (int i = 0; i < s_swapChainImagesViews.size(); i++)
	{
		createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			s_uniformBuffers[i], s_uniformBuffersMemory[i]);
	}

	return EXIT_SUCCESS;
}

int createDescriptorPool()
{
	VkDescriptorPoolSize poolSize = {};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = static_cast<uint32_t>(s_swapChainImagesViews.size());

	VkDescriptorPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.pNext = nullptr;
	poolInfo.poolSizeCount = 1;
	poolInfo.pPoolSizes = &poolSize;
	poolInfo.maxSets = poolSize.descriptorCount;

	vk_res = vkCreateDescriptorPool(s_logicalDevice, &poolInfo, nullptr, &s_descriptorPool);
	ASSERT_VK(vk_res);

	return EXIT_SUCCESS;
}

int createDescriptorSet()
{
	s_descriptorSets.resize(s_swapChainImagesViews.size());

	std::vector<VkDescriptorSetLayout> layouts(s_swapChainImagesViews.size(), s_descriptorLayout);

	VkDescriptorSetAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.descriptorPool = s_descriptorPool;
	allocInfo.descriptorSetCount = s_swapChainImagesViews.size();
	allocInfo.pSetLayouts = layouts.data();

	vk_res = vkAllocateDescriptorSets(s_logicalDevice, &allocInfo, s_descriptorSets.data());
	ASSERT_VK(vk_res);

	for (size_t i = 0; i < s_swapChainImagesViews.size(); ++i)
	{
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.offset = 0;
		bufferInfo.buffer = s_uniformBuffers[i];
		bufferInfo.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet descriptorWrite = {};
		descriptorWrite.pNext = nullptr;
		descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrite.dstSet = s_descriptorSets[i];
		descriptorWrite.dstBinding = 0;
		descriptorWrite.dstArrayElement = 0;
		descriptorWrite.descriptorCount = 1;
		descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrite.pBufferInfo = &bufferInfo;

		vkUpdateDescriptorSets(s_logicalDevice, 1, &descriptorWrite, 0, nullptr);
	}

	return EXIT_SUCCESS;
}

int createVertexAndIndexBuffers()
{
	createBufferWithStaging(vertices.data(), vertices.size(), sizeof Vertex, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, s_vertexBuffer, s_vertexBufferMemory);
	createBufferWithStaging(indices.data(), indices.size(), sizeof uint16_t, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, s_indexBuffer, s_indexBufferMemory);

	return EXIT_SUCCESS;
}

int createCommandPools()
{
	VkCommandPoolCreateInfo commandPoolInfo = {};
	commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolInfo.pNext = nullptr;
	commandPoolInfo.queueFamilyIndex = s_graphicQueueFamilyIndex;

	vk_res = vkCreateCommandPool(s_logicalDevice, &commandPoolInfo, nullptr, &s_commandPool);
	ASSERT_VK(vk_res);

	commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

	vk_res = vkCreateCommandPool(s_logicalDevice, &commandPoolInfo, nullptr, &s_commandTransferPool);
	ASSERT_VK(vk_res);


	return EXIT_SUCCESS;
}

int createDepthResources()
{
	const VkFormat depthFormat = findDepthFormat();

	createImage2D(s_swapChainExtent, depthFormat, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, s_depthImage, s_depthImageMemory);
	s_depthImageView = createImageView(s_depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

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

		std::array<VkClearValue, 2>  clearValues{};
		clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };

		renderPassInfo.clearValueCount = 2;
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(s_commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// Activate pipeline
		vkCmdBindPipeline(s_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, s_graphicsPipeline);

		VkBuffer vertexBuffers[] = { s_vertexBuffer };
		VkDeviceSize offsets[] = { 0 };
		vkCmdBindVertexBuffers(s_commandBuffers[i], 0, 1, vertexBuffers, offsets);
		vkCmdBindIndexBuffer(s_commandBuffers[i], s_indexBuffer, 0, VK_INDEX_TYPE_UINT16);

		// Bind descriptors
		vkCmdBindDescriptorSets(s_commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
			s_pipelineLayout, 0, 1, &s_descriptorSets[i], 0, nullptr);

		// Draw
		vkCmdDrawIndexed(s_commandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

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

int updateUniforms(uint32_t imageIndex)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	UniformBufferObject ubo = {  };

	ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(glm::radians(45.0f), s_swapChainExtent.width / static_cast<float>(s_swapChainExtent.height), 0.1f, 10.0f);

	void* data;
	vkMapMemory(s_logicalDevice, s_uniformBuffersMemory[imageIndex], 0, sizeof ubo, 0, &data);
	memcpy(data, &ubo, sizeof ubo);
	vkUnmapMemory(s_logicalDevice, s_uniformBuffersMemory[imageIndex]);
	return EXIT_SUCCESS;
}

int drawFrame()
{
	// 1 - Get image from the swapchain
	//
	uint32_t imageIndex;
	vk_res = vkAcquireNextImageKHR(s_logicalDevice, s_swapChain, UINT16_MAX, s_imageAvailableSemaphore, nullptr, &imageIndex);
	ASSERT_VK(vk_res);

	// Update uniforms
	//
	updateUniforms(imageIndex);

	// 2 - Execute Command Buffers
	//
	//
	// Submit render pass
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

int loadTexture()
{
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

	VkDeviceSize textureSize = texWidth * texHeight * 4;

	if (!pixels)
	{
		throw std::runtime_error("Fail to load texture!");
	}

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(textureSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* pData;
	vkMapMemory(s_logicalDevice, stagingBufferMemory, 0, textureSize, 0, &pData);
	memcpy(pData, pixels, static_cast<size_t>(textureSize));
	vkUnmapMemory(s_logicalDevice, stagingBufferMemory);

	stbi_image_free(pixels);

	const VkExtent2D extent =
	{
		static_cast<uint32_t>(texWidth),
		static_cast<uint32_t>(texHeight)
	};

	createImage2D(extent, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, s_textureImage, s_textureImageMemory);

	transitionImageLayout(s_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	transferBufferToImage(stagingBuffer, s_textureImage, extent.width, extent.height);
	transitionImageLayout(s_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(s_logicalDevice, stagingBuffer, nullptr);
	vkFreeMemory(s_logicalDevice, stagingBufferMemory, nullptr);

	return EXIT_SUCCESS;
}

int createTextureImageView()
{
	s_textureImageView = createImageView(s_textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);

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

	result = createDescriptorSetLayout();
	ASSERT(result);

	result = createGraphicsPipeline();
	ASSERT(result);

	result = createDepthResources();
	ASSERT(result);

	result = createFrameBuffers();
	ASSERT(result);

	result = createCommandPools();
	ASSERT(result);

	result = loadTexture();
	ASSERT(result);

	result = createTextureImageView();
	ASSERT(result);

	result = createVertexAndIndexBuffers();
	ASSERT(result);

	result = createUniformBuffers();
	ASSERT(result);

	result = createDescriptorPool();
	ASSERT(result);

	result = createDescriptorSet();
	ASSERT(result);

	result = createCommandBuffers();
	ASSERT(result);

	result = createSemaphores();
	ASSERT(result);

	return EXIT_SUCCESS;
}

void cleanUpSwapChain()
{
	vkDeviceWaitIdle(s_logicalDevice);

	for (auto frameBuffer : s_swapChainBuffers)
	{
		vkDestroyFramebuffer(s_logicalDevice, frameBuffer, nullptr);
	}

	vkFreeCommandBuffers(s_logicalDevice, s_commandPool, s_commandBuffers.size(), s_commandBuffers.data());
	vkDestroyPipeline(s_logicalDevice, s_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(s_logicalDevice, s_pipelineLayout, nullptr);
	vkDestroyRenderPass(s_logicalDevice, s_renderPass, nullptr);

	for (auto imageView : s_swapChainImagesViews)
	{
		vkDestroyImageView(s_logicalDevice, imageView, nullptr);
	}

	vkDestroySwapchainKHR(s_logicalDevice, s_swapChain, nullptr);

	vkDestroyImage(s_logicalDevice, s_depthImage, nullptr);
	vkDestroyImageView(s_logicalDevice, s_depthImageView, nullptr);
	vkFreeMemory(s_logicalDevice, s_depthImageMemory, nullptr);
}

int recreateSwapChain()
{
	vkDeviceWaitIdle(s_logicalDevice);

	cleanUpSwapChain();
	createSwapChain();
	createDepthResources();
	createRenderPass();
	createGraphicsPipeline();
	createFrameBuffers();
	createCommandBuffers();

	return EXIT_SUCCESS;
}

void cleanUp()
{
	cleanUpSwapChain();

	vkDestroySemaphore(s_logicalDevice, s_imageAvailableSemaphore, nullptr);
	vkDestroySemaphore(s_logicalDevice, s_renderFinishedSemaphore, nullptr);

	vkDestroyDescriptorSetLayout(s_logicalDevice, s_descriptorLayout, nullptr);
	vkDestroyBuffer(s_logicalDevice, s_vertexBuffer, nullptr);
	vkFreeMemory(s_logicalDevice, s_vertexBufferMemory, nullptr);
	vkDestroyBuffer(s_logicalDevice, s_indexBuffer, nullptr);
	vkFreeMemory(s_logicalDevice, s_indexBufferMemory, nullptr);
	vkDestroyCommandPool(s_logicalDevice, s_commandPool, nullptr);
	vkDestroyCommandPool(s_logicalDevice, s_commandTransferPool, nullptr);

	for (auto uniformBuffer : s_uniformBuffers)
	{
		vkDestroyBuffer(s_logicalDevice, uniformBuffer, nullptr);
	}

	for (auto uniformBufferMemory : s_uniformBuffersMemory)
	{
		vkFreeMemory(s_logicalDevice, uniformBufferMemory, nullptr);
	}

	vkDestroyImageView(s_logicalDevice, s_textureImageView, nullptr);
	vkDestroyImage(s_logicalDevice, s_textureImage, nullptr);
	vkFreeMemory(s_logicalDevice, s_textureImageMemory, nullptr);

	vkDestroyDescriptorPool(s_logicalDevice, s_descriptorPool, nullptr);
	vkDestroyDevice(s_logicalDevice, nullptr);
	vkDestroySurfaceKHR(s_instance, s_surfaceKHR, nullptr);
	vkDestroyInstance(s_instance, nullptr);
}

/// GLFW

static void frameBufferResizeCallback(GLFWwindow* window, int width, int height)
{
	recreateSwapChain();
}

int main()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan window", nullptr, nullptr);

	glfwSetFramebufferSizeCallback(window, frameBufferResizeCallback);

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	int result = setupVulkan(GetModuleHandle(nullptr), glfwGetWin32Window(window));
	ASSERT(result);

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		result = drawFrame();
		ASSERT(result);
	}

	cleanUp();

	glfwDestroyWindow(window);

	glfwTerminate();

	return EXIT_SUCCESS;
}