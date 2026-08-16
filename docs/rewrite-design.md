# StarryEngine 重写设计文档

> **用途**：RHI/渲染骨架重写前的设计输入。写给未来的自己——先写清楚再做，写的过程就是巩固。
> **状态**：参考模板（约束清单已填，架构决策分"已定/待定"，待定的留给你定）
> **日期**：2026-08-12

---

## 0. 目标与范围

**重写什么**（骨架层）：
- RHI 接口层 + 后端（VK）
- render graph（声明式图、依赖分析、barrier、布局管理）
- pass / executor 分工
- 材质 → 反射 → 描述符绑定链路
- 帧循环 / 多线程模型

**不做什么**：
- 不先复刻全部功能。粒子、蒙皮、后处理、Python 绑定、帧捕获、ImGui 是**验证骨架的用例**，不是重写目标。
- 不重写资产加载（ModelLoader、TextureLoader 等已经收敛，直接用）。
- 不把重写做成"多后端抽象设计"——接口只做中性（零 `Vk*` 泄漏），不做投机抽象（ADR-10）。

**验证标准**：
- 与旧引擎跑同一场景，**帧画面一致**、无验证层报错。
- 每个里程碑对照旧引擎验收后才进入下一步。

---

## 1. 约束清单（invariants）

> 这些是**踩坑换来的硬事实**，must 级别，不允许从第一性原理"重新推导"。
> 每一条都是重写时新架构必须满足的约束。写得出 = 已巩固，写不出 = 盲区。

### 图形学 / API 层

- `must:` 深度贴图采样必须走 **DEPTH-only 视图**。默认视图是 DEPTH|STENCIL，当渲染目标合法、当采样图非法（VUID-02275）。→ `getSamplingView()` 懒创建。
- `must:` 纹理**布局追踪必须与真实布局一致**。graph 的 barrier 不更新 RHI 纹理内部布局追踪（构造时全 `Undefined`）；任何读回/转换后必须显式 `transitionLayout` 回来，否则第 2 帧起 oldLayout 错位。
- `must:` swapchain 图像没有 `TRANSFER_SRC`，不能读回最终画面；要输出帧序列必须读**离屏 SceneColor**（且离屏纹理 usage 恒带 TRANSFER_SRC）。
- `must:` 可复用的持久命令缓冲（异步读回、每帧重录）不能从 `TRANSIENT` 命令池分配——TRANSIENT 池禁止 `vkResetCommandBuffer`（VUID-vkResetCommandBuffer-commandBuffer-00046）。要么专用非 TRANSIENT 池（带 `RESET_COMMAND_BUFFER_BIT`），要么每帧从池里重新分配。
- `must:` ResourceManager 按名登记、重名即拒绝 → 多个 staging/读回缓冲必须**每槽唯一名**（如 `FrameCapture_Staging_%u`），否则 createBuffer 返回无效句柄、每帧静默失败。
- `must:` 时间戳读回三条铁律：①`waitForFrame(当前槽位)` **之后**才读；②**非阻塞**读（无 `WAIT_BIT`，`VK_NOT_READY` 容错）；③槽位 **submit 过至少一次**才读（查询池首次用前必须先被 reset+写，否则 VUID-09401）。坑：读上一帧槽位 + WAIT_BIT 会在帧中途插隐式全管线同步，Wayland/NVIDIA 上死锁 `acquireNextImage`（栈 `drmSyncobjTimelineWait`）；读未初始化池报 VUID-09401。已修：读当前槽位 + 去 WAIT_BIT + per-slot `hasSubmittedFirstFrame`（FrameContext）。
- `must:` read-texture 跨 pass 读的同步：graphics 走 render-pass 级 SubpassDependency；compute 走布局转换 barrier，**读侧 stage 按 reader 类型选**（compute reader 必须 `ComputeShader`，`FragmentShader` 在 compute pipeline 里不执行 → barrier 无效）。
- `must:` `vkUpdateDescriptorSets` 的 external sync 在**描述符池**上，不是 set 上。多线程更新描述符必须每线程独立池或串行化 pool。
- `must:` 布局转换 barrier（`vkCmdPipelineBarrier` 带 image layout 变更）**不能放进 secondary command buffer**（能力受限），必须留在主命令缓冲。
- `must:` 改 PSO 字段必须同步改 `operator==` / `hash`（否则管线缓存命中错乱）。
- `must:` RHI 纹理对 depth 纹理须尊重 `mDesc.format`（含 stencil 分量）+ aspect。

