#include "Application.h"

#include <cassert>
#include <string>
#include <cstring>
#include <algorithm>
#include <vector>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_vulkan.h>

#include <vulkan/vk_enum_string_helper.h>

#define VK_CHECK(result) do { if(result != VK_SUCCESS) { printf("VkResult: %s (line: %d, file: %s\n", string_VkResult(result), __LINE__, __FILE__); assert(false); } } while(0);

Application::Application() : m_Width{800}, m_Height{600}
{
    // Window
    {
        assert(glfwInit() && "Failed to initialize glfw!");
        glfwWindowHint(GLFW_VISIBLE, false);
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        
        m_Window = glfwCreateWindow(m_Width, m_Height, "DeferredTest", nullptr, nullptr);
        assert(m_Window && "Failed to create the window!");
    }
    // Instance
    {
        u32 extCount = 0;
        const char** exts = glfwGetRequiredInstanceExtensions(&extCount);

        VkApplicationInfo appInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "Application",
            .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
            .pEngineName = "Application",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_0
        };

        VkInstanceCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &appInfo,
            .enabledExtensionCount = extCount,
            .ppEnabledExtensionNames = exts
        };

        VK_CHECK(vkCreateInstance(&info, nullptr, &m_Instance));
    }
    // Surface
    VK_CHECK(glfwCreateWindowSurface(m_Instance, m_Window, nullptr, &m_Surface));
    // Physical device
    {
        u32 count = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &count, nullptr));
        std::vector<VkPhysicalDevice> devices(count);
        VK_CHECK(vkEnumeratePhysicalDevices(m_Instance, &count, devices.data()));

        for(auto& device : devices)
        {
            u32 queueCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
            std::vector<VkQueueFamilyProperties> queueProps(queueCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, queueProps.data());
            i32 gIdx = -1, pIdx = -1, cIdx = -1;
            for(u32 i = 0; i < queueCount; i++)
            {
                if(queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    gIdx = i;
                if(queueProps[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
                    cIdx = i;
                VkBool32 present = false;
                VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &present));
                if(present)
                    pIdx = i;
                
                if(present && (gIdx != -1) && (pIdx != -1) && (cIdx != -1)) {
                    m_PhysicalDevice = device;
                    m_GraphicsQueueIdx = gIdx;
                    m_PresentQueueIdx = pIdx;
                    m_ComputeQueueIdx = cIdx;
                    break;
                }
            }
        }

        assert((m_PhysicalDevice != nullptr) && "Failed to find a suitable physical device!");

        vkGetPhysicalDeviceFeatures(m_PhysicalDevice, &m_PhysicalDeviceFeatures);
    }
    // Device
    {
        bool extsSupported = false;
        std::vector<const char*> exts = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
        // Checking if extensions supported
        {
            u32 count = 0;
            VK_CHECK(vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &count, nullptr));
            std::vector<VkExtensionProperties> props(count);
            VK_CHECK(vkEnumerateDeviceExtensionProperties(m_PhysicalDevice, nullptr, &count, props.data()));

            for(auto ext : exts)
            {
                for(auto& prop : props)
                {
                    if(strcmp(prop.extensionName, ext) == 0)
                        extsSupported = true;
                }
            }
        }
        assert(extsSupported && "The device extensions aren't supported!");

        float priority = 1.0f;
        i32 indices[3] = {
            m_GraphicsQueueIdx,
            m_PresentQueueIdx,
            m_ComputeQueueIdx
        };

        for(i32 i = 0; i < 3; i++) 
        {
            bool exists = false;
            for(i32 idx : m_UniqueQueues) 
            {
                if(idx == indices[i])
                    exists = true;
            }
            if(!exists)
                m_UniqueQueues.push_back(indices[i]);
        }

        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        for(i32 idx : m_UniqueQueues)
        {
            VkDeviceQueueCreateInfo info = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = (u32)idx,
                .queueCount = 1,
                .pQueuePriorities = &priority
            };
            queueInfos.push_back(info);
        }

        VkDeviceCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = (u32)queueInfos.size(),
            .pQueueCreateInfos = queueInfos.data(),
            .enabledExtensionCount = (u32)exts.size(),
            .ppEnabledExtensionNames = exts.data(),
            .pEnabledFeatures = &m_PhysicalDeviceFeatures
        };

        VK_CHECK(vkCreateDevice(m_PhysicalDevice, &info, nullptr, &m_Device));

        vkGetDeviceQueue(m_Device, m_GraphicsQueueIdx, 0, &m_GraphicsQueue);
        vkGetDeviceQueue(m_Device, m_PresentQueueIdx, 0, &m_PresentQueue);
        vkGetDeviceQueue(m_Device, m_ComputeQueueIdx, 0, &m_ComputeQueue);
    }
    // Depth image
    {
        ImageInfo info{};
        info.width = m_Width;
        info.height = m_Height;
        info.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
        info.gpuResource = false;
        info.memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        info.format = VK_FORMAT_UNDEFINED;

        // Getting format
        {
            VkFormat reqFormats[] = {
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D24_UNORM_S8_UINT
            };

            u32 flags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;

            for(u32 i = 0; i < 3; i++) {
                VkFormatProperties props{};
                vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, reqFormats[i], &props);

                if((props.linearTilingFeatures & flags) == flags) {
                    info.format = reqFormats[i];
                    break;
                }
                if((props.optimalTilingFeatures & flags) == flags) {
                    info.format = reqFormats[i];
                    break;
                }
            }

            if(info.format == VK_FORMAT_UNDEFINED)
                assert(false && "Failed to find suitable depth format!");
        }

        CreateImage(m_DepthImage, info);
    }
    // Renderpass
    {
        m_ScCaps = GetScCaps();
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_ScCaps.format.format;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = m_DepthImage.info.format;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;

        constexpr u32 attachmentCount = 2;
        VkAttachmentDescription attachments[attachmentCount] = {
            colorAttachment,
            depthAttachment
        };

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthRef{};
        depthRef.attachment = 1;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dependency.dependencyFlags = 0;

        VkRenderPassCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        info.attachmentCount = attachmentCount;
        info.pAttachments = attachments;
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        info.dependencyCount = 1;
        info.pDependencies = &dependency;

        VK_CHECK(vkCreateRenderPass(m_Device, &info, nullptr, &m_Pass));
    }
    // Swapchain
    CreateSwapchain();
    // Command pool and buffers
    {
        {
            VkCommandPoolCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            info.queueFamilyIndex = m_GraphicsQueueIdx;
            info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            
            VK_CHECK(vkCreateCommandPool(m_Device, &info, nullptr, &m_CmdPool));
        }
        for(u32 i = 0; i < FRAMES_IN_FLIGHT; i++)
            m_CmdBuffs[i] = CreateCommandBuffer();
    }
    // Sync objs
    {
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        VkSemaphoreCreateInfo semaInfo{};
        semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        for(u32 i = 0; i < FRAMES_IN_FLIGHT; i++)
        {
            VK_CHECK(vkCreateFence(m_Device, &fenceInfo, nullptr, &m_InFlightFences[i]));
            VK_CHECK(vkCreateSemaphore(m_Device, &semaInfo, nullptr, &m_ImageAvailable[i]));
        }
        
        m_RenderFinished.resize(m_ScImages.size());
        for(auto& sema : m_RenderFinished)
        {
            VK_CHECK(vkCreateSemaphore(m_Device, &semaInfo, nullptr, &sema));
        }
    }
    // UI descriptor pool
    {
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE },
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 0;
        for (VkDescriptorPoolSize& pool_size : pool_sizes)
            pool_info.maxSets += pool_size.descriptorCount;
        pool_info.poolSizeCount = 1;
        pool_info.pPoolSizes = pool_sizes;
        VK_CHECK(vkCreateDescriptorPool(m_Device, &pool_info, nullptr, &m_UiDescPool));
    }
    // ImGui
    {
        IMGUI_CHECKVERSION();
		ImGui::CreateContext();

        ImGui::StyleColorsDark();

        ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowPadding = ImVec2(0, 0);

        io.Fonts->AddFontFromFileTTF("assets/fonts/consolas.ttf", 18.0f, nullptr, nullptr);

        ImGui_ImplGlfw_InitForVulkan(m_Window, true);
        
        ImGui_ImplVulkan_InitInfo info{};
        info.Allocator = nullptr;
        info.ApiVersion = VK_API_VERSION_1_0;
        info.DescriptorPool = m_UiDescPool;
        info.Device = m_Device;
        info.ImageCount = FRAMES_IN_FLIGHT;
        info.MinImageCount = FRAMES_IN_FLIGHT;
        info.Instance = m_Instance;
        info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        info.PhysicalDevice = m_PhysicalDevice;
        info.Queue = m_GraphicsQueue;
        info.QueueFamily = m_GraphicsQueueIdx;
        info.RenderPass = m_Pass;
        info.Subpass = 0;
        
        ImGui_ImplVulkan_Init(&info);
        ImGui_ImplVulkan_CreateFontsTexture();

        // ImGui styling
        {
            ImGuiIO* io = &ImGui::GetIO();

            ImGuiStyle* style = &ImGui::GetStyle();
            ImVec4* colors = style->Colors;

            style->WindowPadding = (ImVec2){0.0f, 0.0f};
            style->FramePadding = (ImVec2){8.0f, 5.0f};
            style->ItemSpacing = (ImVec2){10.0f, 6.0f};
            style->ItemInnerSpacing = (ImVec2){9.0f, 5.0f};
            style->ScrollbarSize = 14.0f;
            style->GrabMinSize = 10.0f;
            style->WindowBorderSize = 1.0f;
            style->FrameBorderSize = 1.0f;
            style->TabBorderSize = 0.0f;
            style->WindowRounding = 7.0f;
            style->ChildRounding = 7.0f;
            style->FrameRounding = 6.0f;
            style->PopupRounding = 6.0f;
            style->ScrollbarRounding = 7.0f;
            style->GrabRounding = 5.0f;
            style->TabRounding = 6.0f;

            colors[ImGuiCol_Text] =                     (ImVec4){0.863f, 0.933f, 0.961f, 1.000f};
            colors[ImGuiCol_TextDisabled] =             (ImVec4){0.478f, 0.576f, 0.631f, 1.000f};
            colors[ImGuiCol_WindowBg] =                 (ImVec4){0.024f, 0.063f, 0.102f, 1.000f};
            colors[ImGuiCol_ChildBg] =                  (ImVec4){0.043f, 0.102f, 0.157f, 0.900f};
            colors[ImGuiCol_PopupBg] =                  (ImVec4){0.024f, 0.063f, 0.102f, 0.980f};
            colors[ImGuiCol_Border] =                   (ImVec4){0.090f, 0.184f, 0.259f, 1.000f};
            colors[ImGuiCol_FrameBg] =                  (ImVec4){0.043f, 0.102f, 0.157f, 1.000f};
            colors[ImGuiCol_FrameBgHovered] =           (ImVec4){0.075f, 0.647f, 0.784f, 0.220f};
            colors[ImGuiCol_FrameBgActive] =            (ImVec4){0.075f, 0.647f, 0.784f, 0.350f};
            colors[ImGuiCol_TitleBg] =                  (ImVec4){0.043f, 0.102f, 0.157f, 1.000f};
            colors[ImGuiCol_TitleBgActive] =            (ImVec4){0.090f, 0.184f, 0.259f, 1.000f};
            colors[ImGuiCol_MenuBarBg] =                (ImVec4){0.043f, 0.102f, 0.157f, 1.000f};
            colors[ImGuiCol_ScrollbarBg] =              (ImVec4){0.024f, 0.063f, 0.102f, 0.800f};
            colors[ImGuiCol_ScrollbarGrab] =            (ImVec4){0.090f, 0.184f, 0.259f, 0.900f};
            colors[ImGuiCol_ScrollbarGrabHovered] =     (ImVec4){0.075f, 0.647f, 0.784f, 0.650f};
            colors[ImGuiCol_ScrollbarGrabActive] =      (ImVec4){0.075f, 0.647f, 0.784f, 0.850f};
            colors[ImGuiCol_CheckMark] =                (ImVec4){0.075f, 0.647f, 0.784f, 1.000f};
            colors[ImGuiCol_SliderGrab] =               (ImVec4){0.075f, 0.647f, 0.784f, 0.850f};
            colors[ImGuiCol_SliderGrabActive] =         (ImVec4){0.075f, 0.647f, 0.784f, 1.000f};
            colors[ImGuiCol_Button] =                   (ImVec4){0.075f, 0.647f, 0.784f, 0.230f};
            colors[ImGuiCol_ButtonHovered] =            (ImVec4){0.075f, 0.647f, 0.784f, 0.420f};
            colors[ImGuiCol_ButtonActive] =             (ImVec4){0.075f, 0.647f, 0.784f, 0.650f};
            colors[ImGuiCol_Header] =                   (ImVec4){0.075f, 0.647f, 0.784f, 0.220f};
            colors[ImGuiCol_HeaderHovered] =            (ImVec4){0.075f, 0.647f, 0.784f, 0.420f};
            colors[ImGuiCol_HeaderActive] =             (ImVec4){0.075f, 0.647f, 0.784f, 0.650f};
            colors[ImGuiCol_Separator] =                (ImVec4){0.090f, 0.184f, 0.259f, 1.000f};
            colors[ImGuiCol_SeparatorHovered] =         (ImVec4){0.075f, 0.647f, 0.784f, 0.550f};
            colors[ImGuiCol_SeparatorActive] =          (ImVec4){0.075f, 0.647f, 0.784f, 0.750f};
            colors[ImGuiCol_ResizeGrip] =               (ImVec4){0.075f, 0.647f, 0.784f, 0.260f};
            colors[ImGuiCol_ResizeGripHovered] =        (ImVec4){0.075f, 0.647f, 0.784f, 0.500f};
            colors[ImGuiCol_ResizeGripActive] =         (ImVec4){0.075f, 0.647f, 0.784f, 0.700f};
            colors[ImGuiCol_Tab] =                      (ImVec4){0.043f, 0.102f, 0.157f, 1.000f};
            colors[ImGuiCol_TabHovered] =               (ImVec4){0.075f, 0.647f, 0.784f, 0.320f};
            colors[ImGuiCol_TabSelected] =              (ImVec4){0.075f, 0.647f, 0.784f, 0.240f};
            colors[ImGuiCol_TabHovered] =               (ImVec4){0.043f, 0.102f, 0.157f, 1.000f};
            colors[ImGuiCol_DockingPreview] =           (ImVec4){0.075f, 0.647f, 0.784f, 0.320f};
            colors[ImGuiCol_DockingEmptyBg] =           (ImVec4){0.024f, 0.063f, 0.102f, 1.000f};
            colors[ImGuiCol_TableHeaderBg] =            (ImVec4){0.043f, 0.102f, 0.157f, 1.000f};
            colors[ImGuiCol_TableBorderStrong] =        (ImVec4){0.090f, 0.184f, 0.259f, 1.000f};
            colors[ImGuiCol_TableBorderLight] =         (ImVec4){0.090f, 0.184f, 0.259f, 0.450f};
            colors[ImGuiCol_TableRowBgAlt] =            (ImVec4){0.043f, 0.102f, 0.157f, 0.450f};
            colors[ImGuiCol_TextSelectedBg] =           (ImVec4){0.075f, 0.647f, 0.784f, 0.270f};
        }
    }
    // Pipeline
    {
        PipelineInfo info{};
        info.renderPass = m_Pass;
        info.subpassIndex = 0;
        info.vertPath = "assets/shaders/main.vert.spv";
        info.fragPath = "assets/shaders/main.frag.spv";
        
        CreatePipeline(m_Pipeline, info);
    }
}

