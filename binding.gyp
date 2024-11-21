{
  "targets": [
    {
      "target_name": "addon",
      "sources": [ "src/addon.cpp" ],
      "include_dirs": [
        "<!@(node -p \"require('node-addon-api').include\")",
        "./local/include",
        "./local/include/librdb"
      ],
      "libraries": [
        "-L<!(pwd)/local/lib",
        "-lrdb",
        "-lrdb-ext"
      ],
      "link_settings": {
        "library_dirs": [
          "<!(pwd)/local/lib"
        ]
      },
      "dependencies": [
        "<!(node -p \"require('node-addon-api').gyp\")"
      ],
      "cflags": [
        "-std=c++17",
        "-fPIC"
      ],
      "cflags_cc": [
        "-std=c++17",
        "-fPIC"
      ],
      "cflags!": [ "-fno-exceptions" ],
      "cflags_cc!": [ "-fno-exceptions" ],
      "defines": [
        "NAPI_DISABLE_CPP_EXCEPTIONS",
        "NAPI_VERSION=8"
      ]
    }
  ]
}