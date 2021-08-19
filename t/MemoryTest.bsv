import BRAM::*;

(* synthesize, clock_prefix="clk", reset_prefix="resetn" *)
module memoryTest(Tuple2#(BRAMServer#(Bit#(5), Bit#(5)), BRAMServer#(Bit#(5), Bit#(5))));
  let cfg = defaultValue;
  cfg.loadFormat = tagged Hex "firmware.hex";
  let bram0 <- mkBRAM2Server(cfg);
  return tuple2(bram0.portA, bram0.portB);
endmodule