Application::~Application()
{
    VK_CHECK(vkDeviceWaitIdle(m_Device));

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    DestroyPipeline(m_Pipeline);

    vkDestroyDescriptorPool(m_Device, m_UiDescPool, nullptr);

    for(auto& fence : m_InFlightFences)
        vkDestroyFence(m_Device, fence, nullptr);
    for(auto& sema : m_ImageAvailable)
        vkDestroySemaphore(m_Device, sema, nullptr);
    for(auto& sema : m_RenderFinished)
        vkDestroySemaphore(m_Device, sema, nullptr);

    vkDestroyCommandPool(m_Device, m_CmdPool, nullptr);

    for(auto& fb : m_Framebuffers)
        vkDestroyFramebuffer(m_Device, fb, nullptr);
    for(auto& view : m_ScImageViews)
        vkDestroyImageView(m_Device, view, nullptr);

    DestroyImage(m_DepthImage);
    vkDestroyRenderPass(m_Device, m_Pass, nullptr);
    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
    vkDestroyDevice(m_Device, nullptr);
    vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
    vkDestroyInstance(m_Instance, nullptr);

    glfwDestroyWindow(m_Window);
    glfwTerminate();
}

void Application::Run()
{
    glfwShowWindow(m_Window);
    while(!glfwWindowShouldClose(m_Window))
    {
        // Delta time
        {
            double crntTime = glfwGetTime();
            m_DeltaTime = crntTime - m_LastTime;
            m_LastTime = crntTime;
        }

        glfwGetFramebufferSize(m_Window, &m_Width, &m_Height);
        if(!StartFrame())
            continue;
        
        // Draw commands
        {
            VkViewport vp{};
            vp.x = 0;
            vp.y = 0;
            vp.maxDepth = 1.0f;
            vp.minDepth = 0.0f;
            vp.width = m_ScCaps.extent.width;
            vp.height = m_ScCaps.extent.height;
            vkCmdSetViewport(m_CmdBuffs[m_FrameIdx], 0, 1, &vp);

            VkRect2D scissor{};
            scissor.extent = m_ScCaps.extent;
            scissor.offset = { 0, 0 };
            vkCmdSetScissor(m_CmdBuffs[m_FrameIdx], 0, 1, &scissor);
        
            vkCmdBindPipeline(m_CmdBuffs[m_FrameIdx], VK_PIPELINE_BIND_POINT_GRAPHICS, m_Pipeline.pipeline);
            vkCmdDraw(m_CmdBuffs[m_FrameIdx], 3, 1, 0, 0);
        }

        // ImGui
        ImGui::Begin("Settings");
        ImGui::TextColored(ImVec4(0, 255, 0, 244), "Delta Time: %.3fms", m_DeltaTime * 1000);
        ImGui::TextColored(ImVec4(0, 255, 0, 244), "FPS: %.1f Hz", 1/m_DeltaTime);
        ImGui::End();
        
        EndFrame();
        
        if(glfwGetKey(m_Window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            break;

        glfwPollEvents();
    }
}

bool Application::StartFrame()
{
    VK_CHECK(vkWaitForFences(m_Device, 1, &m_InFlightFences[m_FrameIdx], true, UINT64_MAX));
    
    VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, m_ImageAvailable[m_FrameIdx], nullptr, &m_ImageIdx);
    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        Resize();
        m_FrameIdx = (m_FrameIdx + 1) % FRAMES_IN_FLIGHT; 
        return false;
    }
    else
    {
        VK_CHECK(result);
    }
    
    VK_CHECK(vkResetFences(m_Device, 1, &m_InFlightFences[m_FrameIdx]));
    VK_CHECK(vkResetCommandBuffer(m_CmdBuffs[m_FrameIdx], 0));
    BeginCommandBuffer(m_CmdBuffs[m_FrameIdx], VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VkClearValue clearColors[2] = {};
    clearColors[0].color = {0.1f, 0.1f, 0.1f, 1.0f};
    clearColors[1].depthStencil.depth = 1.0f;

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = clearColors;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = m_ScCaps.extent;
    rpInfo.renderPass = m_Pass;
    rpInfo.framebuffer = m_Framebuffers[m_ImageIdx];

    vkCmdBeginRenderPass(m_CmdBuffs[m_FrameIdx], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();

    return true;
}

void Application::EndFrame()
{
    ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_CmdBuffs[m_FrameIdx], nullptr);

	ImGui::UpdatePlatformWindows();
	ImGui::RenderPlatformWindowsDefault();

    vkCmdEndRenderPass(m_CmdBuffs[m_FrameIdx]);
    VK_CHECK(vkEndCommandBuffer(m_CmdBuffs[m_FrameIdx]));

    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_CmdBuffs[m_FrameIdx];
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_ImageAvailable[m_FrameIdx];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_RenderFinished[m_ImageIdx];
    
    VK_CHECK(vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, m_InFlightFences[m_FrameIdx]));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.pImageIndices = &m_ImageIdx;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_RenderFinished[m_ImageIdx];
    
    VkResult result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);
    if(result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        Resize();
    }

    m_FrameIdx = (m_FrameIdx + 1) % FRAMES_IN_FLIGHT;
}

