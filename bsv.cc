/*
** bsv.cc -- a convenient BlueSpec Verilog synthesis plugin for Yosys.
** Copyright (C) 2017 Austin Seipp. See Copyright Notice in LICENSE.txt
*/
#include "kernel/yosys.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

/*
** Retrieve the value of the BSC_PATH environment variable. If not set,
** return the default "bsc" command for invoking the BlueSpec compiler.
*/
std::string get_compiler(void)
{
  char* e = getenv("BSC_PATH");

  if (e == nullptr)
    return "bsc";
  else
    return std::string(e);
}

/*
** Retrieve the value of the BLUESPECDIR environment variable. If not set,
** return an empty string.
*/
std::string get_bluespecdir(void)
{
  char* e = getenv("BLUESPECDIR");

  if (e == nullptr)
    return "";
  else
    return std::string(e);
}

/*
** Expand unresolved BlueSpec Verilog primitives to their Verilog counterparts
** under $BLUESPECDIR.
**
** This code is based off the 'hierarchy' pass, and ideally, we'd use hierarchy
** instead of this custom code, since the -libdir function of hierarchy has
** essentially the exact semantics we want. Unfortunately, I'm not sure how
** to propagate the library directory from $BLUESPECDIR/Verilog to the
** hierarchy pass automatically to make the UI convenient, as it happens
** during synthesis. Otherwise, the user has to specify the Verilog directory
** manually during a custom synthesis pass (since -libdir isn't availble to
** hierarchy, when scripted with 'synth'.)
**
** Thus, this is a simple, one-shot and convenient approach for users, which
** automatically loads BSV primitives.
*/
void expand_bsv_libs(RTLIL::Design *design, RTLIL::Module *module) {
  std::string bluespecdir = get_bluespecdir();

  // Look over every cell...
  for (const auto &cell_it : module->cells_) {
    auto *cell = cell_it.second;

    // See if there are any modules in the design with this cell type.
    // If there isn't, then that means the module for this cell is unresolved.
    if (design->modules_.count(cell->type) == 0)
    {
      // Skip any private cell names...
      if (cell->type[0] == '$')
        continue;

      // And try to find/load the BlueSpec primitive
      auto unadorned = RTLIL::unescape_id(cell->type);
      auto filename  = bluespecdir + "/Verilog/" + unadorned + ".v";
      log("Looking for Verilog module '%s' in $BLUESPECDIR/Verilog/%s.v\n",
          unadorned.c_str(), unadorned.c_str());

      if (check_file_exists(filename)) {
        Frontend::frontend_call(design, NULL, filename, "verilog");
        goto loaded;
      }

      // If we got to this point, but we still have an unfound, non-Yosys
      // cell name -- then the name couldn't be resolved, so we fail.
      if (cell->type[0] != '$')
        log_error("Module `%s' referenced in module `%s' in cell `%s' is not part of the design.\n",
						cell->type.c_str(), module->name.c_str(), cell->name.c_str());

    loaded:
      // Finally, do a sanity check: make sure the design has a module of
      // the given cell type *somewhere*, to make sure it was read correctly.
      // Otherwise, the search lookup was invalid.
      if (design->modules_.count(cell->type) == 0)
        log_error("File `%s' from $BLUESPECDIR/Verilog does not declare module `%s'.\n",
                  unadorned.c_str(), cell->type.c_str());

      // That'll do, pig
      continue;
    }
  }
}

