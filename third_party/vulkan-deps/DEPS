# This file is used to manage Vulkan dependencies for several repos. It is
# used by gclient to determine what version of each dependency to check out, and
# where.

# Avoids the need for a custom root variable.
use_relative_paths = True

vars = {
  'chromium_git': 'https://chromium.googlesource.com',

  # Current revision of glslang, the Khronos SPIRV compiler.
  'glslang_revision': 'd433cccb8acb76514cf5b70a3e1d9247a21288aa',

  # Current revision of spirv-cross, the Khronos SPIRV cross compiler.
  'spirv_cross_revision': '1964799fba06abcbad2ff684cba360f0067c34a2',

  # Current revision fo the SPIRV-Headers Vulkan support library.
  'spirv_headers_revision': 'e7b49d7fb59808a650618e0a4008d4bae927e112',

  # Current revision of SPIRV-Tools for Vulkan.
  'spirv_tools_revision': '983ee2313c6818d72dc0692b6a7824ca080357fc',

  # Current revision of Khronos Vulkan-Headers.
  'vulkan_headers_revision': 'b8c57b0a09f7324fec5a7c363f5e26ff4d5a3222',

  # Current revision of Khronos Vulkan-Loader.
  'vulkan_loader_revision': '99a7440204c24efab393ae7f67da7caa4179a2a9',

  # Current revision of Khronos Vulkan-Tools.
  'vulkan_tools_revision': 'ba8c4116410ddc8c90d44c3708435f098ef2b2f1',

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
