/* Hardware-negative test: never runs when card0 exists. */
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static unsigned allocations, frees;
static void *VKAPI_PTR allocate(void *user, size_t size, size_t alignment, VkSystemAllocationScope scope)
{ (void)user; (void)alignment; (void)scope; ++allocations; return malloc(size); }
static void *VKAPI_PTR reallocate(void *user, void *old, size_t size, size_t alignment, VkSystemAllocationScope scope)
{ (void)user; (void)alignment; (void)scope; return realloc(old, size); }
static void VKAPI_PTR release(void *user, void *p) { (void)user; if (p) ++frees; free(p); }
int main(int argc, char **argv)
{
    if (access("/dev/dri/card0", F_OK) == 0) return 77;
    CHECK(argc == 2);
    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "%s\n", dlerror()); return 1; }
    PFN_vkCreateInstance create = (PFN_vkCreateInstance)dlsym(lib, "rpi_vkCreateInstance");
    CHECK(create);
    VkAllocationCallbacks callbacks = {0};
    callbacks.pfnAllocation = allocate;
    callbacks.pfnReallocation = reallocate;
    callbacks.pfnFree = release;
    VkApplicationInfo app = {.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO, .apiVersion = VK_API_VERSION_1_0};
    VkInstanceCreateInfo info = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo = &app};
    VkInstance instance;
    for (unsigned i = 0; i < 100; ++i) {
        instance = (VkInstance)(uintptr_t)1;
        CHECK(create(&info, &callbacks, &instance) == VK_ERROR_INITIALIZATION_FAILED);
        CHECK(instance == VK_NULL_HANDLE);
    }
    CHECK(allocations == 100 && frees == 100);
    app.apiVersion = VK_MAKE_VERSION(1, 1, 999);
    CHECK(create(&info, &callbacks, &instance) == VK_ERROR_INITIALIZATION_FAILED);
    CHECK(!instance && allocations == frees);
    app.apiVersion = VK_MAKE_VERSION(1, 3, 0);
    CHECK(create(&info, &callbacks, &instance) == VK_ERROR_INCOMPATIBLE_DRIVER);
    CHECK(!instance && allocations == frees);
    PFN_vkCreateShaderModule shader = (PFN_vkCreateShaderModule)dlsym(lib, "rpi_vkCreateShaderModule");
    CHECK(shader);
    uint32_t code[6] = {0x07230203, 0x00010000, 0, 1, 0, 0};
    VkShaderModuleCreateInfo shaderInfo = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(code), .pCode = code};
    VkShaderModule module;
    CHECK(shader(VK_NULL_HANDLE, &shaderInfo, NULL, &module) == VK_ERROR_INVALID_SHADER_NV);
    CHECK(!module);
    shaderInfo.codeSize = 4;
    CHECK(shader(VK_NULL_HANDLE, &shaderInfo, NULL, &module) == VK_ERROR_INVALID_SHADER_NV);
    PFN_vkCreateComputePipelines compute = (PFN_vkCreateComputePipelines)dlsym(lib, "rpi_vkCreateComputePipelines");
    CHECK(compute);
    VkPipeline pipelines[2] = {(VkPipeline)(uintptr_t)1, (VkPipeline)(uintptr_t)1};
    VkComputePipelineCreateInfo computeInfo[2] = {{0}};
    CHECK(compute(VK_NULL_HANDLE, VK_NULL_HANDLE, 2, computeInfo, NULL, pipelines) == VK_ERROR_FEATURE_NOT_PRESENT);
    CHECK(!pipelines[0] && !pipelines[1]);
    dlclose(lib);
    puts("NO_VC4_PASS: 100 failures cleaned up; unsupported API/shader rejected");
    return 0;
}