struct BsvFrontend : public Pass {
  BsvFrontend() : Pass("bsv", "load BlueSpec Verilog designs using bsc") {}
  virtual void help()
  {
    std::string bluespecdir = get_bluespecdir();

    log("\n");
    log("    bsv [options] <bsv-file>\n");
    log("\n");
    log("This command reads the given BlueSpec Verilog file, compiles\n");
    log("them using the 'bsc' compiler, and then reads the Verilog using\n");
    log("Yosys Verilog frontend.\n");
    log("\n");
    log("The given input .bsv will be compiled using the bsv 'recursive'\n");
    log("compilation mode, which will compile every module automatically.\n");
    log("Therefore, you should run the 'bsv' command on the top-level\n");
    log("module for your design, and specify the top BSV module output\n");
    log("with the -top option. This module should be synthesizable.\n");
    log("\n");
    log("    -top <top-entity-name>\n");
    log("        The name of the top entity. This option is mandatory.\n");
    log("\n");
    log("    -no-autoload-bsv-prims\n");
    log("        Do not incorporate the BlueSpec primitives for Verilog when\n");
    log("        performing synthesis. Compiled BlueSpec designs use\n");
    log("        the included set of primitives bundled with the compiler\n");
    log("        and written in Verilog. For a full synthesis run, you will\n");
    log("        need these libraries available.\n");
    log("\n");
    log("        By default, this frontend will load all Verilog designs\n");
    log("        emitted by the BlueSpec compiler, then examine the RTL\n");
    log("        in order to find unresolved modules used by active cells.\n");
    log("        Then these modules will be searched for in the BlueSpec\n");
    log("        Verilog primitive directory, under $BLUESPECDIR/Verilog\n");
    log("\n");
    log("        If a BSV design emits Verilog using an unresolved module\n");
    log("        'FOO', the plugin will attempt to load the file\n");
    log("        $BLUESPECDIR/Verilog/FOO.v in order to resolve it.\n");
    log("        If this file does not implement the needed module, an\n");
    log("        error will occur.\n");
    log("\n");
    log("        Conceptually, this option is very similar to an automatic\n");
    log("        version of the '-libdir' option for the 'hierarchy' pass,\n");
    log("        tailored for BlueSpec designs. However, when you are using\n");
    log("        custom Verilog or integrating BSV into bespoke designs,\n");
    log("        or implementing BSV interfaces with your own IP cores\n");
    log("        and/or Verilog, this automation may get in the way.\n");
    log("\n");
    log("        If you pass the -no-autoload-bsv-prims flag, you will need\n");
    log("        to later on specify where to find the missing BSV Verilog\n");
    log("        primitives. This can be done using 'hierarchy -libdir' as\n");
    log("        stated before, using the $BLUESPECDIR/Verilog directory.\n");
    log("        Alternatively, you can load the required modules manually.\n");
    log("\n");
    log("        NOTE: while 'hierarchy -libdir' is the recommended way\n");
    log("        of specifying how to load primitives manually, it is not\n");
    log("        exposed by the default 'synth' script. Thus, you will need\n");
    log("        to run 'hierarchy' first, before 'synth' runs its own\n");
    log("        hierarchy pass.\n");
    log("\n");

    if (bluespecdir != "") {
      const char* s = bluespecdir.c_str();
      log("        Currently, BLUESPECDIR is set to the path:\n");
      log("            '%s'\n",s);
      log("        so, Verilog primitives will be loaded from:\n");
      log("            '%s/Verilog'\n",s);
    } else {
      log("        WARNING: BLUESPECDIR is not set! This might mean that\n");
      log("        BlueSpec isn't installed, or is installed incorrectly.\n");
      log("        This plugin will not work as a result, failing with a\n");
      log("        really huge and cool explosion sound when you use it.\n");
    }

    log("\n");
    log("        The value of this flag is false by default.\n");
    log("\n");
    log("The following options are passed as-is to bsc:\n");
    log("\n");
    log("    -D <macro>\n");
    log("    -cpp\n");
    log("    -aggressive-conditions\n");
    log("    -show-schedule\n");
    log("    -show-stats\n");
    log("\n");
    log("By default, the BlueSpec compiler 'bsc' is invoked out of $PATH,\n");
    log("but you may specify the BSC_PATH environment variable to specify\n");
    log("the exact location of the compiler.\n");
    log("\n");
    log("Visit http://www.bluespec.com and contact BlueSpec, Inc. for more\n");
    log("information on BlueSpec Verilog.\n");
    log("\n");
  }