### 资产 / 格式

- `must:` FBX 骨骼四元数是 `(x,y,z,w)` 顺序，反了动画全炸。
- `must:` Assimp **头/库必须同小版本**，否则 49803 个"坏键"是 ABI 错位而非脏数据；改 include 前先 `sizeof(aiQuatKey)` 核对。
- `must:` SSBO 材质链路：`setStorageBuffer` 显式建真实尺寸的 buffer（运行时数组反射 blockSize==0，不预建）。
- `must:` 动画时长直接用 `mDuration`，别按旋转键截断。

### 平台 / 窗口

- `must:` 透明窗口必须走**原生 Wayland**，XWayland 只支持 OPAQUE。
- `must:` Wayland + NVIDIA：RHI 必须在窗口（wayland 连接）**之前销毁**；Application 析构绝不 reset `m_window`（它最后死），只提前释放 scene/renderer/monitor/rhi。

### 构建 / 环境

- `must:` Linux 零系统依赖：assimp 源码静态构建（FetchContent file:// 裁剪版 tar.gz）。
- `must:` fmt 双版本混用会炸 → 拷 bundled 到 `include/fmt/` 单根解析。
- `must:` Python 绑定坑：验证层 `VK_LAYER_PATH`、无 `python3-dev` 头、PIC、multiarch pyconfig。
- `must:` `LINK_GROUP:RESCAN` 须带 `$<>` 且不传播 include。

### 多线程 / 帧架构

- `must:` 帧内并行（软光线追踪式 job/tile 模型，见 ADR-6）的前提：帧工作切成**互不相交**的独立 job（按对象/材质/pass 分片），job 只写自己的输出、只读共享常量；线程池执行 + 帧 barrier（submit 前等全部 job 完成）。帧内并行默认 join-before-submit（无潜伏）；渲染 job 与下一帧数据 job 的帧间解耦是可选增强（per-slot + 帧潜伏，见 ADR-6 统一框架）。
- `must:` 多帧在飞（frameCount=2）时，任何被 CPU 每帧写、GPU 跨帧读的缓冲必须 **per-in-flight-slot**（骨骼 SSBO、globals UBO、材质 SSBO）——单份 buffer 会被下一帧 CPU 写覆盖掉在途帧。`AsyncBoneProducer`（demo/AsyncBoneProducer.hpp）只有 CPU 侧双缓冲、GPU 侧单份，是既有 race，重写必须补 slot。
- `must:` 一个帧循环（acquire → 录制 → submit → present）只能由一个线程持有；描述符更新走 per-thread pool（VK 外同步在 pool 不在 set，见图形学/API 层）。

---

## 2. 现状盘点

### 保留的概念（设计正确，重写时沿用概念）

- **render graph 声明式 + 依赖分析**：按纹理/缓冲读写建依赖，拓扑排序，culling。
- **barrier 生成有序列表**：`m_layoutTransitions` 编译期算好、执行期顺序下发——这是并行命令录制最需要的骨架。
- **pass / executor 分工**：pass 管结构（configure/onSceneData），executor 管执行。
- **反射驱动绑定定位**：spirv-cross → `m_samplerBindings[name]={set,binding}`，`setTexture("uXxx",...)` 按名查，不用硬编码 set/binding。

### 债务（重写时重做）

- **graphics / compute 绑定模型分叉**（机制二）：graphics 走反射按名 + `updateMaterialTextures`；compute 走 `ComputePassDesc` 声明式 + 直写描述符。两套并存，重写时决定要不要统一。
- **单线程命令录制**：所有 pass 录进一条主命令缓冲，CPU 帧内零并行。
- **布局追踪与真实布局易失同步**：graph barrier 不管 RHI 内部追踪，靠调用方自觉恢复。
- **pass 共享逻辑散落**：同 tag/同目标共享 pass 的 dedup 机制、read-texture 机制曾经放错层（见 ADR-1 的教训）。

