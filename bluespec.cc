/*
** bluespec.cc -- a Bluespec frontend for Yosys
** Copyright (C) 2017 Austin Seipp. See Copyright Notice in LICENSE.txt
*/
#include "kernel/yosys.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

void read_verilog(RTLIL::Design *design, std::istream *ff, std::string f)
{
  Frontend::frontend_call(design, ff, f, "verilog");
}

/*
** Retrieve the value of the BSC_PATH environment variable. If not set,
** return the default "bsc" command for invoking the Bluespec compiler.
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
  // [FIXME] (aseipp): we currently just pull `bluectl` out of the environment
  // without allowing an override (cf BSC_PATH). ideally we should fix bsc to
  // give us this path directly anyway
  std::string command = "echo 'puts $env(BLUESPECDIR)' | bluetcl";
  std::string libdir = "";

  int ret = run_command(command, [&](const std::string& line) { libdir = line; });
  if (ret != 0)
    log_error("Execution of command \"%s\" failed: return code %d\n",
              command.c_str(), ret);

  if (!libdir.empty() && libdir[libdir.length()-1] == '\n') {
    libdir.erase(libdir.length()-1);
  }

  return libdir;
}

/*
** Expand unresolved Bluespec Verilog primitives to their Verilog counterparts
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

      // And try to find/load the Bluespec primitive
      auto unadorned = RTLIL::unescape_id(cell->type);
      auto filename  = bluespecdir + "/Verilog/" + unadorned + ".v";
      log("Looking for Verilog module '%s' in $BLUESPECDIR/Verilog/%s.v\n",
          unadorned.c_str(), unadorned.c_str());

      if (check_file_exists(filename)) {
        read_verilog(design, NULL, filename);
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
  BsvFrontend() : Pass("read_bluespec", "typecheck, compile, and load Bluespec code") {}
  virtual void help()
  {
    log("\n");
    log("    read_bluespec [options] source.{bs,bsv}\n");
    log("\n");

    log("Load modules from a Bluespec package. This uses the 'bsc' compiler in\n");
    log("order to typecheck and compile the source code. Bluespec Haskell files\n");
    log("(with .bs extension) and Bluespec SystemVerilog (resp. bsv) are supported.\n");
    log("\n");

    log("Compilation follows basic Bluespec rules: every individual module inside\n");
    log("a package marked with a 'synthesize' attribute will be compiled to an\n");
    log("individual RTL module, and each such module will be read into the current\n");
    log("design.\n");
    log("\n");

    log("By default, the frontend assumes the modules you want to synthesize are\n");
    log("marked with 'synthesize' attributes, and will incorporate all such modules\n");
    log("into the design by default, but if you wish to leave them un-attributed\n");
    log("in the source code, or for simplicity, you can use the '-top' option to\n");
    log("compile and read a single module from the source.\n");
    log("\n");

    log("    -top <top-entity-name>\n");
    log("        The name of the top entity. This option is mandatory.\n");
    log("\n");

    log("    -no-autoload-bsv-prims\n");
    log("        Do not incorporate Verilog primitives during module compilation\n");
    log("        Compiled Bluespec designs use the included set of primitives\n");
    log("        written in Verilog and bundled with the compiler. For a full\n");
    log("        synthesis run, you will need these libraries available.\n");
    log("\n");
    log("        If you pass the -no-autoload-bsv-prims flag, you will need\n");
    log("        to later on specify where to find the missing Verilog\n");
    log("        primitives. This can be done using the 'hierarchy -libdir' pass\n");
    log("        using the Bluespec compiler installation 'Verilog' subdirectory.\n");
    log("        Alternatively, you can load the required modules manually.\n");
    log("\n");

    std::string bluespecdir = get_bluespecdir();
    if (bluespecdir != "") {
      const char* s = bluespecdir.c_str();
      log("        Currently, BLUESPECDIR is set to the path:\n");
      log("            '%s'\n",s);
      log("        so, Verilog primitives will be loaded from:\n");
      log("            '%s/Verilog'\n",s);
    } else {
      log("        WARNING: BLUESPECDIR is not set! This might mean that\n");
      log("        Bluespec isn't installed, or is installed incorrectly.\n");
      log("        This plugin will not work as a result, failing with a\n");
      log("        really huge and cool explosion sound when you use it.\n");
    }

    log("\n");
    log("        The value of this flag is false by default: compiled Bluespec\n");
    log("        modules will have Verilog primitives loaded automatically.\n");
    log("\n");
    log("The following options are passed as-is to bsc:\n");
    log("\n");
    log("    -D <macro>\n");
    log("    -cpp\n");
    log("    -aggressive-conditions\n");
    log("    -show-schedule\n");
    log("    -show-stats\n");
    log("\n");
    log("By default, the Bluespec compiler 'bsc' is invoked out of $PATH,\n");
    log("but you may specify the BSC_PATH environment variable to specify\n");
    log("the exact location of the compiler.\n");
    log("\n");
  }

  virtual void execute(std::vector<std::string> args, RTLIL::Design *design)
  {
    std::vector<std::string> bsc_args;
    std::string top_entity;
    std::string top_package;
    std::string compiler = get_compiler();
    bool no_bsv_autoload = false;

    if (get_bluespecdir() == "")
      log_cmd_error("The BLUESPECDIR environment variable isn't defined.\n"
                    "This indicates Bluespec might not be installed or\n"
                    "not installed correctly. BLUESPECDIR is needed\n"
                    "to locate Verilog primitives correctly. Exiting\n"
                    "without performing synthesis.\n");

    log_header(design, "Executing the Bluespec compiler (with '%s').\n",
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
    log_header(design, "Compiling Bluespec package %s\n", top_package.c_str());

    // Run the Bluespec compiler
    std::string temp_vdir = make_temp_dir("/tmp/yosys-bsv-v-XXXXXX");
    std::string temp_odir = make_temp_dir("/tmp/yosys-bsv-o-XXXXXX");

    log("Compiling Bluespec objects/verilog to %s:%s\n",
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
    log_header(design, "Reading Bluespec compiler output.\n");

    auto files = glob_filename(temp_vdir + "/*.v");
    for (const auto& f : files) {
      std::ifstream ff(f.c_str());
      log("Reading %s\n", f.c_str());

      if (ff.fail())
        log_error("Can't open bsc output file `%s'!\n", f.c_str());

      read_verilog(design, &ff, f);
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