void Application::CreateImage(Image& image, const ImageInfo& imgInfo)
{
    image.info = imgInfo;

    {
        VkImageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.arrayLayers = 1;
        info.extent.width = imgInfo.width;
        info.extent.height = imgInfo.height;
        info.extent.depth = 1;
        info.format = imgInfo.format;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        info.mipLevels = 1;
        info.queueFamilyIndexCount = m_UniqueQueues.size();
        info.pQueueFamilyIndices = m_UniqueQueues.data();
        info.sharingMode = m_UniqueQueues.size() > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = imgInfo.usage;

        VK_CHECK(vkCreateImage(m_Device, &info, nullptr, &image.image));
    }
    {
        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements(m_Device, image.image, &memReq);

        VkMemoryAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = memReq.size;
        info.memoryTypeIndex = FindMemoryType(m_PhysicalDevice, memReq.memoryTypeBits, imgInfo.memProps);
        
        VK_CHECK(vkAllocateMemory(m_Device, &info, nullptr, &image.mem));
        VK_CHECK(vkBindImageMemory(m_Device, image.image, image.mem, 0));
    }
    {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.components = { VK_COMPONENT_SWIZZLE_IDENTITY };
        info.format = imgInfo.format;
        info.image = image.image;
        info.subresourceRange = {
            .aspectMask = imgInfo.aspectFlags,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        };
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        
        VK_CHECK(vkCreateImageView(m_Device, &info, nullptr, &image.view));
    }

    if(!imgInfo.gpuResource)
        return;
    
    {
        VkSamplerCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        info.anisotropyEnable = VK_FALSE;
        info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        info.compareEnable = VK_FALSE;
        info.compareOp = VK_COMPARE_OP_ALWAYS;
        info.minFilter = VK_FILTER_LINEAR;
        info.magFilter = VK_FILTER_LINEAR;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        
        VK_CHECK(vkCreateSampler(m_Device, &info, nullptr, &image.sampler));
    }
}

