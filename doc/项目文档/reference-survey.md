# 同类项目调研：Agent 桌面终端生态（9 个项目）

> 维护语义：**一次性调研**。结论若被采纳，应并入 `decision-log.md` 的 ADR，然后删除本文件。
> 数据来源：GitHub API（star/fork/release/contributor/语言构成）+ 各项目 README 原文。
> 采集时间：2026-09-25。

---

## 0. 一句话结论

**SmartDeskTerminal 的架构方向与整个生态一致，没有走错路。** 所有 9 个项目（除纯 PC 外设类）都是同一个四段式：
`Agent CLI → hooks → 主机 daemon/bridge → 串口或网络 → MCU 渲染`。
差距不在架构，而在**状态模型、断连健壮性、主机端工程化**这三层——那是 SmartDesk 目前的空白区。

---

## 1. 九项目速览

| 项目 | 形态 | MCU / 屏 | GUI | 主机端 | 链路 / 协议 | ★ | 最近推送 | 许可 |
|---|---|---|---|---|---|---|---|---|
| **puritysb/AgentDeck** | 全家桶（PC+手机+多硬件） | ESP32-S3 / P4、e-ink、LED 点阵 | LVGL 9 + 自绘 | pnpm monorepo TS daemon | WiFi WS + USB + BLE，mDNS 发现 | **247** | 09-25（当日） | MIT |
| **YizhengWw/HachimoDock** | 桌面宠物 + 完整硬件 | ESP32-P4 / 480×640 MIPI-DSI | ESP-IDF + LVGL | Tauri+React + Node sidecar | USB 串行（不联网） | 62 | 09-22 | 非商用 |
| **Caldis/esp32-agent-dashboard** | 单一面板 | ESP32-S3 / 480×480 AMOLED | LVGL 9（ESP-IDF v6） | Python daemon（自研 esp-harness） | USB 串口，单行 JSON | 0 | 08-06 | MIT |
| **AhakeyAI/desktop** | 商用键盘配套 | 无（键盘 AhaKey-X1） | Tauri+React / Swift / Java | 多语言多客户端 | BLE + USB | 25 | 09-21 | Apache-2.0 |
| **Positronico/claudeq** | 桌面遥控器 | ESP32-S3 / 172×640 | ESP-IDF + LVGL | Node `bridge.mjs` | **WiFi WS + mDNS + Tailscale** | 13 | 07-17 | MIT |
| **paultyng/agentsd** | 纯 PC 外设 | 无（Stream Deck） | Stream Deck SDK | Node 插件 + HTTP hook 服务 | HTTP `127.0.0.1:9200` | 10 | 08-12 | MIT |
| **vlgutv22/claude_mate** | 极简双版本 | Arduino Nano(OLED) + ESP32-S3 | 自绘（无 LVGL） | Python daemon + PTY wrapper | USB `\|` 分隔行 / BLE / TCP | 9 | 09-01 | **CC BY-NC** |
| **alvis-HaoH/cc-mochi** | 表情摆件 | ESP32-C3 / ST7789 240×320 | Adafruit GFX 自绘 | Python daemon | USB 串口 JSON Lines | 5 | 07-08 | MIT |
| **gurul/claude-pet** | 桌面宠物 + 墨水屏版 | ESP32-S3 / ILI9341 240×320 | Arduino TFT_eSPI | Python `cc-buddy-bridge` | USB serial NDJSON | 0 | 08-22 | **无 LICENSE** |

> star 数与工程质量**严重脱钩**：Caldis 与 gurul 两个 0★ 仓库分别有 211 次提交 / CI / 4 个 release / 官网 / 协议文档。它们是"未被传播"而非"不靠谱"。

---

## 2. 技术选型横向对比

### 2.1 芯片平台：ESP32 一家独大

| 平台 | 项目 | 共同点 |
|---|---|---|
| **ESP32-S3**（主流） | AgentDeck、claudeq、claude_mate、claude-pet、Caldis | WiFi/BLE + PSRAM + 原生 USB，缺一不可 |
| **ESP32-P4**（旗舰） | HachimoDock、AgentDeck `ips10` | MIPI-DSI 高分辨率动画，需要 32MB PSRAM |
| **ESP32-C3**（最低配） | cc-mochi | 一片 240×320 屏 + 表情，够用 |
| **ATmega328P**（复古） | claude_mate 一代 | 128×32 OLED，**故意做减法** |
| **STM32F407** | ← SmartDeskTerminal | 生态里**没有同类** |

