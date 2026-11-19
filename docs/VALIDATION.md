# Windows 回归与 macOS 验证范围

本次在 Windows x64 运行 Release 构建、内容校验和 CTest。macOS 源码入口和 dylib 加载分支的编译与实机运行未验证。

```powershell
.\build.ps1 -SkipRun
.\tools\validate_content.ps1
ctest --test-dir build -C Release --output-on-failure
.\build\bin\orbduel.exe --selftest
.\build\bin\orbduel.exe --headless --scene content/studio/custom-abilities.json --seconds 60
```

- `abilities` 检查真实 DLL 的回溯恢复、冷却、致死边界、实例隔离，以及护卫跟随、拦截、护盾、补充与清理。
- `mod_workflow` 从 PowerShell 生成独立工程，经 Windows 一键入口构建安装，检查运行中重装、重新加载、已有参数和失败时保留安装，并在同一对局运行独立安装的分身与护卫。
- `portable_workflow` 在 Windows 上通过 CMake 生成、构建和安装五种独立工程，检查同 ID 用户覆盖、禁用与恢复内置模组，并运行旋风、召唤、作者模板、回溯和护卫。
- `entities`、`studio`、`integration`、`contact` 与 `balance_smoke` 覆盖战斗实体、地图和既有战斗规则。
- `font`、`ui_skin` 覆盖上游中文字体和工作台数据皮肤。