### 已实现的验证用例：帧序列捕获（异步读回管线）

> 骨架的验证用例之一，全异步管线实测通过：300 帧捕获 exit 0、指定帧导出、验证层零报错、**捕获期间满帧率（60 FPS vsync 上限，此前 10-12 FPS）**。

- **触发**：`STARRY_FRAME_DUMP=<目录>` 激活；`STARRY_FRAME_COUNT=<帧数>`（默认 1，0=持续）；`STARRY_FRAME_INDEX=<N>` 只导出第 N 帧（忽略 COUNT）；`STARRY_FRAME_EXIT=1` 录满自动关窗退出（走 FrameCapture 析构 drain，验证无丢帧/无死锁）。
- **管线**：每帧 present 后（postRender 回调）提交一次异步读回 → worker 池（8 线程，FrameCapture 自有，不占 JobSystem）等 fence → map staging → 软件 sRGB 转换 → PNG。
- **不丢帧**：kSlots=8 个 staging 槽位做环形缓存（内存上界 = 8 帧 ≈ 98MB）；无空闲槽时主线程背压等待（condvar），绝不丢帧。**并发编码数 = min(kSlots, kWorkers)**：worker 拿 job 即占住槽位直到编码完，槽位少于 worker 会让多余 worker 闲置（4槽+8worker 实测 13 FPS）。
- **编码吞吐是瓶颈，已三连提速**：① sRGB OETF 的 powf 每像素 ~100 周期 → 8192 项 LUT（静态初始化，误差 <1/255）；② stbi 自带 zlib 把 quality 钳到 ≥5（`if (quality < 5) quality = 5`，设级别无效）且每行试 5 种滤波 → 换成自写 PNG 编码器（Sub 滤波 + stored-deflate 块，~130 行，仅捕获路径用）；③ 单帧编码 355ms → ~65ms，8 worker 并发 → 60 FPS。代价：文件 ~6MB/帧（stbi 压缩 ~1.9MB）——调试帧序列可接受。
- **同步零阻塞**：读回 = transition(ShaderReadOnly→TransferSrc) + copy + transition(TransferSrc→ShaderReadOnly) 一条持久命令缓冲 + fence，`submitAsync`（无 waitIdle）。调用时机在本帧 submit 之后 → 队列序天然读本帧。布局追踪显式转回 ShaderReadOnly。
- **析构 drain**：先 `waitIdle`（fence 必信号）→ 停 worker（队列空才退出）→ 释放 staging。析构前所有在飞读回全部落盘，不丢帧。
- **文件**：`src/renderer/utils/FrameCapture.{hpp,cpp}`（含极速 PNG 编码器）、`Device::submitAsync`、`RHI_VK_RESOURCE::readbackAsync`、`IRHI` 槽位 API（begin/wait/releaseAsyncReadback）、demo 的 `setExitCallback`。

---

## 3. 架构决策（ADR）

> 格式：**决策 / 为什么 / 放弃**。已定的是这次会话敲定的；待定的留给你，附思考框架。

### 已定（直接沿用）

**ADR-1：read-texture 机制放 IPass（基类）**
- 为什么：声明/名字解析/pass 排序/culling 全部类型无关；同步层补齐 compute 后（读侧 stage 按 reader 类型选）两种 pass 都成立。
- 放弃：只放 GraphicsPass —— 曾因 barrier 仅 graphics 成立而撤回，补 compute barrier 后才重新提升。**教训：不通用就不能提基类；通用性取决于最弱的那个类型。**

**ADR-2：compute 读侧 stage 按 reader 类型选**
- 为什么：`ShaderReadOnly` 默认映射 `FragmentShader`，但 compute pipeline 无 fragment 阶段 → barrier 无效（验证层报 VUID）。
- 放弃：让 compute 也塞 SubpassDependency —— compute 没有 render pass，此路不通。

