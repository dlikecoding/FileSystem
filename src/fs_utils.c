#include "structs/fs_utils.h"

// Calculate number of blocks needed
int computeBlockNeeded(int m, int n) {
    return (m + (n - 1)) /n;
}