**关键事实**：这个生态里没有第二个 STM32 项目。**这不是缺点**——它意味着 SmartDesk 的"寄存器/HAL/FreeRTOS 手写"路线在这个品类里是唯一的，教学价值与稀缺性都在。

### 2.2 GUI 层：LVGL 是事实标准

- **LVGL 8/9**：AgentDeck、claudeq、Caldis、HachimoDock（+ SmartDesk）
- **自绘（Adafruit GFX / TFT_eSPI / 手搓）**：cc-mochi、claude-pet、claude_mate
- 版式选择逻辑：屏幕 ≥240×320 且要动画 → LVGL；只是画几张脸/几行字 → 自绘更省 RAM。

SmartDesk 用 LVGL 8.3.11 与主流一致；`LV_USE_PERF_MONITOR` 这类工具也是生态常规做法。

### 2.3 传输与协议：三种范式

| 范式 | 代表 | 适用 |
|---|---|---|
| **USB 串口 + 行式文本/JSON** | Caldis（单行 JSON）、cc-mochi（JSON Lines）、claude_mate（`\|` 分隔）、HachimoDock、SmartDesk（ADR-016 key=value） | 板子就在电脑旁，零配置，**最稳** |
| **WiFi WS + mDNS 自动发现** | claudeq、AgentDeck、claude_mate S3 | 要脱离线缆 / 多机 |
| **BLE** | claude_mate S3、AhakeyAI | 主机就在旁边，低功耗 |

**协议细节对照（可直接对照 ADR-016）**

| 项目 | 帧格式 | 心跳 | 断连策略 |
|---|---|---|---|
| Caldis | `dash snapshot` / `dash time` / `?stat` → 回复 `OK:` `ERR:` `EVT:` | `dash health` | host 静默 → 退避回 clock 视图 |
| cc-mochi | `{"type":"state"/"usage"/"ping"}` | `{"type":"ping"}` | hook 落盘 `missed-hooks.jsonl` |
| claude_mate | `F\|flags\|sel\|r0..r3`、`V\|KIND`、`B\|G`、`H`（boot） | boot 发 `H` → daemon **重发全量帧** | 30s 静默 → `LINK LOST` 屏 |
| claude-pet | NDJSON + `char_begin` 等动词 | 5s `[alive]` 打印 | daemon 双向看门狗，升级到 **RTS 硬复位** |
| agentsd | HTTP `POST /hooks/{Event}` | — | permission 120s 超时自动拒绝 |

> **共性缺口检查**：所有项目都有「**心跳 / 静默超时 / 重连后重发全量快照**」。SmartDesk 的 `cmd.c` 目前只有请求-响应，这三样都缺。

### 2.4 主机端桥：Python 或 Node，二选一

- **Python**（claude-pet / cc-mochi / claude_mate / Caldis）——pyserial + hook 转发，最轻。
- **Node**（claudeq / agentsd / AgentDeck / HachimoDock）——分发友好（npm/npx/brew），WebSocket 生态好。
- SmartDesk 的 plan 已定 Python，与前半部分一致，**无需改**。

---

## 3. 硬件选型聚类

| 档位 | 代表 | BOM 特征 | 成本量级 |
|---|---|---|---|
| **低配自组** | claude_mate 一代（Nano + 128×32 OLED + 3 键 + 1 LED） | 面包板即可，**代码 1 个 .ino** | < $10 |
| **成品模组**（生态主流） | claude-pet（Freenove ESP32-S3 2.8" 触摸）、cc-mochi（C3+ST7789）、claude_mate S3、claudeq、Caldis、AgentDeck 十余种 | 买板即用，不画 PCB | $10–30 |
| **自研硬件**（旗舰） | HachimoDock（ESP32-P4 + MIPI 屏 + 定制 PCB + 3D 外壳 + OSHWHub 开源） | 打样、BOM、装配教程齐全 | 数十 $ |
| **非 MCU 路线** | agentsd（Stream Deck）、AgentDeck（Stream Deck/Ulanzi/平板/墨水屏） | 直接用市售外设 | $50–200 |

