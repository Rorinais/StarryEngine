#!/usr/bin/env python3
"""
StarryEngine Python 绑定示例：逐帧驱动引擎，渲染默认场景（球体 + 地面）。

运行（仓库根目录）：
    ./script/run_python.sh

关窗或运行 2 分钟后自动结束。
"""
import time

import starryengine_py as se

se.initialize()
try:
    frame = 0
    # 30fps 节奏；关窗或跑满 2 分钟结束
    while se.is_running() and frame < 60 * 60 * 2:
        se.step()
        frame += 1
        if frame % 60 == 0:
            print(f"[py] rendered {frame} frames", flush=True)
        time.sleep(1 / 30)
finally:
    se.shutdown()
    print("[py] engine closed", flush=True)
