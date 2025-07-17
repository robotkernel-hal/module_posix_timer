The module **libmodule_posix_timer.so** is used to generate deterministic triggers for other modules.

# Functional principle 

The posix timer module supports three different modes. 

## Modes

Two different modes are supported by **module_posix_timer**.

* ***nanosleep :*** In this mode the **module_posix_timer** main thread just does a nanosleep until the period time has been elapsed. If the nanosleep call was interrupted by some signal it will sleep until the calculated period end time has been reached. This mode is easy and efficient as well. The **module_posix_timer*** thread should run at a very high priority to ensure, that it will be waken up when it's necessary. 
* ***posix_timer :*** This mode creates a timer with timer_create. It configures the timer and connects it to the given signal number from the configuration string. 
* ***busywait :*** In busywait the timer threads does active wait on the cpu and consumes all cpu time. This can cause higher power consumption and higher temperature (But on a PREEMPT-RT system that should not matter). 

## Example config file

This example config file can be used as a template for own configurations.

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

## Trigger device

This module registers a trigger device for each timer to robotkernel with the following naming schemeː

```
<module_name>.<timer_name>.trigger
```

## Process data device

The module also provides a cyclic process. It contains the actual timer interval.

```
<module_name>.<timer_name>.inputs.pd
```
