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

**ADR-6：多线程做多大范围？**
- 选项 A：只做数据并行（场景分析、draw item 分发、UBO/instance 上传）——低成本、风险小。
- 选项 B：加并行命令录制（每 pass 录进独立 secondary CB，主线程 barrier + vkCmdExecuteCommands）。
- 前提：都需要先建线程池（引擎现在没有 job system）。
- `[TODO: 重写时定]`

**ADR-7：barrier 归属谁？**
- 选项 A：全归图（render graph 编译期算好，执行期主线程下发）——现状，最干净。
- 选项 B：graphics 的走 render pass 内 SubpassDependency，compute 的走图——现状的显式化。
- 思考框架：选项 A 对并行命令录制最友好（barrier 全在主线程）。
- `[TODO: 重写时定]`

**ADR-8：帧潜伏 = 几帧在飞？**
- 现状 `frameCount = 2`（双缓冲）。3 帧在飞进一步摊平 CPU 尖峰，但多一套命令缓冲/栅栏/信号量。
- `[TODO: 重写时定]`

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
- `[TODO]` 写到这里如果哪一节写不出来，那一节就是你的盲区——先补课再动笔。
