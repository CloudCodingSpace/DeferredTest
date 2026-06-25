#pragma once

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <cstdint>
#include <vector>
#include <set>
#include <array>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

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

    struct PipelineInfo {
        const char* vertPath;
        const char* fragPath;
        u32 setLayCount;
        VkDescriptorSetLayout* setLays;
        u32 pushConstRangesCount;
        VkPushConstantRange* pushConstRanges; 
        u32 bindingCount;
        VkVertexInputBindingDescription* bindings;
        u32 attribCount;
        VkVertexInputAttributeDescription* attribs;
        u32 subpassIndex;
        VkRenderPass renderPass;
    };

    struct Pipeline {
        VkPipeline pipeline;
        VkPipelineLayout layout;
    };

    struct ImageInfo {
        u32 width, height;
        VkFormat format;
        bool gpuResource;
        VkImageUsageFlags usage;
        VkMemoryPropertyFlagBits memProps;
        VkImageAspectFlags aspectFlags;
    };

    struct Image {
        ImageInfo info;
        VkImage image;
        VkDeviceMemory mem;
        VkImageView view;
        VkSampler sampler;
    };

    struct BufferInfo {
        u64 size;
        VkMemoryPropertyFlags memProps;
        VkBufferUsageFlags usage;
        void* data;
    };

    struct Buffer {
        BufferInfo info;
        VkBuffer buffer;
        VkDeviceMemory memory;
    };

    struct Vertex {
        glm::vec3 pos;
        glm::vec3 normal;
        glm::vec2 uv;

        static inline std::array<VkVertexInputBindingDescription, 1> GetBindings()
        {
            std::array<VkVertexInputBindingDescription, 1> bindings;
            bindings[0].binding = 0;
            bindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            bindings[0].stride = sizeof(Vertex);

            return bindings;
        }

        static inline std::array<VkVertexInputAttributeDescription, 3> GetAttribs() 
        {
            std::array<VkVertexInputAttributeDescription, 3> attribs;
            attribs[0].binding = 0;
            attribs[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            attribs[0].location = 0;
            attribs[0].offset = offsetof(Vertex, pos);
            
            attribs[1].binding = 0;
            attribs[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            attribs[1].location = 1;
            attribs[1].offset = offsetof(Vertex, normal);
            
            attribs[2].binding = 0;
            attribs[2].format = VK_FORMAT_R32G32_SFLOAT;
            attribs[2].location = 2;
            attribs[2].offset = offsetof(Vertex, uv);

            return attribs;
        }
    };

    class Mesh
    {
    public:
        static std::vector<Mesh> LoadGLTF(Application* app, std::string path);
        
        void Create(Application* app, u64 vertexSize, void* vertexData, u64 indexCount, u32* indices);
        void Destroy();

        void Render();
    
        inline Buffer& GetVertexBuffer() { return m_VertexBuffer; } 
        inline Buffer& GetIndexBuffer() { return m_IndexBuffer; } 
        inline u64 GetIndexCount() { return m_IndexCount; }

    private:
        Buffer m_VertexBuffer{};
        Buffer m_IndexBuffer{};
        u64 m_IndexCount = 0;
        Application* m_App = nullptr;
    };

    class Camera
    {
    public:
        void Create(Application* app);

        void Update();

        inline glm::mat4 GetVP() { return m_Proj * m_View; }
    private:
        glm::mat4 m_Proj = glm::mat4(1.0f);
        glm::mat4 m_View = glm::mat4(1.0f);
        glm::vec3 m_Front, m_Right, m_Up, m_Pos;
        Application* m_App = nullptr;
        bool m_FirstMouse = true;

        float m_LastX = 400, m_LastY = 300, m_Yaw = -90.0f, m_Pitch = 0.0f;
        const float m_Fov = 86.0f, m_Sensitivity = 0.05f, m_Speed = 0.5f;
    };

private:
    bool StartFrame();
    void EndFrame();

    VkCommandBuffer CreateCommandBuffer();
    void DestroyCommandBuffer(VkCommandBuffer buffer);
    void BeginCommandBuffer(VkCommandBuffer buffer, VkCommandBufferUsageFlags flags);

    void CreateBuffer(Buffer& buffer, const BufferInfo& info);
    void DestroyBuffer(Buffer& buffer);
    void UploadDataToBuffer(Buffer& buffer, void* data);

    void CreateImage(Image& image, const ImageInfo& info);
    void DestroyImage(Image& image);

    void CreatePipeline(Pipeline& pipeline, const PipelineInfo& info);
    void DestroyPipeline(Pipeline& pipeline);

private:
    static inline uint32_t FindMemoryType(VkPhysicalDevice device, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties props = {};
        vkGetPhysicalDeviceMemoryProperties(device, &props);

        for (uint32_t i = 0; i < props.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (props.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }

        assert(false && "Failed to find the suitable memory index!");
    }

    ScCaps GetScCaps();
    void CreateSwapchain();
    void Resize();
    u8* ReadFile(const char* path, u64* size, const char* mode);

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
    std::vector<u32> m_UniqueQueues;

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

    VkRenderPass m_Pass = nullptr;
    Image m_DepthImage{};
    Pipeline m_Pipeline;
    std::vector<Mesh> m_Model{};

    Camera m_Camera{};
    u32 m_ImageIdx = 0;
    u32 m_FrameIdx = 0;
    double m_DeltaTime = 0.0, m_LastTime = 0.0;
};
