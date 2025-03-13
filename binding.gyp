{
  "targets": [
    {
      "target_name": "iohook",
      "sources": [
        "src/lib/addon.c",
        "src/lib/napi_helpers.c",
        "src/lib/uiohook_worker.c",
        "src/libuiohook/src/logger.c"
      ],
      "include_dirs": [
        "src/libuiohook/include",
        "src/libuiohook/src"
      ],
      "conditions": [
        ["OS=='win'", {
          "sources": [
            "src/libuiohook/src/windows/input_helper.c",
            "src/libuiohook/src/windows/input_hook.c",
            "src/libuiohook/src/windows/post_event.c",
            "src/libuiohook/src/windows/system_properties.c"
          ],
          "msvs_settings": {
            "VCCLCompilerTool": {
              "AdditionalOptions": ["/utf-8"]
            }
          },
          "defines": [
            "_WIN32_WINNT=0x0601"
          ]
        }],
        ["OS=='linux'", {
          "sources": [
            "src/libuiohook/src/x11/input_helper.c",
            "src/libuiohook/src/x11/input_hook.c",
            "src/libuiohook/src/x11/post_event.c",
            "src/libuiohook/src/x11/system_properties.c"
          ],
          "libraries": [
            "-lX11",
            "-lXtst",
            "-lXt",
            "-lXinerama",
            "-lX11-xcb",
            "-lxcb",
            "-lxcb-xkb"
          ]
        }],
        ["OS=='mac'", {
          "sources": [
            "src/libuiohook/src/darwin/input_helper.c",
            "src/libuiohook/src/darwin/input_hook.c",
            "src/libuiohook/src/darwin/post_event.c",
            "src/libuiohook/src/darwin/system_properties.c"
          ],
          "link_settings": {
            "libraries": [
              "-framework ApplicationServices",
              "-framework Carbon",
              "-framework IOKit",
              "-framework CoreFoundation"
            ]
          },
          "xcode_settings": {
            "OTHER_CFLAGS": [
              "-Wno-deprecated-declarations"
            ]
          }
        }]
      ]
    }
  ]
} 