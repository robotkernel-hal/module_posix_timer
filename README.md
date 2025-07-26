# module_posix_timer

[![Build and Publish Debian Package](https://github.com/robotkernel-hal/module_posix_timer/actions/workflows/build-deb.yaml/badge.svg)](https://github.com/robotkernel-hal/module_posix_timer/actions/workflows/build-deb.yaml)
[![License: LGPL-V3](https://img.shields.io/badge/license-LGPL--V3-green.svg)](LICENSE)
[![Linux](https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=black)](#)
[![Debian](https://img.shields.io/badge/Debian-A81D33?logo=debian&logoColor=fff)](#)
[![Ubuntu](https://img.shields.io/badge/Ubuntu-E95420?logo=ubuntu&logoColor=white)](#)

**Robotkernel handler module for POSIX timer integration**

`module_posix_timer` provides a high‑priority timing mechanism based on POSIX timers for the robotkernel HAL. It abstracts timer setup and notification logic to deliver accurate periodic triggers within the robotkernel execution cycle.

---

## ✨ Features

- High‑resolution periodic timer 
- Real‑time safe execution 
- Integrates seamlessly with robotkernel event loop
- Adjustable timer frequency at runtime
- Optional callback hooks for custom application logic

Three different modes are supported by **module_posix_timer**.

* ***nanosleep :*** In this mode the **module_posix_timer** main thread just does a nanosleep until the period time has been elapsed. If the nanosleep call was interrupted by some signal it will sleep until the calculated period end time has been reached. This mode is easy and efficient as well. The **module_posix_timer*** thread should run at a very high priority to ensure, that it will be waken up when it's necessary. 
* ***posix_timer :*** This mode creates a timer with timer_create. It configures the timer and connects it to the given signal number from the configuration string. 
* ***busywait :*** In busywait the timer threads does active wait on the cpu and consumes all cpu time. This can cause higher power consumption and higher temperature (But on a PREEMPT-RT system that should not matter). 

---

## 🧩 Configuration

Use the following snippet in your Robotkernel handler configuration:

`main.rkc`
```yaml
name: posix_timer
so_file: libmodule_posix_timer.so
config: !include timer_0.rkc
```

This example config file can be used as a template for own configurations.

`timer_0.rkc`
```yaml
# Configuration file for module_posix_timer.
#
# vim: ft=yaml

#########################################################
# logging settings

# Standard robotkernel module local loglevel.
#loglevel: verbose

#########################################################
# timer settings
# module_posix_timer can create multiple timers. For each specified list
# entry in timers a new timer with <name> is created.

timers:
    # Unique timer name
  - name: <timer_name>

    # Defines the tick interval in seconds for the trigger device.
    interval: 0.002

    # Sets the operating mode of the module.
    # Values can be "nanosleep", "busywait" or "timer".
    #mode: nanosleep

    # Signal number to be used in "timer"-mode for timer_create.
    #signo: 35

    # Setting realtime priorities for the timer thread ("nanosleep" or "busywait" mode).
    prio: 60
    affinity: 0x01

    # Enable skipping missed cycles (Usually a bad option, e.g. something wrong, bad realtime , ..)
    # Values can be:
    # "none" - no skipping is done, every tick will be generated.
    # "normal" - skip all ticks which ly in the past minus the next.
    # "strict" - skip all ticks which ly in the past.
    #skip_missed: none 
```
| Parameter         | Description |
|-------------------|-------------|
| `name` | Unique name for trigger Device. |
| `interval`        | Seconds interval between timer triggers |
| `mode`            | Timer mode e.g. 'timer', 'nanosleep' or 'busywait' |
| `signo`     | Signal number to use in 'timer' mode |
| `prio`    | Thread priority in 'nanosleep' or 'busywait' mode |
| `affinity`    | Thread affinity in 'nanosleep' or 'busywait' mode |
| `skip_missed` | Slip missed cycles or catch up. |

---

## ⚙️ Runtime Behavior

- Timer initialized during module startup
- On each timer expiry:
  - Emits a HAL event or counter signal
  - Invokes optional callbacks for downstream modules
- Supports dynamic interval adjustment at runtime

This module registers a trigger device for each timer to robotkernel with the following naming schemeː

```
<module_name>.<timer_name>.trigger
```

The module also provides a cyclic process. It contains the actual timer interval.

```
<module_name>.<timer_name>.inputs.pd
```

---

## 🖼️ Architecture Diagram

```text
+-------------------+           +----------------------------+
|   Robotkernel-5   |<--------->|   module_posix_timer.so    |
+-------------------+           +-------------+--------------+
                                               |
                                     +---------v---------+
                                     | POSIX Timer API   |
                                     | (timer_create)    |
                                     +-------------------+
```

---

## 📦 Build & Installation

```bash
git clone https://github.com/robotkernel-hal/module_posix_timer.git
cd module_posix_timer
mkdir build && cd build
cmake ..
make
sudo make install
```

---

## 🧪 Testing

```bash
TODO
```

---

## 🤝 Contributing

Contributions, pull requests, and issue reports are welcome. Please:

- Keep builds warning-free
- Stick to robotkernel style rules
- Add tests for new features or regressions

---

## 📄 License

Distributed under the **LGPL-V3 License**. Refer to the [LICENSE](LICENSE) file for details.

---

**Robotkernel HAL Project** – Real-time robotics infrastructure powered by modular, modern C++