**ADR-3：深度贴图用普通 `sampler2D` + 手工 PCF，不用 `sampler2DShadow`**
- 为什么：bias 要逐片断按斜率算（`max(1-NdotL,0)`），硬件比较采样器 bias 是每 sampler 固定值；且要读原始深度 `d` 做 `(d+bias < currentDepth)`。
- 放弃：硬件比较 PCF（更快、2×2）——代价是失去逐片断 bias 控制。
- 代价记录：9 次采样 vs 1 次，换取图案可控 + 斜率 bias。

**ADR-4：`writeTexture(binding, ...)` 是唯一通用绑定原语**
- 为什么：graphics（`setTexture` 反射按名）和 compute（声明式 desc）两条机制二路径最终都收敛到这同一个 RHI 原语；它还内部吞掉 `getSamplingView()` 的 depth-aspect 细节。
- 放弃：给两类 pass 各写一套绑定 → 收敛点唯一，上层分叉（binding 怎么定、sampler 哪来）留在政策层。
- 加分证据（ADR-9）：反射按名绑定也是 DX12 友好的形状——DX12 描述符绑定是根签名参数索引，按名映射比按 set/binding 数字自然。

**ADR-9：暂不加第二后端；若将来加，选 DX12 不选 OpenGL；抽象形状按"能被 DX12 接住"写**
- 为什么：
  - OpenGL 与 VK 执行模型正交（隐式同步、无 barrier、无 render pass/subpass），加 GL 会迫使 IRHI 抽象变成"隐式同步"形状，反过来污染唯一后端。
  - DX12 是唯一与 VK 同族的公开 API：resource states≈image layout、`ResourceBarrier`≈`vkCmdPipelineBarrier`、根签名+描述符堆≈pipeline layout+描述符集、command list≈command buffer、`ID3D12Fence`≈`VkFence`。语义几乎 1:1 → 保持 VK 形状的 IRHI，DX12 后端近乎机械移植，是验证抽象中立性的理想目标。
  - 但开发机是 Linux：DX12 无原生实现，vkd3d-proton 底层还是 Vulkan（验证不了"独立后端"），此轮不写。
- 约束：公共层零 `Vk*` 泄漏；接口只声明 VK/DX12 共有概念（布局/状态、barrier、按槽位绑定、命令列表录制、fence），实现细节留给后端。
- 对重写的影响：绑定接口按**名字**走（ADR-4）；别把 subpass 融合当图架构必须项（DX12 无 subpass）；布局同步写"何时需要什么状态"而非强制手动 barrier（DX12 有 state promotion/decay 隐式转换）。
- 放弃：为 GL 把抽象掰成隐式同步形状；用 vkd3d-proton 当"DX12 后端"（那不是独立后端）。

**ADR-10：接口的"通用"只做中性（neutrality），不做投机抽象（abstraction）**
- 为什么：重写时审视接口，"让接口更通用"要拆成两种：
  - **中性（该做）**：公共层不泄漏后端类型（零 `Vk*`），接口只描述 VK/DX12 共有概念（ADR-9 约束的落地）。成本低，防止 IRHI 漂成 VK 形状。
  - **抽象（不该做）**：为"可能的未来"（第二个后端/未知 pass）设计多态、接口层、`virtual`。没有第二个真实消费方之前，抽象是投机——写具体的，等第二个真实消费方出现再提升。
- 教训来源（ADR-1）：通用性由**最弱的类型**决定；"不通用就不能提基类"。提升时机是"第二个真实类型出现"，不是"以后可能有用"。
- 审查清单（重写时逐条过公共接口签名）：
  1. 签名里有 `Vk*` / `Vulkan*` / `RHIVK*` 类型吗？→ 改成概念类型（布局/状态/资源/fence/command list）。
  2. 描述的是概念还是实现？（"要一个 depth-only 采样视图"是概念；`getSamplingView()` 的 VK 细节是实现）。
  3. 命名是按概念还是按 API？（`ImageLayout`、`TextureHandle`，不是 `VkImageLayout`）。
  4. 有吗——没有第二个消费方的 `virtual` / 接口层？→ 拆掉，写具体。
  5. 每个要提到基类的 API：**所有子类都成立吗？**（compute 读侧 stage 的教训）。
