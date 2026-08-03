# 调度器

两种策略：`classic`（通用，默认推荐）与 `choreography`（按任务指定处理器）。配置在 `*.conf` 的 `scheduler_conf`；进程通过调度配置名加载（样例在 `autolink/conf/`）。

路径相对 `AUTOLINK_PATH`（WorkRoot），例如 `conf/example_sched_classic.conf`。

## classic

按 **group** 隔离 CPU（利于 NUMA），组内多优先级任务队列。

样例：`autolink/conf/example_sched_classic.conf`。

```protobuf
scheduler_conf {
    policy: "classic"
    process_level_cpuset: "0-7,16-23"
    threads: [
        { name: "async_log" cpuset: "1" policy: "SCHED_OTHER" prio: 0 }
        { name: "shm" cpuset: "2" policy: "SCHED_FIFO" prio: 10 }
    ]
    classic_conf {
        groups: [
            {
                name: "group1"
                processor_num: 16
                affinity: "range"      # 或 "1to1"
                cpuset: "0-7,16-23"
                processor_policy: "SCHED_OTHER"  # 或 SCHED_FIFO / SCHED_RR
                processor_prio: 0
                tasks: [ { name: "E" prio: 0 } ]
            }
        ]
    }
}
```

| 字段 | 说明 |
|---|---|
| `process_level_cpuset` | 进程可用 CPU |
| `threads[]` | 命名线程（日志、SHM 等）绑核与调度策略 |
| `affinity` | `range`：线程共享一组核；`1to1`：线程与核一一绑定（数量须一致） |
| `processor_policy` / `processor_prio` | `SCHED_FIFO`/`SCHED_RR`：prio 1–99；`SCHED_OTHER`：nice −20–19 |
| `tasks[].prio` | 同优先级进同一队列；高优先级先调度 |

更多样例：`control_sched_classic.conf`、`compute_sched_classic.conf`。

## choreography

在熟悉任务图、需要「任务 → 固定处理器」时使用。样例：`autolink/conf/example_sched_choreography.conf`。

```protobuf
scheduler_conf {
    policy: "choreography"
    process_level_cpuset: "0-7,16-23"
    threads: [
        { name: "lidar" cpuset: "1" policy: "SCHED_RR" prio: 10 }
        { name: "shm" cpuset: "2" policy: "SCHED_FIFO" prio: 10 }
    ]
    choreography_conf {
        choreography_processor_num: 8
        choreography_affinity: "range"
        choreography_cpuset: "0-7"
        choreography_processor_policy: "SCHED_FIFO"
        choreography_processor_prio: 10

        pool_processor_num: 8
        pool_affinity: "range"
        pool_cpuset: "16-23"
        pool_processor_policy: "SCHED_OTHER"
        pool_processor_prio: 0

        tasks: [
            { name: "A" processor: 0 prio: 1 }
            { name: "B" processor: 0 prio: 2 }
            { name: "C" processor: 1 prio: 1 }
            { name: "E" }   # 未指定 processor 时进 pool
        ]
    }
}
```

| 字段 | 说明 |
|---|---|
| `choreography_*` | 编排处理器池：数量、亲和、cpuset、实时策略 |
| `pool_*` | 默认/溢出任务池（未钉死 processor 的任务） |
| `tasks[].processor` | 任务绑定的编排处理器下标 |
| `tasks[].prio` | 同处理器内优先级 |

更多样例：`control_sched_choreography.conf`、`compute_sched_choreography.conf`。

## 选型

| 情况 | 建议 |
|---|---|
| 不清楚车上 DAG / 先跑通功能 | `classic` |
| 已量化任务依赖与核占用 | `choreography` |
| 仅改绑核试性能 | 先改 `process_level_cpuset` / group `cpuset`，再动 policy |

实时策略（`SCHED_FIFO`/`RR`）通常需要相应权限（如 `CAP_SYS_NICE` 或 root）。
