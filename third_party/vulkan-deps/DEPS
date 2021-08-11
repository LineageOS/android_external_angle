# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': 'aa2d4bd2f48184974961dbbbae3f29d967c45803',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': 'bab4e5911b1bfa5a86bc80006b7301ae48363844',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': 'e7b49d7fb59808a650618e0a4008d4bae927e112',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': 'de69f32e8962ecc4e604bfc125da41d00a7a1ca8',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': '521f91d793e1799f0af57e013fa7e799afa1824c',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': 'fbb56aacceef9211d1742a21e9ca40850519fe74',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': '415320f80f74890561df8e52deb0024ecf1cadca',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': '2fe826b04cc4c0ffbba2ef9b0522bd97a8445109',
}

deps = {
  'glslang/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/glslang@{glslang_revision}',
  },

  'spirv-cross/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Cross@{spirv_cross_revision}',
  },

  'spirv-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Headers@{spirv_headers_revision}',
  },

  'spirv-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/SPIRV-Tools@{spirv_tools_revision}',
  },

  'vulkan-headers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Headers@{vulkan_headers_revision}',
  },

  'vulkan-loader/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Loader@{vulkan_loader_revision}',
  },

  'vulkan-tools/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-Tools@{vulkan_tools_revision}',
  },

  'vulkan-validation-layers/src': {
    'url': '{chromium_git}/external/github.com/KhronosGroup/Vulkan-ValidationLayers@{vulkan_validation_revision}',
  },
}