- 放弃：把重写做成"多后端抽象设计"——重写目标是巩固已知，不是给未知后端盖庙。

### 待定（重写时你定）

**ADR-5：机制二（描述符绑定）要不要在骨架层统一成一套模型？**
- 现状两套：graphics 反射按名；compute `ComputePassDesc` 声明式。
- 思考框架：
  - 统一的话，compute 的 sampler/buffer 绑定也走反射？那要 MaterialTemplate 加 compute 反射访问器。
  - 不统一的话，接受"政策层分叉、原语层收敛"（ADR-4）——两套都薄，坏处是可接受。
- `[TODO: 重写时定]`

**ADR-6：多线程做多大范围？——软光线追踪式 job/tile 并行（用户心智模型），分两步，重写时按序实现**

**第 1 步：数据线程（选项 A）——已有参考实现**
- 内容：把每帧 CPU 计算摘到单条数据线程（producer/consumer 双缓冲、软开关）。
- 参考：`demo/AsyncBoneProducer.hpp`（骨骼动画端到端跑通，软开关 `STARRY_ASYNC_BONES=1`）。
- 局限：只并行"算数据"这一块，且是单条线程；不是用户要的软光线追踪式并行。

**第 2 步：job/tile 并行（软光线追踪式）——本次敲定的设计输入**
- 目标：像软件光追把屏幕切成 tile 一样，把每帧工作切成大量**互不相交**的独立 job，交给**线程池**跑；帧 barrier（submit 前等全部 job 完成）后主线程再提交。帧内并行、无管线式潜伏（不存在"领先 N 帧"）。
- 帧工作的切法（对照软 RT 的 tile）——**四类已全部落地**（ADR-6 第 2 步，2026-08）：
  - 骨骼计算：按网格 → 每网格一个 job。**✓ 已建**：demo 的 `AsyncBoneProducer` 在池激活时把"推进时间→算骨骼→memcpy 进材质 SSBO"作为一个 job 提交进共享池（onUpdate 提交、renderFrame 数据 phase `waitAll()` 兜住）；`setStorageBuffer` 首次调用（建 buffer+写描述符）主线程预热，之后只是 memcpy。线程归属三级：池 > 独立线程（`STARRY_ASYNC_BONES`）> 同步内联。
  - 材质 CPU dirty 块：按材质实例 → 每实例一个 job（`applyAllDirtyBlocks`，零描述符写，per-material 安全）。
  - 实例缓冲上传：按对象 → 每对象一个 job（`updateInstanceBuffers` 的 memcpy，per-object 安全）。
  - 命令录制（渲染本体）：每 pass → 一个 job 录独立 **secondary CB**，主线程 barrier + `vkCmdExecuteCommands`。`ParallelRecordingContext` + `RenderGraph::execute(…, parallel)` Phase1（job 录 secondary）/Phase2（主线程 barrier+executeCommands）；FrameContext per-worker 命令池（`allocateWorkerCommandBuffer`/`resetWorkerCommandPools`，[帧槽位][worker]，只复位当前槽位）。
