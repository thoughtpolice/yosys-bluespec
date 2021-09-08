/*
** bluespec.cc -- a Bluespec frontend for Yosys
** Copyright (C) 2017 Austin Seipp. See Copyright Notice in LICENSE.txt
*/
#include "kernel/yosys.h"

USING_YOSYS_NAMESPACE
PRIVATE_NAMESPACE_BEGIN

void read_verilog(RTLIL::Design *design, std::istream *ff, std::string f, std::string reset_string, bool defer = false)
{
  std::vector<std::string> args;
  args.push_back("verilog");
  if (defer)
    args.push_back("-defer");
  args.push_back(reset_string);
  Frontend::frontend_call(design, ff, f, args);
}

/*
** Retrieve the value of the BSC_PATH environment variable. If not set,
** return the default "bsc" command for invoking the Bluespec compiler.
*/
std::string get_compiler(void)
{
#ifdef STATIC_BSC_PATH
  return STATIC_BSC_PATH;
#else
  char* e = getenv("BSC_PATH");

  if (e == nullptr)
    return "bsc";
  else
    return std::string(e);
#endif
}

/*
** Retrieve the value of the BLUESPECDIR environment variable. If not set,
** return an empty string.
*/
std::string get_bluespecdir(void)
{
#ifdef STATIC_BSC_LIBDIR
  return STATIC_BSC_LIBDIR;
#else
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
#endif
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
void expand_bsv_libs(RTLIL::Design *design, RTLIL::Module *module, std::string reset) {
  using namespace std;

  string bluespecdir = get_bluespecdir();
  set<string> seen;

  set<string> bs_modules = {
    "ASSIGN1",
    "BRAM1",
    "BRAM1BE",
    "BRAM1BELoad",
    "BRAM1Load",
    "BRAM2",
    "BRAM2BE",
    "BRAM2BELoad",
    "BRAM2Load",
    "BypassCrossingWire",
    "BypassWire",
    "BypassWire0",
    "CRegA5",
    "CRegN5",
    "CRegUN5",
    "ClockDiv",
    "ClockGater",
    "ClockGen",
    "ClockInverter",
    "ClockMux",
    "ClockSelect",
    "ConfigRegA",
    "ConfigRegN",
    "ConfigRegUN",
    "ConstrainedRandom",
    "ConvertFromZ",
    "ConvertToZ",
    "Counter",
    "CrossingBypassWire",
    "CrossingRegA",
    "CrossingRegN",
    "CrossingRegUN",
    "DualPortRam",
    "Empty",
    "FIFO1",
    "FIFO10",
    "FIFO2",
    "FIFO20",
    "FIFOL1",
    "FIFOL10",
    "FIFOL2",
    "FIFOL20",
    "Fork",
    "GatedClock",
    "GatedClockDiv",
    "GatedClockInverter",
    "InitialReset",
    "InoutConnect",
    "LatchCrossingReg",
    "MakeClock",
    "MakeReset",
    "MakeReset0",
    "MakeResetA",
    "McpRegUN",
    "ProbeCapture",
    "ProbeHook",
    "ProbeMux",
    "ProbeTrigger",
    "ProbeValue",
    "ProbeWire",
    "RWire",
    "RWire0",
    "RegA",
    "RegAligned",
    "RegFile",
    "RegFileLoad",
    "RegN",
    "RegTwoA",
    "RegTwoN",
    "RegTwoUN",
    "RegUN",
    "ResetEither",
    "ResetInverter",
    "ResetMux",
    "ResetToBool",
    "ResolveZ",
    "RevertReg",
    "SampleReg",
    "ScanIn",
    "SizedFIFO",
    "SizedFIFO0",
    "SizedFIFOL",
    "SizedFIFOL0",
    "SyncBit",
    "SyncBit05",
    "SyncBit1",
    "SyncBit15",
    "SyncFIFO",
    "SyncFIFO0",
    "SyncFIFO1",
    "SyncFIFO10",
    "SyncFIFOLevel",
    "SyncFIFOLevel0",
    "SyncHandshake",
    "SyncPulse",
    "SyncRegister",
    "SyncReset",
    "SyncReset0",
    "SyncResetA",
    "SyncWire",
    "TriState",
    "UngatedClockMux",
    "UngatedClockSelect",
  };

  // these modules are invalid and can't be handled by yosys, for whatever
  // reason.
  map<string, string> bad_modules = {
    { "InoutConnect",      "non-ANSI port aliases aren't supported (issue #2613)"},
    { "ProbeHook",         "non-ANSI port aliases aren't supported (issue #2613)"},
    { "ConstrainedRandom", "simulation-only $random task isn't supported"},
  };

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

      auto unadorned = RTLIL::unescape_id(cell->type);

      // Only load Bluespec modules; we don't want to interfere with user FFI modules...
      if (bs_modules.count(unadorned) == 0) continue;

      // And try to find/load the Bluespec primitive. Mark seen primitives and don't load
      // them more than once, just because they get _used_ more than once.
      if (seen.count(unadorned) > 0) continue;

      // these modules are invalid, so help users by telling them why
      if (auto it { bad_modules.find(unadorned) }; it != end(bad_modules)) {
        auto reason{it->second};

        log_error("Bluespec Verilog module `%s', referenced in module `%s' in cell `%s', is unsupported by Yosys.\n"
                  "Reason: %s. Exiting unsuccessfully.\n",
          unadorned.c_str(), module->name.c_str(), cell->name.c_str(), reason.c_str());
      }

      auto filename  = bluespecdir + "/Verilog/" + unadorned + ".v";
      log("Looking for Verilog module '%s' in $BLUESPECDIR/Verilog/%s.v\n",
          unadorned.c_str(), unadorned.c_str());

      if (check_file_exists(filename)) {
        read_verilog(design, NULL, filename, reset, true);
        seen.insert(unadorned);
      } else {
        // If we got to this point, but we still have an unfound, non-Yosys
        // cell name -- then the name couldn't be resolved, so we fail.
        if (cell->type[0] != '$')
          log_error("Module `%s' referenced in module `%s' in cell `%s' is not part of the design.\n",
            cell->type.c_str(), module->name.c_str(), cell->name.c_str());
      }
    }
  }
}

