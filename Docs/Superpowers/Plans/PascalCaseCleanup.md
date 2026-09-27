# PascalCase 整理实施计划

> **历史执行说明：** 原计划要求执行者使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans，逐项勾选（`- [ ]`）。

**目标：** 清理生成物和过时文件，只保留一个 `build` 目录，将项目源码、测试、文档及目录名迁移到 PascalCase，同时保持 CMake、include 与测试可用。

**方案：** 保留当时的 `wan` 库架构与用户改动；仅重命名 `build` 外的项目路径，同步路径引用，并保留工具发现机制要求的配置文件名。

**技术栈：** CMake 3.23+、C++23、Boost.Asio/Beast/JSON、BoringSSL、jsoncpp、Crypto++、PowerShell、CTest。

**原始要求：** 清理 clangd 等杂项，将项目文件与目录改为 PascalCase，只保留一个构建目录并同步必要引用。

## 全局约束

- 保留当时全部已跟踪与未跟踪源码改动；不执行 reset、clean、checkout、amend，也不创建 worktree。
- 仅保留名为 `build` 的构建目录，不重命名或编辑其中第三方依赖。
- 确认准确路径后，才删除 `.cache`、`build_test`、`out`、无关 `.qwen` 设置及空的过时源码目录。
- 将项目源码、测试、文档与目录路径改为 PascalCase；固定发现文件名 `CMakeLists.txt`、`CMakePresets.json`、`CMakeUserPresets.json`、`.vscode/*` 和 `.claude/settings.local.json` 不变。
- 重命名后同步 CMake、C++ include、文档链接与源码引用；依赖允许时重新配置、构建并运行全部 CTest。

---

### 任务 1：基线与清理目标

**文件：** 删除 `.cache/`、`build_test/`、`out/`、`.qwen/`；仅当 `model/`、`src/`、`test/` 为空时删除；保留 `build/` 及其依赖内容。

**输入与产出：** 依据当时的 Git 工作树和目录清单，产出单一构建目录，清除重复的 clangd/缓存/构建目录。

- [ ] **步骤 1：复核删除目标及其内容**

运行 PowerShell 清单命令，确认目标位于 `I:\code\Custom-libraries` 内。

- [ ] **步骤 2：仅删除已确认的生成物或过时目标**

使用解析后的明确路径和 `Remove-Item -LiteralPath`，不使用宽泛通配符。

- [ ] **步骤 3：验证清理结果**

运行 `Get-ChildItem -Force`，确认保留 `build/` 且删除目标已消失。

### 任务 2：PascalCase 路径迁移

**文件：** 将当时 `include`、`wan`、`tests`、`docs`、`examples` 下的项目目录及 `.hpp`、`.cpp`、`.md` 文件改为 PascalCase；若工作流仍存在，将 `.github/workflows/ci.yml` 改为 `.github/workflows/CI.yml`；保留固定发现的 CMake 与工具配置文件名。

**输入与产出：** 在清理后的树上形成 `Include/Wan.hpp`、`Wan/Web/Server/Server.hpp`、`Tests/TestWeb.cpp`、`Docs/Container/Overview.md` 等路径。

- [ ] **步骤 1：生成确定的旧新路径映射**

根据当时目录建立明确映射，拒绝冲突，排除 `.git`、`build`、工具目录和固定 CMake/配置名。

- [ ] **步骤 2：从最深层路径开始重命名**

先移动文件再移动父目录，避免 Windows 大小写不敏感行为掩盖冲突。

- [ ] **步骤 3：核验新路径清单**

确认项目源码、测试与文档路径均为 PascalCase，旧路径均已消失。

### 任务 3：同步引用与文档

**文件：** 修改 `CMakeLists.txt`、迁移后的 `Tests/CMakeLists.txt` 与 `Examples/CMakeLists.txt`、包含项目 include 路径的 C++ 文件、包含仓库路径或链接的 Markdown，以及说明当时 `Wan`/C++23/FetchContent 布局的 `README.md`。

**输入与产出：** 使用最终路径映射，产出可编译的 include 与有效文档链接，不残留过时 `model/` 或小写 `wan/`、`tests/`、`docs/`、`include/` 仓库路径。

- [ ] **步骤 1：按映射改写路径引用**

仅替换仓库路径，不修改 `boost/json/src.hpp` 等外部路径。

- [ ] **步骤 2：更新 README 和构建注释**

记录 C++23、当时的 `Wan` 模块、接口目标及单构建目录工作流。

- [ ] **步骤 3：搜索过时仓库引用**

运行 `rg` 并排除 `build/` 与工具目录；剩余小写路径须属于外部路径、命名空间或已记录的固定名称例外。

### 任务 4：构建与测试验证

**文件：** 不计划修改源码。

**输入与产出：** 基于清理、重命名并同步引用的仓库，产出已配置的 `build/`、成功构建及通过的 CTest 结果。

- [ ] **步骤 1：配置保留的构建目录**

运行 `cmake --preset default`。

- [ ] **步骤 2：构建所有已配置目标**

运行 `cmake --build build --parallel`。

- [ ] **步骤 3：运行全部测试**

运行 `ctest --test-dir build --output-on-failure`，要求全部已注册测试通过。

- [ ] **步骤 4：最终工作树审查**

运行 `git status --short --branch`，确认不残留禁止的构建/缓存目录，并将保留的既有改动与本次清理分开报告。
