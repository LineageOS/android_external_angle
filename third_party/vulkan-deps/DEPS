# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': '1978c7692f3682d2f806e8c2879be24193e9fc0f',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': '1964799fba06abcbad2ff684cba360f0067c34a2',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': 'cf653e4ca4858583802b0d1656bc934edff6bd7f',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': '11cd875ed88484f93943071083b4821b4c3d2193',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': 'b8c57b0a09f7324fec5a7c363f5e26ff4d5a3222',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': '99a7440204c24efab393ae7f67da7caa4179a2a9',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': 'a4ee3cacbb1c7a5104a6895890bbddcb0762fa80',

  # Current revision of Khronos Vulkan-ValidationLayers.
  'vulkan_validation_revision': '8fb97f0282bc0e163325dfe75d640e3e077e4c90',
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