struct BsvFrontend : public Pass {
private:
  std::vector<std::tuple<std::string, bool>> passthru_flags = {
    // flag -> does it take an argument?
    { "-D",                     true  },
    { "-cpp",                   false },
    { "-check-assert",          false },
    { "-show-schedule",         false },
    { "-show-stats",            false },
    { "-aggressive-conditions", false },
    // optimization flags
    { "-remove-unused-modules", false },
    { "-opt-undetermined-vals", false },
    { "-unspecified-to",        true  },
  };

public:
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
    log("        By default, the frontend loads all individual modules marked with\n");
    log("        'synthesize' attributes. If none exist, or you wish to only use\n");
    log("        one particular module, this option can be used to select a single\n");
    log("        Bluespec module to compile\n");
    log("\n");

    log("    -reset {pos,neg}\n");
    log("        Specify the module reset sensitivity. Compiled Bluespec designs\n");
    log("        can use both positive or negative reset values for DUT reset.\n");
    log("        When compiling a Bluespec design, this option can be used to\n");
    log("        choose which to use. Note that this choice applies to all\n");
    log("        Bluespec code.\n");
    log("        \n");
    log("        A positive reset value means that a value of '1' applied to the\n");
    log("        reset line will put the device into reset. A negative reset by\n");
    log("        contrast requires a '0' to put the device into reset.\n");
    log("        \n");
    log("        By default, compiled Bluespec modules use negative reset: a\n");
    log("        value of 0 will put the device into reset.\n");
    log("\n");

    log("    -I <dir>\n");
    log("        Add a directory to the Bluespec compiler search path.\n");
    log("        This is useful when modules are loaded outside the CWD.\n");
    log("        Modules specified here are searched in reverse priority order,\n");
    log("        i.e. the last directory given is searched first. The Prelude is\n");
    log("        searched last.\n");
    log("\n");

