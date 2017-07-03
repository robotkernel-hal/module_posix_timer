# module_posix_timer

The **module_posix_timer** is used to as a generic trigger generator. 
It supports two different modes. In *posix_timer* mode a POSIX per-process
timer is created. Otherwise in 'nanosleep' mode the timer increments are 
slept with the nanosleep function.

## robotkernel devices

The **module_posix_timer** creates two **robotkernel** devices:
* trigger device: <module_name>.posix_timer.trigger
* process data device: <module_name>.inputs.pd

## config file example

This example loads the posix timer module and sets up a nanosleep trigger with 3kHz.

     -   name: timer                                                                 
         so_file: libmodule_posix_timer.so                                           
         config:                                                                     
             loglevel: info                                                          
             interval: 0.000333333                                                   
             mode: nanosleep                                                         
             prio: 90 
             affinity: 1                                                             
         power_up: op             
