<div class="title-block" style="text-align: center;" align="center">

# Bluespec integration for Yosys

![Continuous Integration]
![License]

[Continuous Integration]: https://github.com/thoughtpolice/yosys-bsv/workflows/CI/badge.svg
[License]:        https://img.shields.io/badge/license-ISC%20-blueviolet.svg

<strong>
  <a href="https://github.com/yosyshq">Yosys HQ</a>&nbsp;&nbsp;&bull;&nbsp;&nbsp;<a href="https://github.com/B-Lang-org/bsc">Bluespec</a>
</strong>

---

</div>

This is a plugin for **[Yosys]** that allows you to easily integrate
**[Bluespec]** code into your Yosys-based synthesis flows and tools. It uses
the Bluespec compiler (`bsc`) to compile your designs to Verilog, and then
ingests the results.

> **NOTE**: Although it should go without saying, this plugin **doesn't**
> attempt to assist you with simulation using Bluesim/Icarus/Verilator, etc. It
> is only meant to assist in actual HDL synthesis.

[Yosys]: https://github.com/yosyshq/yosys
[Bluespec]: https://github.com/B-Lang-org/bsc

---

## Installation

Just run `make install`. The `Makefile` will use `yosys-config` to determine
how to link and compile the plugin, and install it in the Yosys data directory.
You can specify the `yosys-config` binary to use by running `make
YOSYS_CONFIG=foo`.  By default the Makefile uses whatever is in `$PATH`.

If Yosys is installed globally, you'll need to use `sudo` with the `install`
target. The `install` target supports `PREFIX=` and `DESTDIR=` so you can
control the installation paths as you expect.

---

## Usage

Run `plugin -i bsv` once you've dropped inside `yosys`. This gives you the
`bsv` command.

Run `help bsv` inside Yosys for detailed information.

---

## Quick Example

Given you have some BlueSpec package `Foo.bsv`, with a top-level module you
want to synthesize called `mkFoo`, you can spit out a "prepped" Verilog file as
follows (using escaped strings in Bash):

```lang=bash
$ yosys -p "\
plugin -i bsv;
bsv -top mkFoo Foo.bsv;
prep;
write_verilog -attr2comment out.v;
"
```

The resulting `out.v` file will contain your prepped Verilog output. This file
is standalone and does not depend on BSV or BSV Verilog primitives at all, and
should work in a verification/synthesis toolchain.

The compiled `Foo.bsv` module will have all of its dependencies automatically
compiled as well. All BlueSpec modules that have been marked `(* synthesize *)`
to emit Verilog output will be incorporated into the resulting Verilog design.

The `bsv` command always completely recompiles its input files, so it may not
be appropriate for fast development iteration, only for true synthesis runs
through your toolchain.

> **NOTE**: the `bsv` plugin attempts to automatically resolves modules for BSV
> primitives written in Verilog. For example, using the `FIFO::*` package in
> BSV and compiling to Verilog results in a module that depends on `FIFO2.v`,
> located in `$BLUESPECDIR/Verilog/FIFO2.v`. This module will be loaded
> automatically.
>
> This feature is on by default, which makes it relatively easy to incorporate
> custom BlueSpec designs into Yosys quickly. See `help bsv` for more
> information on this feature and how to turn it off for more advanced use
> cases.

---

## Future work

This plugin is currently very primitive, and can only be used to synthesize
designs for straight-forward use cases. That said, it should probably work fine
with FOSS EDA tools. Please feel free to submit ideas or improvements.

Now that Bluespec is open source, future work might revolve around improved
integration with `bluetcl` or somesuch.

---

## License

ISC License, the same as Yosys. See `LICENSE.txt` for the exact terms of
copyright and redistribution.
