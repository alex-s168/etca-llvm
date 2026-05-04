//=- ETCASchedule.td - ETCA Scheduling Definitions ----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// ETCA scheduling model.
//
// The ETCA ISA is a simple in-order architecture.  We define a basic
// scheduling model with separate resources for ALU and load/store units.
// This model is forward-looking for potential OOO implementations.
//
//===----------------------------------------------------------------------===//

//===----------------------------------------------------------------------===//
// Functional units
//===----------------------------------------------------------------------===//

def ALU_FU  : FuncUnit;
def LDST_FU : FuncUnit;

//===----------------------------------------------------------------------===//
// Instruction itinerary classes
//===----------------------------------------------------------------------===//

def IIC_ALU : InstrItinClass;
def IIC_LD  : InstrItinClass;
def IIC_ST  : InstrItinClass;

def ETCAItinerary : ProcessorItineraries<[ALU_FU, LDST_FU], [], [
  InstrItinData<IIC_LD,  [InstrStage<1, [LDST_FU]>]>,
  InstrItinData<IIC_ST,  [InstrStage<1, [LDST_FU]>]>,
  InstrItinData<IIC_ALU, [InstrStage<1, [ALU_FU]>]>
]>;

//===----------------------------------------------------------------------===//
// SchedMachineModel
//===----------------------------------------------------------------------===//

def ETCASchedModel : SchedMachineModel {
  let LoadLatency         = 2;
  let LoopMicroOpBufferSize = 0;
  let CompleteModel       = 0;  // Not complete — we use itineraries
  let IssueWidth          = 1;
  let MispredictPenalty   = 10;
  let PostRAScheduler     = 0;
  let MicroOpBufferSize   = 0;
  let Itineraries         = ETCAItinerary;
}

//===----------------------------------------------------------------------===//
// SchedReadWrite resources
//===----------------------------------------------------------------------===//

def ALU  : ProcResource<1> { let BufferSize = 0; }
def LdSt : ProcResource<1> { let BufferSize = 0; }

def WriteLD   : SchedWrite;
def WriteST   : SchedWrite;
def WriteALU  : SchedWrite;

let SchedModel = ETCASchedModel in {
  def : WriteRes<WriteLD,  [LdSt]> { let Latency = 2; }
  def : WriteRes<WriteST,  [LdSt]> { let Latency = 2; }
  def : WriteRes<WriteALU, [ALU]>  { let Latency = 1; }
}