- 线程池：**已提前建好**（[src/core/include/core/JobSystem.hpp](src/core/include/core/JobSystem.hpp)，`submit`/`waitAll` 帧 barrier + 软开关 `enableThreads=false` 同步内联 + 自测 `JobSystem_test`）。实现 = mutex+deque+condvar + pending 计数器；无锁 grab-next 是可选性能演进（ADR-10 先不做）。
- 软开关：同步（现架构）→ 数据线程（AsyncBoneProducer）→ job 池，逐级加、每级 A/B 对照（AsyncBoneProducer 已验证"先内联后线程"两变量解耦的流程）。**已统一**：`STARRY_PARALLEL_RECORD=inline|threads` 现在是帧内并行主开关（数据 job + 录制 job 全部走同一池；无 env = 串行零变化）。JobSystem 在 Renderer 构造时按 env 建好（`getJobSystem()` 对 demo 可见）。
- **统一框架（渲染线程 ≠ job 系统的对立物）**：渲染就是 job 系统里的"收尾 job"——有依赖（帧 barrier 后）、串行独占（帧循环单线程持有，Vulkan 队列操作不能并发）。**默认形态**：帧内并行 + join-before-submit（软光追式，无潜伏）；**可选增强**：数据 per-slot + 接受帧潜伏后，让这个串行渲染 job 与下一帧的数据 job 并行（持续领活 = 渲染线程）。两者是同一 job 系统的两种调度形态，不是二选一。**帧间解耦已落地（ADR-6 终点，2026-08）**：
  - **Stage 1（per-slot 双缓冲，行为中性）**：每帧 CPU 写/GPU 读的缓冲 ×2 按帧槽位（globals UBO、材质 UBO/SSBO、骨骼 SSBO、实例缓冲）+ 引用它们的描述符集 ×2。帧 N 数据写槽 N%2、录制绑槽 N%2 → GPU(N) 只读槽 N%2；帧 N+2 重写前 `waitForFrame(N%2)` 已放行 → 修掉"单份 buffer 被下一帧 CPU 写覆盖在途帧"的 latent race。槽位在 kick 时捕获进 job lambda（JobSystem inline 模式 job 在 `submit()` 同步执行，job 内读成员拿到旧值）；`setBufferData`/`applyAllDirtyBlocks` 首写灌满两槽（`writtenOnce`），纹理/输入附件绑两槽。数据 job 零描述符写（缓冲建/描述符写在主线程预热/反射期），worker 内只 memcpy → per-slot 安全。
  - **Stage 2（帧间解耦调度，软开关 `STARRY_FRAME_IN_FLIGHT=1`）**：主线程持续渲染 N，worker 在 GPU(N) 执行期算 N+1 的数据（1 帧数据潜伏）。`renderFrame` FID 分支：开头 `waitAll()` join 上帧末尾 kick 的 data(N) → `updateGlobals(clock, N%2)`（相机/时间在帧内，不潜伏）→ `m_renderPath->render(N)` → 末尾 kick `submitFrameDataJobs((N+1)%2)` + 骨骼 provider（不等待，帧 N+1 开头 waitAll join）。**首帧**没有上一帧的数据可 join，同步填本帧槽位再录（材质块首写灌两槽必须在 GPU(0) 读之前）。**骨骼 kick 挪进渲染器**：demo 注册 `Renderer::setFrameDataBoneProvider`，渲染器在数据相调用（onUpdate 只存 `m_lastDelta`；若在 onUpdate kick，会被开头 waitAll 一并 join，overlap 失效）。无 `STARRY_PARALLEL_RECORD` 时 FID 隐式开线程池（需要 worker 才有重叠）。默认（无 FID env）保持 join-before-submit 结构、per-slot 仍在。
  - 残余 race（诚实标注）：FID 下 data(N+1) 写槽 (N+1)%2 时 GPU(N-1) 可能仍读同槽位（(N-1)%2==(N+1)%2）；仅 GPU 帧超时(>16ms)才显形，@60fps 有大量余量。airtight = 帧潜伏 3（ADR-8，已推迟，见下）。

**ADR-7：barrier 归属谁？**
- 选项 A：全归图（render graph 编译期算好，执行期主线程下发）——现状，最干净。
- 选项 B：graphics 的走 render pass 内 SubpassDependency，compute 的走图——现状的显式化。
- 思考框架：选项 A 对并行命令录制最友好（barrier 全在主线程）。
- `[TODO: 重写时定]`