void Application::DestroyImage(Image& image)
{
    vkDestroyImage(m_Device, image.image, nullptr);
    vkDestroyImageView(m_Device, image.view, nullptr);
    vkFreeMemory(m_Device, image.mem, nullptr);

    if(!image.info.gpuResource)
        return;
    vkDestroySampler(m_Device, image.sampler, nullptr);

    memset(&image, 0, sizeof(image));
}

void Application::CreatePipeline(Pipeline& pipeline, const PipelineInfo& pipelineInfo)
{
    // Pipeline layout
    {
        VkPipelineLayoutCreateInfo layInfo{};
        layInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layInfo.setLayoutCount = pipelineInfo.setLayCount;
        layInfo.pSetLayouts = pipelineInfo.setLays;
        layInfo.pushConstantRangeCount = pipelineInfo.pushConstRangesCount;
        layInfo.pPushConstantRanges = pipelineInfo.pushConstRanges;

        VK_CHECK(vkCreatePipelineLayout(m_Device, &layInfo, nullptr, &pipeline.layout));
    }

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = pipelineInfo.bindingCount;
    vertexInput.pVertexBindingDescriptions = pipelineInfo.bindings;
    vertexInput.vertexAttributeDescriptionCount = pipelineInfo.attribCount;
    vertexInput.pVertexAttributeDescriptions = pipelineInfo.attribs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;

    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] =
    {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineViewportStateCreateInfo viewport{};
    viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport.viewportCount = 1;
    viewport.scissorCount = 1;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    VkShaderModule vertMod = nullptr, fragMod = nullptr;
    u8* vertCode, *fragCode;
    VkPipelineShaderStageCreateInfo stages[2];
    memset(stages, 0, sizeof(VkPipelineShaderStageCreateInfo) * 2);
    {
        u64 vertSize = 0, fragSize = 0;
        vertCode = ReadFile(pipelineInfo.vertPath, &vertSize, "rb");
        VkShaderModuleCreateInfo modInfo{};
        modInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        modInfo.codeSize = vertSize;
        modInfo.pCode = reinterpret_cast<u32*>(vertCode);
        VK_CHECK(vkCreateShaderModule(m_Device, &modInfo, nullptr, &vertMod));
        
        fragCode = ReadFile(pipelineInfo.fragPath, &fragSize, "rb");
        modInfo.codeSize = fragSize;
        modInfo.pCode = reinterpret_cast<u32*>(fragCode);
        VK_CHECK(vkCreateShaderModule(m_Device, &modInfo, nullptr, &fragMod));
    }
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].module = vertMod;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].module = fragMod;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].pName = "main";

    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 2;
    info.pStages = stages;
    info.pVertexInputState = &vertexInput;
    info.pInputAssemblyState = &inputAssembly;
    info.pRasterizationState = &rasterizer;
    info.pMultisampleState = &multisampling;
    info.pDepthStencilState = &depthStencil;
    info.pColorBlendState = &colorBlending;
    info.pDynamicState = &dynamicState;
    info.pViewportState = &viewport;
    info.layout = pipeline.layout;
    info.renderPass = pipelineInfo.renderPass;
    info.subpass = pipelineInfo.subpassIndex;
    info.basePipelineHandle = nullptr;
    info.basePipelineIndex = -1;

    VK_CHECK(vkCreateGraphicsPipelines(m_Device, nullptr, 1, &info, nullptr, &pipeline.pipeline));
    vkDestroyShaderModule(m_Device, vertMod, nullptr);
    vkDestroyShaderModule(m_Device, fragMod, nullptr);
    free(vertCode);
    free(fragCode);
}

