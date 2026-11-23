# VGM Player for GameShell

一个可在 ClockworkPi GameShell（armv7l / Debian 9）上运行的 VGM/VGZ 音乐播放器。
播放内核移植自 [ValleyBell/libvgm](https://github.com/ValleyBell/libvgm)，前端用 SDL 1.2
实现 320×240 全屏界面，可显示 VGM 文件内的 GD3 元数据（曲名、游戏、系统、作者、日期、备注、音芯片）。
<img width="1080" height="1919" alt="6e5267482328be2667add5f50c55d1af" src="https://github.com/user-attachments/assets/3fe45ca5-04fe-4671-b2eb-2d25bfb8d014" />
<img width="1047" height="852" alt="image" src="https://github.com/user-attachments/assets/f1d80f65-d541-4f89-bf0c-a13c5cbfc8a9" />

支持：

- 顺序播放指定目录中的 `.vgm` / `.vgz`（gzip 压缩 VGM）文件，播完自动下一首并循环
- 文件系统浏览器：START 进入，SELECT 选目录，选中后自动写入配置文件
- 多个已保存目录之间用 Y/A 键前后切换
- GD3 元数据 + 进度条 + 时间显示
- Shift+ABXY 控制，与 GameShell 其他模拟器的映射习惯一致

---

## 1. 目录内容

| 文件 | 用途 |
|---|---|
| `vgm_player` | **已编译好的 ARMv7 可执行文件**（从实机取回，可直接部署，免编译） |
| `vgm_player.cpp` | 播放器前端全部源码（SDL 界面、文件浏览器、目录管理、按键处理） |
| `Makefile` | 编译脚本：直接从 libvgm 源码编译内核并链接前端（无需 cmake） |
| `libvgm.tar.gz` | libvgm 播放内核源码快照（ValleyBell/libvgm master） |
| `42_VGM Player.sh` | GameShell 启动器菜单项脚本 |
| `VGM Player.png` | 80×80 菜单图标 |
| `make_icon.py` | 生成菜单图标的脚本（PC 上运行，需 Pillow） |
| `inject_key.py` | 调试用：通过 XTest 注入按键（仅测试时需要，设备需 python2-xlib） |
| `LICENSE` | 本项目自有代码的许可证全文（GPL-2.0） |
| `THIRD_PARTY_LICENSES.md` | 第三方组件（libvgm 内核、SDL、字体等）的许可声明 |

---

## 2. 环境与依赖

目标设备：ClockworkPi GameShell，`armv7l`，Debian 9（buster），GLIBC 2.28，屏幕 320×240。

设备上需已安装（GameShell 默认基本都带）：

```bash
sudo apt-get install build-essential gcc g++ make \
    libsdl1.2-dev libsdl-ttf2.0-dev zlib1g-dev
```

字体依赖（GameShell 默认已有）：

- `/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf` —— 拉丁字符
- `/usr/share/fonts/truetype/droid/DroidSansFallbackFull.ttf` —— 中文/日文

> 前端对每个字符做字体回退（latin/cjk 双字体绘制），因此 GD3 里任意语言的曲名都能正常显示，
> 不会出现豆腐块。若字体路径不同，修改 `vgm_player.cpp` 顶部的 `FONT_PATH` / `FONT_CJK_PATH`。

**为什么不用 cmake：** 设备上 apt 装 cmake 会连带升级 curl/feh 并移除 libcurl3，风险大；
而 libvgm 自带的纯 Makefile 已过时。因此本包提供了专用 Makefile，
直接把 libvgm 的 `emu/`、`utils/`、`player/` 源文件编译进来，只依赖 g++/gcc/make。

---

## 3. 编译（在 GameShell 上操作）

> **懒人通道：本包已附带实机编译好的 `vgm_player`（ARMv7 / 32-bit ELF，MD5
> `5f9a10e25631e32e24dfe9877da9e7af`）。若设备环境与当初一致，可直接跳到第 4 节安装，
> 无需编译。只有想改源码或环境不同（如换内核版本）时才需要重新编译。**
> 该二进制为 ARM 架构，不能在 x86 PC 上运行，只能放到 GameShell 上。

假设已把本目录内容放到设备上的某个路径（下面以 `/home/cpi/vgm4gameshell` 为例，
也可用 U 盘/scp 拷贝）。

### 3.1 准备 libvgm 源码

Makefile 默认从 `/tmp/libvgm` 读取 libvgm 源码：

```bash
cd /home/cpi/vgm4gameshell
tar xzf libvgm.tar.gz
# 解压出来是 libvgm-master，重命名到 Makefile 期望的位置
mv libvgm-master /tmp/libvgm
```

如不想用 `/tmp/libvgm`，可自行指定：

```bash
tar xzf libvgm.tar.gz
make ROOT=$PWD/libvgm-master
```

### 3.2 编译

```bash
cd /home/cpi/vgm4gameshell
make -j2
```

成功后当前目录生成可执行文件 `vgm_player`（约 650 KB）。
如需清理重编：`make clean && make -j2`。

> Makefile 已开启 libvgm 的全部声音核心（YM2612、SN76489、AY8910、OPL、YM2610、NES APU 等），
> 链接 `-lSDL -lSDL_ttf -lz -lm -lpthread`。编译约 1～2 分钟。

---

## 4. 安装到 GameShell

### 4.1 安装程序与配置

```bash
mkdir -p /home/cpi/games/VGMPlayer
cp vgm_player /home/cpi/games/VGMPlayer/     # 用本包自带的预编译二进制，或自己编译的产物
chmod 755 /home/cpi/games/VGMPlayer/vgm_player

# 默认播放目录（每行一个目录；浏览器选目录会自动追加到这里）
printf '/home/cpi/games/DOSBOX/MODM/vgm\n' > /home/cpi/games/VGMPlayer/vgm_dir.txt
printf '0\n' > /home/cpi/games/VGMPlayer/vgm_dir_index.txt
```

### 4.2 安装菜单项与图标

```bash
cp '42_VGM Player.sh' /home/cpi/launcher/Menu/GameShell/
chmod 755 '/home/cpi/launcher/Menu/GameShell/42_VGM Player.sh'

cp 'VGM Player.png' /home/cpi/launcher/skin/default/Menu/GameShell/
```

在启动器里执行一次 **Reload UI**（或重启设备），菜单中即可看到「VGM Player」入口，
点击即可启动。

> 菜单脚本里写死了程序路径 `/home/cpi/games/VGMPlayer`，若改了安装位置需同步修改脚本。

---

## 5. 使用方法

### 5.1 按键

播放器内（GameShell 默认键位：`START=Return`、`SELECT=Space`、`MENU=Esc`）：

| 按键 | 功能 |
|---|---|
| Shift + X | 上一首 |
| Shift + B | 下一首 |
| Shift + Y | 播放 / 暂停 |
| Shift + A | 详细信息 / 简洁视图切换 |
| Y | 上一个保存目录 |
| A | 下一个保存目录 |
| START | 打开文件浏览器 |
| MENU | 退出播放器 |

未换挡的 A/B/X/Y 以及十字键左右也是上一首/下一首等操作的别名。

文件浏览器内：

| 按键 | 功能 |
|---|---|
| 十字键 上/下 | 移动选择 |
| 十字键 左/右（或 A/B） | 进入子目录 / 返回上级 |
| SELECT | 用当前选中的目录开始播放，并自动保存到 `vgm_dir.txt` |
| MENU | 退出浏览器（不改变当前播放目录） |

浏览器中目录项右侧会显示该目录内 `.vgm/.vgz` 的数量，方便快速找到曲库；隐藏以 `.` 开头的文件。

### 5.2 播放目录配置

`vgm_dir.txt` 每行一个目录，可预先手写多行，也可用浏览器逐条追加：

```
/home/cpi/games/DOSBOX/MODM/vgm
/home/cpi/music/vgm/Kirby
/home/cpi/music/vgm/Contra
```

- 浏览器用 SELECT 选中的目录若**成功播放**，会自动追加到该文件（去重）并设为当前目录。
- `vgm_dir_index.txt` 记录当前目录索引（从 0 开始），Y/A 切换时自动更新，重启后保持。
- 界面顶部 `D2/3` 表示「第 2 个目录 / 共 3 个目录」。

### 5.3 显示信息

标题、游戏名、系统、作曲者、日期、备注均来自 VGM 内的 GD3 标签；另外显示音芯片、文件名、
播放进度条与时间。无 GD3 时回退显示文件名。

---

## 6. 技术说明

- **播放内核**：libvgm 的 `PlayerA` + `VGMPlayer`，通过 `Render()` 直接输出 PCM，
  由 SDL 音频回调播放，采样率 44100 Hz / 16 bit / 立体声。
- **VGM/VGZ**：libvgm 的 `FileLoader` 自动识别 gzip 头，`.vgz` 无需预先解压。
- **GD3 编码**：libvgm 内部转换为 UTF-8（iconv）。
- **字体回退**：SDL_ttf 1.x 不支持 fallback，前端按 UTF-8 逐字符判断码点，
  `>= 0x2E80` 用 Droid CJK 字体，其余用 DejaVu，解决 CJK 字体缺拉丁字形的问题。
- **目录统计**：浏览器进入目录时对该层子目录做一次浅扫描统计 vgm 数（最多统计 40 个目录，
  避免 `/proc` 这类大目录卡顿）。
- **按键映射来源**：GameShell 启动器 `keys_def.py` 定义 `START=K_RETURN`、`SELECT=K_SPACE`、
  `MENU=K_ESCAPE`；注入键码参考：Return=36, Space=65, Esc=9, i(Y)=31, j(A)=44,
  y=29, u=30, h=43, l=46, o=32, 方向键 Up=111/Down=116/Left=113/Right=114。

---

## 7. 故障排查

- **界面文字显示成方框**：检查两个字体路径是否存在；或确认用的是本包的前端（含字体回退）。
- **没有声音**：确认没有其他程序占用 ALSA（`fuser -v /dev/snd/pcmC0D0p`），
  并检查 SDL_OpenAudio 是否在日志里报错。
- **提示 No playable VGM file**：该目录下文件损坏或芯片不受支持；
  Makefile 已开启全部核心，一般不会出现。
- **菜单里看不到 VGM Player**：确认文件名与图标名完全一致（含空格），
  执行 Reload UI 或重启启动器。
- **编译报找不到头文件**：检查 libvgm 是否解压到 `/tmp/libvgm`，
  或用 `make ROOT=<实际路径>` 指定。

---

## 8. 来源与许可

**本项目自有代码采用 GPL-2.0-or-later（GPLv2+）**，见 `LICENSE` 全文。
所有自有源码文件头部均带 `SPDX-License-Identifier: GPL-2.0-or-later` 标记。

选择 GPLv2+ 而非 MIT/Apache 或 GPLv3 的原因：

- 播放内核 libvgm 把多个芯片仿真核心**静态链接**进同一个可执行文件，其中
  `ymf278b.c`（openMSX 的 YMF278B/OPL4 核心）是 **GPL-2.0-only**，
  因此整个二进制的分发必须满足 GPL-2.0；GPLv3 与之不兼容。
- LGPL-2.1+（DOSBox OPL）、BSD-3-Clause（MAME 核心）、MIT 等其余组件都与 GPLv2 兼容。
- 采用 GPL-2.0-**or-later** 可在满足兼容性的同时，为自有代码保留“或更新版本”的灵活性。

> 注意：libvgm 上游仓库本身**未声明许可证**（GitHub `license: null`，无 LICENSE 文件）。
> 详细逐组件许可清单、约束说明与合规提示见 `THIRD_PARTY_LICENSES.md`。

- libvgm 项目主页：https://github.com/ValleyBell/libvgm
- 若需更宽松的前端许可（如 MIT），须改用裁剪版 Makefile 剔除全部 GPL 核心，详见第三方声明文件。
