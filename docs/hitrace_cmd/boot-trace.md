# Boot trace（hitrace 命令行）

本文档汇总 **`hitrace --boot_trace`** 与 **`hitrace boot-trace`** 的运行原理、流程与功能规格，便于功能实现与行为对照查阅。

---

## 1. 运行原理与端到端流程

1. **配置阶段**（执行 boot trace 配置命令）  
   用户执行 `hitrace --boot_trace [选项] tag1 [tag2 ...]`（走通用 `main` → `InitAndCheckArgs` → `getopt_long` → **`TASK_TABLE` 中的 `HandleBootTraceConfig`**）：
   - **通用前置**（均在 **`HandleOpt` / `--boot_trace` 解析之前**）：**开发者模式**（`IsDeveloperMode()` → 系统参数 **`const.security.developermode.state`**）、**`TraceCollector` 创建**、**trace 节点挂载**（`IsTraceMounted()`）等；任一失败则按 `hitrace_cmd.cpp` 中对应 **`ConsoleLog`** 退出（例如 **`error: not in developermode, exit`**、**`error: traceCollector create failed, exit.`**、**`error: trace isn't mounted, exit.`**），**`main` 返回 -1`**，**尚未**进入 **`HandleOptBootTrace`** / **`HandleBootTraceConfig`**。
   - **有效用户 ID**：解析到 **`--boot_trace`**（含 **`--boot`** 作为其唯一前缀缩写）时，若 **`geteuid() != 0`**，立即输出 **`error: unrecognized option '--boot_trace'.`**，**不**再输出第二行 **`error: parsing args failed, exit.`**，`InitAndCheckArgs` 失败，进程退出 **-1**（与非法参数区分：表现为「不识别的长选项」）。
   - **调试镜像门控**：进入 **`HandleBootTraceConfig`** 时若 **`const.debuggable`** 不为真（实现为 **`IsRootVersion()`** → 读系统参数 **`const.debuggable`**），则输出 **`error: parsing args failed, exit.`** 并返回失败，**不写** count 与 cfg。
   - 通过上述门控后：校验参数与 tag → 将 **`persist.hitrace.boot_trace.count`** 写成 repeat（1～100）→ 在 **`/data/local/tmp/`** 生成或覆盖 **`boot_trace.cfg`**（JSON）。`hitrace --boot_trace off` **仅**将 **`persist.hitrace.boot_trace.count`** 置 **0**，**不删除** `boot_trace.cfg`（避免为 hitrace 申请 **`data_local_tmp`** 下通用 unlink/remove_name 权限、降低误删其它用户文件风险）；见规格节 §3.4.2。

2. **开机决策**（`startup_init` 仓 `init_trace`）  
   在 **`INIT_POST_PERSIST_PARAM_LOAD`** 之后，`TryRunBootTraceByCount()` 读取 **`persist.hitrace.boot_trace.count`**。在以下条件**全部**满足时：**先**将 count **减一写回**，再 **`fork` + `execl("/system/bin/hitrace", "hitrace", "boot-trace", NULL)`**（**init 不在 fork 前写 `debug.hitrace.boot_trace.active=1`**，由子进程在 `RunBootTraceControl` 内首道逻辑置位）：
   - **`persist.init.trace.enabled` 不为 `"1"`**（为 `"1"` 时表示 init trace 占用，与 boot_trace **互斥**，直接跳过）；
   - count 解析后在 **1～100**；
   - **`debug.hitrace.boot_trace.active` 不为 `"1"`**（否则视为仍处 boot-trace 窗口或残留，**不**消耗 count、不拉起）；
   - **`const.debuggable` 为 `"1"`**（与 CLI 侧可抓形态对齐；非 debuggable 镜像上 init **不**拉起 boot_trace）。
   子进程在 `execl` 前将附加组设为 **`shell`**（与策略里 hitrace 域所需一致；具体 SELinux 约束见设计文档仓）。若 `fork`/`execl` 失败，init 会尝试将 **`debug.hitrace.boot_trace.active`** 写回 **`0`**（防止异常残留锁死窗口）。

3. **执行阶段**（`hitrace boot-trace` 子进程，含 init 拉起与 shell 手工执行）  
   **`main` 在 `InitAndCheckArgs` 之前**对 **`argv[1]=="boot-trace"`** 做早退分支（见 `cmd/hitrace_cmd.cpp`）：  
   - 若 **`geteuid() != 0`**：输出 **`error: unrecognized command 'boot-trace'.`**，退出 **-1**（**不**进入 `RunBootTraceControl`，**不**访问 `boot_trace.cfg`）。  
   - 若 **`const.debuggable`** 不为真（同上，**`IsRootVersion()`**）：输出 **`error: parsing args failed, exit.`**，退出 **-1**。  
   - 否则进入 **`RunBootTraceControl()`**：先读 **`debug.hitrace.boot_trace.active`**：若已为 **`"1"`**（并发/重复拉起），则输出 **`boot_trace: duplicate launch ignored (debug.hitrace.boot_trace.active already 1)`**，将 **`boot_trace.cfg` 内 `result` 字段** 更新为 **1**，进程以**退出码 1** 结束（**不**读配置、**不**再次置位 active）。否则 **置 `active=1`**，从 **`/data/local/tmp/boot_trace.cfg`** 读取并校验；按配置 **OpenTrace → 等待 duration → DumpTrace → CloseTrace**；在会话析构/护栏中把 **`active` 清回 `0`**。每次 `RunBootTraceControl` 结束前会尽量把**最后一次退出语义**写入 **`boot_trace.cfg` 的 `result` 整数字段**（与进程退出码一致：`0` 成功、`1` 重复拉起、`2` 配置/置位失败或抓取失败等，见 §3.6）。**persist 计数只**在 init 侧递减，**`hitrace boot-trace` 进程不修改** `persist.hitrace.boot_trace.count`。

4. **与 hiview / libhitrace_dump 互斥与资源策略**  
   当 **`debug.hitrace.boot_trace.active=="1"`** 且调用方 **非 root（`getuid()!=0`）** 时，`OpenTrace` / `DumpTrace` / `RecordTraceOn` 等路径在持锁仲裁下返回 **`BOOT_TRACE_ACTIVE`**，避免与 boot_trace 子进程争用 trace 管线；**`CloseTrace`** 对非 root 在该窗口内返回 **`SUCCESS`（no-op）**，避免 hiview 等上层在「无法真正关闭」时拿到错误状态。  
   **磁盘预留**：`ProcessDumpSync` / `ProcessDumpAsync` 在 **`debug.hitrace.boot_trace.active=="1"`** 时，将 **`/data` 分区最小剩余空间阈值** 从默认的 **300MB**（`SNAPSHOT_MIN_REMAINING_SPACE`）降为 **20MB**，以适配开机早期剩余空间较紧仍可完成 boot trace 落盘；非 boot 窗口仍用 300MB 阈值。
   
5. **init-hitrace job**  
   产品 init 在启动早期按配置触发 **`init-hitrace`** job（定义见本仓 **`config/hitrace.cfg`**），用于准备内核 trace 相关节点环境；inittrace 在 stop/interrupt 后会再次调度该 job 以恢复环境（与 init 仓 inittrace 生命周期配合）。

---

## 2. 路径与参数速查

| 项 | 说明 |
|----|------|
| 配置目录 | `/data/local/tmp/`（`BOOT_TRACE_CONFIG_DIR`） |
| 配置文件 | `boot_trace.cfg`（`BOOT_TRACE_CONFIG_FILE`） |
| 计数 persist | `persist.hitrace.boot_trace.count`（0 关闭；1～100 为剩余次数） |
| 非持久 active | `debug.hitrace.boot_trace.active`（`"1"` 表示 boot-trace 窗口） |
| init trace 互斥 | `persist.init.trace.enabled` 为 `"1"` 时不触发 boot_trace |
| debuggable 门控 | `const.debuggable` 须为 `"1"`，init 才会拉起 boot_trace |
| CLI 分发（`boot-trace`） | `argv[1]=="boot-trace"`：`main` 早退；**`geteuid()==0`** 且 **`const.debuggable`** 允许 → **`RunBootTraceControl()`**；否则见 §3.3.9 / §3.6-B |
| CLI 分发（`--boot_trace`） | 走 **`InitAndCheckArgs` → `HandleOpt` → TASK_TABLE → `HandleBootTraceConfig`**；**`geteuid()==0`** 方可匹配 **`--boot_trace` / `--boot`**；**`const.debuggable`** 在 **`HandleBootTraceConfig`** 内再次校验 |
| 长选项缩写 | 在 `getopt_long` 中，若 **`--boot`** 为 **`--boot_trace`** 的唯一前缀匹配，则与 **`--boot_trace`** 走同一 **`HandleOptBootTrace`** 路径；**`geteuid()!=0`** 时输出 **`error: unrecognized option '--boot_trace'.`**，退出 **-1**，且**不**追加 **`error: parsing args failed, exit.`** |

---

## 3. 功能规格说明

以下描述 `hitrace --boot_trace` 的参数、边界与可观察行为（错误信息、退出码、写盘内容）。

### 3.1 命令与子命令

| 形式 | 说明 |
|------|------|
| `hitrace --boot_trace [options] tag1 [tag2 ...]` | 配置开机 trace：生成/覆盖配置文件，供开机时抓取 |
| `hitrace --boot_trace off` | 关闭开机 trace：**仅**将 **`persist.hitrace.boot_trace.count`** 置 **0**；**不删除** `boot_trace.cfg` |

### 3.2 参数规格

#### 3.2.1 必选与可选

| 参数 | 形式 | 必选 | 说明 |
|------|------|------|------|
| tag | 命令末尾空格分隔的 tag 列表 | **必选**（配置时） | 至少一个 tag；`off` 为保留字表示关闭 |
| `-b` / `--buffer_size` | 数值（KB） | 可选 | trace buffer 大小 |
| `-t` / `--time` | 数值（秒） | 可选 | 抓取时长 |
| `--file_size` | 数值（KB） | 可选 | 单文件大小限制（录制切分） |
| `--trace_clock` | 字符串 | 可选 | 时钟类型 |
| `--repeat` | 数值 | 可选 | 开机抓取次数（1～100）；写入 **`persist.hitrace.boot_trace.count`** 为 repeat（即 1～100） |
| `--overwrite` | 开关 | 可选 | 每次开机 trace 覆盖上一份，仅保留最后一份；**默认关闭**，即按文件名递增多份并存 |
| `--increment` | 开关 | 可选 | 启用后每次抓取目标文件名为 **`…/{file_prefix}_default_{n}.sys`**，`n` 从 **0** 起每次成功抓取后 **+1** 并写回 cfg 的 **`increment_index`** 与 **`output`**；未指定时 **`increment_index` 为 -1** 表示关闭（见 §3.3.9）。**须写在 `--boot_trace` 之后**（与其它 boot_trace 专属长选项一致） |
| `--file_prefix` | 字符串 | 可选 | 输出文件名前缀；仅允许与 **`--boot_trace`** 同用；默认 `boot_trace`；写入 cfg 的 `file_prefix` 并参与 `output` 路径 |

#### 3.2.2 参数边界

| 参数 | 最小值 | 最大值 | 默认值 | 单位/说明 |
|------|--------|--------|--------|------------|
| **buffer_size** (`-b`) | 256 | 300 MB (307200 KB)；HM 内核为 1024 MB (1048576 KB) | 18432 (18 MB) | KB；按 4 KB 页对齐 |
| **time** (`-t`) | 1 | 无上限（实现依赖） | 30 | 秒；必须大于 0 |
| **file_size** | 51200 (50 MB) | 512000 (500 MB) | 102400 (100 MB) | KB |
| **trace_clock** | - | - | `boot` | 取值见下表 |
| **repeat** | 1 | 100 | 1 | 整数；写入 **`persist.hitrace.boot_trace.count`** 的值为 repeat（1～100） |
| **overwrite** | - | - | false | 布尔；未指定时为 false |
| **increment** | - | - | 关闭（`increment_index=-1`） | 布尔开关；指定 `--increment` 时配置阶段将 **`increment_index` 置 0** 并重算 **`output`** 为带 **`_0`** 后缀的路径 |

**trace_clock 合法取值**：`boot`、`mono`、`global`、`perf`、`uptime`。

#### 3.2.3 配置文件与路径常量

| 项 | 值 | 说明 |
|----|-----|------|
| 配置与输出目录 | `/data/local/tmp/` | `BOOT_TRACE_CONFIG_DIR`（CLI 写入，boot-trace 子进程读取） |
| 配置文件名 | `boot_trace.cfg` | `BOOT_TRACE_CONFIG_FILE` |
### 3.3 边界与校验行为

#### 3.3.1 Tag

- **至少一个 tag**：配置时（非 `off`）若未提供任何 tag，报错并退出，不写配置。  
  错误信息：`error: boot_trace requires at least one tag.`
- **不支持 tag group**：若命令中出现的是 tag group 名称（来自 `hitrace_utils.json` 的 tag_groups），报错。  
  错误信息：`error: tag group is not supported in boot_trace. please use concrete tags.`
- **Tag 合法性**：所有 tag 必须在当前设备的 `hitrace_utils.json` 的类别中存在，否则报错并退出。  
  错误信息形态：`error: <tag> is not support category on this device.`

#### 3.3.2 Buffer size (`-b` / `--buffer_size`)

- 必须为合法数字；否则：`error: buffer size is illegal input. eg: "--buffer_size 18432".`
- 必须在 [256, max] KB 范围内（max 见上表）；否则：`error: buffer size must be from 256 KB to <max> MB. eg: "--buffer_size 18432".`
- 内部按 4 KB 对齐：`bufferSize = value / 4 * 4`（向下取整到 4 KB 的倍数）。

#### 3.3.3 Time (`-t` / `--time`)

- 必须为合法数字且 **大于 0**；否则：`error: "-t <value>" to be greater than zero. eg: "--time 5".`

#### 3.3.4 File size (`--file_size`)

- 必须为合法数字；否则：`error: file size is illegal input. eg: "--file_size 102400".`
- 必须在 [51200, 512000] KB（50 MB～500 MB）；否则：`error: file size must be from 50 MB to 500 MB. eg: "--file_size 102400".`

#### 3.3.5 Trace clock (`--trace_clock`)

- 取值必须在集合 `{ boot, mono, global, perf, uptime }` 内；否则：`error: "--trace_clock" is illegal input. eg: "--trace_clock boot".`

#### 3.3.6 Repeat (`--repeat`)

- 必须为合法整数且在 [1, 100]；否则：`error: --repeat must be from 1 to 100. eg: "--repeat 5".`
- 写入 **`persist.hitrace.boot_trace.count`** 的值为 **repeat**（1～100）。init 侧仅当取值在 1～100 时按 boot trace 处理（读后减一写回并拉起 hitrace）；取值为 0 表示关闭。

#### 3.3.7 Overwrite (`--overwrite`)

- 为可选布尔开关，**默认关闭**。打开时，配置文件中 `overwrite` 为 true，对应 **`hitrace boot-trace`** 内 **`OpenTrace`** 写入 tracefs **`options/overwrite`**（环形缓冲覆盖策略），**不是**输出 `.sys` 文件名的轮换逻辑。与 **`--increment`** 正交，可同时使用。

#### 3.3.8 File prefix (`--file_prefix`)

- **仅**在 **`--boot_trace`** 配置流程中合法；与其它子命令组合时报错：`error: --file_prefix only supports --boot_trace.`
- 值**不得为空**；否则：`error: file_prefix must not be empty.`

#### 3.3.9 Increment (`--increment`)

- **可选开关**；仅在与 **`--boot_trace`** 同一条配置命令中使用，且 **`--boot_trace` 须先于 `--increment` 出现在 argv**（否则：`error: --increment only supports --boot_trace.`）。
- **未指定 `--increment`**（默认）：写入 **`increment_index: -1`**，输出路径为 **`/data/local/tmp/{file_prefix}_default.sys`**（与既有行为一致）。
- **指定 `--increment`**：每次执行 **`hitrace --boot_trace … --increment …`** 都会将 **`increment_index` 重置为 `0`**，并将 **`output`** 设为 **`/data/local/tmp/{file_prefix}_default_0.sys`**。
- **`hitrace boot-trace` 抓取成功（退出码 0）后**：在同一份 **`boot_trace.cfg`** 内将 **`increment_index`** 递增至 **`原值 + 1`**，并同步更新 **`output`** 为下一抓取的 **`…_default_{新值}.sys`**，供下一次开机抓取使用；抓取失败、重复拉起、配置错误等**非成功路径不修改** `increment_index` / `output`。
- 手工编辑 cfg 时：若 **`increment_index` ≥ 0**，执行 **`hitrace boot-trace`** 时落盘路径按 **`{BOOT_TRACE_CONFIG_DIR}{file_prefix}_default_{increment_index}.sys`** 解析（与 `output` 字段冗余时以 **`increment_index` + `file_prefix`** 为准）。

#### 3.3.10 权限、`euid` 与 `const.debuggable`（与 `cmd/hitrace_cmd.cpp` 一致）

以下均为 **`hitrace_cmd.cpp`** 当前实现；与 **init 侧**是否 fork（另需 **`const.debuggable`** 等，见 §1 第 2 步）相互独立。

| 入口 | 检查位置 | 条件 | 失败时控制台关键信息 | 进程退出码 |
|------|-----------|------|----------------------|------------|
| **`hitrace boot-trace`** | `main` 早退，`IsBootTraceEuidRoot()` | **`geteuid() == 0`** | `error: unrecognized command 'boot-trace'.` | **-1** |
| **`hitrace boot-trace`** | `main` 早退，`IsBootTraceAllowedByConstDebuggable()` | 生产：**`const.debuggable`** 为 true（`IsRootVersion()`）；单测：恒为 true | `error: parsing args failed, exit.` | **-1** |
| **`hitrace --boot_trace` / `--boot`** | `HandleOptBootTrace`，解析长选项时 | **`geteuid() == 0`** | `error: unrecognized option '--boot_trace'.` | **-1**（经 **`InitAndCheckArgs` → main**） |
| **`--boot_trace` 配置任务** | `HandleBootTraceConfig` 首行 | **`const.debuggable`** 允许 | `error: parsing args failed, exit.` | **-1** |

**说明**：

- **`geteuid()`**：有效用户须为 **root**，init **`execl`** 子进程通常为 root；shell/应用直接执行且无 **SUID** 时多为非 root，命中 **unrecognized** 行。
- **非 root 的 `unrecognized` 与「解析失败」**：非 root **不**打印第二行 **`error: parsing args failed, exit.`**（由 `g_suppressParsingArgsFailedLog` 抑制，避免与「无法识别选项」语义重复）。
- **非 debuggable**：仍使用 **`error: parsing args failed, exit.`**（与部分非法参数退出表现相同），**不**使用 unrecognized 文案。
- **单测编译 `HITRACE_UNITTEST`**：`IsBootTraceAllowedByConstDebuggable()` 恒 true；可用 **`SetBootTraceForceRootForTest(false)`** 配合 **`geteuid()`** 路径模拟非 root（见 **`HitraceCMDTest045`**）。

### 3.4 行为规格

#### 3.4.1 配置成功（`hitrace --boot_trace [options] tag1 [tag2 ...]`）

1. 已通过 **`InitAndCheckArgs`** 通用前置条件，且 **`geteuid()==0`**（否则在 **`HandleOptBootTrace`** 阶段已失败，见 §3.3.9）。在 **`HandleBootTraceConfig`** 内 **`const.debuggable`** 须允许；然后校验所有参数与 tag（见第 3.3 节）；任一不通过则报错并退出，**不**生成/覆盖配置文件。
2. 若目录 `/data/local/tmp/` 不存在，则创建（权限 755）。
3. 生成或覆盖 `/data/local/tmp/boot_trace.cfg`，内容为 JSON，包含：
   - `version`、`description`、`duration_sec`、`output`、`file_size_kb`、`file_prefix`、**`overwrite`**、**`increment_index`**
   - `kernel.enabled`、`kernel.tags`、`kernel.buffer_size_kb`、`kernel.clock`
   - `userspace.enabled`、`userspace.tags`
   - （执行 `boot-trace` 后可能由进程追加/更新）**`result`**：整型退出语义，与 §3.6 中 **`hitrace boot-trace`** 进程退出码一致
4. 将 **`persist.hitrace.boot_trace.count`** 设为 **repeat**（1～100）。
5. 将 tag 按内核态/用户态分类（依据 `hitrace_utils.json` 的类别）。
6. 输出：`boot_trace configuration success.`，退出码 0。

#### 3.4.2 关闭开机 trace（`hitrace --boot_trace off`）

1. 将 **`persist.hitrace.boot_trace.count`** 置 **0**（失败则报错并返回失败）。
2. **不**对 **`/data/local/tmp/boot_trace.cfg`** 执行 **`unlink`**（也不依赖 SELinux 对 **`data_local_tmp`** 的删除类权限）；遗留 cfg 不影响「已关闭」：init 仅在 count 为 1～100 时触发抓取；再次执行 **`hitrace --boot_trace …`** 会覆盖写入同一 cfg 路径。
3. 输出：`boot_trace off success.`，退出码 0。

#### 3.4.3 参数解析失败与其它 CLI 失败

- **参数 / tag 校验失败**：输出对应 **`error: ...`** 日志，**不**写配置文件（或 **`off` 未改文件**），经通用 `main` 路径返回 **`-1`**（shell 中或显示为 **255**）。
- **`geteuid()!=0` 使用 `--boot_trace` / `--boot`**：见 §3.3.9（**`unrecognized option`**，**-1**）。
- **`const.debuggable` 不允许时执行 `--boot_trace` 配置**：**`HandleBootTraceConfig`** 首行失败，**`parsing args failed, exit.`**，**-1**。
- **`hitrace boot-trace` 非 root / 非 debuggable**：见 §3.3.9 与 §3.6-B。

#### 3.4.4 与其它 running state 的关系

- `--boot_trace`、`--repeat`、`--overwrite`、`--increment` 同属运行状态 `CONFIG_BOOT_TRACE`；解析时仅在此状态下处理 tag 与 **`--repeat` / `--overwrite` / `--increment`**。
- 上述选项仅在 `CONFIG_BOOT_TRACE` 下生效；**`--increment` 须在 `--boot_trace` 之后**（见 §3.3.9）。

#### 3.4.5 boot-trace 执行说明

- init 在 `INIT_POST_PERSIST_PARAM_LOAD` 阶段调用 `TryRunBootTraceByCount()`：除 count 与 active 外，还须 **`persist.init.trace.enabled` 不为 `"1"`**、**`const.debuggable` 为 `"1"`**（见 §1 第 2 步）。满足时 **先**减一写回 **count**，再 **fork+exec** `hitrace boot-trace`（**不在此处写 `active`**）。
- 若 **`debug.hitrace.boot_trace.active` 已为 `"1"`**，init **不**消耗 count、**不**拉起。
- **`hitrace boot-trace`**：若入口时 **`active=="1"`** → 视为重复拉起，打印 **`boot_trace: duplicate launch ignored (debug.hitrace.boot_trace.active already 1)`**，更新 cfg 中 **`result=1`**，进程 **退出码 1**（**不**读配置、**不**再次置位 active）；否则 **置 `active=1`**，读 `/data/local/tmp/boot_trace.cfg`，执行 `OpenTrace`、等待、`DumpTrace`、`CloseTrace`，结束时 **清 `active`**，并写 **`result`** 与 **退出码** 对齐（见 §3.6）。
- **成功退出（码 0）且 cfg 中 `increment_index` ≥ 0**：写回 **`result`** 的同时将 **`increment_index` 加一**并更新 **`output`** 为下一文件名（§3.3.9）；非成功退出**不**修改 **`increment_index` / `output`**。
- **手工执行**：只要已有合法 **`boot_trace.cfg`**，可直接 **`hitrace boot-trace`**；仍须 **`geteuid()==0`** 且 **`const.debuggable`** 允许（与 init 拉起子进程一致），无需 init 预先写 `active`。
- **单元测试**：`HitraceCMDTest` 中依赖 `--boot_trace` / `boot-trace` 特权路径的用例在 **`geteuid() != 0`** 时 **`GTEST_SKIP`**（计为通过）；**`HitraceCMDTest045`** 等显式验证非 root 行为：子命令 **`hitrace boot-trace`** 输出 **`error: unrecognized command 'boot-trace'.`**；**`--boot_trace` / `--boot`** 输出 **`error: unrecognized option '--boot_trace'.`**，退出码 **-1**。

#### 3.4.6 触发机制

- `InitAddPostPersistParamLoadHook` 在 `load_private_persist_params` 后触发 `TryRunBootTraceByCount()`。
- `TryRunBootTraceByCount()` 有幂等保护（count=0 时跳过）。

### 3.5 配置文件字段与默认值（写入时）

| 字段 | 来源 | 默认或规则 |
|------|------|------------|
| `duration_sec` | `-t` | 未指定或 ≤0 时为 30 |
| `file_size_kb` | `--file_size` | 未指定或越界时 102400 |
| `kernel.buffer_size_kb` | `-b` | 未指定或 0 时为 18432 |
| `kernel.clock` | `--trace_clock` | 未指定或空时为 `boot` |
| `output` | 固定规则 | **`increment_index` 为 -1**：**`/data/local/tmp/` + `file_prefix` + `_default.sys`**；**`increment_index` ≥ 0**：**`/data/local/tmp/` + `file_prefix` + `_default_` + 序号 + `.sys`**；成功抓取后由 **`hitrace boot-trace`** 更新为下一文件名 |
| `file_prefix` | `--file_prefix` | 未指定时为 `boot_trace`；用于生成最终 trace 文件名前缀 |
| `overwrite` | `--overwrite` | 未指定时为 false；为 true 时对应 tracefs 环形缓冲覆盖（见 §3.3.7） |
| `increment_index` | `--increment` | **未指定 `--increment`**：每次 **`hitrace --boot_trace …`** 写入 **-1**；**指定 `--increment`**：每次配置写入 **0**（重置）；**`hitrace boot-trace` 成功**后 **+1** 并同步 **`output`** |
| `result` | **`hitrace boot-trace` 运行时写入** | 整数；与 §3.6 **`boot-trace` 子命令**退出码一致；配置命令 `--boot_trace …` 不写该字段 |

### 3.6 退出码约定

**A. `hitrace --boot_trace …` / `hitrace --boot_trace off`（配置路径，走通用 `main`）**

| 退出码 | 含义 |
|--------|------|
| 0 | 配置成功写入，或 `off` 成功将 count 置 0 |
| **-1**（shell 中或见 **255**） | **`geteuid()!=0`** 且解析到 **`--boot_trace` / `--boot`**：控制台含 **`error: unrecognized option '--boot_trace'.`**（见 §3.3.9） |
| **-1** | **`const.debuggable` 不允许**（`HandleBootTraceConfig`）：**`error: parsing args failed, exit.`** |
| **-1** | 参数/tag 校验失败、`off` 写 persist 失败、写 cfg 失败、**`InitAndCheckArgs`** 其它前置失败等（与仓内 `main` 返回值一致；除 **unrecognized** 行外，常见尾部为 **`parsing args failed, exit.`**） |

**B. `hitrace boot-trace`（`main` 早退；先门控，再 `RunBootTraceControl`）**

| 退出码 | 含义 |
|--------|------|
| **-1** | **`geteuid()!=0`**：**`error: unrecognized command 'boot-trace'.`**（**不**进入 `RunBootTraceControl`） |
| **-1** | **`const.debuggable` 不允许**：**`error: parsing args failed, exit.`**（**不**进入 `RunBootTraceControl`） |
| 0 | **`RunBootTraceControl`**：配置加载成功且 **OpenTrace → sleep → DumpTrace → CloseTrace** 全流程成功 |
| 1 | **`RunBootTraceControl`**：**重复拉起**：进入时 **`debug.hitrace.boot_trace.active` 已为 `"1"`**；会写 **`result=1`** |
| 2 | **`RunBootTraceControl`**：**无法置 `active=1`**、**配置加载失败**，或 **抓取失败**（Open/Dump/Close 任一步失败；与「纯配置 JSON 损坏」在数值上共用 **2**，请结合 **`result` 字段** 与控制台 **`error: boot_trace …`** 日志区分） |

**说明**：表 A 中 **`-1`** 需结合 **argv** 与**日志关键字**区分；表 B 中 **`-1`** 与 **`RunBootTraceControl` 的 0/1/2** 分层判断。配置子命令的 **1** 与执行子命令的 **1**（重复拉起）语义不同；自动化解析退出码时应区分 argv 是否含 **`boot-trace`**。

---

## 4. 相关代码索引

| 类型 | 路径 |
|------|------|
| CLI `boot_trace` / `boot-trace` | `cmd/hitrace_cmd.cpp` |
| 公共宏 | `common/common_define.h` |
| 错误码 | `common/hitrace_define.h` |
| Dump 仲裁 / 磁盘阈值 / `CloseTrace` no-op | `interfaces/native/innerkits/src/hitrace_dump.cpp` |
| init 触发与互斥 | `base/startup/init/services/modules/trace/init_trace.c`（`TryRunBootTraceByCount`、`SpawnHitraceBootTrace`） |
