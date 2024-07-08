#pragma once
#include "kernel/utils.h"
#include "kernel/types.h"
#include "kernel/page_tables.h"

// NOTE: these are chosen to fit in the first PDPT
//       so that sharing the kernel mappings with 
//       user process page tables is easier
const u64 MAX_KERNEL_PSPACE_SIZE = 32*GB;
const u64 KERNEL_VSPACE_START = MAX_KERNEL_PSPACE_SIZE;
const u64 KERNEL_VSPACE_SIZE = 32*GB;

// NOTE: user vspace starts on a different PDPT than
//       kernelspace to make sharing kernelspace with
//       user process page tables easier
const u64 USER_VSPACE_START = bytes_mapped_by_pdpt;
const u64 USER_VSPACE_SIZE = 32*GB;

static_assert(KERNEL_VSPACE_START + KERNEL_VSPACE_SIZE <= USER_VSPACE_START);
static_assert(KERNEL_VSPACE_START + KERNEL_VSPACE_SIZE <= bytes_mapped_by_pdpt);
static_assert(USER_VSPACE_START / bytes_mapped_by_pdpt > 0);
