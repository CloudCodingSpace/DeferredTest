#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <cstdint>
#include <vector>
#include <set>

#include <glm/glm.hpp>

typedef uint32_t u32;
typedef uint64_t u64;
typedef uint8_t u8;
typedef int32_t i32;

#define FRAMES_IN_FLIGHT 2

struct ScCaps
{
    VkExtent2D extent;
    VkSurfaceFormatKHR format;
    VkPresentModeKHR presentMode;
    VkSurfaceCapabilitiesKHR caps; 
};

class Application
{
public:
    Application();
    ~Application();

    void Run();

private:
    struct Pipeline {
        VkPipeline pipeline;
        VkPipelineLayout layout;
    };

    struct PipelineInfo {
        const char* vertPath;
        const char* fragPath;
        u32 setLayCount;
        VkDescriptorSetLayout* setLays;
        u32 binding;
        VkVertexInputBindingDescription* bindings;
        u32 attribCount;
        VkVertexInputAttributeDescription* attribs;
        u32 subpassIndex;
        VkRenderPass renderPass;
    };

private:
    bool StartFrame();
    void EndFrame();

    VkCommandBuffer CreateCommandBuffer();
    void DestroyCommandBuffer(VkCommandBuffer buffer);
    void BeginCommandBuffer(VkCommandBuffer buffer, VkCommandBufferUsageFlags flags);

    void CreatePipeline(Pipeline& pipeline, const PipelineInfo& info);

    ScCaps GetScCaps();
    void CreateSwapchain();
    void Resize();

private:
    GLFWwindow* m_Window = nullptr;
    int m_Width, m_Height;

    VkInstance m_Instance = nullptr;
    VkSurfaceKHR m_Surface = nullptr;
    VkPhysicalDevice m_PhysicalDevice = nullptr;
    VkPhysicalDeviceFeatures m_PhysicalDeviceFeatures = {};
    VkDevice m_Device = nullptr;
    i32 m_GraphicsQueueIdx = -1, m_PresentQueueIdx = -1, m_ComputeQueueIdx = -1;
    VkQueue m_GraphicsQueue = nullptr, m_PresentQueue = nullptr, m_ComputeQueue = nullptr;
    VkRenderPass m_Pass = nullptr;

    ScCaps m_ScCaps{};
    VkSwapchainKHR m_Swapchain = nullptr;
    std::vector<VkImage> m_ScImages;
    std::vector<VkImageView> m_ScImageViews;
    std::vector<VkFramebuffer> m_Framebuffers;

    VkCommandPool m_CmdPool = nullptr;
    VkCommandBuffer m_CmdBuffs[FRAMES_IN_FLIGHT];
    VkFence m_InFlightFences[FRAMES_IN_FLIGHT];
    VkSemaphore m_ImageAvailable[FRAMES_IN_FLIGHT];
    std::vector<VkSemaphore> m_RenderFinished;
    VkDescriptorPool m_UiDescPool = nullptr;

    Pipeline m_Pipeline;
    
    u32 m_ImageIdx = 0;
    u32 m_FrameIdx = 0;
    double m_DeltaTime = 0.0, m_LastTime = 0.0;
};
