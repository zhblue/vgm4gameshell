# 第三方组件与许可声明

本项目自有代码（前端 `vgm_player.cpp`、`Makefile`、脚本、图标生成脚本）采用
**GPL-2.0-or-later**（见 `LICENSE`）。下面列出打包进来的第三方组件的许可情况，
以便合规分发。

## 1. libvgm（播放内核）

- 来源：https://github.com/ValleyBell/libvgm （本包内为 `libvgm.tar.gz` 快照）
- **仓库本身未声明许可证**：GitHub API 中 `license` 字段为 `null`，源码根目录也没有
  LICENSE / COPYING 文件。因此其“默认版权”状态由作者保留。
- libvgm 由 VGMPlay 的组件模块化重写而来，内部**汇集了大量第三方芯片仿真核心**，
  每个核心文件头部带有自己的 `// license:` 标记。

被本 Makefile 编译进二进制的核心，其许可分布（以实际源码标签统计）：

| 许可 | 文件数量 | 说明 |
|---|---|---|
| BSD-3-Clause | 32 | MAME 衍生芯片核心（AY8910、ES5503、C6280 等），多数版权归 MAMEdev 贡献者 |
| GPL-2.0-or-later | 8 | fmopl.c / fmopn.c / nukedopl3.c / nukedopll.c / nukedopm.c / ym2151.c / ym2413.c / ymf262.c 等 |
| LGPL-2.1-or-later | 3 | DOSBox Team 的 AdLib/OPL 仿真（adlibemu_opl_inc.*） |
| MIT | 2 | emu2413.c、emu2149.c 等 |
| **GPL-2.0（仅）** | **1** | `emu/cores/ymf278b.c`，移植自 openMSX 的 YMF278B（OPL4）核心 |

### 对本项目的约束

- 本 Makefile 用 `SNDDEV_SELECT` 把**全部核心静态链接进同一个 `vgm_player` 可执行文件**。
- 其中 `ymf278b.c` 为 **GPL-2.0-only**，因此**整个二进制的再分发必须满足 GPL-2.0**；
  **不能**改用 GPL-3.0（与 GPL-2.0-only 不兼容）。
- 这就是本项目选择 **GPL-2.0-or-later** 作为总许可的原因：它与被链接代码兼容，
  同时给自有代码保留“或更新版本”的灵活性。
- 若确实需要更宽松（如 MIT）的前端许可，唯一合规做法是**裁剪 Makefile**，
  只编译 BSD/MIT/LGPL 核心，剔除全部 GPL 核心（需删除 `SNDDEV_YMF262`、
  `SNDDEV_YMF278B`、`SNDDEV_YM2413`、`SNDDEV_YM2151`、`SNDDEV_YM2612` 等相关宏），
  并另行处理 LGPL 静态链接的附带条件。本项目未采用该方案。

## 2. SDL 1.2 / SDL_ttf

- 动态链接（-lSDL -lSDL_ttf），这两个库为 **LGPL-2.1-or-later**。
- LGPL 允许与 GPL 项目链接，合规；系统库由 Debian 提供，未随包分发。

## 3. zlib

- `libz` 动态链接（-lz），使用 **zlib License**（宽松许可），与 GPL 兼容。

## 4. 字体

- `DejaVuSans.ttf`：Bitstream Vera 衍生，**宽松字体许可**，非 GPL。
- `DroidSansFallbackFull.ttf`：Android/Droid 字体，**Apache License 2.0**。
- 字体由系统提供，未随包分发，仅作显示用途，不构成衍生作品链接问题。

---

**免责声明**：以上为基于源码头部标签的整理，不构成法律意见。
如需在任何公开发布场景分发，请自行核实各上游组件的最新许可条款；
尤其注意 libvgm 上游未显式声明许可证这一点。
