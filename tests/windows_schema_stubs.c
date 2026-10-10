/* SPDX-License-Identifier: MIT
 * ONLY for the schema-validation test executable on non-Windows hosts.
 * These functions are NEVER linked into inv. They do not simulate WinAPI or
 * return invented hardware values; only descriptor addresses are needed.
 */
#include "internal.h"
#define STUB(name,index_) \
 inv_status inv_##name##_read(inv_read_ctx*c,inv_instance*i,inv_value*v,uint64_t*w,uint32_t*q) \
 {(void)c;(void)i;(void)v;(void)w;(void)q;return INV_UNSUPPORTED;} \
 const inv_provider_desc inv_##name##_provider={index_,"schema_only." #name,"Descriptor-only validation",NULL,NULL,NULL,NULL};
STUB(os,0) STUB(memory,1) STUB(cpu,2) STUB(dmi,3) STUB(network,4)
STUB(hwmon,5) STUB(cpuid,6) STUB(filesystem,7) STUB(process,8)
