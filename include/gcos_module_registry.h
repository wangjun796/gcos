/**
 * @file gcos_module_registry.h
 * @brief GCOS Module Registry Management
 * 
 * Manages module code sharing across multiple application instances.
 * Similar to cref's Package Entry mechanism.
 * 
 * Key features:
 * - Module code loaded once, shared by multiple app instances
 * - Instance tracking (which apps use which module)
 * - Global data template management
 * - Module lifecycle management (load/unload)
 * 
 * @version 1.0.0
 * @date 2026-05-09
 */

#ifndef GCOS_MODULE_REGISTRY_H
#define GCOS_MODULE_REGISTRY_H

#include "gcos_vm.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Module Registry API
 * ============================================================================ */

/**
 * @brief Initialize module registry
 * 
 * Called during VM initialization to set up the module registry.
 * 
 * @param vm VM instance
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult module_registry_init(GCOSVM *vm);

/**
 * @brief Register a new module in the registry
 * 
 * Creates a new module registry entry and stores module metadata.
 * The module code is loaded from SEF data but not yet linked.
 * 
 * @param vm VM instance
 * @param sef_data SEF file data
 * @param sef_size SEF file size
 * @param[out] module_id Output: assigned module ID
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult module_registry_register(GCOSVM *vm, const u8 *sef_data, u32 sef_size, u8 *module_id);

/**
 * @brief Find module registry entry by AID
 * 
 * Searches for a registered module by its Application ID.
 * 
 * @param vm VM instance
 * @param aid Module AID to search for
 * @return Pointer to module registry entry, NULL if not found
 */
GCOSModuleRegistry* module_registry_find_by_aid(GCOSVM *vm, const GCOSAID *aid);

/**
 * @brief Find module registry entry by module ID
 * 
 * Direct lookup by internal module ID.
 * 
 * @param vm VM instance
 * @param module_id Module ID
 * @return Pointer to module registry entry, NULL if invalid
 */
GCOSModuleRegistry* module_registry_find_by_id(GCOSVM *vm, u8 module_id);

/**
 * @brief Add an application instance to module's instance list
 * 
 * Tracks which applications are using this module.
 * Called when creating a new app instance.
 * 
 * @param vm VM instance
 * @param module_id Module ID
 * @param app_id Application instance ID
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult module_registry_add_instance(GCOSVM *vm, u8 module_id, u8 app_id);

/**
 * @brief Remove an application instance from module's instance list
 * 
 * Called when deleting an app instance.
 * If this was the last instance, the module can be unloaded.
 * 
 * @param vm VM instance
 * @param module_id Module ID
 * @param app_id Application instance ID
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult module_registry_remove_instance(GCOSVM *vm, u8 module_id, u8 app_id);

/**
 * @brief Get instance count for a module
 * 
 * Returns the number of application instances currently using this module.
 * 
 * @param vm VM instance
 * @param module_id Module ID
 * @return Number of instances, 0 if module not found
 */
u8 module_registry_get_instance_count(GCOSVM *vm, u8 module_id);

/**
 * @brief Unload a module from the registry
 * 
 * Removes the module from the registry and frees associated resources.
 * Can only be called when instance_count == 0.
 * 
 * @param vm VM instance
 * @param module_id Module ID to unload
 * @return GCOS_SUCCESS on success, error code otherwise
 */
GCOSResult module_registry_unload(GCOSVM *vm, u8 module_id);

/**
 * @brief Verify module dependencies are satisfied
 * 
 * Checks that all imported modules are available and have compatible versions.
 * 
 * @param vm VM instance
 * @param module_id Module ID to verify
 * @return GCOS_SUCCESS if all dependencies satisfied, error code otherwise
 */
GCOSResult module_registry_verify_dependencies(GCOSVM *vm, u8 module_id);

/**
 * @brief Dump module registry information (for debugging)
 * 
 * Prints all registered modules and their instance counts.
 * 
 * @param vm VM instance
 */
void module_registry_dump(GCOSVM *vm);

#ifdef __cplusplus
}
#endif

#endif /* GCOS_MODULE_REGISTRY_H */