void Application::DestroyPipeline(Pipeline& pipeline)
{
    vkDestroyPipelineLayout(m_Device, pipeline.layout, nullptr);
    vkDestroyPipeline(m_Device, pipeline.pipeline, nullptr);
}

VkCommandBuffer Application::CreateCommandBuffer()
{
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandBufferCount = 1;
    info.commandPool = m_CmdPool;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    VkCommandBuffer buffer = nullptr;
    VK_CHECK(vkAllocateCommandBuffers(m_Device, &info, &buffer));
    return buffer;
}

void Application::DestroyCommandBuffer(VkCommandBuffer buffer)
{
    vkFreeCommandBuffers(m_Device, m_CmdPool, 1, &buffer);
}

void Application::BeginCommandBuffer(VkCommandBuffer buffer, VkCommandBufferUsageFlags flags)
{
    VkCommandBufferBeginInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    info.flags = flags;
    
    VK_CHECK(vkBeginCommandBuffer(buffer, &info));
}

ScCaps Application::GetScCaps()
{
    ScCaps caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &caps.caps));

    {
        caps.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;

        u32 count = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &count, nullptr));
        std::vector<VkPresentModeKHR> modes(count);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &count, modes.data()));
    
        for(auto& mode : modes)
        {
            if(mode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                caps.presentMode = mode;
                break;
            }
        }
    }

    {
        u32 count = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &count, nullptr));
        std::vector<VkSurfaceFormatKHR> formats(count);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &count, formats.data()));

        caps.format = formats[0];

        for(auto& format : formats)
        {
            if((format.format == VK_FORMAT_R8G8B8A8_UNORM) && (format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
            {
                caps.format = format;
                break;
            }
        }
    }

    {
        if(caps.caps.currentExtent.width != UINT32_MAX)
        {
            caps.extent = caps.caps.currentExtent;
        }
        else
        {
            int width, height;
            glfwGetFramebufferSize(m_Window, &width, &height);

            caps.extent.width = width;
            caps.extent.height = height;
            caps.extent.width = glm::clamp(caps.extent.width, caps.caps.minImageExtent.width, caps.caps.maxImageExtent.width);
            caps.extent.height = glm::clamp(caps.extent.height, caps.caps.minImageExtent.height, caps.caps.maxImageExtent.height);
        }
    }

    return caps;
}