    log("    -no-prelude\n");
    log("        Do not load the default Bluespec Prelude.\n");
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
    log("The following options are passed as-is to bsc, if given, so please\n");
    log("refer to the manual if necessary to understand their use. Note that\n");
    log("any arguments are parsed as a single token, so use quotes for spaces\n");
    log("if needed:\n");
    log("\n");

    for (auto [ flag, has_param ] : passthru_flags)
      log("    %s\t%s\n", flag.c_str(), has_param ? "<param>" : "");

    log("\n");
    log("By default, the Bluespec compiler 'bsc' is invoked out of $PATH,\n");
    log("but you may specify the BSC_PATH environment variable to specify\n");
    log("the exact location of the compiler.\n");
    log("\n");
  }

  virtual void execute(std::vector<std::string> args, RTLIL::Design *design)
  {
    std::vector<std::string> bsc_args;
    std::vector<std::string> bsc_search_dirs;
    std::string top_entity;
    std::string top_package;
    std::string compiler = get_compiler();
    std::string reset_string = "-DBSV_NEGATIVE_RESET=1";

    bool no_bsv_autoload = false;
    bool bsc_no_prelude = false;

    if (get_bluespecdir() == "")
      log_cmd_error("The BLUESPECDIR environment variable isn't defined.\n"
                    "This indicates Bluespec might not be installed or\n"
                    "not installed correctly. BLUESPECDIR is needed\n"
                    "to locate Verilog primitives correctly. Exiting\n"
                    "without performing synthesis.\n");

    log_header(design, "Executing the Bluespec compiler (with '%s').\n",
               compiler.c_str());
    log_push();

    bsc_search_dirs.push_back(".");

    size_t argidx;
    for (argidx = 1; argidx < args.size(); argidx++) {
      if (args[argidx] == "-top" && argidx+1 < args.size()) {
        top_entity = args[++argidx];
        continue;
      }

      if (args[argidx] == "-reset" && argidx+1 < args.size()) {
        if (args[argidx+1] == "pos") {
          reset_string = "-DBSV_POSITIVE_RESET=1";
        } else if (args[argidx+1] == "neg") {
          reset_string = "-DBSV_NEGATIVE_RESET=1";
        } else {
          log_cmd_error("Invalid argument for -reset\n");
        }

        ++argidx;
        continue;
      }

      if (args[argidx] == "-no-autoload-bsv-prims") {
        no_bsv_autoload = true;
        continue;
      }

      if (args[argidx] == "-I" && argidx+1 < args.size()) {
        bsc_search_dirs.push_back(args[++argidx]);
        continue;
      }

      if (args[argidx] == "-no-prelude") {
        bsc_no_prelude = true;
        continue;
      }

      for (auto [ flag, has_param ] : passthru_flags) {
        if (args[argidx] == flag) {
          if (has_param && argidx+1 < args.size()) {
            bsc_args.push_back(args[  argidx]);
            bsc_args.push_back(args[++argidx]);
            goto loop;
          } else if (!has_param) {
            bsc_args.push_back(args[argidx]);
            goto loop;
          }
        }
      }

      break;
loop: continue; // this can't be a no-op? grammar nit, i guess
    }

    if (argidx == args.size())
      cmd_error(args, argidx, "Missing filename for top-level module.");

    // set search path first
    if (!bsc_no_prelude)
      bsc_search_dirs.push_back("%/Libraries");

    std::string full_search_path = "";
    for (unsigned long i = 0, sz = bsc_search_dirs.size(); i < sz; i++) {
      full_search_path += bsc_search_dirs[i];
      if (i != sz-1) full_search_path += ":";
    }

    // NB: always of at least size == 1, bc '.' is always included
    bsc_args.push_back("-p");
    bsc_args.push_back("'" + full_search_path + "'");

    // Run the Bluespec compiler
    top_package = args[argidx];
    log_header(design, "Compiling Bluespec package %s\n", top_package.c_str());

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
    if (!top_entity.empty())
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

      read_verilog(design, &ff, f, reset_string);
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
        expand_bsv_libs(design, mod, reset_string);
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
