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
  if(length < 0 || (length % 4)) goto borked;
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
        "VK_LAYER_LUNARG_standard_validation"
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
      layers.names = &(const char *) {"VK_LAYER_LUNARG_standard_validation"};
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
      log_error("No lunarg validation layer found");
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
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT     \
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT        \
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT     \
                         | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT             \
                     | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT          \
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

int init_vk_devices(vk_devices_t *devices, vk_instance_t instance) {
  *devices = (const vk_devices_t) { 0 };
  sds s = sdsnew("Discovered the following devices:");
  VkResult err = vkEnumeratePhysicalDevices(
    instance.handle,
    &devices->count,
    NULL
  );
  if(err != VK_SUCCESS) goto borked;
  if(!devices->count) {
    log_error("Vulkan reports no available physical devices");
    return VGFX_FAIL;
  }

  devices->handles = malloc(devices->count * sizeof(*devices->handles));
  if(!devices->handles) goto borked_malloc;
  devices->props = malloc(devices->count * sizeof(*devices->props));
  if(!devices->props) goto borked_malloc;
  devices->feats = malloc(devices->count * sizeof(*devices->feats));
  if(!devices->feats) goto borked_malloc;
  if((devices->qfams = malloc(devices->count * sizeof(*devices->qfams))))
    for(unsigned int i = 0; i < devices->count; i++)
      devices->qfams[i].props = NULL;
  else goto borked_malloc;
  if((devices->exts = malloc(devices->count * sizeof(*devices->exts))))
    for(unsigned int i = 0; i < devices->count; i++)
      devices->exts[i].props = NULL;
  else goto borked_malloc;

  err = vkEnumeratePhysicalDevices(
    instance.handle,
    &devices->count,
    devices->handles
  );
  if(err != VK_SUCCESS) goto borked;
  for(unsigned int i = 0; i < devices->count; i++) {
    vkGetPhysicalDeviceProperties(devices->handles[i], &devices->props[i]);
    vkGetPhysicalDeviceFeatures(devices->handles[i], &devices->feats[i]);
    vkGetPhysicalDeviceQueueFamilyProperties(
      devices->handles[i],
      &devices->qfams[i].count,
      NULL
    );
    devices->qfams[i].props = malloc(
      devices->qfams[i].count * sizeof(*devices->qfams[i].props)
    );
    if(!devices->qfams[i].props) goto borked_malloc;
    vkGetPhysicalDeviceQueueFamilyProperties(
      devices->handles[i],
      &devices->qfams[i].count,
      devices->qfams[i].props
    );
    s = sdscatfmt(s, "\n\t%s", devices->props[i].deviceName);
    err = vkEnumerateDeviceExtensionProperties(
      devices->handles[i],
      NULL,
      &devices->exts[i].count,
      NULL
    );
    if(err != VK_SUCCESS) goto borked;
    devices->exts[i].props = malloc(
      devices->exts[i].count * sizeof(*devices->exts[i].props)
    );
    if(!devices->exts[i].props) goto borked_malloc;
    err = vkEnumerateDeviceExtensionProperties(
      devices->handles[i],
      NULL,
      &devices->exts[i].count,
      devices->exts[i].props
    );
    if(err != VK_SUCCESS) goto borked;
  }
  log_debug(s);
  sdsfree(s);

  return VGFX_SUCCESS;
  borked_malloc:
    log_error("Failed to allocate memory for devices");
    destroy_vk_devices(*devices);
    sdsfree(s);
    return VGFX_MEM_FAIL;
  borked:
    log_error("Vulkan device enumeration failed on: %s", vkr2str(err));
    destroy_vk_devices(*devices);
    sdsfree(s);
    return VGFX_FAIL;
}

void destroy_vk_devices(vk_devices_t devices) {
  free(devices.handles);
  free(devices.props);
  free(devices.feats);
  for(unsigned int i = 0; i < devices.count; i++) {
    free(devices.qfams[i].props);
    free(devices.exts[i].props);
  }
  free(devices.qfams);
  free(devices.exts);
}

