#pragma once

#include <stdint.h>
#include <stddef.h>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * assembly to vulkan resource mapping
 *
 * map vulkan resources such as
 * -push constants
 * -descriptor set entries
 *	-images
 *  -buffers
 *
 * to assembly uniform reads
 *
 * push constants should be one read
 *
 * buffers and images are handled through the TMU pipeline
 * and therefore carry implicit uniform reads
 * buffers should be one uniform (general memory read)
 * number of uniforms for images are dependent on type (and TMU writes)
 *
 * therefore what we need is a mapping for each assembly uniform read
 * to some vulkan resource
 * and the driver should be able to figure out what to put in the uniform queue
 * based on the mapping
 *
 * Coordinate shader mappings are tricky as they are not a concept in Vulkan.
 * However, assuming they were compiled from the same vertex shader, they must share the same uniforms.
 * Therefore Coordinate shaders will share the same uniforms, but may employ a different mappping.
 * If coordinate shader mapping is absent, they'll just use the same mapping as vertex shaders.
 *
 * Vertex and coordinate shaders must be in the same shader module.
 *
 */

typedef enum VkRpiAssemblyTypeEXT {
	VK_RPI_ASSEMBLY_TYPE_COORDINATE = 0,
	VK_RPI_ASSEMBLY_TYPE_VERTEX = 1,
	VK_RPI_ASSEMBLY_TYPE_FRAGMENT = 2,
	VK_RPI_ASSEMBLY_TYPE_COMPUTE = 3,
	VK_RPI_ASSEMBLY_TYPE_MAX,
} VkRpiAssemblyTypeEXT;

typedef enum VkRpiAssemblyMappingTypeEXT {
	VK_RPI_ASSEMBLY_MAPPING_TYPE_DESCRIPTOR = 0,
	VK_RPI_ASSEMBLY_MAPPING_TYPE_PUSH_CONSTANT = 1,
	VK_RPI_ASSEMBLY_MAPPING_TYPE_MAX
} VkRpiAssemblyMappingTypeEXT;

//defines mapping for a single uniform FIFO read to a Vulkan resource
typedef struct VkRpiAssemblyMappingEXT {
	VkRpiAssemblyMappingTypeEXT	mappingType;
	VkDescriptorType			descriptorType;
	uint32_t					descriptorSet;
	uint32_t					descriptorBinding;
	uint32_t					descriptorArrayElement;
	uint32_t					resourceOffset; //in bytes
} VkRpiAssemblyMappingEXT;

typedef struct VkRpiShaderModuleAssemblyCreateInfoEXT {
	VkStructureType               sType;
	const void*                   pNext;
	uint64_t**					  instructions;
	uint32_t*					  numInstructions;
	VkRpiAssemblyMappingEXT**	  mappings;
	uint32_t*					  numMappings;
} VkRpiShaderModuleAssemblyCreateInfoEXT;

/* Private assembly transport, not standard SPIR-V. Keep the historical
 * six-word layout on 32-bit; use the last word for pointer high bits on 64-bit.
 * All AArch64 clients must rebuild using this helper. */
static inline void vkRpiEncodeAssemblyEXT(uint32_t code[6],
                                        const VkRpiShaderModuleAssemblyCreateInfoEXT *info)
{
    uintptr_t address = (uintptr_t)info;
    code[0] = 0x07230203;
    code[1] = 0x00010000;
    code[2] = 0x14E45250;
    code[3] = 1;
    code[4] = (uint32_t)address;
#if UINTPTR_MAX > UINT32_MAX
    code[5] = (uint32_t)(address >> 32);
#else
    code[5] = 1u << 16;
#endif
}

static inline const VkRpiShaderModuleAssemblyCreateInfoEXT *
vkRpiDecodeAssemblyEXT(const uint32_t *code, size_t size)
{
    if (!code || size != 6 * sizeof(uint32_t) || code[0] != 0x07230203 ||
        code[1] != 0x00010000 || code[2] != 0x14E45250 || code[3] != 1)
        return NULL;
    uintptr_t address = code[4];
#if UINTPTR_MAX > UINT32_MAX
    address |= (uintptr_t)code[5] << 32;
#endif
    return (const VkRpiShaderModuleAssemblyCreateInfoEXT *)address;
}

#ifdef __cplusplus
}
#endif

