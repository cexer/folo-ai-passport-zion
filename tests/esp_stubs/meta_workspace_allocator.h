#pragma once
#include <stddef.h>
void *meta_workspace_test_calloc(size_t count,size_t size);
#define META_CARVE_WORKSPACE_CALLOC meta_workspace_test_calloc
