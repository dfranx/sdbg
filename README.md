# SDBG

sdbg is a cross-platform application that can debug shaders.
It uses [ShaderDebugger](https://github.com/dfranx/ShaderDebugger) and is only made to showcase [ShaderDebugger](https://github.com/dfranx/ShaderDebugger).
It shouldn't be used as an actual debugger (wait for the [SHADERed](https://github.com/dfranx/SHADERed) implementation).

## Building
To build sdbg:
```bash
git clone https://github.com/dfranx/sdbg.git
cd sdbg
git submodule update --init --recursive
cmake .
make
```

After that, you can run the shader debugger:
```cmake
./bin/sdbg shader.hlsl --hlsl
```

sdbg requires **glm** to be installed.

## Screenshots
<p align="center">
    <img width="500" src="./misc/screen1.gif">
</p>

## Commands
 - **step / stepover / stepout**
   - step one line
 - **get \<var_name\>**
   - get variable value
 - **imd \<expression\>**
   - execute expression and get the result
 - **cbkpt \<line\> \<condition\>**
   - set a conditional breakpoint on a `line`
 - **bkpt \<line\>**
   - set a normal breakpoint on a `line`
 - **rembkpt \<line\>**
   - remove a breakpoint from the line
 - **jump \<line\>**
   - go to the `line`
 - **continue**
   - execute code until we hit a breakpoint
 - **func**
   - get current function
 - **fstack**
   - get function stack

## Arguments
 - **--file, -f \<shader\>**
   - specify input file
 - **--compiler, -c hlsl/glsl**
   - specify compiler
 - **--stage, -s**
   - specify shader stage
 - **--entry \<fname\>, -e \<fname\>**
   - set the entry function ("main" by default)

## Partners / Sponsors
...

Contact: **dfranx00 at gmail dot com**

## Supporters
**Silver supporter(s):**
[Hugo Locurcio](https://github.com/Calinou)


## Support
Support the development of this project on Patreon: [<img width="120" src="https://c5.patreon.com/external/logo/become_a_patron_button@2x.png">](https://www.patreon.com/dfranx)

You can support the development of this project via **PayPal**: [<img src="https://www.paypalobjects.com/webstatic/en_US/i/buttons/pp-acceptance-medium.png" alt="Buy now with PayPal" />](https://paypal.me/dfranx) 

My e-mail address for businesses (or if you just want to contact me):
**dfranx00 at gmail dot com**

## LICENSE
sdbg is licensed under MIT license. See [LICENSE](./LICENSE) for more details.