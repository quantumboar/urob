# Urob(oron): a lightweight framework for ESP32 using ESP-IDF, with lwip and netconn examples

## Background
As a hobby, I like to write code for embedded systems (mostly uc-based dev boards, or systems designed by me). Just for fun, my objective is to have said code to be designed for best performance, minimizing things like latencies, memory, or power consumption.

When designing the network subsystem for one of my esp32-based projects (ChickenWorld) I realized that the good old [BSD sockets](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/lwip.html#bsd-sockets-api) are affected by more memory and CPU overheads than I'd like. The memory redundancy comes from the multiple copies one buffer has to go through before reaching the application layer, while the cpu usage comes from the threads that under the hood handle the communication (and the memory copies, of course ;) ).

### lwip
The wise designers of esp-idf, probably aware of the resource contrains that the adoption of BSD sockets might cause in some cases, decided to base the implementation on [LWIP](https://www.nongnu.org/lwip/2_1_x/index.html), which stands for Light Weight IP. And lighweight it truly is! The implementation in esp-idf (should be 2.1.2 at the moment I'm writing this) offers three levels of API:

#### RAW
The RAW API offers all the perf goodies: no additional threads, no memory copies, all the control to the client. It's also the most complex to adopt, as it's not thread-safe, and you'll have to make sure that all the lwip raw API calls are made from the same thread. You'll also find out that there are little to no examples of how to use it, especially for esp-idf. Oh, and it's not directly supported by esp-idf. The version of LWIP-raw shipped with it has some modifications that could make examples -even if you chance to find any- hard to impossible to adapt without alterations to the esp-idf codebase.

#### Netconn
Netconn is just one step up (or down, as some might say) from the RAW API. It still allows zero-copy approaches but, at least in the esp-idf implementation, it will use an additional thread to handle low-level communications (which are performed using the RAW API, see above). Another pro is that Netconn is [supported by esp-idf](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/lwip.html#netconn-api), but with a warning. The link above explains that 

>Espressif does not test the Netconn API in ESP-IDF. As such, this functionality is enabled but not supported. Some functionality may only work correctly when used from the BSD Sockets API.

It's also still quite hard to find any documentation other than the lwip API docs, and I could find very little examples googling around. The few I found, furthermore, are not for esp-idf and very often adopt a Netconn API customized for a specific board.

#### BSD Sockets
Well, you already know :) . The good is that you can find example code for anything with a network interface (and even some things without it)

### The examples

Netconn seems to be low level enough to reduce the waste of resources, especially considering that the esp32 sdk will configure and launch the network thread for you every time network is configured. For this reason I've decided to implement a super-simple set of examples that make use on netconn with esp-idf.

#### IDE/SDK/Boards

The tests are embedded within a platformio project, developed using VSCode. I don't think the latter is essential, but it would come handy. The SDK used is, as already mentioned, ESP-IDF. So far this was tested on ESP32-dev boards, but I see no reasons for this not to work on other esp-32 based hardware. I'm going to test it on esp8266 and esp32s2 too, when possible.

#### Threading and performance
In this exercise I'm adding a few more constraints:

- minimize memory overhead from the framework
- predictable latencies (possibly low)

Memory reduction allows a constrained system to do more with less: as the framework uses less memory, more remains for other things like buffering. The latency requirement can be seen as the wish to be able to predictably know when a specific task is executed.

Threads are a convenient way to abstract the complexity of multitasking, but they come with costs in terms of resources like the stack, almost never fully used and definitely unused for off-cpu threads. Additionally, execution latencies are at the mercy of [preemptive schedulers](https://www.freertos.org/implementation/a00005.html). Granted, FreeRTOS could be configured to [disable preemption and time slicing](https://www.freertos.org/single-core-amp-smp-rtos-scheduling.html#:~:text=By%20default%2C%20FreeRTOS%20uses%20a,task%20due%20to%20priority%20inheritance.), but this would introduce additional complexity which is probably beyond the scope of this example.

The consequence is that thread usage should be minimized and so, except for the mainloop, no additional threads should in theory be necessary. Currently the project still uses a few additional threads:

- the event loop for network events
- the idle thread, necessary to feed the watchdog
- the lwip thread which handles asynchronously the IP layer

I believe it might be possible to rewrite the project to get rid of the event loop, but the other two threads might require major reconfiguration and possibly rewriting of esp-idf which is, again, a bit out of Urob(oron)'s scope.

Because of latency variability (and the "absence" of threads), locks are a big no-no in this context. It's much more fun (and efficient) to use atomics with barriers.

#### Components
Aping an object-oriented construct, most components/tests will have:

- an init function
- a loop function: will be invoked by the main loop, must be blocking it for the shortest time possible (let's see this as single-threaded cooperative multitasking)
- an uninit function

Additionally, the component state is to be handled a component-specific structure, with its own state machine handled by the functions above, and passed to them. Because of the loopy nature of the framework, all network calls are non-blocking. 

#### Examples
At the moment, we have two (well, four) tests for non-blocking, synchronous netconn-based operations:

- http server
- http client
- host address resolution
- tcp socket-like (synchronous but minimally-blocking) objects

The last two are currently tested via the http client/server tests.