//Select a device and queue family for a given surface
int init_vk_seldev(
  vk_seldev_t *seldev,
  vk_devices_t devices,
  vk_surface_t surface
) {
  VkResult err;
  int success;
  *seldev = (const vk_seldev_t) {0};
  for(unsigned int i = 0, j; i < devices.count; i++) {
    for(j = 0, success = 0; j < devices.qfams[i].count; j++) {
      VkBool32 surface_support;
      err = vkGetPhysicalDeviceSurfaceSupportKHR(
        devices.handles[i],
        j,
        surface.handle,
        &surface_support
      );
      if(err != VK_SUCCESS) goto borked;
      if(
        devices.qfams[i].props[j].queueFlags & VK_QUEUE_GRAPHICS_BIT &&
        surface_support == VK_TRUE //Supposedly this implies swapchain support
      ) {
        seldev->qfam_idx = j;
        seldev->qfam_props = devices.qfams[i].props[j];
        success = 1;
        break;
      }
    }
    if(!success) continue;

    //Explicitly verify swapchain support
    for(j = 0, success = 0; j < devices.exts[i].count; j++) {
      if(!strcmp(
        devices.exts[i].props[j].extensionName,
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
      )) {
        success = 1;
        break;
      }
    }
    if(!success) continue;
    success = 0;

    err = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      devices.handles[i],
      surface.handle,
      &seldev->caps
    );
    if(err != VK_SUCCESS) goto borked;

    //Verify at least one surface format
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(
      devices.handles[i],
      surface.handle,
      &seldev->formats_count,
      NULL
    );
    if(err != VK_SUCCESS) goto borked;
    if(!seldev->formats_count) continue;
    seldev->formats = malloc(seldev->formats_count * sizeof(*seldev->formats));
    if(!seldev->formats) goto borked_malloc;
    err = vkGetPhysicalDeviceSurfaceFormatsKHR(
      devices.handles[i],
      surface.handle,
      &seldev->formats_count,
      seldev->formats
    );
    if(err != VK_SUCCESS) goto borked;

    //Verify at least one present mode
    err = vkGetPhysicalDeviceSurfacePresentModesKHR(
      devices.handles[i],
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
      devices.handles[i],
      surface.handle,
      &seldev->pmodes_count,
      seldev->pmodes
    );
    if(err != VK_SUCCESS) goto borked;

    seldev->handle = devices.handles[i];
    seldev->dev_props = devices.props[i];
    seldev->feats = devices.feats[i];
    success = 1;
    break;
  }
  if(success) log_debug(
    "Selected device: %s qfam: %d",
    seldev->dev_props.deviceName,
    seldev->qfam_idx
  );

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
  VkResult err = vkCreateDevice(
    seldev.handle,
    &(const VkDeviceCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &(const VkDeviceQueueCreateInfo) {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = seldev.qfam_idx,
        .queueCount = 1,
        .pQueuePriorities = &(const float) {1.0}
      },
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = NULL,
      .enabledExtensionCount = 1,
      .ppEnabledExtensionNames = &(const char *const) {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
      },
      .pEnabledFeatures = &seldev.feats
    },
    NULL,
    &logicdev->handle
  );
  if(err != VK_SUCCESS) goto borked;
  vkGetDeviceQueue(logicdev->handle, seldev.qfam_idx, 0, &logicdev->q);

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
  if(
    seldev.formats_count == 1 &&
    seldev.formats[0].format == VK_FORMAT_UNDEFINED
  ) {
    swapchain->image_format = VK_FORMAT_B8G8R8A8_UNORM;
    color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
  } else {
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

  VkPresentModeKHR pmode = VK_PRESENT_MODE_FIFO_KHR;
  for(unsigned int i = 0; i < seldev.pmodes_count; i++) {
    log_trace(
      "Discovered the following Present Mode: %s",
      pmode2str(seldev.pmodes[i])
    );
    if(seldev.pmodes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
      pmode = VK_PRESENT_MODE_MAILBOX_KHR;
      break;
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
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL
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

int init_vk_commandpool(
  vk_commandpool_t *commandpool,
  vk_seldev_t seldev,
  vk_logicdev_t logicdev
) {
  commandpool->logicdev = logicdev;
  VkResult err = vkCreateCommandPool(
    logicdev.handle,
    &(const VkCommandPoolCreateInfo) {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .queueFamilyIndex = seldev.qfam_idx
    },
    NULL,
    &commandpool->handle
  );
  if(err != VK_SUCCESS) {
    log_error("Failed to init commandpool on: %s", vkr2str(err));
    return VGFX_FAIL;
  }
  return VGFX_SUCCESS;
}

void destroy_vk_commandpool(vk_commandpool_t commandpool) {
  vkDestroyCommandPool(commandpool.logicdev.handle, commandpool.handle, NULL);
}

//This should probably be split into two functions, one for pure instantiation
//and the other for all the draw stuff.
int init_vk_commandbuffers(
  vk_commandbuffers_t *commandbuffers,
  vk_swapchain_t swapchain,
  vk_renderpass_t renderpass,
  vk_gpipe_t gpipe,
  vk_framebuffers_t framebuffers,
  vk_commandpool_t commandpool
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
    commandpool.logicdev.handle,
    &(const VkCommandBufferAllocateInfo) {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = NULL,
      .commandPool = commandpool.handle,
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
    vkCmdDraw(commandbuffers->handles[i], 3, 1, 0, 0);
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
  if(init_vk_devices(&vk->devices, vk->instance)) {
    log_error("Failed to init devices");
    goto cleanup_surface;
  }
  if(init_vk_seldev(&vk->seldev, vk->devices, vk->surface)) {
    log_error("Failed to init seldev");
    goto cleanup_devices;
  }
  if(init_vk_logicdev(&vk->logicdev, vk->seldev)) {
    log_error("Failed to init logicdev");
    goto cleanup_seldev;
  }
  if(init_vk_swapchain(
    &vk->swapchain,
    vk->surface,
    vk->seldev,
    vk->logicdev
  )) {
    log_error("Failed to init swapchain");
    goto cleanup_logicdev;
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
  if(init_vk_commandpool(&vk->commandpool, vk->seldev, vk->logicdev)) {
    log_error("Failed to init commandpool");
    goto cleanup_framebuffers;
  }
  if(init_vk_commandbuffers(
    &vk->commandbuffers,
    vk->swapchain,
    vk->renderpass,
    vk->gpipe,
    vk->framebuffers,
    vk->commandpool
  )) {
    log_error("Failed to init commandbuffers");
    goto cleanup_commandpool;
  }
  if(init_vk_syncobjects(&vk->syncobjects, vk->max_fif, vk->logicdev)) {
    log_error("Failed to init syncobjects");
    goto cleanup_commandbuffers;
  }

  return VGFX_SUCCESS;
  cleanup_commandbuffers:
    destroy_vk_commandbuffers(vk->commandbuffers);
  cleanup_commandpool:
    destroy_vk_commandpool(vk->commandpool);
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
  cleanup_logicdev:
    destroy_vk_logicdev(vk->logicdev);
  cleanup_seldev:
    destroy_vk_seldev(vk->seldev);
  cleanup_devices:
    destroy_vk_devices(vk->devices);
  cleanup_surface:
    destroy_vk_surface(vk->surface, vk->instance);
  cleanup_instance:
    destroy_vk_instance(vk->instance);
    return VGFX_FAIL;
}

void destroy_vk(vgfx_vk_t vk) {
  destroy_vk_syncobjects(vk.syncobjects);
  destroy_vk_commandbuffers(vk.commandbuffers);
  destroy_vk_commandpool(vk.commandpool);
  destroy_vk_framebuffers(vk.framebuffers);
  destroy_vk_gpipe(vk.gpipe);
  destroy_vk_renderpass(vk.renderpass);
  destroy_vk_imageviews(vk.imageviews);
  destroy_vk_swapchain(vk.swapchain);
  destroy_vk_logicdev(vk.logicdev);
  destroy_vk_seldev(vk.seldev);
  destroy_vk_devices(vk.devices);
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
    vk->commandpool.handle,
    vk->commandbuffers.count,
    vk->commandbuffers.handles
  );
  destroy_vk_commandbuffers(vk->commandbuffers);
  destroy_vk_framebuffers(vk->framebuffers);
  destroy_vk_imageviews(vk->imageviews);
  const VkFormat old_format = vk->swapchain.image_format;
  destroy_vk_swapchain(vk->swapchain);

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
  if(vk->swapchain.image_format != old_format) {
    destroy_vk_gpipe(vk->gpipe);
    destroy_vk_renderpass(vk->renderpass);
    if(init_vk_renderpass(&vk->renderpass, vk->swapchain))
      goto cleanup_swapchain;
    if(init_vk_gpipe(&vk->gpipe, vk->swapchain, vk->renderpass))
      goto cleanup_renderpass;
  }
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
    vk->commandpool
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
