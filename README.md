memstat - little tool to show memory usage of all processes

This is just a little and dirty hack to collect data from the /proc/PID/smaps
files to get a quick overview of the memory usage.

It is written using C++17 and concepts. If you run into trouble using concepts,
just replace them by similar defines.

I am aware of similar programs written in Python or Perl or whatever. This is
small tool is made for embedded environments, where installing of scripting
language interpreters is no option. But C/C++ runtimes are often already part
of such an embedded system, so this ends up to be a pretty small binary (about
30 KiB) on an ARM based machines.

To compile it I usually run this:
g++ -std=c++17 -fconcepts -W -Wall -Wextra -Os -s -o memstat memstat.cxx