**ADR-8：帧潜伏 = 几帧在飞？**
- 现状 `frameCount = 2`（双缓冲）。3 帧在飞进一步摊平 CPU 尖峰，但多一套命令缓冲/栅栏/信号量。
- 关联 ADR-6 帧间解耦：FID 下 data(N+1) 写槽 (N+1)%2 与 GPU(N-1) 读同槽位的残余 race，airtight 解法就是**帧潜伏 3**——槽位 3 后 data(N+1) 与 GPU(N-2) 不再撞。**已推迟（本阶段不做）**：@60fps 的 GPU 帧余量远大于 16ms，残余 race 不触发；且 frameCount=3 要动 FrameContext/命令池骨架/全部 per-slot 数组宽度（`RHI::kMaxFramesInFlight` 是单一事实源，改一处）。

---

## 4. 数据流（当前帧，重写时画目标帧对照）

```
Application::step
  └→ m_rhi->renderFrame(cb)                    // 单线程主帧循环
       ├→ Renderer::renderFrame(encoder, idx)
       │    ├→ updateInstanceBuffers(obj*)     // 逐对象 memcpy，可并行
       │    ├→ buildSceneResources            // 场景分析（变化才做）+ 图重建
       │    ├→ updateDynamicBuffers           // camera UBO + 材质 applyAllDirtyBlocks（逐材质，可并行）
       │    └→ m_renderPath->render(encoder)  // → RenderGraph::execute
       │         └→ 逐 sorted pass：
       │              ├→ pipelineBarrier(布局转换)   // m_layoutTransitions 有序列表
       │              └→ pass->execute(encoder)     // 全部录进同一条主命令缓冲 ← 并行化主战场
       ├→ submitFrame（栅栏 + 信号量 + present，单线程）
```

**关键观察**：
- frameCount=2，CPU/GPU 已部分重叠；缺的是 CPU 帧内并行。
- barrier 已算成有序列表 → 拆"主线程发 barrier + worker 录 pass body"有现成骨架。

**目标帧（重写后，软光线追踪式 job/tile 并行，ADR-6 第 2 步）**：
```
主线程 step:
  ├→ clock/camera/scene update（只写共享常量，job 只读）
  ├→ 提交帧 job 集：
  │    骨骼×网格 / 材质 dirty×实例 / 实例缓冲×对象 / 录制×pass(→secondary CB)
  ├→ 帧 barrier：等全部 job 完成（submit 前不往前走）
  ├→ 主命令缓冲：barrier + vkCmdExecuteCommands(各 secondary CB)
  └→ submit + present（acquire/主录制/提交仍单线程持有）
线程池 worker×N:
  └→ 循环 grab 下一个 job → 算 → 只写自己的 tile（对象/材质/pass）输出，帧末 join
```
**关键变化**：
- 帧工作切粒并行（像软 RT 切 tile），多核摊平 CPU 尖峰；无管线式潜伏（帧 barrier 保证 submit 前全部 job 完成）。
- 录制并行需要 per-thread 命令池（FrameContext 已有）+ secondary CB 能力检查；布局转换 barrier 仍留主命令缓冲（能力受限，见约束清单）。

**FID 变体（帧间解耦，`STARRY_FRAME_IN_FLIGHT=1`，ADR-6 终点）**：上面是默认 join-before-submit 形态（数据 job 在录制前 join）。FID 把数据相挪到"本帧渲染之后、GPU(N) 执行期"，worker 算 N+1 的数据（1 帧数据潜伏），帧 N+1 开头 `waitAll()` join：
```
renderFrame(N) [FID]:
  waitAll()                        # join 上帧末尾 kick 的 data(N)
  buildSceneResources()
  updateGlobals(clock, N%2)        # 相机/时间在帧内，不潜伏
  m_renderPath->render(...)        # 录制(N) + Phase1 waitAll + execute(N)
  submitFrameDataJobs((N+1)%2)     # kick 实例/材质 dirty → (N+1)%2，不等待
  m_boneProvider((N+1)%2)          # demo 骨骼 job（Renderer::setFrameDataBoneProvider），不等待
```
首帧无上一帧数据可 join → 同步填本帧槽位再录（材质块首写灌两槽须在 GPU(0) 读之前）。

---

## 5. 组件划分（目标骨架）

