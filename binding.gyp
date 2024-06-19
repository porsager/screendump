{
  "targets": [
    {
      "target_name": "screendump",
      "include_dirs": [
        "<!(node -e \"require('nan')\")"
      ],
      "conditions": [
        ["OS=='linux'", {
          "sources": [ "linux.cc" ],
          "libraries": [
            "-lX11",
            "-lXinerama",
            "-ljpeg"
          ]
        }],
        ["OS=='win'", {
          "sources": [ "win.cc" ],
          "libraries": [
            "-lgdiplus",
            "-luser32",
            "-lgdi32",
            "-lShcore"
          ]
        }],
        ["OS=='mac'", {
          "sources": [ "mac.cc" ],
          "libraries": [
            "-framework CoreGraphics",
            "-framework ImageIO"
          ]
        }]
      ]
    }
  ]
}
