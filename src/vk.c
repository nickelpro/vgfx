#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vgfx.h"
#include "logc/log.h"
#include "sds/sds.h"

//Utility stuff
char *pmode2str(VkPresentModeKHR pmode) {
	switch(pmode) {
		case VK_PRESENT_MODE_IMMEDIATE_KHR:
			return "Immediate";
		case VK_PRESENT_MODE_MAILBOX_KHR:
			return "Mailbox";
		case VK_PRESENT_MODE_FIFO_KHR:
			return "FIFO";
		case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
			return "FIFO Relaxed";
		default:
			return "Unkown";
	}
}

char *vkr2str(VkResult err) {
	switch(err) {
		case VK_SUCCESS:
			return "Success";
		case VK_NOT_READY:
			return "Not Ready";
		case VK_TIMEOUT:
			return "Timeout";
		case VK_EVENT_SET:
			return "Event Set";
		case VK_EVENT_RESET:
			return "Event Reset";
		case VK_INCOMPLETE:
			return "Incomplete";
		case VK_ERROR_OUT_OF_HOST_MEMORY:
			return "Out of Host Memory";
		case VK_ERROR_OUT_OF_DEVICE_MEMORY:
			return "Out of Device Memory";
		case VK_ERROR_INITIALIZATION_FAILED:
			return "Initialization Failed";
		case VK_ERROR_DEVICE_LOST:
			return "Device Lost";
		case VK_ERROR_MEMORY_MAP_FAILED:
			return "Memory Map Failed";
		case VK_ERROR_LAYER_NOT_PRESENT:
			return "Layer Not Present";
		case VK_ERROR_EXTENSION_NOT_PRESENT:
			return "Extension Not Present";
		case VK_ERROR_FEATURE_NOT_PRESENT:
			return "Feature Not Present";
		case VK_ERROR_INCOMPATIBLE_DRIVER:
			return "Incompatible Driver";
		case VK_ERROR_TOO_MANY_OBJECTS:
			return "Too Many Objects";
		case VK_ERROR_FORMAT_NOT_SUPPORTED:
			return "Format Not Supported";
		case VK_ERROR_FRAGMENTED_POOL:
			return "Fragmented Pool";
		case VK_ERROR_OUT_OF_POOL_MEMORY:
			return "Out of Pool Memory";
		case VK_ERROR_INVALID_EXTERNAL_HANDLE:
			return "Invalid External Handle";
		case VK_ERROR_SURFACE_LOST_KHR:
			return "Surface Lost";
		case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
			return "Native Window in Use";
		case VK_SUBOPTIMAL_KHR:
			return "Suboptimal";
		case VK_ERROR_OUT_OF_DATE_KHR:
			return "Out of Date";
		case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
			return "Incompatible Display";
		case VK_ERROR_VALIDATION_FAILED_EXT:
			return "Validation Failed";
		case VK_ERROR_INVALID_SHADER_NV:
			return "Invalid Shader";
		case VK_ERROR_FRAGMENTATION_EXT:
			return "Fragmentation";
		case VK_ERROR_NOT_PERMITTED_EXT:
			return "Not Permitted";
		default:
			return "Unknown";
	}
}

int read_file(const char *path, sds *out) {
	*out = NULL;
	FILE *f = fopen(path, "rb");
	if(!f) goto failed_open;
	if(fseek(f, 0, SEEK_END)) goto borked;
	long length = ftell(f);
	if(length < 0) goto borked;
	if(!(*out = sdsnewlen(NULL, (size_t) length))) goto borked;
	if(fseek(f, 0, SEEK_SET)) goto borked;
	if(fread(*out, 1, length, f) != (size_t) length) goto borked;
	fclose(f);

	return VGFX_SUCCESS;
	borked:
		sdsfree(*out);
		fclose(f);
	failed_open:
		log_error("Failed to read: %s", path);
		return VGFX_FAIL;
}

int read_spirv(vk_spirvbuf_t *buf, const char *path) {
	buf->base = NULL;
	FILE *f = fopen(path, "rb");
	if(!f) goto failed_open;
	if(fseek(f, 0, SEEK_END)) goto borked;
	long length = ftell(f);
	if(length <= 0 || (length % 4)) goto borked;
	if(!(buf->base = malloc(length))) goto borked;
	if(fseek(f, 0, SEEK_SET)) goto borked;
	if(fread((char *) buf->base, 1, length, f) != (size_t) length) goto borked;
	fclose(f);
	buf->len = (size_t) length;

	return VGFX_SUCCESS;
	borked:
		fclose(f);
		free(buf->base);
	failed_open:
		log_error("Failed to read/invalid spirv: %s", path);
		return VGFX_FAIL;
}

void destroy_spirv(vk_spirvbuf_t buf) {
	free(buf.base);
}