void Application::CreateSwapchain()
{
    {
        u32 imgCount = m_ScCaps.caps.minImageCount + 1;
        if(m_ScCaps.caps.maxImageCount > 0 && imgCount > m_ScCaps.caps.maxImageCount)
        {
            imgCount = m_ScCaps.caps.maxImageCount;
        }

        VkSwapchainCreateInfoKHR info = {
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .surface = m_Surface,
            .minImageCount = imgCount,
            .imageFormat = m_ScCaps.format.format,
            .imageColorSpace = m_ScCaps.format.colorSpace,
            .imageExtent = m_ScCaps.extent,
            .imageArrayLayers = 1,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .preTransform = m_ScCaps.caps.currentTransform,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = m_ScCaps.presentMode,
            .clipped = VK_TRUE,
            .oldSwapchain = nullptr
        };

        if(m_UniqueQueues.size() == 1)
        {
            info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        else
        {
            info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            info.queueFamilyIndexCount = m_UniqueQueues.size();
            info.pQueueFamilyIndices = m_UniqueQueues.data();
        }

        VK_CHECK(vkCreateSwapchainKHR(m_Device, &info, nullptr, &m_Swapchain));
    }
    {
        u32 count = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &count, nullptr));
        m_ScImages.resize(count);
        VK_CHECK(vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &count, m_ScImages.data()));
    }
    {
        m_ScImageViews.reserve(m_ScImages.size());
        for(auto& image : m_ScImages)
        {
            VkImageViewCreateInfo info {};
            info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            info.format = m_ScCaps.format.format;
            info.subresourceRange.levelCount = 1;
            info.subresourceRange.layerCount = 1;
            info.subresourceRange.baseMipLevel = 0;
            info.subresourceRange.baseArrayLayer = 0;
            info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            info.image = image;
            info.viewType = VK_IMAGE_VIEW_TYPE_2D;

            VkImageView view;
            VK_CHECK(vkCreateImageView(m_Device, &info, nullptr, &view));
            m_ScImageViews.push_back(view);
        }
    }
    {
        m_Framebuffers.reserve(m_ScImages.size());
        for(auto& view : m_ScImageViews)
        {
            VkImageView views[] = {
                view,
                m_DepthImage.view
            };

            VkFramebufferCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            info.attachmentCount = 2;
            info.pAttachments = views;
            info.renderPass = m_Pass;
            info.layers = 1;
            info.width = m_ScCaps.extent.width;
            info.height = m_ScCaps.extent.height;

            VkFramebuffer fb;
            VK_CHECK(vkCreateFramebuffer(m_Device, &info, nullptr, &fb));
            m_Framebuffers.push_back(fb);
        }
    }
}

