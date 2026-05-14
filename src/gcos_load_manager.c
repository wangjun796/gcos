/**
 * @file gcos_load_manager.c
 * @brief GCOS VM LOAD Command Implementation
 * 
 * Implements the three-phase LOAD command state machine as per COS3 specification:
 * - Phase 1: INSTALL FOR LOAD (P1=0x00) - Initialize loading session
 * - Phase 2: LOAD BLOCKS (P1=0x01) - Load SEF file data blocks
 * - Phase 3: FINALIZE (P1=0x02) - Parse, link and create module
 * 
 * Reference: COS3 Specification Section 8.2.1 (Loading Management)
 */

#include "gcos_vm.h"
#include "gcos_app_manager.h"
#include <stdio.h>
#include <string.h>

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Check if module AID already exists
 * 
 * @param vm VM instance
 * @param aid Module AID to check
 * @return true if exists, false otherwise
 */
static bool module_aid_exists(GCOSVM *vm, const GCOSAID *aid) {
    for (u8 i = 0; i < MAX_MODULES; i++) {
        if (vm->modules[i].loaded && 
            vm->modules[i].module_aid.length == aid->length &&
            memcmp(vm->modules[i].module_aid.aid, aid->aid, aid->length) == 0) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Find a free module slot
 * 
 * @param vm VM instance
 * @return Free module ID, or 0xFF if no free slot
 */
static u8 find_free_module_slot(GCOSVM *vm) {
    for (u8 i = 0; i < MAX_MODULES; i++) {
        if (!vm->modules[i].loaded) {
            return i;
        }
    }
    return 0xFF;
}

/**
 * @brief Check if another channel has active load session
 * 
 * @param vm VM instance
 * @param current_channel Current channel
 * @return true if conflict exists
 */
static bool check_cross_channel_conflict(GCOSVM *vm, u8 current_channel) {
    // For now, simple check - can be enhanced later
    (void)current_channel;
    return (vm->load_context.state != LOAD_STATE_IDLE);
}

/**
 * @brief Reset load context
 * 
 * @param vm VM instance
 */
void reset_load_context(GCOSVM *vm) {
    memset(&vm->load_context, 0, sizeof(GCOSLoadContext));
    vm->load_context.state = LOAD_STATE_IDLE;
}

/* ============================================================================
 * Phase 1: INSTALL FOR LOAD Handler
 * ============================================================================ */

/**
 * @brief Handle INSTALL FOR LOAD command (P1=0x00)
 * 
 * Initializes the loading session and validates the request.
 * 
 * APDU Format:
 *   CLA INS P1 P2 Lc [Data]
 *   80  E6 00 00 xx  [TLV data]
 * 
 * Expected TLV tags in data:
 *   0x4F: Package AID
 *   0xC4: Load parameters (version, SD ID, etc.)
 * 
 * @param apdu APDU data
 * @param apdu_len APDU length
 * @param response Response buffer
 * @param resp_len Response length output
 * @return Status word
 */
u16 handle_install_for_load(const u8 *apdu, u16 apdu_len,
                            u8 *response, u16 *resp_len) {
    GCOSVM *vm = gcos_vm_get_instance();
    
    if (vm == NULL) {
        return 0x6F00;  // SW_NO_PRECISE_DIAGNOSIS
    }
    
    printf("[LOAD] === INSTALL FOR LOAD ===\n");
    
    // Check if another channel has active load session
    if (check_cross_channel_conflict(vm, vm->current_channel)) {
        printf("[LOAD] ERROR: Another channel has active load session\n");
        return 0x6985;  // SW_CONDITIONS_NOT_SATISFIED
    }
    
    // Parse TLV data from APDU
    if (apdu_len < 5) {
        printf("[LOAD] ERROR: No data provided\n");
        return 0x6A80;  // SW_WRONG_DATA
    }
    
    u8 lc = apdu[4];
    const u8 *data = &apdu[5];
    u16 data_len = lc;
    
    // Extract Package AID (Tag 0x4F)
    GCOSAID pkg_aid;
    memset(&pkg_aid, 0, sizeof(GCOSAID));
    
    u8 offset = 0;
    while (offset < data_len) {
        u8 tag = data[offset++];
        
        if (tag == 0x4F) {
            // Package AID
            if (offset >= data_len) {
                return 0x6A80;
            }
            u8 aid_len = data[offset++];
            
            if (aid_len < 5 || aid_len > 16 || offset + aid_len > data_len) {
                printf("[LOAD] ERROR: Invalid AID length %u\n", aid_len);
                return 0x6A80;
            }
            
            pkg_aid.length = aid_len;
            memcpy(pkg_aid.aid, &data[offset], aid_len);
            offset += aid_len;
            
            printf("[LOAD] Package AID: ");
            for (int i = 0; i < aid_len; i++) {
                printf("%02X", pkg_aid.aid[i]);
            }
            printf("\n");
            
        } else if (tag == 0xC4) {
            // Load parameters (optional)
            u8 param_len = data[offset++];
            offset += param_len;  // Skip for now
            
        } else {
            printf("[LOAD] WARNING: Unknown tag 0x%02X\n", tag);
            return 0x6A80;
        }
    }
    
    // Validate Package AID
    if (pkg_aid.length == 0) {
        printf("[LOAD] ERROR: Package AID not provided\n");
        return 0x6A80;
    }
    
    // Check for duplicate package
    if (module_aid_exists(vm, &pkg_aid)) {
        printf("[LOAD] ERROR: Package AID already exists\n");
        return 0x6A89;  // SW_FILE_ALREADY_EXISTS
    }
    
    // Allocate new module ID
    u8 module_id = find_free_module_slot(vm);
    if (module_id == 0xFF) {
        printf("[LOAD] ERROR: No free module slot (max %d modules)\n", MAX_MODULES);
        return 0x6A84;  // SW_MEMORY_FAILURE
    }
    
    // Check module count limit
    if (vm->module_count >= MAX_MODULES) {
        printf("[LOAD] ERROR: Module count limit reached (%d/%d)\n", 
               vm->module_count, MAX_MODULES);
        return 0x6A84;
    }
    
    // End any existing load session on this channel
    if (vm->load_context.state != LOAD_STATE_IDLE) {
        printf("[LOAD] WARNING: Ending previous load session\n");
        reset_load_context(vm);
    }
    
    // Initialize load context
    vm->load_context.state = LOAD_STATE_INITIALIZATION;
    vm->load_context.target_module_id = module_id;
    vm->load_context.package_aid = pkg_aid;
    vm->load_context.buffer_size = 0;
    vm->load_context.buffer_offset = 0;
    vm->load_context.import_count = 0;
    vm->load_context.app_count = 0;
    
    printf("[LOAD] Session initialized. Module ID: %u\n", module_id);
    
    if (resp_len) {
        *resp_len = 0;
    }
    
    return 0x9000;  // SW_SUCCESS
}

/* ============================================================================
 * Phase 2: LOAD BLOCKS Handler
 * ============================================================================ */

/**
 * @brief Handle LOAD BLOCKS command (P1=0x01)
 * 
 * Receives SEF file data blocks and stores them in the buffer.
 * Multiple APDUs may be needed for large files.
 * 
 * APDU Format:
 *   CLA INS P1 P2 Lc [Block Data]
 *   80  E8 01 xx yy  [SEF data bytes]
 * 
 * @param apdu APDU data
 * @param apdu_len APDU length
 * @param response Response buffer
 * @param resp_len Response length output
 * @return Status word
 */
u16 handle_load_blocks(const u8 *apdu, u16 apdu_len,
                       u8 *response, u16 *resp_len) {
    GCOSVM *vm = gcos_vm_get_instance();
    
    if (vm == NULL) {
        return 0x6F00;
    }
    
    printf("[LOAD] === LOAD BLOCKS ===\n");
    
    // Verify load session is active
    if (vm->load_context.state != LOAD_STATE_INITIALIZATION &&
        vm->load_context.state != LOAD_STATE_LOADING_BLOCKS) {
        printf("[LOAD] ERROR: No active load session\n");
        return 0x6985;  // SW_CONDITIONS_NOT_SATISFIED
    }
    
    // Check if we have data
    if (apdu_len < 5) {
        printf("[LOAD] ERROR: No block data\n");
        return 0x6A80;
    }
    
    u8 p2 = apdu[3];  // P2: Block sequence number or flags
    u8 lc = apdu[4];
    const u8 *block_data = &apdu[5];
    u16 block_len = lc;
    
    printf("[LOAD] Block received: P2=0x%02X, Length=%u bytes\n", p2, block_len);
    
    // Check buffer space
    if (vm->load_context.buffer_size + block_len > GCOS_MODULE_CODE_SIZE) {
        printf("[LOAD] ERROR: Buffer overflow (%u + %u > %u)\n",
               vm->load_context.buffer_size, block_len, GCOS_MODULE_CODE_SIZE);
        reset_load_context(vm);
        return 0x6A84;  // SW_MEMORY_FAILURE
    }
    
    // Append block to buffer
    memcpy(&vm->load_context.buffer[vm->load_context.buffer_size],
           block_data, block_len);
    vm->load_context.buffer_size += block_len;
    
    printf("[LOAD] Buffer size: %u bytes\n", vm->load_context.buffer_size);
    
    // Update state
    vm->load_context.state = LOAD_STATE_LOADING_BLOCKS;
    
    if (resp_len) {
        *resp_len = 0;
    }
    
    return 0x9000;  // SW_SUCCESS
}

/* ============================================================================
 * Phase 3: FINALIZE Handler
 * ============================================================================ */

/**
 * @brief Parse SEF file header (First Section)
 * 
 * @param sef_data SEF file data
 * @param sef_len SEF file length
 * @param load_ctx Load context to update
 * @return true if successful, false otherwise
 */
static bool parse_sef_header(const u8 *sef_data, u32 sef_len,
                             GCOSLoadContext *load_ctx) {
    if (sef_len < 20) {
        printf("[LOAD] ERROR: SEF file too short for header\n");
        return false;
    }
    
    // Parse SEF magic number ("sef" = 0x00736566)
    u32 magic = (sef_data[0] << 24) | (sef_data[1] << 16) | 
                (sef_data[2] << 8) | sef_data[3];
    
    if (magic != 0x00736566) {
        printf("[LOAD] ERROR: Invalid SEF magic number: 0x%08X\n", magic);
        return false;
    }
    
    // Parse version (u32)
    load_ctx->package_version = (sef_data[4] << 24) | (sef_data[5] << 16) |
                                (sef_data[6] << 8) | sef_data[7];
    
    printf("[LOAD] SEF Version: 0x%08X\n", load_ctx->package_version);
    
    // Parse section count (u32)
    u32 section_count = (sef_data[8] << 24) | (sef_data[9] << 16) |
                        (sef_data[10] << 8) | sef_data[11];
    
    printf("[LOAD] Section Count: %u\n", section_count);
    
    if (section_count == 0 || section_count > 16) {
        printf("[LOAD] ERROR: Invalid section count: %u\n", section_count);
        return false;
    }
    
    // Parse sections (simplified - full parsing would iterate through all sections)
    u32 offset = 12;
    for (u32 i = 0; i < section_count && offset + 8 <= sef_len; i++) {
        u8 section_id = sef_data[offset++];
        u32 section_size = (sef_data[offset] << 24) | (sef_data[offset+1] << 16) |
                           (sef_data[offset+2] << 8) | sef_data[offset+3];
        offset += 4;
        
        printf("[LOAD] Section %u: ID=0x%02X, Size=%u\n", i, section_id, section_size);
        
        // Skip section content for now
        if (offset + section_size > sef_len) {
            printf("[LOAD] ERROR: Section %u extends beyond file\n", i);
            return false;
        }
        offset += section_size;
    }
    
    return true;
}

/**
 * @brief Parse Import Section and validate dependencies
 * 
 * @param vm VM instance
 * @param import_data Import section data
 * @param import_len Import section length
 * @param load_ctx Load context to update
 * @return true if all imports resolved, false otherwise
 */
static bool parse_import_section(GCOSVM *vm, const u8 *import_data, u32 import_len,
                                 GCOSLoadContext *load_ctx) {
    if (import_len < 3) {
        printf("[LOAD] ERROR: Import section too short\n");
        return false;
    }
    
    u8 import_module_count = import_data[0];
    u16 import_function_count = (import_data[1] << 8) | import_data[2];
    
    printf("[LOAD] Import Modules: %u, Import Functions: %u\n",
           import_module_count, import_function_count);
    
    if (import_module_count > MAX_IMPORTS) {
        printf("[LOAD] ERROR: Too many imports (%u > %u)\n",
               import_module_count, MAX_IMPORTS);
        return false;
    }
    
    load_ctx->import_count = import_module_count;
    
    // Parse each imported module
    u32 offset = 3;
    for (u8 i = 0; i < import_module_count; i++) {
        if (offset + 5 > import_len) {
            printf("[LOAD] ERROR: Import data truncated\n");
            return false;
        }
        
        // Parse module version (u32)
        u32 module_version = (import_data[offset] << 24) | 
                            (import_data[offset+1] << 16) |
                            (import_data[offset+2] << 8) | 
                            import_data[offset+3];
        offset += 4;
        
        // Parse module AID
        u8 aid_len = import_data[offset++];
        
        if (aid_len < 5 || aid_len > 16 || offset + aid_len > import_len) {
            printf("[LOAD] ERROR: Invalid import AID length\n");
            return false;
        }
        
        GCOSAID import_aid;
        import_aid.length = aid_len;
        memcpy(import_aid.aid, &import_data[offset], aid_len);
        offset += aid_len;
        
        printf("[LOAD] Import[%u]: Version=0x%08X, AID=", i, module_version);
        for (int j = 0; j < aid_len; j++) {
            printf("%02X", import_aid.aid[j]);
        }
        printf("\n");
        
        // Store import info
        load_ctx->imports[i].module_version = module_version;
        load_ctx->imports[i].module_aid = import_aid;
        load_ctx->imports[i].resolved = false;
        load_ctx->imports[i].resolved_module_id = 0xFF;
        
        // Check if module exists in system
        for (u8 mid = 0; mid < MAX_MODULES; mid++) {
            if (vm->modules[mid].loaded &&
                vm->modules[mid].module_aid.length == aid_len &&
                memcmp(vm->modules[mid].module_aid.aid, import_aid.aid, aid_len) == 0) {
                
                // Check version compatibility
                if (vm->modules[mid].version >= module_version) {
                    load_ctx->imports[i].resolved = true;
                    load_ctx->imports[i].resolved_module_id = mid;
                    printf("[LOAD]   -> Resolved to Module ID %u (Version 0x%08X)\n",
                           mid, vm->modules[mid].version);
                    break;
                } else {
                    printf("[LOAD]   -> Version mismatch (required 0x%08X, found 0x%08X)\n",
                           module_version, vm->modules[mid].version);
                    return false;
                }
            }
        }
        
        if (!load_ctx->imports[i].resolved) {
            printf("[LOAD] ERROR: Import module not found\n");
            return false;
        }
    }
    
    return true;
}

/**
 * @brief Handle FINALIZE command (P1=0x02)
 * 
 * Parses the complete SEF file, validates imports, links functions,
 * and creates the module.
 * 
 * @param apdu APDU data
 * @param apdu_len APDU length
 * @param response Response buffer
 * @param resp_len Response length output
 * @return Status word
 */
u16 handle_finalize_load(const u8 *apdu, u16 apdu_len,
                         u8 *response, u16 *resp_len) {
    GCOSVM *vm = gcos_vm_get_instance();
    
    if (vm == NULL) {
        return 0x6F00;
    }
    
    printf("[LOAD] === FINALIZE LOAD ===\n");
    
    // Verify load session is active
    if (vm->load_context.state != LOAD_STATE_LOADING_BLOCKS) {
        printf("[LOAD] ERROR: No active load session\n");
        return 0x6985;
    }
    
    // Verify we have data
    if (vm->load_context.buffer_size == 0) {
        printf("[LOAD] ERROR: No data loaded\n");
        reset_load_context(vm);
        return 0x6A80;
    }
    
    const u8 *sef_data = vm->load_context.buffer;
    u32 sef_len = vm->load_context.buffer_size;
    
    printf("[LOAD] Total SEF size: %u bytes\n", sef_len);
    
    // Step 1: Parse SEF header
    printf("[LOAD] Step 1: Parsing SEF header...\n");
    if (!parse_sef_header(sef_data, sef_len, &vm->load_context)) {
        printf("[LOAD] ERROR: SEF header parsing failed\n");
        reset_load_context(vm);
        return 0x6A80;
    }
    
    // Step 2: Parse and validate imports
    printf("[LOAD] Step 2: Validating imports...\n");
    // Note: In real implementation, we would locate the import section
    // For now, assume imports are at a fixed offset (this needs proper SEF parsing)
    
    // Simplified: skip to import section (assuming it's the second section)
    // This is a placeholder - full implementation needs proper section navigation
    u32 import_offset = 20;  // Skip header
    if (import_offset + 3 <= sef_len) {
        u32 import_len = sef_len - import_offset;
        if (!parse_import_section(vm, &sef_data[import_offset], import_len, 
                                  &vm->load_context)) {
            printf("[LOAD] ERROR: Import validation failed\n");
            reset_load_context(vm);
            return 0x6A88;  // SW_REFERENCED_DATA_NOT_FOUND
        }
    }
    
    // Step 3: Create module instance
    printf("[LOAD] Step 3: Creating module...\n");
    u8 module_id = vm->load_context.target_module_id;
    GCOSModule *module = &vm->modules[module_id];
    
    // Initialize module fields
    memset(module, 0, sizeof(GCOSModule));
    
    module->module_id = module_id;
    module->module_aid = vm->load_context.package_aid;
    module->version = vm->load_context.package_version;
    module->type = 0x00;  // Regular application module
    module->state = MODULE_LOADED;
    module->security_domain_id = vm->load_context.sd_id;
    module->loaded = true;
    module->initialized = false;
    
    // Copy code/data to module (simplified - real impl would parse sections)
    module->code = module->domain_data;  // Use domain_data as code storage for now
    module->code_size = sef_len;
    
    // Copy imports
    module->import_count = vm->load_context.import_count;
    for (u8 i = 0; i < module->import_count; i++) {
        module->imports[i] = vm->load_context.imports[i];
    }
    
    // Increment module count
    vm->module_count++;
    
    printf("[LOAD] Module created successfully:\n");
    printf("  Module ID: %u\n", module_id);
    printf("  AID: ");
    for (int i = 0; i < module->module_aid.length; i++) {
        printf("%02X", module->module_aid.aid[i]);
    }
    printf("\n");
    printf("  Version: 0x%08X\n", module->version);
    printf("  Imports: %u\n", module->import_count);
    printf("  Code Size: %u bytes\n", module->code_size);
    
    // Reset load context
    reset_load_context(vm);
    
    if (resp_len) {
        *resp_len = 0;
    }
    
    return 0x9000;  // SW_SUCCESS
}

/* ============================================================================
 * Main LOAD Command Dispatcher
 * ============================================================================ */

/**
 * @brief Main LOAD command handler (INS=0xE4)
 * 
 * Dispatches to appropriate phase handler based on P1.
 * 
 * @param app ISD application instance
 * @param apdu APDU data
 * @param apdu_len APDU length
 * @param response Response buffer
 * @param resp_len Response length output
 * @return Status word
 */
u16 isd_handler_load(GCOSAppInstance *app,
                     const u8 *apdu,
                     u16 apdu_len,
                     u8 *response,
                     u16 *resp_len) {
    u8 p1 = apdu[2];  // P1: Sub-command
    
    printf("[LOAD] LOAD command: P1=0x%02X\n", p1);
    
    switch (p1) {
        case 0x00:  // INSTALL FOR LOAD
            return handle_install_for_load(apdu, apdu_len, response, resp_len);
        
        case 0x01:  // LOAD BLOCKS
            return handle_load_blocks(apdu, apdu_len, response, resp_len);
        
        case 0x02:  // FINALIZE
            return handle_finalize_load(apdu, apdu_len, response, resp_len);
        
        default:
            printf("[LOAD] ERROR: Invalid P1=0x%02X\n", p1);
            return 0x6A86;  // SW_INCORRECT_P1P2
    }
}