**对 SmartDesk 的三个事实性结论**：

1. **"开发板 + 屏幕 + 杜邦线"是完全站得住的形态**。claude_mate 一代、cc-mochi、claude-pet 都是这么干的，HachimoDock 官方 FAQ 还专门写「**不必须做定制 PCB，调试阶段可以先用杜邦线直连**」。
2. **屏尺寸与交互档次**：240×320 属于"低配自组"档（cc-mochi 同规格）。它够做**状态 + 表情 + 计数**，不够做富信息仪表盘——这一点应在产品定位上认账，别硬塞。
3. **触摸不是必需品**。HachimoDock 明确「固件不依赖触摸屏」，交互以实体键 + 摇杆 + 语音为主；claude_mate 也是 3~4 个物理键。SmartDesk 有 XPT2046 电阻触摸，属于加分项，不是必须项。

**电气/电源侧值得注意**：claude_mate S3 用 14500 锂电并给出「三段电量条而非百分比」的理由（表面电荷导致百分比虚高）；claudeq 待机只关背光保留 WS 连接。SmartDesk 无电池，**不适用**，但"不要显示会撒谎的数值"这条设计原则通用。

---

## 4. 热门 / 靠谱程度分级

判据：★/fork 数（传播度）× release 数、贡献者数、CI、协议/硬件文档完整度、最近推送（维护活跃）× 许可清晰度。

### S 级 —— 可当作权威模板

| 项目 | 证据 |
|---|---|
**AgentDeck** | 247★ / 52 fork / 9 贡献者 / **100 次 release** / 当日仍在推送 / pnpm monorepo + Vitest + 设计令牌 + 十余种硬件环境 / 上架 App Store、Google Play、Elgato、Ulanzi 四个商店 / MIT |
**HachimoDock** | 62★ / 15 fork / 8 release / **PCB + BOM + 装配教程 + 3D 外壳全开源**（OSHWHub）/ SHA256 完整性校验 / 有微信社群 / 明确商业授权路径 |

> 这两家的共同点：**把发布当工程做**（版本号、变更日志、清单校验、多平台分发、许可声明）。

### A 级 —— 工程质量高、传播未起，最适合精读源码

| 项目 | 证据 |
|---|---|
**Caldis/esp32-agent-dashboard** | 211 commits / 4 release / CI / 独立协议文档 `PROTOCOL.md` / 自研构建工具链 esp-harness / **有性能方法论**（`?perf` 实测、trans_bench 回归基线、`perf-dead-ends.md` 记录失败优化）/ MIT。0★ 纯粹是没传播开 |
**claudeq** | 13★ / **23 release** / 浏览器烧录（ESP Web Tools）/ 固件 OTA + 回滚 / 设备-桥配对（AES-256-GCM）/ 本地 whisper 语音 / brew 分发 / MIT |
**claude_mate** | 9★ / 极详细文档（USING / WIRING / PROTOCOL / POWER / TESTING 五份）/ 双设备同协议 / PTY wrapper 读活屏 / 负向设计写进 README（Limitations 章节诚恳） |

### B 级 —— 小而完整，适合当"最小可抄范例"

| 项目 | 证据 |
|---|---|
**cc-mochi** | 5★ / 中文 / **"事件 → 14 种状态 → 表情"的映射表最完整** / 空闲轮播用量卡片 / daemon 不在也不阻塞 CLI / MIT |
**paultyng/agentsd** | 10★ / 纯主机侧无 MCU / 但它把 **hook 服务做成教科书**：permission 队列 + 120s 超时自动拒 + 60s 陈旧会话清理 + 未知会话自动建 + 模型名从 transcript 回填 / 三层测试（unit/integration/E2E，用假 CLI 做确定性 E2E）/ MIT |

### C 级 —— 需要谨慎