void Application::Resize()
{
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_Window, &width, &height);

    while(width == 0 || height == 0)
    {
        glfwGetFramebufferSize(m_Window, &width, &height);
        glfwWaitEvents();
    }

    VK_CHECK(vkDeviceWaitIdle(m_Device));

    for(auto& fb : m_Framebuffers)
        vkDestroyFramebuffer(m_Device, fb, nullptr);
    for(auto& view : m_ScImageViews)
        vkDestroyImageView(m_Device, view, nullptr);
    for(auto& sema : m_RenderFinished)
        vkDestroySemaphore(m_Device, sema, nullptr);
    for(auto& sema : m_ImageAvailable)
        vkDestroySemaphore(m_Device, sema, nullptr);

    VkFormat depthFormat = m_DepthImage.info.format;
    DestroyImage(m_DepthImage);
    vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
    m_ScImages.clear();
    m_ScImageViews.clear();
    m_Framebuffers.clear();
    m_RenderFinished.clear();

    // Depth image
    {
        ImageInfo info{};
        info.width = m_Width;
        info.height = m_Height;
        info.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT;
        info.gpuResource = false;
        info.memProps = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        info.format = depthFormat;

        CreateImage(m_DepthImage, info);
    }

    m_ScCaps = GetScCaps();
    CreateSwapchain();

    VkSemaphoreCreateInfo semaInfo{};
    semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    m_RenderFinished.resize(m_ScImages.size());
    for(auto& sema : m_RenderFinished)
        VK_CHECK(vkCreateSemaphore(m_Device, &semaInfo, nullptr, &sema));
    for(auto& sema : m_ImageAvailable)
        VK_CHECK(vkCreateSemaphore(m_Device, &semaInfo, nullptr, &sema));
}

u8* Application::ReadFile(const char* path, u64* size, const char* mode)
{
    FILE* file = fopen(path, mode);
    assert(file);
    fseek(file, 0, SEEK_END);
    u64 s = ftell(file);
    rewind(file);

    u8* buffer = (u8*)calloc(1, sizeof(u8) * s);
    fread(buffer, 1, sizeof(u8) * s, file);

    fclose(file);
    *size = s;
    return buffer;
}