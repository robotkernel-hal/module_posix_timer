=============
Configuration
=============

Use the following snippet in your Robotkernel handler configuration:

.. code-block:: yaml
   :caption: main.rkc

   name: posix_timer
   so_file: libmodule_posix_timer.so
   config: !include timer_0.rkc

This example config file can be used as a template for own configurations.

.. code-block:: yaml
   :caption: timer_0.rkc

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
     - name: main
   
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

Available configuration parameters:

+-------------------+----------------------------------------------------+
| Parameter         | Description                                        |
+===================+====================================================+
| *name*            | Unique name for trigger Device.                    |
+-------------------+----------------------------------------------------+
| *interval*        | Seconds interval between timer triggers            |
+-------------------+----------------------------------------------------+
| *mode*            | Timer mode e.g. 'timer', 'nanosleep' or 'busywait' |
+-------------------+----------------------------------------------------+
| *signo*           | Signal number to use in 'timer' mode               |
+-------------------+----------------------------------------------------+
| *prio*            | Thread priority in 'nanosleep' or 'busywait' mode  |
+-------------------+----------------------------------------------------+
| *affinity*        | Thread affinity in 'nanosleep' or 'busywait' mode  |
+-------------------+----------------------------------------------------+
| *skip_missed*     | Skip missed cycles or catch up.                    |
+-------------------+----------------------------------------------------+

This example configuration will create two robotkernel device (one trigger, one pd):

.. code-block::

   posix_timer.main.trigger
   posix_timer.main.pd

The process data device will have the following structure:

.. code-block:: yaml

   pds:
     module_posix_timer/inputs:
       actual_interval_nsec: { type: uint64_t }
       initial_interval_nsec: { type: uint64_t }
       virtual_time_nsec: { type: uint64_t }
       elapsed_time_nsec: { type: uint64_t }