| 项目 | 风险 |
|---|---|
| **gurul/claude-pet** | **仓库无 LICENSE 文件** → 默认「保留所有权利」，**代码不可借用**（设计思路可参考）。1 贡献者，0★ |
| **AhakeyAI/desktop** | 25★ 但 18 fork / **30 个 open issues** / 商用键盘的配套（主线是键盘不是屏）/ 仓库里 Swift + Java + Python + Rust + C# 多套历史客户端并存，属包袱。参考价值低于表面 |

---

## 5. SmartDeskTerminal 该借鉴什么（按 Phase 对号）

### Phase 8（USB CDC）——当前不是在收尾，是在定位花屏

> 注：截至 2026-09-25 深夜，花屏仍在排查中（T-007 的 `-fcommon` 别名 bug 已证真、也是 USB 必需的，**但已不是花屏的根因**）。
> 因此下表**优先级重排**：先解决"看得见"的问题，再做健壮性。

| 借鉴点 | 出处 | 具体做法 |
|---|---|---|
| ⭐ **先立观测通道，再谈定位** | Caldis（`?stat / ?perf / ?deco / ?ghost` 运行时自检；性能死胡同专门写 `perf-dead-ends.md`）、claude-pet（`diag` 打印上次复位原因，事件环跨 panic 存活）、AgentDeck（`agentdeck diag`） | 这个生态的共同信条是**"截图会骗人、计时器会骗人，所以每个结论都要配一个仪表"**。SmartDesk 现在最缺的就是这个：CH340 的 UART printf 是**独立于 USB 协议栈的通道**，能回答"程序跑到哪一行"，正是花屏排查该先补的一环（同时会让 ADR-008 的"跳过 UART"失效） |
| **重连后重发全量快照** | claude_mate（boot 发 `H` → daemon 重发整帧）、claude-pet | MCU 上电/枚举完成时主动发一行 hello，PC 端收到就**全量重推状态**，而不是等下次变化 |
| **静默超时 → `LINK LOST`** | claude_mate（30s） | 屏上把"数据陈旧"当一等状态显示，而不是继续显示过期数字 |
| **版本 + 构建时间上报** | 生态通用 | SmartDesk 已有 `FW_VERSION + __DATE__` ✓，把 `version` 命令做成协议里的标准 `DAT kind=version` |
| **板载自检命令** | 出处同上第 1 行 | 现有 `help/version/ping/hits` 扩成 `?stat`（栈水位、堆余量、任务状态、最近复位原因），**这是"屏幕当调试通道"的正确形态** |

### Phase 9（PC Agent + hooks）——这是借鉴价值最大的一层

| 借鉴点 | 出处 | 为什么重要 |
|---|---|---|
| **事件清单直接照抄** | agentsd 列全了 16 个事件 | `SessionStart / SessionEnd / UserPromptSubmit / PreToolUse / PostToolUse / PostToolUseFailure / Stop / StopFailure / PermissionRequest / Notification / SubagentStart / SubagentStop / TaskCreated / TaskCompleted / Elicitation / ElicitationResult`。Phase 9 不必自己设计 |
| **最小可用挂点 = `Stop` + `Notification`** | Caldis（Stop）、claude_mate（4 个 hook） | 先跑通 Stop，再逐步加 |
| **hook 必须非阻塞且永不让 CLI 卡住** | cc-mochi（落盘 missed-hooks）、claude-pet（超时熔断） | 判据：**daemon 不在运行时，hook 也必须 0 秒成功退出** |
| **补一个 PTY wrapper** | claude_mate | **hooks 看不到 permission prompt / API 错误 / 交互选项**。要"等授权"这个最有价值的状态，必须包 PTY 读活屏（pyte）。这是 hooks-only 方案的天花板 |
| **状态词表** | cc-mochi（14 态）、claude_mate（5 态）、HachimoDock（working/done/error） | 建议 SmartDesk 收敛为 6 态：`idle / working / permission / error / done / disconnected` |
| **ack（已读）模型** | claude_mate "finished but not seen" | done 状态**保持到用户确认**；SmartDesk 有触摸 + PC1 按键，天然适合做 ack。**这是可持续的差异化** |
| **mock / dry-run 通道** | claudeq `mock-device.html`、claude-pet `mock_device_v1.py`、claude_mate `--mock`、AgentDeck web 模拟器 | 无板也能调 UI：PC 侧先实现"假设备"，让 Phase 10 的界面逻辑在板子不在手边时也能迭代 |
| **协议心跳 + 序号** | claude-pet 5s `[alive]` | ADR-016 里"是否加校验和/序号"的待定项 → 参考结论：**USB CDC 有硬件重传，先只加心跳，不加校验和** |

