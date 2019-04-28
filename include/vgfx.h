#pragma once
#include <stdint.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

enum vgfx_errors {
  VGFX_SUCCESS = 0,
  VGFX_FAIL,
  VGFX_MEM_FAIL,
  VGFX_NO_LUNARG,
};

char *vkr2str(VkResult err);

typedef struct {
  VkInstance handle;
  VkDebugUtilsMessengerEXT debug_cb;
} vk_instance_t;

int init_vk_instance(vk_instance_t *instance, int use_validation);
void destroy_vk_instance(vk_instance_t instance);

typedef struct {
  VkSurfaceKHR handle;
  GLFWwindow *window;
} vk_surface_t;

int init_vk_surface(
  vk_surface_t *surface,
  vk_instance_t instance,
  GLFWwindow *window
);
void destroy_vk_surface(vk_surface_t surface, vk_instance_t instance);

typedef struct {
  uint32_t count;
  VkQueueFamilyProperties *props;
} vk_qfamilies_t;

typedef struct {
  uint32_t count;
  VkExtensionProperties *props;
} vk_device_exts_t;

typedef struct {
  uint32_t count;
  VkPhysicalDevice *handles;
  VkPhysicalDeviceProperties *props;
  VkPhysicalDeviceFeatures *feats;
  vk_qfamilies_t *qfams;
  vk_device_exts_t *exts;
} vk_devices_t;

int init_vk_devices(vk_devices_t *devices, vk_instance_t instance);
void destroy_vk_devices(vk_devices_t devices);

typedef struct {
  VkPhysicalDevice handle;
  VkPhysicalDeviceProperties dev_props;
  VkPhysicalDeviceFeatures feats;
  VkSurfaceCapabilitiesKHR caps;
  uint32_t qfam_idx;
  VkQueueFamilyProperties qfam_props;
  uint32_t formats_count;
  VkSurfaceFormatKHR *formats;
  uint32_t pmodes_count;
  VkPresentModeKHR *pmodes;
} vk_seldev_t;

int init_vk_seldev(
  vk_seldev_t *seldev,
  vk_devices_t devices,
  vk_surface_t surface
);
void destroy_vk_seldev(vk_seldev_t seldev);

typedef struct {
  VkDevice handle;
  VkQueue q;
} vk_logicdev_t;

int init_vk_logicdev(vk_logicdev_t *logicdev, vk_seldev_t seldev);
void destroy_vk_logicdev(vk_logicdev_t logicdev);

typedef struct {
  VkSwapchainKHR handle;
  VkFormat image_format;
  VkExtent2D image_extent;
  uint32_t image_count;
  VkImage *image_handles;
  vk_logicdev_t logicdev;
} vk_swapchain_t;

int init_vk_swapchain(
  vk_swapchain_t *swapchain,
  vk_surface_t surface,
  vk_seldev_t seldev,
  vk_logicdev_t logicdev
);
void destroy_vk_swapchain(vk_swapchain_t swapchain);
int recycle_vk_swapchain(
  vk_swapchain_t *swapchain,
  vk_surface_t surface,
  vk_seldev_t seldev
);

typedef struct {
  uint32_t count;
  VkImageView *handles;
  vk_logicdev_t logicdev;
} vk_imageviews_t;

int init_vk_imageviews(
  vk_imageviews_t *imageviews,
  vk_swapchain_t swapchain
);
void destroy_vk_imageviews(vk_imageviews_t imageviews);

typedef struct {
  uint32_t *base;
  size_t len;
} vk_spirvbuf_t;

int read_spirv(vk_spirvbuf_t *buf, const char *path);
void destroy_spirv(vk_spirvbuf_t buf);

typedef struct {
  VkRenderPass handle;
  vk_logicdev_t logicdev;
} vk_renderpass_t;

int init_vk_renderpass(vk_renderpass_t *rendepass, vk_swapchain_t swapchain);
void destroy_vk_renderpass(vk_renderpass_t renderpass);

typedef struct {
  VkPipeline handle;
  VkPipelineLayout layout_handle;
  vk_logicdev_t logicdev;
} vk_gpipe_t;

int init_vk_gpipe(
  vk_gpipe_t *gpipe,
  vk_swapchain_t swapchain,
  vk_renderpass_t renderpass
);
void destroy_vk_gpipe(vk_gpipe_t gpipe);

typedef struct {
  uint32_t count;
  VkFramebuffer *handles;
  vk_logicdev_t logicdev;
} vk_framebuffers_t;

int init_vk_framebuffers(
  vk_framebuffers_t *framebuffers,
  vk_swapchain_t swapchain,
  vk_renderpass_t renderpass,
  vk_imageviews_t imageviews
);
void destroy_vk_framebuffers(vk_framebuffers_t framebuffers);

typedef struct {
  VkCommandPool handle;
  vk_logicdev_t logicdev;
} vk_commandpool_t;

int init_vk_commandpool(
  vk_commandpool_t *commandpool,
  vk_seldev_t seldev,
  vk_logicdev_t logicdev
);
void destroy_vk_commandpool(vk_commandpool_t commandpool);

typedef struct {
  uint32_t count;
  VkCommandBuffer *handles;
} vk_commandbuffers_t;

int init_vk_commandbuffers(
  vk_commandbuffers_t *commandbuffers,
  vk_swapchain_t swapchain,
  vk_renderpass_t renderpass,
  vk_gpipe_t gpipe,
  vk_framebuffers_t framebuffers,
  vk_commandpool_t commandpool
);
void destroy_vk_commandbuffers(vk_commandbuffers_t commandbuffers);

typedef struct {
  uint32_t count;
  VkSemaphore *image_available;
  VkSemaphore *render_finished;
  VkFence *in_flight;
  vk_logicdev_t logicdev;
} vk_syncobjects_t;

int init_vk_syncobjects(
  vk_syncobjects_t *syncobjects,
  uint32_t max_fif,
  vk_logicdev_t logicdev
);
void destroy_vk_syncobjects(vk_syncobjects_t syncobjects);

typedef struct {
  unsigned int resized;
  uint32_t max_fif;
  vk_instance_t instance;
  vk_surface_t surface;
  vk_devices_t devices;
  vk_seldev_t seldev;
  vk_logicdev_t logicdev;
  vk_swapchain_t swapchain;
  vk_imageviews_t imageviews;
  vk_renderpass_t renderpass;
  vk_gpipe_t gpipe;
  vk_framebuffers_t framebuffers;
  vk_commandpool_t commandpool;
  vk_commandbuffers_t commandbuffers;
  vk_syncobjects_t syncobjects;
} vgfx_vk_t;

int init_vk(vgfx_vk_t *vk, GLFWwindow *window, uint32_t max_fif);
void destroy_vk(vgfx_vk_t vk);

int rebuild_vk_swapchain(vgfx_vk_t *vk);