//Debugging Callbacks
VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_cb(
	VkDebugUtilsMessageSeverityFlagBitsEXT sev,
	VkDebugUtilsMessageTypeFlagsEXT type,
	const VkDebugUtilsMessengerCallbackDataEXT *cb_data,
	void *user_data
) {
	int is_simple = log_is_simple();
	log_set_simple(1);
	static const char *const s = "Vulkan Debug: %s";
	switch(sev) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
			log_trace(s, cb_data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
			log_trace(s, cb_data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			log_warn(s, cb_data->pMessage);
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			log_error(s, cb_data->pMessage);
			break;
		default:
			log_error("Invalid Vulkan Error Level: %s", cb_data->pMessage);
			break;
	};
	log_set_simple(is_simple);
	return VK_FALSE;
}

VkResult create_vk_debug_msg(
	VkInstance instance,
	const VkDebugUtilsMessengerCreateInfoEXT *create_info,
	const VkAllocationCallbacks *allocator,
	VkDebugUtilsMessengerEXT *cb
) {
	PFN_vkCreateDebugUtilsMessengerEXT f = (
		PFN_vkCreateDebugUtilsMessengerEXT
	) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if(!f) {
		log_error("Failed to get proc addr for vkCreateDebugUtilsMessengerEXT");
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
	return f(instance, create_info, allocator, cb);
}

void destroy_vk_debug_msg(
	VkInstance instance,
	VkDebugUtilsMessengerEXT cb,
	const VkAllocationCallbacks* allocator
) {
	PFN_vkDestroyDebugUtilsMessengerEXT f = (
		PFN_vkDestroyDebugUtilsMessengerEXT
	) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if(f) f(instance, cb, allocator);
}

int get_vk_validation(uint32_t *layer_count, VkLayerProperties *layer_props) {
	log_debug("Enabling vulkan validation layers");
	VkResult err = vkEnumerateInstanceLayerProperties(layer_count, NULL);
	if(err != VK_SUCCESS) goto borked;
	int found_lunarg_validation = 0;
	if(*layer_count) {
		layer_props = malloc(*layer_count * sizeof(*layer_props));
		if(!layer_props) {
			log_error("Failed to allocate memory for layer_props");
			return VGFX_MEM_FAIL;
		}
		err = vkEnumerateInstanceLayerProperties(layer_count, layer_props);
		if(err != VK_SUCCESS) goto borked;
		sds s = sdsnew("Discovered the following layers:");
		for(unsigned int i = 0; i < *layer_count; i++) {
			s = sdscatfmt(s, "\n\t%s", layer_props[i].layerName);
			if(!strcmp(
				layer_props[i].layerName,
				"VK_LAYER_KHRONOS_validation"
			)) found_lunarg_validation = 1;
		}
		log_debug(s);
		sdsfree(s);
	} else {
		log_debug("No vulkan validation layers found");
	}
	if(!found_lunarg_validation) return VGFX_NO_LUNARG;

	return VGFX_SUCCESS;
	borked:
		log_error("Vulcan layer enumeration failed on: %s", vkr2str(err));
		return VGFX_FAIL;
}

int init_vk_instance(vk_instance_t *instance, int use_validation) {
	const char ** vlayer_name = &(const char *) {"VK_LAYER_KHRONOS_validation"};
	log_debug("Creating vulkan instance");
	struct {
		uint32_t count;
		const char** names;
	} req_exts, tot_exts, layers;
	VkResult err;
	req_exts.names = glfwGetRequiredInstanceExtensions(&req_exts.count);
	tot_exts = req_exts;
	layers.count = 0;
	layers.names = NULL;

	if(use_validation) {
		//This is dumb, we're only looking for the return value, the function
		//parameters are ignored in all cases. Eventually this should test for more
		//than just the standard validation layer, in which case that free is gonna
		//have to move.
		VkLayerProperties *layer_props = NULL;
		int ret = get_vk_validation(&layers.count, layer_props);
		free(layer_props);
		if(ret == VGFX_SUCCESS) {
			layers.count = 1;
			layers.names = vlayer_name;
			tot_exts.names = malloc(++tot_exts.count * sizeof(*tot_exts.names));
			if(!tot_exts.names) {
				log_error("Failed to allocate memory for tot_exts.names");
				return VGFX_MEM_FAIL;
			}
			memcpy(
				tot_exts.names,
				req_exts.names,
				req_exts.count * sizeof(*req_exts.names)
			);
			tot_exts.names[req_exts.count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
		} else {
			layers.count = 0;
			log_error("No vulkan validation layer found");
		}
	}

	err = vkCreateInstance(
		&(VkInstanceCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.enabledLayerCount = layers.count,
			.ppEnabledLayerNames = layers.names,
			.enabledExtensionCount = tot_exts.count,
			.ppEnabledExtensionNames = tot_exts.names,
			.pApplicationInfo = &(VkApplicationInfo) {
				.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
				.pNext = NULL,
				.pApplicationName = NULL,
				.applicationVersion = 0,
				.pEngineName = NULL,
				.engineVersion = 0,
				.apiVersion = VK_API_VERSION_1_1
			}
		},
		NULL,
		&instance->handle
	);
	if(tot_exts.names != req_exts.names) free(tot_exts.names);
	if(err != VK_SUCCESS) goto borked;

	if(use_validation) {
		err = create_vk_debug_msg(
			instance->handle,
			&(const VkDebugUtilsMessengerCreateInfoEXT) {
				.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
				.pNext = NULL,
				.flags = 0,
				.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT    \
												 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT       \
												 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT    \
												 | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
				.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT            \
										 | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT         \
										 | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
				.pfnUserCallback = vk_debug_cb,
				.pUserData = NULL
			},
			NULL,
			&instance->debug_cb
		);
		if(err != VK_SUCCESS) goto borked;
	}

	return VGFX_SUCCESS;
	borked:
		log_error("Vulkan instancing failed on: %s", vkr2str(err));
		return VGFX_FAIL;
}

void destroy_vk_instance(vk_instance_t instance) {
	destroy_vk_debug_msg(instance.handle, instance.debug_cb, NULL);
	vkDestroyInstance(instance.handle, NULL);
}

int init_vk_surface(
	vk_surface_t *surface,
	vk_instance_t instance,
	GLFWwindow *window
) {
	VkResult err = glfwCreateWindowSurface(
		instance.handle,
		window,
		NULL,
		&surface->handle
	);
	if(err != VK_SUCCESS) {
		log_error("Surface creation failed on: %s", vkr2str(err));
		return VGFX_FAIL;
	}
	surface->window = window;
	return VGFX_SUCCESS;
}

void destroy_vk_surface(vk_surface_t surface, vk_instance_t instance) {
	vkDestroySurfaceKHR(instance.handle, surface.handle, NULL);
}

int init_vk_phydevs(vk_phydevs_t *phydevs, vk_instance_t instance) {
	*phydevs = (const vk_phydevs_t) { 0 };
	sds s = sdsnew("Discovered the following devices:");
	VkResult err = vkEnumeratePhysicalDevices(
		instance.handle,
		&phydevs->count,
		NULL
	);
	if(err != VK_SUCCESS) goto borked;
	if(!phydevs->count) {
		log_error("Vulkan reports no available physical devices");
		return VGFX_FAIL;
	}

	phydevs->handles = malloc(phydevs->count * sizeof(*phydevs->handles));
	if(!phydevs->handles) goto borked_malloc;
	phydevs->props = malloc(phydevs->count * sizeof(*phydevs->props));
	if(!phydevs->props) goto borked_malloc;
	phydevs->feats = malloc(phydevs->count * sizeof(*phydevs->feats));
	if(!phydevs->feats) goto borked_malloc;
	if((phydevs->qfams = malloc(phydevs->count * sizeof(*phydevs->qfams))))
		for(unsigned int i = 0; i < phydevs->count; i++)
			phydevs->qfams[i].props = NULL;
	else goto borked_malloc;
	if((phydevs->exts = malloc(phydevs->count * sizeof(*phydevs->exts))))
		for(unsigned int i = 0; i < phydevs->count; i++)
			phydevs->exts[i].props = NULL;
	else goto borked_malloc;

	err = vkEnumeratePhysicalDevices(
		instance.handle,
		&phydevs->count,
		phydevs->handles
	);
	if(err != VK_SUCCESS) goto borked;
	for(unsigned int i = 0; i < phydevs->count; i++) {
		vkGetPhysicalDeviceProperties(phydevs->handles[i], &phydevs->props[i]);
		vkGetPhysicalDeviceFeatures(phydevs->handles[i], &phydevs->feats[i]);
		vkGetPhysicalDeviceQueueFamilyProperties(
			phydevs->handles[i],
			&phydevs->qfams[i].count,
			NULL
		);
		phydevs->qfams[i].props = malloc(
			phydevs->qfams[i].count * sizeof(*phydevs->qfams[i].props)
		);
		if(!phydevs->qfams[i].props) goto borked_malloc;
		vkGetPhysicalDeviceQueueFamilyProperties(
			phydevs->handles[i],
			&phydevs->qfams[i].count,
			phydevs->qfams[i].props
		);
		s = sdscatfmt(s, "\n\t%s", phydevs->props[i].deviceName);
		err = vkEnumerateDeviceExtensionProperties(
			phydevs->handles[i],
			NULL,
			&phydevs->exts[i].count,
			NULL
		);
		if(err != VK_SUCCESS) goto borked;
		phydevs->exts[i].props = malloc(
			phydevs->exts[i].count * sizeof(*phydevs->exts[i].props)
		);
		if(!phydevs->exts[i].props) goto borked_malloc;
		err = vkEnumerateDeviceExtensionProperties(
			phydevs->handles[i],
			NULL,
			&phydevs->exts[i].count,
			phydevs->exts[i].props
		);
		if(err != VK_SUCCESS) goto borked;
	}
	log_debug(s);
	sdsfree(s);

	return VGFX_SUCCESS;
	borked_malloc:
		log_error("Failed to allocate memory for devices");
		destroy_vk_phydevs(*phydevs);
		sdsfree(s);
		return VGFX_MEM_FAIL;
	borked:
		log_error("Vulkan device enumeration failed on: %s", vkr2str(err));
		destroy_vk_phydevs(*phydevs);
		sdsfree(s);
		return VGFX_FAIL;
}

void destroy_vk_phydevs(vk_phydevs_t phydevs) {
	free(phydevs.handles);
	free(phydevs.props);
	free(phydevs.feats);
	for(unsigned int i = 0; i < phydevs.count; i++) {
		free(phydevs.qfams[i].props);
		free(phydevs.exts[i].props);
	}
	free(phydevs.qfams);
	free(phydevs.exts);
}

int select_vk_queues(
	VkPhysicalDevice handle,
	vk_qfamilies_t qfams,
	VkSurfaceKHR surf_handle,
	uint32_t *gfx_qfam,
	uint32_t *xfr_qfam
) {
	VkResult err;
	*gfx_qfam = UINT32_MAX;
	*xfr_qfam = UINT32_MAX;
	for(unsigned int i = 0; i < qfams.count; i++) {
		VkBool32 surface_support;
		err = vkGetPhysicalDeviceSurfaceSupportKHR(
			handle,
			i,
			surf_handle,
			&surface_support
		);
		if(err != VK_SUCCESS) goto borked;
		if(
			qfams.props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
			surface_support == VK_TRUE
		) {
			*gfx_qfam = i;
		} else if( //Look for a dedicated transfer queue
			qfams.props[i].queueFlags & VK_QUEUE_TRANSFER_BIT &&
			!(qfams.props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
			!(qfams.props[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
		) {
			*xfr_qfam = i;
		}
	}
	if(*gfx_qfam == UINT32_MAX) return VGFX_NO_GRAPHICS_QUEUE;
	// If there isn't a dedicated transfer queue family, just use the graphics
	// queue family
	if(*xfr_qfam == UINT32_MAX) *xfr_qfam = *gfx_qfam;
	return VGFX_SUCCESS;
	borked:
		log_error("Vulkan queue selection failed on: %s", vkr2str(err));
		return VGFX_FAIL;
}

//Select a device and queue family for a given surface
int init_vk_seldev(
	vk_seldev_t *seldev,
	vk_phydevs_t phydevs,
	vk_surface_t surface
) {
	VkResult err;
	int success = 0;
	*seldev = (const vk_seldev_t) {0};
	for(unsigned int i = 0; i < phydevs.count; i++) {
		int ret = select_vk_queues(
			phydevs.handles[i],
			phydevs.qfams[i],
			surface.handle,
			&seldev->gfx_qfam,
			&seldev->xfr_qfam
		);
		if(ret == VGFX_NO_GRAPHICS_QUEUE) continue;
		if(ret == VGFX_FAIL) return VGFX_FAIL;
		seldev->gfx_props = phydevs.qfams[i].props[seldev->gfx_qfam];
		seldev->xfr_props = phydevs.qfams[i].props[seldev->xfr_qfam];

		//Explicitly verify swapchain support
		for(unsigned int j = 0; j < phydevs.exts[i].count; j++) {
			if(!strcmp(
				phydevs.exts[i].props[j].extensionName,
				VK_KHR_SWAPCHAIN_EXTENSION_NAME
			)) {
				success = 1;
				break;
			}
		}
		if(!success) continue;
		success = 0;

		err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			phydevs.handles[i],
			surface.handle,
			&seldev->caps
		);
		if(err != VK_SUCCESS) goto borked;

		//Verify at least one surface format
		err = vkGetPhysicalDeviceSurfaceFormatsKHR(
			phydevs.handles[i],
			surface.handle,
			&seldev->formats_count,
			NULL
		);
		if(err != VK_SUCCESS) goto borked;
		if(!seldev->formats_count) continue;
		seldev->formats = malloc(seldev->formats_count * sizeof(*seldev->formats));
		if(!seldev->formats) goto borked_malloc;
		err = vkGetPhysicalDeviceSurfaceFormatsKHR(
			phydevs.handles[i],
			surface.handle,
			&seldev->formats_count,
			seldev->formats
		);
		if(err != VK_SUCCESS) goto borked;

		//Verify at least one present mode
		err = vkGetPhysicalDeviceSurfacePresentModesKHR(
			phydevs.handles[i],
			surface.handle,
			&seldev->pmodes_count,
			NULL
		);
		if(err != VK_SUCCESS) goto borked;
		if(!seldev->pmodes_count) {
			free(seldev->formats);
			seldev->formats = NULL;
			continue;
		}
		seldev->pmodes = malloc(seldev->pmodes_count * sizeof(*seldev->pmodes));
		if(!seldev->pmodes) goto borked_malloc;
		err = vkGetPhysicalDeviceSurfacePresentModesKHR(
			phydevs.handles[i],
			surface.handle,
			&seldev->pmodes_count,
			seldev->pmodes
		);
		if(err != VK_SUCCESS) goto borked;

		seldev->handle = phydevs.handles[i];
		seldev->dev_props = phydevs.props[i];
		seldev->feats = phydevs.feats[i];
		success = 1;
		break;
	}
	if(success) log_debug("Selected device: %s", seldev->dev_props.deviceName);

	return success ? VGFX_SUCCESS : VGFX_FAIL;
	borked_malloc:
		log_error("Failed to allocate memory for seldev");
		destroy_vk_seldev(*seldev);
		return VGFX_MEM_FAIL;
	borked:
		log_error("Vulkan device selection failed on: %s", vkr2str(err));
		destroy_vk_seldev(*seldev);
		return VGFX_FAIL;
}

void destroy_vk_seldev(vk_seldev_t seldev) {
	free(seldev.formats);
	free(seldev.pmodes);
}

int init_vk_logicdev(
	vk_logicdev_t *logicdev,
	vk_seldev_t seldev
) {
	const float *priorities = &(const float) {1.0};
	uint32_t create_info_count;
	VkDeviceQueueCreateInfo *create_info;
	logicdev->gfx_qfam = seldev.gfx_qfam;
	logicdev->xfr_qfam = seldev.xfr_qfam;
	logicdev->unified_q = 0;
	//Device doesn't support dedicated transfer queue
	if(seldev.gfx_qfam == seldev.xfr_qfam) {
		logicdev->single_qfam = 1;
		create_info_count = 1;
		create_info = &(VkDeviceQueueCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.queueFamilyIndex = seldev.gfx_qfam,
			.queueCount = 2,
			.pQueuePriorities = priorities
		};
		//Worst case scenario, transfers will be done on main graphics queue
		if(seldev.gfx_props.queueCount == 1) {
			create_info->queueCount = 1;
			logicdev->unified_q = 1;
		}
	} else {
		logicdev->single_qfam = 0;
		create_info_count = 2;
		create_info = (VkDeviceQueueCreateInfo[]) {
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.queueFamilyIndex = seldev.gfx_qfam,
				.queueCount = 1,
				.pQueuePriorities = priorities
			},
			{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.queueFamilyIndex = seldev.xfr_qfam,
				.queueCount = 1,
				.pQueuePriorities = priorities
			}
		};
	}
	VkResult err = vkCreateDevice(
		seldev.handle,
		&(const VkDeviceCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.queueCreateInfoCount = create_info_count,
			.pQueueCreateInfos = create_info,
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = NULL,
			.enabledExtensionCount = 1,
			.ppEnabledExtensionNames = &(const char *const) {
				VK_KHR_SWAPCHAIN_EXTENSION_NAME
			},
			//ToDo: Don't just enable all features
			.pEnabledFeatures = &seldev.feats
		},
		NULL,
		&logicdev->handle
	);
	if(err != VK_SUCCESS) goto borked;

	vkGetDeviceQueue(logicdev->handle, seldev.gfx_qfam, 0, &logicdev->gfx_q);
	if(logicdev->unified_q) {
		logicdev->xfr_q = logicdev->gfx_q;
	} else if(logicdev->single_qfam) {
		vkGetDeviceQueue(logicdev->handle, seldev.xfr_qfam, 1, &logicdev->xfr_q);
	} else {
		vkGetDeviceQueue(logicdev->handle, seldev.xfr_qfam, 0, &logicdev->xfr_q);
	}

	return VGFX_SUCCESS;
	borked:
		log_error("Vulkan logical device creation failed on: %s", vkr2str(err));
		return VGFX_FAIL;
}

void destroy_vk_logicdev(vk_logicdev_t logicdev) {
	vkDestroyDevice(logicdev.handle, NULL);
}

int init_vk_swapchain(
	vk_swapchain_t *swapchain,
	vk_surface_t surface,
	vk_seldev_t seldev,
	vk_logicdev_t logicdev
) {
	swapchain->image_handles = NULL;
	swapchain->logicdev = logicdev;
	VkColorSpaceKHR color_space;

	//1 & format undefined mean "use whatever"
	if(
		seldev.formats_count == 1 &&
		seldev.formats[0].format == VK_FORMAT_UNDEFINED
	) {
		swapchain->image_format = VK_FORMAT_B8G8R8A8_UNORM;
		color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	} else {
		//We want rgba8 and srgb nonlinear, if that fails we'll just grab whatever
		//the first listed color format and space are
		swapchain->image_format = seldev.formats[0].format;
		color_space = seldev.formats[0].colorSpace;
		for(unsigned int i = 0; i < seldev.formats_count; i++) {
			if(
				seldev.formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
				seldev.formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
			) {
				swapchain->image_format = VK_FORMAT_B8G8R8A8_UNORM;
				color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
				break;
			}
		}
	}

	//Spec requires this always be available
	VkPresentModeKHR pmode = VK_PRESENT_MODE_FIFO_KHR;
	for(unsigned int i = 0; i < seldev.pmodes_count; i++) {
		log_trace(
			"Discovered the following Present Mode: %s",
			pmode2str(seldev.pmodes[i])
		);
		//Prefered mode, allows triple buffering and vsync
		if(seldev.pmodes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			pmode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		//Second choice, just blit images as fast as possible
		} else if(seldev.pmodes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
			pmode = VK_PRESENT_MODE_IMMEDIATE_KHR;
	}
	log_trace(
		"Selected the %s Present Mode",
		pmode2str(pmode)
	);
	if(seldev.caps.currentExtent.width != UINT32_MAX) {
		swapchain->image_extent = seldev.caps.currentExtent;
	} else {
		//UINT32_MAX means the WM is telling us to wing it, so we try to meet the
		//glfw framebuffer size, limited by the device capabilities.
		uint32_t w, h;
		glfwGetFramebufferSize(surface.window, (int *) &w, (int *) &h);
		w = w < seldev.caps.maxImageExtent.width ?
			w : seldev.caps.maxImageExtent.width;
		w = w > seldev.caps.minImageExtent.width ?
			w : seldev.caps.maxImageExtent.width;
		h = h < seldev.caps.maxImageExtent.height ?
			h : seldev.caps.maxImageExtent.height;
		h = h > seldev.caps.minImageExtent.height ?
			h : seldev.caps.maxImageExtent.height;
		swapchain->image_extent.width = w;
		swapchain->image_extent.height = h;
	}
	//See:
	//https://github.com/KhronosGroup/Vulkan-Docs/blob/master/appendices/VK_KHR_swapchain.txt#L231
	if(
		pmode == VK_PRESENT_MODE_MAILBOX_KHR &&
		seldev.caps.minImageCount != seldev.caps.maxImageCount
	) swapchain->image_count = seldev.caps.minImageCount + 1;
	else swapchain->image_count = seldev.caps.minImageCount;

	VkResult err = vkCreateSwapchainKHR(
		logicdev.handle,
		&(const VkSwapchainCreateInfoKHR) {
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = NULL,
			.flags = 0,
			.surface = surface.handle,
			.minImageCount = swapchain->image_count,
			.imageFormat = swapchain->image_format,
			.imageColorSpace = color_space,
			.imageExtent = swapchain->image_extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = NULL,
			.preTransform = seldev.caps.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
			.presentMode = pmode,
			.clipped = VK_TRUE,
			.oldSwapchain = VK_NULL_HANDLE
		},
		NULL,
		&swapchain->handle
	);
	if(err != VK_SUCCESS) goto borked;
	err = vkGetSwapchainImagesKHR(
		logicdev.handle,
		swapchain->handle,
		&swapchain->image_count,
		NULL
	);
	if(err != VK_SUCCESS) goto borked2;
	if(!(swapchain->image_handles = malloc(
		swapchain->image_count * sizeof(*swapchain->image_handles)
	))) {
		log_error("Failed to allocate memory for swapchain images");
		destroy_vk_swapchain(*swapchain);
		return VGFX_MEM_FAIL;
	}
	err = vkGetSwapchainImagesKHR(
		logicdev.handle,
		swapchain->handle,
		&swapchain->image_count,
		swapchain->image_handles
	);
	if(err != VK_SUCCESS) goto borked2;

	return VGFX_SUCCESS;
	borked2:
		destroy_vk_swapchain(*swapchain);
	borked:
		log_error("Vulkan swapchain creation failed on: %s", vkr2str(err));
		return VGFX_FAIL;
}

void destroy_vk_swapchain(vk_swapchain_t swapchain) {
	free(swapchain.image_handles);
	vkDestroySwapchainKHR(swapchain.logicdev.handle, swapchain.handle, NULL);
}

int init_vk_imageviews(vk_imageviews_t *imageviews, vk_swapchain_t swapchain) {
	imageviews->handles = NULL;
	imageviews->count = swapchain.image_count;
	imageviews->logicdev = swapchain.logicdev;
	if(!(imageviews->handles = malloc(
		imageviews->count * sizeof(*imageviews->handles)
	))) {
		log_error("Failed to allocate memory for image views");
		return VGFX_MEM_FAIL;
	}
	for(unsigned int i = 0; i < imageviews->count; i++) {
		VkResult err = vkCreateImageView(
			imageviews->logicdev.handle,
			&(const VkImageViewCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.image = swapchain.image_handles[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = swapchain.image_format,
				.components = (const VkComponentMapping) {
					.r = VK_COMPONENT_SWIZZLE_IDENTITY,
					.g = VK_COMPONENT_SWIZZLE_IDENTITY,
					.b = VK_COMPONENT_SWIZZLE_IDENTITY,
					.a = VK_COMPONENT_SWIZZLE_IDENTITY
				},
				.subresourceRange = (const VkImageSubresourceRange) {
					.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
					.baseMipLevel = 0,
					.levelCount = 1,
					.baseArrayLayer = 0,
					.layerCount = 1
				}
			},
			NULL,
			&imageviews->handles[i]
		);
		if(err != VK_SUCCESS) {
			imageviews->count = i;
			destroy_vk_imageviews(*imageviews);
			log_error("Vulkan imageview creation failed on: %s", vkr2str(err));
			return VGFX_FAIL;
		}
	}
	return VGFX_SUCCESS;
}

void destroy_vk_imageviews(vk_imageviews_t imageviews) {
	if(imageviews.handles) {
		for(unsigned int i = 0; i < imageviews.count; i++) {
			vkDestroyImageView(
				imageviews.logicdev.handle,
				imageviews.handles[i],
				NULL
			);
		}
		free(imageviews.handles);
	}
}

int init_vk_renderpass(vk_renderpass_t *renderpass, vk_swapchain_t swapchain) {
	renderpass->logicdev = swapchain.logicdev;
	VkResult err = vkCreateRenderPass(
		swapchain.logicdev.handle,
		&(const VkRenderPassCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = &(const VkAttachmentDescription) {
				.flags = 0,
				.format = swapchain.image_format,
				.samples = VK_SAMPLE_COUNT_1_BIT,
				.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			},
			.subpassCount = 1,
			.pSubpasses = &(const VkSubpassDescription) {
				.flags = 0,
				.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
				.inputAttachmentCount = 0,
				.pInputAttachments = NULL,
				.colorAttachmentCount = 1,
				.pColorAttachments = &(const VkAttachmentReference) {
					.attachment = 0,
					.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				},
				.pResolveAttachments = NULL,
				.pDepthStencilAttachment = NULL,
				.preserveAttachmentCount = 0,
				.pPreserveAttachments = NULL,
			},
			.dependencyCount = 1,
			.pDependencies = &(const VkSubpassDependency) {
				.srcSubpass = VK_SUBPASS_EXTERNAL,
				.dstSubpass = 0,
				.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask = 0,
				.dstAccessMask = (
					VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
					VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
				),
				.dependencyFlags = 0
			}
		},
		NULL,
		&renderpass->handle
	);
	if(err != VK_SUCCESS) return VGFX_FAIL;
	return VGFX_SUCCESS;
}

void destroy_vk_renderpass(vk_renderpass_t renderpass) {
	vkDestroyRenderPass(renderpass.logicdev.handle, renderpass.handle, NULL);
}

int init_vk_gpipe(
	vk_gpipe_t *gpipe,
	vk_swapchain_t swapchain,
	vk_renderpass_t renderpass
) {
	gpipe->logicdev = swapchain.logicdev;
	vk_spirvbuf_t vert_code, frag_code;
	if(read_spirv(&vert_code, "shaders/shader.vert.spv")) return VGFX_FAIL;
	if(read_spirv(&frag_code, "shaders/shader.frag.spv")) {
		destroy_spirv(vert_code);
		return VGFX_FAIL;
	}

	VkShaderModule vert, frag;
	VkResult err = vkCreateShaderModule(
		swapchain.logicdev.handle,
		&(const VkShaderModuleCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.codeSize = vert_code.len,
			.pCode = vert_code.base
		},
		NULL,
		&vert
	);
	destroy_spirv(vert_code);
	if(err != VK_SUCCESS) {
		destroy_spirv(frag_code);
		goto borked_shader;
	}

	err = vkCreateShaderModule(
		swapchain.logicdev.handle,
		&(const VkShaderModuleCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.codeSize = frag_code.len,
			.pCode = frag_code.base
		},
		NULL,
		&frag
	);
	destroy_spirv(frag_code);
	if(err != VK_SUCCESS) {
		vkDestroyShaderModule(swapchain.logicdev.handle, vert, NULL);
		goto borked_shader;
	}

	VkPipelineShaderStageCreateInfo shader_stages[] = {
		//Vertex
		(const VkPipelineShaderStageCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vert,
			.pName = "main",
			.pSpecializationInfo = NULL
		},
		//Fragment
		(const VkPipelineShaderStageCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = frag,
			.pName = "main",
			.pSpecializationInfo = NULL
		}
	};

	err = vkCreatePipelineLayout(
		swapchain.logicdev.handle,
		&(const VkPipelineLayoutCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.setLayoutCount = 0,
			.pSetLayouts = NULL,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = NULL
		},
		NULL,
		&gpipe->layout_handle
	);
	if(err != VK_SUCCESS) goto borked_layout;

	err = vkCreateGraphicsPipelines(
		swapchain.logicdev.handle,
		VK_NULL_HANDLE,
		1,
		&(const VkGraphicsPipelineCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0,
			.stageCount = 2,
			.pStages = shader_stages,
			.pVertexInputState = &(const VkPipelineVertexInputStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.vertexBindingDescriptionCount = 1,
				.pVertexBindingDescriptions =
					&(const VkVertexInputBindingDescription) {
						.binding = 0,
						.stride = sizeof(*vertices),
						.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
					},
				.vertexAttributeDescriptionCount = 2,
				.pVertexAttributeDescriptions =
					(const VkVertexInputAttributeDescription[]) {
						{
							.location = 0,
							.binding = 0,
							.format = VK_FORMAT_R32G32_SFLOAT,
							.offset = offsetof(vgfx_vertex, pos)
						},
						{
							.location = 1,
							.binding = 0,
							.format = VK_FORMAT_R32G32B32_SFLOAT,
							.offset = offsetof(vgfx_vertex, color)
						}
					}
			},
			.pInputAssemblyState = &(const VkPipelineInputAssemblyStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
				.primitiveRestartEnable = VK_FALSE
			},
			.pTessellationState = NULL,
			.pViewportState = &(const VkPipelineViewportStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.viewportCount = 1,
				.pViewports = &(const VkViewport) {
					.x = 0.0f,
					.y = 0.0f,
					.width = (float) swapchain.image_extent.width,
					.height = (float) swapchain.image_extent.height,
					.minDepth = 0.0f,
					.maxDepth = 1.0f
				},
				.scissorCount = 1,
				.pScissors = &(const VkRect2D) {
					.offset = {0, 0},
					.extent = swapchain.image_extent
				},
			},
			.pRasterizationState = &(const VkPipelineRasterizationStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.depthClampEnable = VK_FALSE,
				.rasterizerDiscardEnable = VK_FALSE,
				.polygonMode = VK_POLYGON_MODE_FILL,
				.cullMode = VK_CULL_MODE_BACK_BIT,
				.frontFace = VK_FRONT_FACE_CLOCKWISE,
				.depthBiasEnable = VK_FALSE,
				.depthBiasConstantFactor = 0.0f,
				.depthBiasClamp = 0.0f,
				.depthBiasSlopeFactor = 0.0f,
				.lineWidth = 1.0f
			},
			.pMultisampleState = &(const VkPipelineMultisampleStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
				.sampleShadingEnable = VK_FALSE,
				.minSampleShading = 1.0f,
				.pSampleMask = NULL,
				.alphaToCoverageEnable = VK_FALSE,
				.alphaToOneEnable = VK_FALSE
			},
			.pDepthStencilState = NULL,
			.pColorBlendState = &(const VkPipelineColorBlendStateCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.logicOpEnable = VK_FALSE,
				.logicOp = VK_LOGIC_OP_COPY,
				.attachmentCount = 1,
				.pAttachments = &(const VkPipelineColorBlendAttachmentState) {
					.blendEnable = VK_FALSE,
					.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
					.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
					.colorBlendOp = VK_BLEND_OP_ADD,
					.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
					.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
					.alphaBlendOp = VK_BLEND_OP_ADD,
					.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
														VK_COLOR_COMPONENT_G_BIT |
														VK_COLOR_COMPONENT_B_BIT |
														VK_COLOR_COMPONENT_A_BIT
				},
				.blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
			},
			.pDynamicState = NULL,
			.layout = gpipe->layout_handle,
			.renderPass = renderpass.handle,
			.subpass = 0,
			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = -1
		},
		NULL,
		&gpipe->handle
	);
	if(err != VK_SUCCESS) goto borked_pipeline;

	vkDestroyShaderModule(swapchain.logicdev.handle, vert, NULL);
	vkDestroyShaderModule(swapchain.logicdev.handle, frag, NULL);
	return VGFX_SUCCESS;

	borked_pipeline:
		vkDestroyPipelineLayout(
			swapchain.logicdev.handle,
			gpipe->layout_handle,
			NULL
		);
	borked_layout:
		vkDestroyShaderModule(swapchain.logicdev.handle, vert, NULL);
		vkDestroyShaderModule(swapchain.logicdev.handle, frag, NULL);
	borked_shader:
		log_error("Graphics Pipeline initiation failed on: %s", vkr2str(err));
		return VGFX_FAIL;
}

void destroy_vk_gpipe(vk_gpipe_t gpipe) {
	vkDestroyPipeline(gpipe.logicdev.handle, gpipe.handle, NULL);
	vkDestroyPipelineLayout(gpipe.logicdev.handle, gpipe.layout_handle, NULL);
}

int init_vk_framebuffers(
	vk_framebuffers_t *framebuffers,
	vk_swapchain_t swapchain,
	vk_renderpass_t renderpass,
	vk_imageviews_t imageviews
) {
	framebuffers->logicdev = imageviews.logicdev;
	framebuffers->count = imageviews.count;
	framebuffers->handles = malloc(framebuffers->count * sizeof(*framebuffers));
	if(!framebuffers) {
		log_error("Failed to allocate memory for framebuffers");
		return VGFX_MEM_FAIL;
	}
	for(unsigned int i = 0; i < framebuffers->count; i++) {
		VkResult err = vkCreateFramebuffer(
			framebuffers->logicdev.handle,
			&(const VkFramebufferCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.pNext = NULL,
				.flags = 0,
				.renderPass = renderpass.handle,
				.attachmentCount = 1,
				.pAttachments = &imageviews.handles[i],
				.width = swapchain.image_extent.width,
				.height = swapchain.image_extent.height,
				.layers = 1
			},
			NULL,
			&framebuffers->handles[i]
		);
		if(err != VK_SUCCESS) {
			for(unsigned int j = 0; j < i; j++) {
				vkDestroyFramebuffer(
					framebuffers->logicdev.handle,
					framebuffers->handles[j],
					NULL
				);
			}
			free(framebuffers->handles);
			log_error("Failed to init framebuffers on: %s", vkr2str(err));
			return VGFX_FAIL;
		}
	}
	return VGFX_SUCCESS;
}

void destroy_vk_framebuffers(vk_framebuffers_t framebuffers) {
	for(unsigned int i = 0; i < framebuffers.count; i++) {
		vkDestroyFramebuffer(
			framebuffers.logicdev.handle,
			framebuffers.handles[i],
			NULL
		);
	}
	free(framebuffers.handles);
}

int init_vk_commandpools(
	vk_commandpools_t *commandpools,
	vk_seldev_t seldev,
	vk_logicdev_t logicdev
) {
	VkCommandPoolCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.queueFamilyIndex = seldev.gfx_qfam
	};
	commandpools->logicdev = logicdev;
	VkResult err = vkCreateCommandPool(
		logicdev.handle,
		&info,
		NULL,
		&commandpools->gfx_handle
	);
	if(err != VK_SUCCESS) goto borked;
	if(logicdev.unified_q) {
		commandpools->xfr_handle = commandpools->gfx_handle;
	} else {
		info.queueFamilyIndex = seldev.xfr_qfam;
		info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
		err = vkCreateCommandPool(
			logicdev.handle,
			&info,
			NULL,
			&commandpools->xfr_handle
		);
		if(err != VK_SUCCESS) {
			vkDestroyCommandPool(logicdev.handle, commandpools->gfx_handle, NULL);
			goto borked;
		}
	}
	return VGFX_SUCCESS;
	borked:
		log_error("Failed to init commandpool on: %s", vkr2str(err));
		return VGFX_FAIL;
}

void destroy_vk_commandpools(vk_commandpools_t commandpools) {
	vkDestroyCommandPool(
		commandpools.logicdev.handle, commandpools.gfx_handle, NULL
	);
	if(!commandpools.logicdev.unified_q) {
		vkDestroyCommandPool(
			commandpools.logicdev.handle, commandpools.xfr_handle, NULL
		);
	}
}

//begin_single is specifically for one-off buffers
int get_command_buffer(vk_combuf_t *combuf, int begin_single) {
	VkResult err = vkAllocateCommandBuffers(
		combuf->dev,
		&(const VkCommandBufferAllocateInfo) {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = NULL,
			.commandPool = combuf->cpool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		},
		&combuf->buf
	);
	if(err != VK_SUCCESS) goto borked;
	if(begin_single) {
		err = vkBeginCommandBuffer(
			combuf->buf,
			&(const VkCommandBufferBeginInfo) {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = NULL,
				.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
				.pInheritanceInfo = NULL
			}
		);
		if(err != VK_SUCCESS) {
			vkFreeCommandBuffers(combuf->dev, combuf->cpool, 1, &combuf->buf);
			goto borked;
		}
	}
	return VGFX_SUCCESS;
	borked:
		log_error("Failed to get combuf on: %s", vkr2str(err));
		return VGFX_FAIL;
}

int flush_command_buffer(vk_combuf_t combuf) {
	VkResult err = vkEndCommandBuffer(combuf.buf);
	if(err != VK_SUCCESS) goto borked;
	VkFence fence;
	err = vkCreateFence(
		combuf.dev,
		&(const VkFenceCreateInfo) {
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = NULL,
			.flags = 0
		},
		NULL,
		&fence
	);
	if(err != VK_SUCCESS) goto borked;
	err = vkQueueSubmit(
		combuf.q,
		1,
		&(const VkSubmitInfo) {
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = NULL,
			.waitSemaphoreCount = 0,
			.pWaitSemaphores = NULL,
			.pWaitDstStageMask = NULL,
			.commandBufferCount = 1,
			.pCommandBuffers = &combuf.buf,
			.signalSemaphoreCount = 0,
			.pSignalSemaphores = NULL
		},
		fence
	);
	if(err != VK_SUCCESS) goto borked_submit;
	err = vkWaitForFences(combuf.dev, 1, &fence, VK_TRUE, UINT64_MAX);
	if(err != VK_SUCCESS) goto borked_submit;

	vkDestroyFence(combuf.dev, fence, NULL);
	vkFreeCommandBuffers(combuf.dev, combuf.cpool, 1, &combuf.buf);
	return VGFX_SUCCESS;
	borked_submit:
		vkDestroyFence(combuf.dev, fence, NULL);
	borked:
		vkFreeCommandBuffers(combuf.dev, combuf.cpool, 1, &combuf.buf);
		log_error("Failed to flush combuf on: %s", vkr2str(err));
		return VGFX_FAIL;
}

//Basically this:
//https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
//Holy shit this function got out of control
//For vertex buffers:
//  usage: VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
//  dst_access: VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT
//For index buffers:
//  usage: VK_BUFFER_USAGE_INDEX_BUFFER_BIT
//  dst_access: VK_ACCESS_INDEX_READ_BIT
int build_and_copy_buf(
	vk_buffer_t *local_buf,
	const void *data,
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkAccessFlags dst_access,
	VmaAllocator allocator,
	vk_commandpools_t cpools
) {
	VkBufferCreateInfo buf_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.pNext = NULL,
		.flags = 0,
		.size = size,
		.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount = 0,
		.pQueueFamilyIndices = NULL
	};
	VmaAllocationCreateInfo vma_info = {
		.flags = 0,
		//Guaranteed to be HOST_VISIBLE and HOST_COHERENT
		.usage = VMA_MEMORY_USAGE_CPU_ONLY,
		.requiredFlags = 0,
		.preferredFlags = 0,
		.memoryTypeBits = 0,
		.pool = VK_NULL_HANDLE,
		.pUserData = NULL
	};
	//ToDo: Create a static buffer for these transfers and leave it mapped
	//instead of constantly reallocating and mapping a new one everytime we do a
	//transfer. Consider VMA_ALLOCATION_CREATE_MAPPED_BIT
	vk_buffer_t host_buf;
	VkResult err = vmaCreateBuffer(
		allocator,
		&buf_info,
		&vma_info,
		&host_buf.handle,
		&host_buf.alloc,
		NULL
	);
	if(err != VK_SUCCESS) goto borked;

	void *host_mem;
	//ToDo: Error check this?
	vmaMapMemory(allocator, host_buf.alloc, &host_mem);
	memcpy(host_mem, data, size);
	vmaUnmapMemory(allocator, host_buf.alloc);

	buf_info.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	vma_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	err = vmaCreateBuffer(
		allocator,
		&buf_info,
		&vma_info,
		&local_buf->handle,
		&local_buf->alloc,
		NULL
	);
	if(err != VK_SUCCESS) {
		vmaDestroyBuffer(allocator, host_buf.handle, host_buf.alloc);
		goto borked;
	}

	vk_combuf_t xfr_combuf = {
		.dev = cpools.logicdev.handle,
		.q = cpools.logicdev.xfr_q,
		.cpool = cpools.xfr_handle,
	};
	if(get_command_buffer(&xfr_combuf, 1)) {
		err = VK_RESULT_MAX_ENUM;
		goto borked_buffers;
	}
	vkCmdCopyBuffer(
		xfr_combuf.buf,
		host_buf.handle,
		local_buf->handle,
		1,
		&(const VkBufferCopy) {
			.srcOffset = 0,
			.dstOffset = 0,
			.size = size
		}
	);

	VkSemaphore semaphore;
	VkFence fence;
	if(cpools.logicdev.single_qfam) {
		if(flush_command_buffer(xfr_combuf)) {
			err = VK_RESULT_MAX_ENUM;
			goto borked_buffers;
		}
	} else {
		vkCmdPipelineBarrier(
			xfr_combuf.buf,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			0, 0, NULL, 1,
			&(const VkBufferMemoryBarrier) {
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				.pNext = NULL,
				.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
				.dstAccessMask = 0,
				.srcQueueFamilyIndex = cpools.logicdev.xfr_qfam,
				.dstQueueFamilyIndex = cpools.logicdev.gfx_qfam,
				.buffer = local_buf->handle,
				.offset = 0,
				.size = size
			},
			0, NULL
		);
		err = vkEndCommandBuffer(xfr_combuf.buf);
		if(err != VK_SUCCESS) goto borked_xfrbuf;

		err = vkCreateSemaphore(
			cpools.logicdev.handle,
			&(const VkSemaphoreCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0
			},
			NULL,
			&semaphore
		);
		if(err != VK_SUCCESS) goto borked_xfrbuf;
		err = vkQueueSubmit(
			cpools.logicdev.xfr_q,
			1,
			&(const VkSubmitInfo) {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.pNext = NULL,
				.waitSemaphoreCount = 0,
				.pWaitSemaphores = NULL,
				.pWaitDstStageMask = NULL,
				.commandBufferCount = 1,
				.pCommandBuffers = &xfr_combuf.buf,
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = &semaphore
			},
			VK_NULL_HANDLE
		);
		if(err != VK_SUCCESS) goto borked_semaphore;

		vk_combuf_t gfx_combuf = {
			.dev = cpools.logicdev.handle,
			.q = cpools.logicdev.gfx_q,
			.cpool = cpools.gfx_handle,
		};
		if(get_command_buffer(&gfx_combuf, 1)) {
			err = VK_RESULT_MAX_ENUM;
			goto borked_semaphore;
		}
		vkCmdPipelineBarrier(
			gfx_combuf.buf,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,
			0, 0, NULL, 1,
			&(const VkBufferMemoryBarrier) {
				.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
				.pNext = NULL,
				.srcAccessMask = 0,
				.dstAccessMask = dst_access,
				.srcQueueFamilyIndex = cpools.logicdev.xfr_qfam,
				.dstQueueFamilyIndex = cpools.logicdev.gfx_qfam,
				.buffer = local_buf->handle,
				.offset = 0,
				.size = size
			},
			0, NULL
		);
		//ToDo: This fence is necessary because we want to destroy the command
		//buffers when we're done, but if we used the command buffers statically
		//we wouldn't need to destroy them and we wouldn't need to sync before
		//moving on because the memory barrier handles that for us.
		err = vkCreateFence(
			cpools.logicdev.handle,
			&(const VkFenceCreateInfo) {
				.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
				.pNext = NULL,
				.flags = 0
			},
			NULL,
			&fence
		);
		if(err != VK_SUCCESS) goto borked_gfxbuf;

		err = vkEndCommandBuffer(gfx_combuf.buf);
		if(err != VK_SUCCESS) goto borked_fence;
		err = vkQueueSubmit(
			cpools.logicdev.gfx_q,
			1,
			&(const VkSubmitInfo) {
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				.pNext = NULL,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &semaphore,
				.pWaitDstStageMask = &dst_access,
				.commandBufferCount = 1,
				.pCommandBuffers = &gfx_combuf.buf,
				.signalSemaphoreCount = 0,
				.pSignalSemaphores = NULL
			},
			fence
		);
		if(err != VK_SUCCESS) goto borked_fence;
		err = vkWaitForFences(gfx_combuf.dev, 1, &fence, VK_TRUE, UINT64_MAX);
		if(err != VK_SUCCESS) goto borked_fence;

		vkFreeCommandBuffers(xfr_combuf.dev, xfr_combuf.cpool, 1, &xfr_combuf.buf);
		vkFreeCommandBuffers(gfx_combuf.dev, gfx_combuf.cpool, 1, &gfx_combuf.buf);
		vkDestroyFence(cpools.logicdev.handle, fence, NULL);
		vkDestroySemaphore(cpools.logicdev.handle, semaphore, NULL);
	}

	vmaDestroyBuffer(allocator, host_buf.handle, host_buf.alloc);
	return VGFX_SUCCESS;
	borked_fence:
		vkDestroyFence(cpools.logicdev.handle, fence, NULL);
	borked_gfxbuf:
		vkFreeCommandBuffers(xfr_combuf.dev, xfr_combuf.cpool, 1, &xfr_combuf.buf);
	borked_semaphore:
		vkDestroySemaphore(cpools.logicdev.handle, semaphore, NULL);
	borked_xfrbuf:
		vkFreeCommandBuffers(xfr_combuf.dev, xfr_combuf.cpool, 1, &xfr_combuf.buf);
	borked_buffers:
		vmaDestroyBuffer(allocator, host_buf.handle, host_buf.alloc);
		vmaDestroyBuffer(allocator, local_buf->handle, local_buf->alloc);
	borked:
		if(err != VK_RESULT_MAX_ENUM)
			log_error("Failed to build and copy local buffer on: %s", vkr2str(err));
		return VGFX_FAIL;
}

//Lol this fucking thing
int init_vk_vertexbuffer(
	vk_vertexbuffer_t *vbuf,
	VmaAllocator allocator,
	vk_commandpools_t cpools
) {
	vbuf->allocator = allocator;
	if(build_and_copy_buf(
		&vbuf->buf,
		vertices,
		vertices_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
		allocator,
		cpools
	)) return VGFX_FAIL;
	return VGFX_SUCCESS;
}

void destroy_vk_vertexbuffer(vk_vertexbuffer_t vbuf) {
	vmaDestroyBuffer(vbuf.allocator, vbuf.buf.handle, vbuf.buf.alloc);
}

int init_vk_indexbuffer(
	vk_indexbuffer_t *ibuf,
	VmaAllocator allocator,
	vk_commandpools_t cpools
) {
	ibuf->allocator = allocator;
	if(build_and_copy_buf(
		&ibuf->buf,
		indices,
		indices_size,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
		VK_ACCESS_INDEX_READ_BIT,
		allocator,
		cpools
	)) return VGFX_FAIL;
	return VGFX_SUCCESS;
}

void destroy_vk_indexbuffer(vk_indexbuffer_t ibuf) {
	vmaDestroyBuffer(ibuf.allocator, ibuf.buf.handle, ibuf.buf.alloc);
}

//This should probably be split into two functions, one for pure instantiation
//and the other for all the draw stuff.
int init_vk_commandbuffers(
	vk_commandbuffers_t *commandbuffers,
	vk_swapchain_t swapchain,
	vk_renderpass_t renderpass,
	vk_gpipe_t gpipe,
	vk_framebuffers_t framebuffers,
	vk_commandpools_t commandpools,
	vk_vertexbuffer_t vbuf,
	vk_indexbuffer_t ibuf
) {
	commandbuffers->count = swapchain.image_count;
	commandbuffers->handles = malloc(
		commandbuffers->count * sizeof(*commandbuffers->handles)
	);
	if(!commandbuffers->handles) {
		log_error("Failed to allocate memory for commandbuffers");
		return VGFX_MEM_FAIL;
	}
	VkResult err = vkAllocateCommandBuffers(
		commandpools.logicdev.handle,
		&(const VkCommandBufferAllocateInfo) {
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = NULL,
			.commandPool = commandpools.gfx_handle,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = commandbuffers->count
		},
		commandbuffers->handles
	);
	if(err != VK_SUCCESS) goto borked;

	for(unsigned int i = 0; i < commandbuffers->count; i++) {
		err = vkBeginCommandBuffer(
			commandbuffers->handles[i],
			&(const VkCommandBufferBeginInfo) {
				.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
				.pNext = NULL,
				//This might break
				.flags = 0,
				.pInheritanceInfo = NULL
			}
		);
		if(err != VK_SUCCESS) goto borked;
		vkCmdBeginRenderPass(
			commandbuffers->handles[i],
			&(const VkRenderPassBeginInfo) {
				.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.pNext = NULL,
				.renderPass = renderpass.handle,
				.framebuffer = framebuffers.handles[i],
				.renderArea = (const VkRect2D) {
					.offset = {0, 0},
					.extent = swapchain.image_extent
				},
				.clearValueCount = 1,
				//VkClearValue is a union with another nested union, which functionally
				//works out to an array of four floats. But because GCC we need three
				//sets of braces to remove the compiler warning.
				//{outer union {inner union {float array}}}
				.pClearValues = &(const VkClearValue) {{{0.0f, 0.0f, 0.0f, 1.0f}}}
			},
			VK_SUBPASS_CONTENTS_INLINE
		);
		vkCmdBindPipeline(
			commandbuffers->handles[i],
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			gpipe.handle
		);
		vkCmdBindVertexBuffers(
			commandbuffers->handles[i],
			0, 1,
			&vbuf.buf.handle,
			&(const VkDeviceSize) {0}
		);
		vkCmdBindIndexBuffer(
			commandbuffers->handles[i],
			ibuf.buf.handle,
			0,
			VK_INDEX_TYPE_UINT16
		);
		vkCmdDrawIndexed(commandbuffers->handles[i], indices_len, 1, 0, 0, 0);
		vkCmdEndRenderPass(commandbuffers->handles[i]);
		err = vkEndCommandBuffer(commandbuffers->handles[i]);
		if(err != VK_SUCCESS) goto borked;
	}
	return VGFX_SUCCESS;
	borked:
		free(commandbuffers->handles);
		log_error("Failed to init commandbuffers on: %s", vkr2str(err));
		return VGFX_FAIL;
}

void destroy_vk_commandbuffers(vk_commandbuffers_t commandbuffers) {
	//Vulkan frees vkAllocate'd commandbuffers when the associated commandpool
	//is destroyed. All we have to clean up is the memory we're using to hold
	//the handles.
	free(commandbuffers.handles);
}

int init_vk_syncobjects(
	vk_syncobjects_t *syncobjects,
	uint32_t max_fif,
	vk_logicdev_t logicdev
) {
	*syncobjects = (const vk_syncobjects_t) {0};
	syncobjects->count = max_fif;
	syncobjects->logicdev = logicdev;

	syncobjects->image_available = malloc(
		syncobjects->count * sizeof(*syncobjects->image_available)
	);
	if(!syncobjects->image_available) goto borked_malloc;
	syncobjects->render_finished = malloc(
		syncobjects->count * sizeof(*syncobjects->render_finished)
	);
	if(!syncobjects->render_finished) goto borked_malloc;
	syncobjects->in_flight = malloc(
		syncobjects->count * sizeof(*syncobjects->in_flight)
	);
	if(!syncobjects->in_flight) goto borked_malloc;

	const VkSemaphoreCreateInfo sem_info = {
		.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		.pNext = NULL,
		.flags = 0
	};
	const VkFenceCreateInfo fence_info = {
		.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
		.pNext = NULL,
		.flags = VK_FENCE_CREATE_SIGNALED_BIT
	};

	unsigned int i = 0;
	VkResult err;
	for(; i < syncobjects->count; i++) {
		err = vkCreateSemaphore(
			logicdev.handle,
			&sem_info,
			NULL,
			&syncobjects->image_available[i]
		);
		if(err != VK_SUCCESS) goto borked;
		err = vkCreateSemaphore(
			logicdev.handle,
			&sem_info,
			NULL,
			&syncobjects->render_finished[i]
		);
		if(err != VK_SUCCESS) goto borked;
		err = vkCreateFence(
			logicdev.handle,
			&fence_info,
			NULL,
			&syncobjects->in_flight[i]
		);
		if(err != VK_SUCCESS) goto borked;
	}

	return VGFX_SUCCESS;
	borked_malloc:
		free(syncobjects->image_available);
		free(syncobjects->render_finished);
		free(syncobjects->in_flight);
		log_error("Failed to allocate memory for syncobjects");
		return VGFX_MEM_FAIL;
	borked:
		for(unsigned int j = 0; j < i; j++) {
			vkDestroySemaphore(
				syncobjects->logicdev.handle,
				syncobjects->image_available[j],
				NULL
			);
			vkDestroySemaphore(
				syncobjects->logicdev.handle,
				syncobjects->render_finished[j],
				NULL
			);
			vkDestroyFence(
				syncobjects->logicdev.handle,
				syncobjects->in_flight[j],
				NULL
			);
		}
		free(syncobjects->image_available);
		free(syncobjects->render_finished);
		free(syncobjects->in_flight);
		log_error("Failed to init syncobjects on: %s", vkr2str(err));
		return VGFX_FAIL;
}

void destroy_vk_syncobjects(vk_syncobjects_t syncobjects) {
	for(unsigned int i = 0; i < syncobjects.count; i++) {
		vkDestroySemaphore(
			syncobjects.logicdev.handle,
			syncobjects.image_available[i],
			NULL
		);
		vkDestroySemaphore(
			syncobjects.logicdev.handle,
			syncobjects.render_finished[i],
			NULL
		);
		vkDestroyFence(
			syncobjects.logicdev.handle,
			syncobjects.in_flight[i],
			NULL
		);
	}
}

int init_vk(vgfx_vk_t *vk, GLFWwindow *window, uint32_t max_fif) {
	vk->resized = 0;
	vk->max_fif = max_fif;
	if(init_vk_instance(&vk->instance, 1)) {
		log_error("Failed to init instance");
		return VGFX_FAIL;
	}
	if(init_vk_surface(&vk->surface, vk->instance, window)) {
		log_error("Failed to init surface");
		goto cleanup_instance;
	}
	if(init_vk_phydevs(&vk->phydevs, vk->instance)) {
		log_error("Failed to init physical devices");
		goto cleanup_surface;
	}
	if(init_vk_seldev(&vk->seldev, vk->phydevs, vk->surface)) {
		log_error("Failed to init seldev");
		goto cleanup_phydevs;
	}
	if(init_vk_logicdev(&vk->logicdev, vk->seldev)) {
		log_error("Failed to init logicdev");
		goto cleanup_seldev;
	}
	if(vmaCreateAllocator(
		&(const VmaAllocatorCreateInfo) {
			.flags = 0,
			.physicalDevice = vk->seldev.handle,
			.device = vk->logicdev.handle,
			.preferredLargeHeapBlockSize = 0,
			.pAllocationCallbacks = NULL,
			.pDeviceMemoryCallbacks = NULL,
			.frameInUseCount = 0,
			.pHeapSizeLimit = NULL,
			.pVulkanFunctions = NULL,
			.pRecordSettings = NULL,
		},
		&vk->allocator
	) != VK_SUCCESS) {
		log_error("Failed to init allocator");
		goto cleanup_logicdev;
	}
	if(init_vk_swapchain(
		&vk->swapchain,
		vk->surface,
		vk->seldev,
		vk->logicdev
	)) {
		log_error("Failed to init swapchain");
		goto cleanup_allocator;
	}
	if(init_vk_imageviews(&vk->imageviews, vk->swapchain)) {
		log_error("Failed to init imageviews");
		goto cleanup_swapchain;
	}
	if(init_vk_renderpass(&vk->renderpass, vk->swapchain)) {
		log_error("Failed to init renderpass");
		goto cleanup_imageviews;
	}
	if(init_vk_gpipe(&vk->gpipe, vk->swapchain, vk->renderpass)) {
		log_error("Failed to init graphics pipeline");
		goto cleanup_renderpass;
	}
	if(init_vk_framebuffers(
		&vk->framebuffers,
		vk->swapchain,
		vk->renderpass,
		vk->imageviews
	)) {
		log_error("Failed to init framebuffers");
		goto cleanup_gpipe;
	}
	if(init_vk_commandpools(&vk->commandpools, vk->seldev, vk->logicdev)) {
		log_error("Failed to init commandpool");
		goto cleanup_framebuffers;
	}
	if(init_vk_vertexbuffer(
		&vk->vertexbuffer,
		vk->allocator,
		vk->commandpools
	)) {
		log_error("Failed to init vertexbuffer");
		goto cleanup_commandpools;
	}
	if(init_vk_indexbuffer(
		&vk->indexbuffer,
		vk->allocator,
		vk->commandpools
	)) {
		log_error("Failed to init indexbuffer");
		goto cleanup_vertexbuffer;
	}
	if(init_vk_commandbuffers(
		&vk->commandbuffers,
		vk->swapchain,
		vk->renderpass,
		vk->gpipe,
		vk->framebuffers,
		vk->commandpools,
		vk->vertexbuffer,
		vk->indexbuffer
	)) {
		log_error("Failed to init commandbuffers");
		goto cleanup_indexbuffer;
	}
	if(init_vk_syncobjects(&vk->syncobjects, vk->max_fif, vk->logicdev)) {
		log_error("Failed to init syncobjects");
		goto cleanup_commandbuffers;
	}

	return VGFX_SUCCESS;
	cleanup_commandbuffers:
		destroy_vk_commandbuffers(vk->commandbuffers);
	cleanup_indexbuffer:
		destroy_vk_indexbuffer(vk->indexbuffer);
	cleanup_vertexbuffer:
		destroy_vk_vertexbuffer(vk->vertexbuffer);
	cleanup_commandpools:
		destroy_vk_commandpools(vk->commandpools);
	cleanup_framebuffers:
		destroy_vk_framebuffers(vk->framebuffers);
	cleanup_gpipe:
		destroy_vk_gpipe(vk->gpipe);
	cleanup_renderpass:
		destroy_vk_renderpass(vk->renderpass);
	cleanup_imageviews:
		destroy_vk_imageviews(vk->imageviews);
	cleanup_swapchain:
		destroy_vk_swapchain(vk->swapchain);
	cleanup_allocator:
		vmaDestroyAllocator(vk->allocator);
	cleanup_logicdev:
		destroy_vk_logicdev(vk->logicdev);
	cleanup_seldev:
		destroy_vk_seldev(vk->seldev);
	cleanup_phydevs:
		destroy_vk_phydevs(vk->phydevs);
	cleanup_surface:
		destroy_vk_surface(vk->surface, vk->instance);
	cleanup_instance:
		destroy_vk_instance(vk->instance);
		return VGFX_FAIL;
}

void destroy_vk(vgfx_vk_t vk) {
	destroy_vk_syncobjects(vk.syncobjects);
	destroy_vk_indexbuffer(vk.indexbuffer);
	destroy_vk_vertexbuffer(vk.vertexbuffer);
	destroy_vk_commandbuffers(vk.commandbuffers);
	destroy_vk_commandpools(vk.commandpools);
	destroy_vk_framebuffers(vk.framebuffers);
	destroy_vk_gpipe(vk.gpipe);
	destroy_vk_renderpass(vk.renderpass);
	destroy_vk_imageviews(vk.imageviews);
	destroy_vk_swapchain(vk.swapchain);
	vmaDestroyAllocator(vk.allocator);
	destroy_vk_logicdev(vk.logicdev);
	destroy_vk_seldev(vk.seldev);
	destroy_vk_phydevs(vk.phydevs);
	destroy_vk_surface(vk.surface, vk.instance);
	destroy_vk_instance(vk.instance);
}


//Generally for a window resize
//ToDo: We're free'ing and allocating a bunch of handles here by using the
//init/destroy functions. Probably should make those granular to allow for
//resources to be reused.
int rebuild_vk_swapchain(vgfx_vk_t *vk) {
	vk->resized = 0;
	vkDeviceWaitIdle(vk->logicdev.handle);
	//destroy_vk_commandbuffers assumes this free will be done when the command
	//pool is freed. We're not freeing the command pool, so we need to free the
	//buffers before freeing the handles.
	vkFreeCommandBuffers(
		vk->logicdev.handle,
		vk->commandpools.gfx_handle,
		vk->commandbuffers.count,
		vk->commandbuffers.handles
	);
	destroy_vk_commandbuffers(vk->commandbuffers);
	destroy_vk_framebuffers(vk->framebuffers);
	destroy_vk_imageviews(vk->imageviews);
	//const VkFormat old_format = vk->swapchain.image_format;
	destroy_vk_swapchain(vk->swapchain);

	//Refresh seldev caps before creating the swapchain
	if(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			vk->seldev.handle,
			vk->surface.handle,
			&vk->seldev.caps
	)) {
		destroy_vk_gpipe(vk->gpipe);
		destroy_vk_renderpass(vk->renderpass);
		goto borked;
	}
	if(init_vk_swapchain(
		&vk->swapchain,
		vk->surface,
		vk->seldev,
		vk->logicdev)
	) {
		destroy_vk_gpipe(vk->gpipe);
		destroy_vk_renderpass(vk->renderpass);
		goto borked;
	}
	//Renderpass and gpipe depend on image formats remaining constant
	//It also depends on image extent remaining constant, so for now leave
	//the if's commented out
	//if(vk->swapchain.image_format != old_format) {
		destroy_vk_gpipe(vk->gpipe);
		destroy_vk_renderpass(vk->renderpass);
		if(init_vk_renderpass(&vk->renderpass, vk->swapchain))
			goto cleanup_swapchain;
		if(init_vk_gpipe(&vk->gpipe, vk->swapchain, vk->renderpass))
			goto cleanup_renderpass;
	//}
	if(init_vk_imageviews(&vk->imageviews, vk->swapchain))
		goto cleanup_gpipe;
	if(init_vk_framebuffers(
		&vk->framebuffers,
		vk->swapchain,
		vk->renderpass,
		vk->imageviews
	)) goto cleanup_imageviews;
	if(init_vk_commandbuffers(
		&vk->commandbuffers,
		vk->swapchain,
		vk->renderpass,
		vk->gpipe,
		vk->framebuffers,
		vk->commandpools,
		vk->vertexbuffer,
		vk->indexbuffer
	)) goto cleanup_framebuffers;
	return VGFX_SUCCESS;

	cleanup_framebuffers:
		destroy_vk_framebuffers(vk->framebuffers);
	cleanup_imageviews:
		destroy_vk_imageviews(vk->imageviews);
	cleanup_gpipe:
		destroy_vk_gpipe(vk->gpipe);
	cleanup_renderpass:
		destroy_vk_renderpass(vk->renderpass);
	cleanup_swapchain:
		destroy_vk_swapchain(vk->swapchain);
	borked:
		log_error("Swapchain recreation failed");
		return VGFX_FAIL;
}