### Phase 10（整合）——产品定位与 UI 契约

| 借鉴点 | 出处 | 具体做法 |
|---|---|---|
| **"颜色跟随状态，不跟随页面"** | Caldis 的设备级色彩契约 | 单页 + 状态色（gold=需要你 / teal=工作中 / dim=空闲），不要多页面菜单 |
| **不要做成"第二个终端"** | Caldis 明确 retired 掉设备端 approve/deny 按钮与吉祥物 | 决策留在终端，面板只做 `状态 in / 一眼 out`。这条能防止范围失控 |
| **无 UI 模式，按钮语义恒定** | claude_mate | 触摸按钮永远同一含义，不用模式机 |
| **设计令牌 / 字号阶梯** | AgentDeck（5 档字号，0.6–1m 视距）、HachimoDock | SmartDesk 的 LVGL 字号应固定成 3–4 档，禁止随手写像素值 |
| **诚实的边界写进 README** | claude_mate `Limitations`、HachimoDock FAQ | 把"只有 USB 有线 / 无 WiFi / 不开源商用"等限制写成正式章节 |

---

## 6. 不要借鉴 / 反面教训

| 教训 | 出处 | 对 SmartDesk 的意义 |
|---|---|---|
| **许可红线**：claude_mate 与 HachimoDock 是 CC BY-NC / 非商用；claude-pet **无 LICENSE** | 三处 | **只能借鉴设计与文档思路，不能复制代码**。可复制的只有 MIT 系（AgentDeck、Caldis、claudeq、cc-mochi、agentsd） |
| **墨水屏 `EPD_Wake` 不重设数据入口 → 静默不刷新** | claude-pet 文档 | 与 T-007 同族故障：**驱动 wake/resume 必须重配控制器状态**，否则"固件在正常计数但屏不动" |
| **upload 标志跨板复制会反噬** | AgentDeck（`--before=no_reset` 抄到另一些板导致上传失败） | `platformio.ini` 的 `upload_flags`/`build_flags` 必须按自己的板核实，不能照抄 |
| **多硬件 = 多倍配置分叉** | AgentDeck 的 `platformio.ini` 有 17 个 env、大量 `build_src_filter` | **单人项目不要学**。SmartDesk 坚持单板单 env，是正确的自我约束 |
| **STM32/USB 中间件同名句柄被 `-fcommon` 合并** | SmartDesk T-007 自身 | 该生态用 ESP32（新工具链）不会遇到，所以**这条经验在生态里找不到同类，属 SmartDesk 独有资产**，应保留在 troubleshooting.md |
| **不要为对标加 WiFi** | claudeq / AgentDeck / claude_mate S3 都为此付出复杂度（配对、mDNS、OTA、网络安全） | SmartDesk 的 USB-only 与 claude-pet / Caldis / HachimoDock 同一档，是**被验证可行的形态**，不需要升级 |

---

## 7. 待决策项（不替用户拍板）

1. **Phase 9 的 hooks 挂点范围**：只做 `Stop`+`Notification` 最小集，还是一次接入 agentsd 的 16 事件清单？
2. **是否引入 PTY wrapper**（claude_mate 路线）？做 → 能看到"等授权"，但复杂度上一个台阶；不做 → 状态模型天然缺一格，且要接受这个天花板。
3. **状态模型是否采用 ack（已读）语义**？决定 UI 与协议都要为此留字段，宜在 ADR-016 落地前定。
4. **产品定位确认**：只做"状态显示 + 一眼可知"，还是保留屏上操作（触摸按钮触发 Agent 动作）？Caldis 的结论是前者；但 SmartDesk 是学习项目，后者教学价值更高。
5. **本文件去向**：结论并入 `decision-log.md`（ADR-017）后删除，还是作为常驻参考保留？
