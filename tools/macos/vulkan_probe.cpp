// Exercise the real macOS Vulkan presentation path without any game files.
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void check(VkResult r, const char *where)
{
    if (r != VK_SUCCESS) { fprintf(stderr, "%s: Vulkan error %d\n", where, r); std::exit(1); }
}
int main()
{
    SDL_SetMainReady();
    if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    if (!SDL_Vulkan_LoadLibrary(HP2_MOLTENVK_LIBRARY)) { fprintf(stderr, "Vulkan: %s\n", SDL_GetError()); return 1; }
    SDL_Window *window = SDL_CreateWindow("NFSHP2 macOS graphics test - no game data", 800, 600, SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!window) { fprintf(stderr, "Window: %s\n", SDL_GetError()); return 1; }
    Uint32 count;
    const char *const *names = SDL_Vulkan_GetInstanceExtensions(&count);
    if (!names) return 1;
    std::vector<const char *> extensions(names, names + count);
    uint32_t n = 0;
    check(vkEnumerateInstanceExtensionProperties(nullptr, &n, nullptr), "instance extensions");
    std::vector<VkExtensionProperties> avail(n);
    check(vkEnumerateInstanceExtensionProperties(nullptr, &n, avail.data()), "instance extensions");
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "HP2 macOS Vulkan probe"; app.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO}; ci.pApplicationInfo = &app;
    for (auto &e : avail) if (!strcmp(e.extensionName, "VK_KHR_portability_enumeration")) {
        extensions.push_back("VK_KHR_portability_enumeration"); ci.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
    ci.enabledExtensionCount = (uint32_t)extensions.size(); ci.ppEnabledExtensionNames = extensions.data();
    VkInstance instance;
    check(vkCreateInstance(&ci, nullptr, &instance), "create instance");
    VkSurfaceKHR surface;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) { fprintf(stderr, "%s\n", SDL_GetError()); return 1; }
    check(vkEnumeratePhysicalDevices(instance, &n, nullptr), "list GPUs");
    std::vector<VkPhysicalDevice> devices(n);
    check(vkEnumeratePhysicalDevices(instance, &n, devices.data()), "list GPUs");
    VkPhysicalDevice phys = VK_NULL_HANDLE; uint32_t family = 0;
    for (int discrete = 1; discrete >= 0 && !phys; --discrete) for (auto d : devices) {
        VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(d, &props);
        if ((props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) != discrete) continue;
        vkGetPhysicalDeviceQueueFamilyProperties(d, &n, nullptr);
        std::vector<VkQueueFamilyProperties> qs(n); vkGetPhysicalDeviceQueueFamilyProperties(d, &n, qs.data());
        for (uint32_t i = 0; i < n; ++i) {
            VkBool32 present = false; check(vkGetPhysicalDeviceSurfaceSupportKHR(d, i, surface, &present), "surface support");
            if (present && (qs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) { phys = d; family = i; break; }
        }
        if (phys) break;
    }
    if (!phys) { fprintf(stderr, "No GPU can present to this window.\n"); return 1; }
    VkPhysicalDeviceProperties props; vkGetPhysicalDeviceProperties(phys, &props);
    VkPhysicalDeviceFeatures features; vkGetPhysicalDeviceFeatures(phys, &features);
    fprintf(stderr, "GPU: %s; Vulkan %u.%u; depth clamp %u, clip distance %u, anisotropy %u\n", props.deviceName,
        VK_VERSION_MAJOR(props.apiVersion), VK_VERSION_MINOR(props.apiVersion), features.depthClamp, features.shaderClipDistance, features.samplerAnisotropy);
    for (auto format : {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}) {
        VkFormatProperties f; vkGetPhysicalDeviceFormatProperties(phys, format, &f);
        fprintf(stderr, "depth format %d: attachment %s\n", format, f.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT ? "yes" : "no");
    }
    std::vector<const char *> dext{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    check(vkEnumerateDeviceExtensionProperties(phys, nullptr, &n, nullptr), "device extensions");
    avail.resize(n); check(vkEnumerateDeviceExtensionProperties(phys, nullptr, &n, avail.data()), "device extensions");
    for (auto &e : avail) if (!strcmp(e.extensionName, "VK_KHR_portability_subset")) dext.push_back("VK_KHR_portability_subset");
    float priority = 1;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO}; qi.queueFamilyIndex=family; qi.queueCount=1; qi.pQueuePriorities=&priority;
    VkDeviceCreateInfo di{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO}; di.queueCreateInfoCount=1; di.pQueueCreateInfos=&qi;
    di.enabledExtensionCount=(uint32_t)dext.size(); di.ppEnabledExtensionNames=dext.data();
    VkDevice device; check(vkCreateDevice(phys, &di, nullptr, &device), "create device");
    VkQueue queue; vkGetDeviceQueue(device,family,0,&queue);
    VkSurfaceCapabilitiesKHR caps; check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys,surface,&caps),"surface capabilities");
    check(vkGetPhysicalDeviceSurfaceFormatsKHR(phys,surface,&n,nullptr),"surface formats");
    std::vector<VkSurfaceFormatKHR> formats(n); check(vkGetPhysicalDeviceSurfaceFormatsKHR(phys,surface,&n,formats.data()),"surface formats");
    if (formats.empty() || !(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) return 1;
    auto format = formats[0];
    for (auto f : formats) if (f.format==VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) format=f;
    VkSwapchainCreateInfoKHR si{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR}; si.surface=surface;
    si.minImageCount=caps.minImageCount+1;
    if(caps.maxImageCount) si.minImageCount=std::min(si.minImageCount,caps.maxImageCount);
    si.imageFormat=format.format; si.imageColorSpace=format.colorSpace; si.imageExtent=caps.currentExtent;
    if(si.imageExtent.width==UINT32_MAX) { int w,h; SDL_GetWindowSizeInPixels(window,&w,&h); si.imageExtent={(uint32_t)w,(uint32_t)h}; }
    si.imageArrayLayers=1; si.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT; si.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;
    si.preTransform=caps.currentTransform; si.compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    si.presentMode=VK_PRESENT_MODE_FIFO_KHR; si.clipped=VK_TRUE;
    VkSwapchainKHR swap; check(vkCreateSwapchainKHR(device,&si,nullptr,&swap),"create swapchain");
    check(vkGetSwapchainImagesKHR(device,swap,&n,nullptr),"swapchain images");
    std::vector<VkImage> images(n); check(vkGetSwapchainImagesKHR(device,swap,&n,images.data()),"swapchain images");
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO}; pi.queueFamilyIndex=family; pi.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VkCommandPool pool; check(vkCreateCommandPool(device,&pi,nullptr,&pool),"command pool");
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; ai.commandPool=pool; ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount=1;
    VkCommandBuffer cmd; check(vkAllocateCommandBuffers(device,&ai,&cmd),"command buffer");
    VkSemaphoreCreateInfo sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; VkSemaphore acquired,done;
    check(vkCreateSemaphore(device,&sci,nullptr,&acquired),"acquire semaphore"); check(vkCreateSemaphore(device,&sci,nullptr,&done),"render semaphore");
    const auto start=SDL_GetTicks(); unsigned frames=0; bool quit=false;
    while(!quit && SDL_GetTicks()-start<3000) {
        SDL_Event e; while(SDL_PollEvent(&e)) if(e.type==SDL_EVENT_QUIT || e.type==SDL_EVENT_WINDOW_CLOSE_REQUESTED) quit=true;
        uint32_t idx; check(vkAcquireNextImageKHR(device,swap,UINT64_MAX,acquired,VK_NULL_HANDLE,&idx),"acquire");
        check(vkResetCommandBuffer(cmd,0),"reset commands");
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; check(vkBeginCommandBuffer(cmd,&bi),"begin commands");
        VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER}; b.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED; b.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED; b.image=images[idx]; b.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&b);
        VkClearColorValue color{{0.02f,0.12f+(frames%60)/300.0f,0.3f,1.0f}};
        vkCmdClearColorImage(cmd,images[idx],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&color,1,&b.subresourceRange);
        b.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; b.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT; b.dstAccessMask=0;
        vkCmdPipelineBarrier(cmd,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,nullptr,0,nullptr,1,&b);
        check(vkEndCommandBuffer(cmd),"end commands");
        VkPipelineStageFlags wait=VK_PIPELINE_STAGE_TRANSFER_BIT;
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO}; submit.waitSemaphoreCount=1; submit.pWaitSemaphores=&acquired; submit.pWaitDstStageMask=&wait;
        submit.commandBufferCount=1; submit.pCommandBuffers=&cmd; submit.signalSemaphoreCount=1; submit.pSignalSemaphores=&done;
        check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"submit");
        VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR}; present.waitSemaphoreCount=1; present.pWaitSemaphores=&done; present.swapchainCount=1; present.pSwapchains=&swap; present.pImageIndices=&idx;
        check(vkQueuePresentKHR(queue,&present),"present"); check(vkQueueWaitIdle(queue),"wait idle"); ++frames;
    }
    fprintf(stderr,"PASS: %u frames presented through MoltenVK at %u x %u.\n",frames,si.imageExtent.width,si.imageExtent.height);
    vkDestroySemaphore(device,done,nullptr); vkDestroySemaphore(device,acquired,nullptr); vkDestroyCommandPool(device,pool,nullptr);
    vkDestroySwapchainKHR(device,swap,nullptr); vkDestroyDevice(device,nullptr); vkDestroySurfaceKHR(instance,surface,nullptr); vkDestroyInstance(instance,nullptr);
    SDL_DestroyWindow(window); SDL_Quit(); return frames ? 0 : 1;
}