```
[应用层]  Application / demo
[渲染路径] RenderPath（纯编排，pass 列表）        ← 旧: DeferredRenderPath
[图]     RenderGraph（虚拟纹理/缓冲、依赖、barrier、culling） ← 旧: RenderGraph
[Pass]   IPass / GraphicsPass / ComputePass / ParticlePass
[Executor] IPassExecutor / SceneDrawExecutor / ComputeExecutor
[RHI]   IRHI / RHICommandEncoder / ResourceManager / DescriptorSet
[材质]  MaterialTemplate（反射）→ MaterialInstance（描述符）  ← 旧: DefaultMaterialTemplate
```

依赖方向：应用 → 渲染路径 → 图 → pass → executor → RHI；材质被 pass 引用，不反向依赖。

**重写时每模块的落点**（对照旧文件，写代码时逐条核）：
- 布局追踪：谁的职责？（当前是"graph 不管 + 调用方自觉"——ADR 里该有答案）
- 描述符绑定：统一还是分叉？（ADR-5）
- 命令录制：单线程还是并行？（ADR-6）

---

## 6. 反模式 / 陷阱

- **把不通用的事提到基类**：`addReadTexture` 曾因 barrier 仅 graphics 成立被撤回。教训：先确认所有类型通用，再谈提升。
- **布局追踪与真实布局脱节**：graph barrier 不更新 RHI 追踪，读回后不恢复 → 第 2 帧起验证层报错/UB。
- **硬编码绑定位置**：`addTextureDependency("ShadowMap", 2, 6)` 能工作，但绑定位置已在反射里——硬编码会漂移。
- **单点提交阻塞**：每帧 `waitForFrame(当前槽)` 已双缓冲；别再回到"每帧 waitIdle"。
- **把经验事实当"可推导"**：FBX 四元数顺序、ABI 版本、Wayland 销毁顺序——重写时忘了就是重新踩。

---

## 7. 里程碑（每步对照旧引擎验收）

| 里程碑 | 内容 | 验收 |
|---|---|---|
| M0 | 写本文档 + 建线程池 | 文档过一遍约束清单，每行都懂 |
| M1 | 新骨架最小链路：swapchain → 一个三角 → present | 画面出现、无验证层报错 |
| M2 | render graph：虚拟纹理、依赖分析、barrier、多 pass | 两个 pass 正确衔接、布局正确 |
| M3 | 反射绑定链路：模板 → 反射 → 描述符 | `setTexture("uXxx")` 按名可用 |
| M4 | 场景分析 + draw item 分发 + 网格 PSO | 角色/网格与旧引擎一致 |
| M5 | 多线程（按 ADR-6 范围） | 帧率提升 / 无验证层报错 |
| M6 | 深度/阴影、stencil 等用例回填 | 与旧引擎画面一致 |

---

## 8. 开放问题

- `[TODO]` 机制二统一模型？（ADR-5）
- `[TODO]` 多线程范围？（ADR-6）
- `[TODO]` barrier 归属？（ADR-7）
- `[TODO]` 布局追踪到底归 graph 还是 RHI？（关联 ADR-7）
- `[TODO]` 帧潜伏 2 还是 3？（ADR-8）
- `[FIXED]` **teardown 卡死（2026-08-14 已修，根因是读回 fence bug，非 NVIDIA）**：程序化关窗后进程挂死。根因：`destroyAsyncReadbackSlots` 对**从未 submit 过**的 fence 无条件 `vkWaitForFences(UINT64_MAX)` —— fence 在 `initialize()` 就创建，无捕获/捕获帧数 < 槽位数时从未提交，wait 永不返回。怪象全解：8 帧序列恰好灌满 8 个槽 → 干净；1 帧/无捕获 → 死等。修复：per-slot `m_readbackSubmitted` 标志，只等 submit 过的 fence（clear() 已 waitIdle 必信号，此处兜底）。当时 gdb 栈里的 `drmSyncobjTimelineWait` 是红鲱鱼（vsync acquire 等待），插桩才定位到真挂点。
- `[TODO]` 写到这里如果哪一节写不出来，那一节就是你的盲区——先补课再动笔。
