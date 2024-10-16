#!/bin/bash
sed -e s/-fno-tree-switch-conversion//g -e s/-fstrict-volatile-bitfields//g -e s/-fno-shrink-wrap//g -e s/-mlongcalls//g build/compile_commands.json >compile_commands.json

