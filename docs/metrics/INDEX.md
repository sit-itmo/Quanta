> Это каталог Linux. Windows: [WINDOWS_METRICS.md](../WINDOWS_METRICS.md).

# Каталог метрик / semantic version 1

- [os.name](os.name.md) — Operating system; `string`, `-`.
- [os.kernel.release](os.kernel.release.md) — Kernel release; `string`, `-`.
- [os.kernel.version](os.kernel.version.md) — Kernel build; `string`, `-`.
- [os.arch](os.arch.md) — Machine architecture; `string`, `-`.
- [os.hostname](os.hostname.md) — Host name; `string`, `-`.
- [os.uptime](os.uptime.md) — Time since boot; `f64`, `s`.
- [os.load.1m](os.load.1m.md) — Load average (1 minute); `f64`, `-`.
- [os.load.5m](os.load.5m.md) — Load average (5 minutes); `f64`, `-`.
- [os.cpu.online](os.cpu.online.md) — Online logical processors; `u64`, `-`.
- [os.memory.total_bytes](os.memory.total_bytes.md) — Physical memory; `u64`, `B`.
- [os.memory.available_bytes](os.memory.available_bytes.md) — Available memory estimate; `u64`, `B`.
- [os.memory.free_bytes](os.memory.free_bytes.md) — Completely free memory; `u64`, `B`.
- [os.memory.used_bytes](os.memory.used_bytes.md) — Memory: total minus available; `u64`, `B`.
- [os.memory.utilization](os.memory.utilization.md) — Memory utilization; `f64`, `1`.
- [os.swap.total_bytes](os.swap.total_bytes.md) — Swap capacity; `u64`, `B`.
- [os.swap.used_bytes](os.swap.used_bytes.md) — Used swap; `u64`, `B`.
- [cpu.utilization](cpu.utilization.md) — CPU busy fraction; `f64`, `1`.
- [cpu.time_total](cpu.time_total.md) — CPU accounted ticks; `u64`, `ticks`.
- [cpu.idle_total](cpu.idle_total.md) — CPU idle ticks; `u64`, `ticks`.
- [cpu.core.utilization](cpu.core.utilization.md) — CPU busy fraction; `f64`, `1`.
- [cpu.core.time_total](cpu.core.time_total.md) — CPU accounted ticks; `u64`, `ticks`.
- [cpu.core.idle_total](cpu.core.idle_total.md) — CPU idle ticks; `u64`, `ticks`.
- [board.vendor](board.vendor.md) — Board manufacturer; `string`, `-`.
- [board.model](board.model.md) — Board model; `string`, `-`.
- [board.serial](board.serial.md) — Board serial number; `string`, `-`.
- [bios.vendor](bios.vendor.md) — BIOS vendor; `string`, `-`.
- [bios.version](bios.version.md) — BIOS version; `string`, `-`.
- [bios.date](bios.date.md) — BIOS release date; `string`, `-`.
- [device.net.mac](device.net.mac.md) — Link-layer address; `string`, `-`.
- [device.net.state](device.net.state.md) — Operational state; `string`, `-`.
- [device.net.rx_bytes_total](device.net.rx_bytes_total.md) — Received bytes; `u64`, `B`.
- [device.net.tx_bytes_total](device.net.tx_bytes_total.md) — Transmitted bytes; `u64`, `B`.
- [device.net.rx_packets_total](device.net.rx_packets_total.md) — Received packets; `u64`, `packets`.
- [device.net.tx_packets_total](device.net.tx_packets_total.md) — Transmitted packets; `u64`, `packets`.
- [device.net.rx_errors_total](device.net.rx_errors_total.md) — Receive errors; `u64`, `errors`.
- [device.net.tx_errors_total](device.net.tx_errors_total.md) — Transmit errors; `u64`, `errors`.
- [sensor.temperature](sensor.temperature.md) — Temperature channel; `f64`, `degC`.
- [sensor.fan_rpm](sensor.fan_rpm.md) — Fan speed channel; `u64`, `rpm`.
- [cpu.vendor](cpu.vendor.md) — CPU vendor; `string`, `-`.
- [cpu.brand](cpu.brand.md) — CPU brand string; `string`, `-`.
- [cpu.cpuid_reference](cpu.cpuid_reference.md) — Logical CPU used for CPUID; `u64`, `-`.
- [cpu.vmx](cpu.vmx.md) — VMX hardware capability; `bool`, `-`.
- [cpu.svm](cpu.svm.md) — SVM hardware capability; `bool`, `-`.
- [cpu.sse2](cpu.sse2.md) — SSE2 capability; `bool`, `-`.
- [cpu.avx](cpu.avx.md) — AVX hardware capability; `bool`, `-`.
- [cpu.avx2](cpu.avx2.md) — AVX2 hardware capability; `bool`, `-`.
- [cpu.hypervisor](cpu.hypervisor.md) — Hypervisor-present CPUID bit; `bool`, `-`.
- [fs.root.total_bytes](fs.root.total_bytes.md) — Root filesystem capacity; `u64`, `B`.
- [fs.root.available_bytes](fs.root.available_bytes.md) — Root filesystem space available to this user; `u64`, `B`.
- [fs.root.used_bytes](fs.root.used_bytes.md) — Root filesystem allocated space; `u64`, `B`.
- [fs.root.utilization](fs.root.utilization.md) — Root filesystem allocated fraction; `f64`, `1`.
- [process.self.rss_bytes](process.self.rss_bytes.md) — Collector resident memory; `u64`, `B`.

Файл описывает полную сборку. `inv list --compiled` — авторитетный каталог текущего бинарника.