  virtual void execute(std::vector<std::string> args, RTLIL::Design *design)
  {
    std::vector<std::string> bsc_args;
    std::string top_entity;
    std::string top_package;
    std::string compiler = get_compiler();
    std::string bluespecdir = get_bluespecdir();
    bool no_bsv_autoload = false;

    if (bluespecdir == "")
      log_cmd_error("The BLUESPECDIR environment variable isn't defined.\n"
                    "This indicates BlueSpec might not be installed or\n"
                    "not installed correctly. BLUESPECDIR is needed\n"
                    "to locate Verilog primitives correctly. Exiting\n"
                    "without performing synthesis.\n");

    log_header(design, "Executing the BlueSpec compiler (with '%s').\n",
               compiler.c_str());
    log_push();

    size_t argidx;
    for (argidx = 1; argidx < args.size(); argidx++) {
      if (args[argidx] == "-top" && argidx+1 < args.size()) {
        top_entity = args[++argidx];
        continue;
      }

      if (args[argidx] == "-no-autoload-bsv-prims") {
        no_bsv_autoload = true;
        continue;
      }

      if (args[argidx] == "-D" && argidx+1 < args.size()) {
        bsc_args.push_back("-D");
        bsc_args.push_back(args[++argidx]);
        continue;
      }

      if (args[argidx] == "-aggressive-conditions") {
        bsc_args.push_back(args[argidx]);
        continue;
      }

      if (args[argidx] == "-show-schedule") {
        bsc_args.push_back(args[argidx]);
        continue;
      }

      if (args[argidx] == "-show-stats") {
        bsc_args.push_back(args[argidx]);
        continue;
      }

      break;
    }

    if (argidx == args.size())
      cmd_error(args, argidx, "Missing filename for top-level module.");
    if (top_entity.empty())
      log_cmd_error("Missing -top option.\n");

    top_package = args[argidx];
    log_header(design, "Compiling BlueSpec module %s:%s to Verilog.\n",
               top_package.c_str(), top_entity.c_str());

    // Run the Bluespec compiler
    std::string temp_vdir = make_temp_dir("/tmp/yosys-bsv-v-XXXXXX");
    std::string temp_odir = make_temp_dir("/tmp/yosys-bsv-o-XXXXXX");

    log("Compiling BlueSpec objects/verilog to %s:%s\n",
        temp_odir.c_str(), temp_vdir.c_str());

    std::string command = "exec 2>&1; ";
    command += compiler;
    command += stringf(" -vdir '%s'", temp_vdir.c_str());
    command += stringf(" -bdir '%s'", temp_odir.c_str());

    for (const auto& a : bsc_args)
      command = command + " " + a;

    command += stringf(" -verilog");
    command += stringf(" -g '%s'", top_entity.c_str());
    command += stringf(" -u '%s'", top_package.c_str());

    log("Running \"%s\"...\n", command.c_str());
    int ret = run_command(command, [](const std::string& line) { log("%s", line.c_str()); });
    if (ret != 0)
      log_error("Execution of command \"%s\" failed: return code %d\n",
                command.c_str(), ret);

    // Read all of the Verilog output
    log_header(design, "Reading BlueSpec Verilog output.\n");

    auto files = glob_filename(temp_vdir + "/*.v");
    for (const auto& f : files) {
      std::ifstream ff(f.c_str());
      log("Reading %s\n", f.c_str());

      if (ff.fail())
        log_error("Can't open bsc output file `%s'!\n", f.c_str());

      Frontend::frontend_call(design, &ff, f, "verilog");
    }

    // Read all of the BSV Verilog libraries, unless told otherwise
    if (no_bsv_autoload == false) {
      log_header(design, "Attempting autoload of BSV primitives.\n");

      // NOTE: We have to make a copy of the used modules, because if we
      // don't, then we'll get a unbalanced refcount on the RTLIL when
      // we iterate over/insert new RTL it into the references we get.
      std::set<RTLIL::Module*, IdString::compare_ptr_by_name<Module>> used_modules;
      for (auto mod : design->modules())
        used_modules.insert(mod);

      for (const auto& mod : used_modules)
        expand_bsv_libs(design, mod);
    } else {
      log_header(design, "Not attempting autoload of BSV primitives.\n");
    }

    // Clean up and finish
    log_header(design, "Removing temp directories.\n");
    remove_directory(temp_vdir);
    remove_directory(temp_odir);
    log_pop();
  }
} BsvFrontend;

PRIVATE_NAMESPACE_END
