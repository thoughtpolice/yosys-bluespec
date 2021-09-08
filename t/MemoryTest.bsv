import BRAM::*;

(* synthesize, clock_prefix="clk", reset_prefix="resetn" *)
module memoryTest(BRAMServer#(Bit#(5), Bit#(32)));
  let cfg = defaultValue;
  cfg.loadFormat = tagged Hex "firmware.hex";
  let bram0 <- mkBRAM1Server(cfg);
  return bram0.portA;
endmodule
