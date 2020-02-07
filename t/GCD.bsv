package GCD;

import StmtFSM::*;

interface ArithIO#(type a);
   method Action start (a n, a m);
   method a result;
endinterface: ArithIO

module mkGCD(ArithIO#(Bit#(size_t)));
   Reg#(Bit#(size_t)) n <- mkRegU;
   Reg #(Bit#(size_t)) m <- mkRegU;

   rule swap (n > m && m != 0);
      n <= m;
      m <= n;
   endrule

   rule sub (n <= m && m != 0);
      m <= m - n;
   endrule

   method Action start(Bit#(size_t) in_n, Bit#(size_t) in_m) if (m == 0);
      action
         n <= in_n;
         m <= in_m;
      endaction
   endmethod: start

   method Bit#(size_t) result() if (m == 0);
      result = n;
   endmethod: result
endmodule: mkGCD

module mkTest();
  Reg#(Bit#(32)) n <- mkReg(1);
  Reg#(Bit#(32)) m <- mkReg(1);
  ArithIO#(Bit#(32)) gcd <- mkGCD;

  Stmt test =
   seq
      for (n <= 1; n < 7; n <= n + 1)
         for (m <= 1; m < 11; m <= m + 1) seq
            gcd.start(n, m);
         endseq
   endseq;

   mkAutoFSM(test);
endmodule: mkTest
endpackage: GCD
