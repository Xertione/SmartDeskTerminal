# extra_script.py - 强制链接器使用 hard float ABI（ADR-012）
# build_flags 的 -mfloat-abi=hard 只传给编译器，链接器需要单独传
Import("env")

env.Append(
    LINKFLAGS=[
        "-mfloat-abi=hard",
        "-mfpu=fpv4-sp-d16",
    ]
)
