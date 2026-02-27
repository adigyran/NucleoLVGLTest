LLEXT Sample
============
 
This project includes a basic Linkable Loadable Extension sample.
 
Files:
- `llext/hello_world_ext.c` - external extension functions.
 
Build extension
---------------
 
1. Build firmware:
   - `west build -b nucleo_h743zi/stm32h743xx -d build .`
2. Build extension:
   - `west build -d build -t hello_world_ext`
 
Output:
- `build/hello_world_ext.llext`
 
Load from shell
---------------
 
Convert to one-line hex:
- `xxd -p build/hello_world_ext.llext | tr -d '\n' > build/hello_world_ext.hex`
 
Then in device shell:
- `llext load_hex hello_world <paste_hex_here>`
- `llext list`
- `llext list_symbols hello_world`
- `llext call_fn hello_world hello_world`
- `llext call_fn hello_world hello_board`
 
Notes
-----
 
- This sample uses `CONFIG_LLEXT_IMPORT_ALL_GLOBALS=y` for simplicity.
- If extension loading fails on your target, memory protection settings may need adjustment.