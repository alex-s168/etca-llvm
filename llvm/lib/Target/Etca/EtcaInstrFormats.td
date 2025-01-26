//====- EtcaInstrFormats.td - Etca Instruction Formats ----*- tablegen -*-====//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

class InstEtca<dag outs, dag ins, string asmstr, list<dag> pattern>
    : Instruction {
  field bits<16> Inst;
  let Size = 2;

  let Namespace = "Etca";
  let DecoderNamespace = "Etca";

  bits<4> Opcode;
  let Inst{7-4} = Opcode;

  dag OutOperandList = outs;
  dag InOperandList = ins;
  let AsmString = asmstr;
  let Pattern = pattern;
}

class InstRI<bits<4> op, dag outs, dag ins, string asmstr,
             list<dag> pattern>
    : InstEtca<outs, ins, asmstr, pattern>, Sched<[WriteALU]> {
  bits<3> Ra;
  bits<5> imm5;
  bits<2> ss;

  let Opcode{3 - 0} = op;
  let Inst{1-0} = 0b01;
  let Inst{3-2} = ss;
  let Inst{10-8} = Ra;
  let Inst{15-11} = imm5;
}

class InstRR<bits<4> op, dag outs, dag ins, string asmstr,
             list<dag> pattern>
    : InstEtca<outs, ins, asmstr, pattern>, Sched<[WriteALU]> {
  bits<3> Ra;
  bits<3> Rb;
  bits<2> ss;

  let Opcode{3 - 0} = op;
  let Inst{1-0} = 0b00;
  let Inst{3-2} = ss;
  let Inst{10-8} = Ra;
  let Inst{13-11} = Rb;
  let Inst{15-14} = 0b00;
}

// conditional relative jump
class InstBR_Rel<dag outs, dag ins, string asmstr, list<dag> pattern>
    : InstEtca<outs, ins, asmstr, pattern> {
  bits<9> rel;
  bits<4> cc;

  let Inst{2-0} = 0b100;
  let Inst{3-3} = rel{8-8};
  let Inst{7-4} = cc;
  let Inst{15-8} = rel{7-0};
}

// conditional register jump
class InstBR_Reg<dag outs, dag ins, string asmstr, list<dag> pattern>
    : InstEtca<outs, ins, asmstr, pattern> {
  bits<4> reg;
  bits<4> cc;

  let Inst{7-0} = 0b10101111;
  let Inst{10-8} = reg{2-0};
  let Inst{11} = 0;
  let Inst{15-12} = cc;
}

class ImmAsmOperand<string name> : AsmOperandClass {
  let Name = name;
  let RenderMethod = "addImmOperands";
  let DiagnosticType = !strconcat("Invalid", name);
}

class Immediate<ValueType vt, code pred, string asmop>
  : Operand<vt>, ImmLeaf<vt, pred> {
  let PrintMethod = "print"#asmop;
  let ParserMatchClass = !cast<AsmOperandClass>(asmop);
}

def Imm5_AsmOperand : ImmAsmOperand<"Imm5">;
def Imm5 : Immediate<i16, [{ return Imm >= -16 & Imm <= 15; }], "Imm5_AsmOperand"> {
  let EncoderMethod = "getImm5OpValue";
  let DecoderMethod = "decodeImm5Operand";
}
