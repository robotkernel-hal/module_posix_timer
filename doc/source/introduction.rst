============
Introduction
============

Robotkernel handler module for POSIX timer integration.

module_posix_timer provides a high‑priority timing mechanism based on POSIX 
timers for the robotkernel HAL. It abstracts timer setup and notification logic 
to deliver accurate periodic triggers within the robotkernel execution cycle.

Features
________

- High‑resolution periodic timer
- Real‑time safe execution
- Integrates seamlessly with robotkernel event loop
- Adjustable timer frequency at runtime
- Optional callback hooks for custom application logic

Three different modes are supported by module_posix_timer.

**nanosleep**
  In this mode the module_posix_timer main thread just does a nanosleep until the 
  period time has been elapsed. If the nanosleep call was interrupted by some signal 
  it will sleep until the calculated period end time has been reached. This mode 
  is easy and efficient as well. The module_posix_timer* thread should run at a very 
  high priority to ensure, that it will be waken up when it's necessary.

**posix_timer**
  This mode creates a timer with timer_create. It configures the timer and connects it 
  to the given signal number from the configuration string.

**busywait**
  In busywait the timer threads does active wait on the cpu and consumes all cpu time. 
  This can cause higher power consumption and higher temperature (But on a PREEMPT-RT 
  system that should not matter).

