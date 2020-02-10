memstat - little tool to show memory usage of all processes

This is just a little, dirty hack to collect data from the /proc/<pid>/smaps
files to get a quick overview of the memory usage.

It's written using C++17 and concepts. You you run into trouble using concepts,
you can replace them quite easily with some other defines.

I'm aware of similar programs written on Python or Perl or whatever. I'm working
a lot on embedded machines where is no space available for scripting language
installations. So this gives me a pretty small binary (about 30 KiB) on an ARM
based machine, only with dependencies to C and C++ runtimes.

To compile it I usually run this:
g++ -std=c++17 -fconcepts -W -Wall -Wextra -Os -s -o memstat memstat.cxx
