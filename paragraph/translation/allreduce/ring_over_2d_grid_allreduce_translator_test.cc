/* Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "paragraph/translation/allreduce/ring_over_2d_grid_allreduce_translator.h"

#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "gtest/gtest.h"
#include "paragraph/shim/test_macros.h"
#include "paragraph/translation/translation_map.h"

// Tests expanding 2D-Grid all-reduce
TEST(RingOver2dGridAllReduce, NoBarrier) {
  auto graph = absl::make_unique<paragraph::Graph>("test_graph", 2);
  auto sub = absl::make_unique<paragraph::Subroutine>(
      "test_subroutine", graph.get());
  auto sub_ptr = sub.get();
  sub_ptr->SetId(3);
  graph->SetEntrySubroutine(std::move(sub));

  ASSERT_OK_AND_ASSIGN(auto instr_1, paragraph::Instruction::Create(
      paragraph::Opcode::kDelay, "first_instruction", sub_ptr));
  instr_1->SetOps(4);

  ASSERT_OK_AND_ASSIGN(auto allreduce, paragraph::Instruction::Create(
      paragraph::Opcode::kAllReduce, "all-reduce", sub_ptr));
  allreduce->SetBytesOut(48);
  paragraph::CommunicationGroup allreduce_group = {0, 1, 2, 3};
  allreduce->AppendCommunicationGroup(allreduce_group);

  auto reduction_sub = absl::make_unique<paragraph::Subroutine>(
      "reduction_subroutine", graph.get());
  auto reduction_ptr = reduction_sub.get();
  ASSERT_OK_AND_ASSIGN(auto op1, paragraph::Instruction::Create(
      paragraph::Opcode::kDelay, "op1", reduction_ptr));
  op1->SetBytesOut(48);
  ASSERT_OK_AND_ASSIGN(auto op2, paragraph::Instruction::Create(
      paragraph::Opcode::kDelay, "op2", reduction_ptr));
  op2->SetBytesOut(48);
  ASSERT_OK_AND_ASSIGN(auto sum_op, paragraph::Instruction::Create(
      paragraph::Opcode::kDelay, "sum", reduction_ptr, true));
  sum_op->SetOps(96);
  sum_op->AddOperand(op1);
  sum_op->AddOperand(op2);
  allreduce->AppendInnerSubroutine(std::move(reduction_sub));

  ASSERT_OK_AND_ASSIGN(auto instr_3, paragraph::Instruction::Create(
      paragraph::Opcode::kDelay, "last_instruction", sub_ptr, true));
  instr_3->SetOps(4);

  nlohmann::json config = R"(
    {
      "all-reduce": {
        "algorithm": "ring-2d-grid",
        "concentration": 1,
        "dimension_widths": [2, 2]
      }
    }
  )"_json;

  ASSERT_OK_AND_ASSIGN(auto translators, paragraph::CreateTranslators(
      paragraph::TranslatorType::kCollective, config));
  EXPECT_OK(translators["all-reduce"]->Translate(allreduce));

  paragraph::InstructionProto allreduce_proto;
  std::string allreduce_str =
      R"proto(
name: "all-reduce"
opcode: "all-reduce"
instruction_id: 2
bytes_out: 48
communication_groups {
  group_ids: 0
  group_ids: 1
  group_ids: 2
  group_ids: 3
}
inner_subroutines {
  name: "all-reduce_mesh-2d"
  subroutine_root_id: 7
  execution_probability: 1
  execution_count: 1
  instructions {
    name: "all-reduce_ring-2d-grid"
    opcode: "all-reduce"
    instruction_id: 7
    bytes_out: 48
    communication_groups {
      group_ids: 0
      group_ids: 2
      group_ids: 3
      group_ids: 1
    }
    inner_subroutines {
      name: "all-reduce_ring-2d-grid_bidir-ring"
      subroutine_root_id: 42
      execution_probability: 1
      execution_count: 1
      instructions {
        name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter"
        opcode: "reduce-scatter"
        instruction_id: 8
        bytes_out: 48
        communication_groups {
          group_ids: 0
          group_ids: 2
          group_ids: 3
          group_ids: 1
        }
        inner_subroutines {
          name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring"
          subroutine_root_id: 41
          execution_probability: 1
          execution_count: 1
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw"
            opcode: "reduce-scatter"
            instruction_id: 9
            bytes_out: 24
            communication_groups {
              group_ids: 0
              group_ids: 2
              group_ids: 3
              group_ids: 1
            }
            inner_subroutines {
              name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring"
              subroutine_root_id: 21
              execution_probability: 1
              execution_count: 1
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring_sendrecv_1"
                opcode: "sendrecv"
                instruction_id: 10
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 0
                  group_ids: 3
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring_reduction_1"
                opcode: "call"
                instruction_id: 11
                operand_ids: 10
                inner_subroutines {
                  name: "reduction_subroutine_phase_1"
                  subroutine_root_id: 14
                  execution_probability: 1
                  execution_count: 1
                  instructions {
                    name: "op1_phase_1"
                    opcode: "delay"
                    instruction_id: 12
                    bytes_out: 6
                  }
                  instructions {
                    name: "op2_phase_1"
                    opcode: "delay"
                    instruction_id: 13
                    bytes_out: 6
                  }
                  instructions {
                    name: "sum_phase_1"
                    opcode: "delay"
                    instruction_id: 14
                    ops: 12
                    operand_ids: 12
                    operand_ids: 13
                  }
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring_sendrecv_2"
                opcode: "sendrecv"
                instruction_id: 15
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 0
                  group_ids: 3
                }
                operand_ids: 11
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring_reduction_2"
                opcode: "call"
                instruction_id: 16
                operand_ids: 15
                inner_subroutines {
                  name: "reduction_subroutine_phase_2"
                  subroutine_root_id: 19
                  execution_probability: 1
                  execution_count: 1
                  instructions {
                    name: "op1_phase_2"
                    opcode: "delay"
                    instruction_id: 17
                    bytes_out: 6
                  }
                  instructions {
                    name: "op2_phase_2"
                    opcode: "delay"
                    instruction_id: 18
                    bytes_out: 6
                  }
                  instructions {
                    name: "sum_phase_2"
                    opcode: "delay"
                    instruction_id: 19
                    ops: 12
                    operand_ids: 17
                    operand_ids: 18
                  }
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring_sendrecv_3"
                opcode: "sendrecv"
                instruction_id: 20
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 0
                  group_ids: 3
                }
                operand_ids: 16
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring_reduction_3"
                opcode: "call"
                instruction_id: 21
                operand_ids: 20
                inner_subroutines {
                  name: "reduction_subroutine_phase_3"
                  subroutine_root_id: 24
                  execution_probability: 1
                  execution_count: 1
                  instructions {
                    name: "op1_phase_3"
                    opcode: "delay"
                    instruction_id: 22
                    bytes_out: 6
                  }
                  instructions {
                    name: "op2_phase_3"
                    opcode: "delay"
                    instruction_id: 23
                    bytes_out: 6
                  }
                  instructions {
                    name: "sum_phase_3"
                    opcode: "delay"
                    instruction_id: 24
                    ops: 12
                    operand_ids: 22
                    operand_ids: 23
                  }
                }
              }
            }
          }
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw"
            opcode: "reduce-scatter"
            instruction_id: 25
            bytes_out: 24
            communication_groups {
              group_ids: 1
              group_ids: 3
              group_ids: 2
              group_ids: 0
            }
            inner_subroutines {
              name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring"
              subroutine_root_id: 37
              execution_probability: 1
              execution_count: 1
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring_sendrecv_1"
                opcode: "sendrecv"
                instruction_id: 26
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 3
                  group_ids: 0
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring_reduction_1"
                opcode: "call"
                instruction_id: 27
                operand_ids: 26
                inner_subroutines {
                  name: "reduction_subroutine_phase_1"
                  subroutine_root_id: 30
                  execution_probability: 1
                  execution_count: 1
                  instructions {
                    name: "op1_phase_1"
                    opcode: "delay"
                    instruction_id: 28
                    bytes_out: 6
                  }
                  instructions {
                    name: "op2_phase_1"
                    opcode: "delay"
                    instruction_id: 29
                    bytes_out: 6
                  }
                  instructions {
                    name: "sum_phase_1"
                    opcode: "delay"
                    instruction_id: 30
                    ops: 12
                    operand_ids: 28
                    operand_ids: 29
                  }
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring_sendrecv_2"
                opcode: "sendrecv"
                instruction_id: 31
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 3
                  group_ids: 0
                }
                operand_ids: 27
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring_reduction_2"
                opcode: "call"
                instruction_id: 32
                operand_ids: 31
                inner_subroutines {
                  name: "reduction_subroutine_phase_2"
                  subroutine_root_id: 35
                  execution_probability: 1
                  execution_count: 1
                  instructions {
                    name: "op1_phase_2"
                    opcode: "delay"
                    instruction_id: 33
                    bytes_out: 6
                  }
                  instructions {
                    name: "op2_phase_2"
                    opcode: "delay"
                    instruction_id: 34
                    bytes_out: 6
                  }
                  instructions {
                    name: "sum_phase_2"
                    opcode: "delay"
                    instruction_id: 35
                    ops: 12
                    operand_ids: 33
                    operand_ids: 34
                  }
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring_sendrecv_3"
                opcode: "sendrecv"
                instruction_id: 36
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 3
                  group_ids: 0
                }
                operand_ids: 32
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring_reduction_3"
                opcode: "call"
                instruction_id: 37
                operand_ids: 36
                inner_subroutines {
                  name: "reduction_subroutine_phase_3"
                  subroutine_root_id: 40
                  execution_probability: 1
                  execution_count: 1
                  instructions {
                    name: "op1_phase_3"
                    opcode: "delay"
                    instruction_id: 38
                    bytes_out: 6
                  }
                  instructions {
                    name: "op2_phase_3"
                    opcode: "delay"
                    instruction_id: 39
                    bytes_out: 6
                  }
                  instructions {
                    name: "sum_phase_3"
                    opcode: "delay"
                    instruction_id: 40
                    ops: 12
                    operand_ids: 38
                    operand_ids: 39
                  }
                }
              }
            }
          }
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_root_2"
            opcode: "null"
            instruction_id: 41
            operand_ids: 9
            operand_ids: 25
          }
        }
      }
      instructions {
        name: "all-reduce_ring-2d-grid_bidir-ring_all-gather"
        opcode: "all-gather"
        instruction_id: 42
        bytes_out: 48
        communication_groups {
          group_ids: 0
          group_ids: 2
          group_ids: 3
          group_ids: 1
        }
        operand_ids: 8
        inner_subroutines {
          name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring"
          subroutine_root_id: 51
          execution_probability: 1
          execution_count: 1
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_cw"
            opcode: "all-gather"
            instruction_id: 43
            bytes_out: 24
            communication_groups {
              group_ids: 0
              group_ids: 2
              group_ids: 3
              group_ids: 1
            }
            inner_subroutines {
              name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_cw_unidir-ring"
              subroutine_root_id: 46
              execution_probability: 1
              execution_count: 1
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_cw_unidir-ring_sendrecv_1"
                opcode: "sendrecv"
                instruction_id: 44
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 0
                  group_ids: 3
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_cw_unidir-ring_sendrecv_2"
                opcode: "sendrecv"
                instruction_id: 45
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 0
                  group_ids: 3
                }
                operand_ids: 44
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_cw_unidir-ring_sendrecv_3"
                opcode: "sendrecv"
                instruction_id: 46
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 0
                  group_ids: 3
                }
                operand_ids: 45
              }
            }
          }
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_ccw"
            opcode: "all-gather"
            instruction_id: 47
            bytes_out: 24
            communication_groups {
              group_ids: 1
              group_ids: 3
              group_ids: 2
              group_ids: 0
            }
            inner_subroutines {
              name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_ccw_unidir-ring"
              subroutine_root_id: 50
              execution_probability: 1
              execution_count: 1
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_ccw_unidir-ring_sendrecv_1"
                opcode: "sendrecv"
                instruction_id: 48
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 3
                  group_ids: 0
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_ccw_unidir-ring_sendrecv_2"
                opcode: "sendrecv"
                instruction_id: 49
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 3
                  group_ids: 0
                }
                operand_ids: 48
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_ccw_unidir-ring_sendrecv_3"
                opcode: "sendrecv"
                instruction_id: 50
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 3
                  group_ids: 0
                }
                operand_ids: 49
              }
            }
          }
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_root_2"
            opcode: "null"
            instruction_id: 51
            operand_ids: 43
            operand_ids: 47
          }
        }
      }
    }
  }
}
      )proto";
  google::protobuf::TextFormat::ParseFromString(allreduce_str,
                                                &allreduce_proto);
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      allreduce->ToProto().value(), allreduce_proto));
}

// Tests expanding 2D-Grid all-reduce with barrier
TEST(RingOver2dGridAllReduce, WithBarrier) {
  auto graph = absl::make_unique<paragraph::Graph>("test_graph", 2);
  auto sub = absl::make_unique<paragraph::Subroutine>(
      "test_subroutine", graph.get());
  auto sub_ptr = sub.get();
  sub_ptr->SetId(3);
  graph->SetEntrySubroutine(std::move(sub));

  ASSERT_OK_AND_ASSIGN(auto instr_1, paragraph::Instruction::Create(
      paragraph::Opcode::kDelay, "first_instruction", sub_ptr));
  instr_1->SetOps(4);

  ASSERT_OK_AND_ASSIGN(auto allreduce, paragraph::Instruction::Create(
      paragraph::Opcode::kAllReduce, "all-reduce", sub_ptr));
  allreduce->SetBytesOut(48);
  paragraph::CommunicationGroup allreduce_group = {0, 1, 2, 3};
  allreduce->AppendCommunicationGroup(allreduce_group);

  auto reduction_sub = absl::make_unique<paragraph::Subroutine>(
      "reduction_subroutine", graph.get());
  auto reduction_ptr = reduction_sub.get();
  ASSERT_OK_AND_ASSIGN(auto op1, paragraph::Instruction::Create(
      paragraph::Opcode::kDelay, "op1", reduction_ptr));
  op1->SetBytesOut(48);
  ASSERT_OK_AND_ASSIGN(auto op2, paragraph::Instruction::Create(
      paragraph::Opcode::kDelay, "op2", reduction_ptr));
  op2->SetBytesOut(48);
  ASSERT_OK_AND_ASSIGN(auto sum_op, paragraph::Instruction::Create(
      paragraph::Opcode::kDelay, "sum", reduction_ptr, true));
  sum_op->SetOps(96);
  sum_op->AddOperand(op1);
  sum_op->AddOperand(op2);
  allreduce->AppendInnerSubroutine(std::move(reduction_sub));

  ASSERT_OK_AND_ASSIGN(auto instr_3, paragraph::Instruction::Create(
      paragraph::Opcode::kDelay, "last_instruction", sub_ptr, true));
  instr_3->SetOps(4);

  nlohmann::json config = R"(
    {
      "all-reduce": {
        "algorithm": "ring-2d-grid",
        "concentration": 1,
        "dimension_widths": [2, 2],
        "barrier": {
          "algorithm": "centralized"
        }
      }
    }
  )"_json;

  ASSERT_OK_AND_ASSIGN(auto translators, paragraph::CreateTranslators(
      paragraph::TranslatorType::kCollective, config));
  EXPECT_OK(translators["all-reduce"]->Translate(allreduce));

  paragraph::InstructionProto allreduce_proto;
  std::string allreduce_str =
      R"proto(
name: "all-reduce"
opcode: "all-reduce"
instruction_id: 2
bytes_out: 48
communication_groups {
  group_ids: 0
  group_ids: 1
  group_ids: 2
  group_ids: 3
}
inner_subroutines {
  name: "all-reduce_mesh-2d"
  subroutine_root_id: 7
  execution_probability: 1
  execution_count: 1
  instructions {
    name: "all-reduce_ring-2d-grid"
    opcode: "all-reduce"
    instruction_id: 7
    bytes_out: 48
    communication_groups {
      group_ids: 0
      group_ids: 2
      group_ids: 3
      group_ids: 1
    }
    inner_subroutines {
      name: "all-reduce_ring-2d-grid_bidir-ring"
      subroutine_root_id: 45
      execution_probability: 1
      execution_count: 1
      instructions {
        name: "all-reduce_ring-2d-grid_bidir-ring_barrier"
        opcode: "barrier"
        instruction_id: 8
        communication_groups {
          group_ids: 0
          group_ids: 2
          group_ids: 3
          group_ids: 1
        }
        inner_subroutines {
          name: "all-reduce_ring-2d-grid_bidir-ring_barrier_centralized"
          subroutine_root_id: 10
          execution_probability: 1
          execution_count: 1
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_barrier_centralized_send_to_0"
            opcode: "send"
            instruction_id: 9
            communication_groups {
              group_ids: 0
            }
          }
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_barrier_centralized_recv_from_0"
            opcode: "recv"
            instruction_id: 10
            communication_groups {
              group_ids: 0
            }
            operand_ids: 9
          }
        }
      }
      instructions {
        name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter"
        opcode: "reduce-scatter"
        instruction_id: 11
        bytes_out: 48
        communication_groups {
          group_ids: 0
          group_ids: 2
          group_ids: 3
          group_ids: 1
        }
        operand_ids: 8
        inner_subroutines {
          name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring"
          subroutine_root_id: 44
          execution_probability: 1
          execution_count: 1
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw"
            opcode: "reduce-scatter"
            instruction_id: 12
            bytes_out: 24
            communication_groups {
              group_ids: 0
              group_ids: 2
              group_ids: 3
              group_ids: 1
            }
            inner_subroutines {
              name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring"
              subroutine_root_id: 24
              execution_probability: 1
              execution_count: 1
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring_sendrecv_1"
                opcode: "sendrecv"
                instruction_id: 13
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 0
                  group_ids: 3
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring_reduction_1"
                opcode: "call"
                instruction_id: 14
                operand_ids: 13
                inner_subroutines {
                  name: "reduction_subroutine_phase_1"
                  subroutine_root_id: 17
                  execution_probability: 1
                  execution_count: 1
                  instructions {
                    name: "op1_phase_1"
                    opcode: "delay"
                    instruction_id: 15
                    bytes_out: 6
                  }
                  instructions {
                    name: "op2_phase_1"
                    opcode: "delay"
                    instruction_id: 16
                    bytes_out: 6
                  }
                  instructions {
                    name: "sum_phase_1"
                    opcode: "delay"
                    instruction_id: 17
                    ops: 12
                    operand_ids: 15
                    operand_ids: 16
                  }
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring_sendrecv_2"
                opcode: "sendrecv"
                instruction_id: 18
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 0
                  group_ids: 3
                }
                operand_ids: 14
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring_reduction_2"
                opcode: "call"
                instruction_id: 19
                operand_ids: 18
                inner_subroutines {
                  name: "reduction_subroutine_phase_2"
                  subroutine_root_id: 22
                  execution_probability: 1
                  execution_count: 1
                  instructions {
                    name: "op1_phase_2"
                    opcode: "delay"
                    instruction_id: 20
                    bytes_out: 6
                  }
                  instructions {
                    name: "op2_phase_2"
                    opcode: "delay"
                    instruction_id: 21
                    bytes_out: 6
                  }
                  instructions {
                    name: "sum_phase_2"
                    opcode: "delay"
                    instruction_id: 22
                    ops: 12
                    operand_ids: 20
                    operand_ids: 21
                  }
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring_sendrecv_3"
                opcode: "sendrecv"
                instruction_id: 23
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 0
                  group_ids: 3
                }
                operand_ids: 19
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_cw_unidir-ring_reduction_3"
                opcode: "call"
                instruction_id: 24
                operand_ids: 23
                inner_subroutines {
                  name: "reduction_subroutine_phase_3"
                  subroutine_root_id: 27
                  execution_probability: 1
                  execution_count: 1
                  instructions {
                    name: "op1_phase_3"
                    opcode: "delay"
                    instruction_id: 25
                    bytes_out: 6
                  }
                  instructions {
                    name: "op2_phase_3"
                    opcode: "delay"
                    instruction_id: 26
                    bytes_out: 6
                  }
                  instructions {
                    name: "sum_phase_3"
                    opcode: "delay"
                    instruction_id: 27
                    ops: 12
                    operand_ids: 25
                    operand_ids: 26
                  }
                }
              }
            }
          }
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw"
            opcode: "reduce-scatter"
            instruction_id: 28
            bytes_out: 24
            communication_groups {
              group_ids: 1
              group_ids: 3
              group_ids: 2
              group_ids: 0
            }
            inner_subroutines {
              name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring"
              subroutine_root_id: 40
              execution_probability: 1
              execution_count: 1
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring_sendrecv_1"
                opcode: "sendrecv"
                instruction_id: 29
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 3
                  group_ids: 0
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring_reduction_1"
                opcode: "call"
                instruction_id: 30
                operand_ids: 29
                inner_subroutines {
                  name: "reduction_subroutine_phase_1"
                  subroutine_root_id: 33
                  execution_probability: 1
                  execution_count: 1
                  instructions {
                    name: "op1_phase_1"
                    opcode: "delay"
                    instruction_id: 31
                    bytes_out: 6
                  }
                  instructions {
                    name: "op2_phase_1"
                    opcode: "delay"
                    instruction_id: 32
                    bytes_out: 6
                  }
                  instructions {
                    name: "sum_phase_1"
                    opcode: "delay"
                    instruction_id: 33
                    ops: 12
                    operand_ids: 31
                    operand_ids: 32
                  }
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring_sendrecv_2"
                opcode: "sendrecv"
                instruction_id: 34
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 3
                  group_ids: 0
                }
                operand_ids: 30
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring_reduction_2"
                opcode: "call"
                instruction_id: 35
                operand_ids: 34
                inner_subroutines {
                  name: "reduction_subroutine_phase_2"
                  subroutine_root_id: 38
                  execution_probability: 1
                  execution_count: 1
                  instructions {
                    name: "op1_phase_2"
                    opcode: "delay"
                    instruction_id: 36
                    bytes_out: 6
                  }
                  instructions {
                    name: "op2_phase_2"
                    opcode: "delay"
                    instruction_id: 37
                    bytes_out: 6
                  }
                  instructions {
                    name: "sum_phase_2"
                    opcode: "delay"
                    instruction_id: 38
                    ops: 12
                    operand_ids: 36
                    operand_ids: 37
                  }
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring_sendrecv_3"
                opcode: "sendrecv"
                instruction_id: 39
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 3
                  group_ids: 0
                }
                operand_ids: 35
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_ccw_unidir-ring_reduction_3"
                opcode: "call"
                instruction_id: 40
                operand_ids: 39
                inner_subroutines {
                  name: "reduction_subroutine_phase_3"
                  subroutine_root_id: 43
                  execution_probability: 1
                  execution_count: 1
                  instructions {
                    name: "op1_phase_3"
                    opcode: "delay"
                    instruction_id: 41
                    bytes_out: 6
                  }
                  instructions {
                    name: "op2_phase_3"
                    opcode: "delay"
                    instruction_id: 42
                    bytes_out: 6
                  }
                  instructions {
                    name: "sum_phase_3"
                    opcode: "delay"
                    instruction_id: 43
                    ops: 12
                    operand_ids: 41
                    operand_ids: 42
                  }
                }
              }
            }
          }
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_reduce-scatter_bidir-ring_root_2"
            opcode: "null"
            instruction_id: 44
            operand_ids: 12
            operand_ids: 28
          }
        }
      }
      instructions {
        name: "all-reduce_ring-2d-grid_bidir-ring_all-gather"
        opcode: "all-gather"
        instruction_id: 45
        bytes_out: 48
        communication_groups {
          group_ids: 0
          group_ids: 2
          group_ids: 3
          group_ids: 1
        }
        operand_ids: 11
        inner_subroutines {
          name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring"
          subroutine_root_id: 54
          execution_probability: 1
          execution_count: 1
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_cw"
            opcode: "all-gather"
            instruction_id: 46
            bytes_out: 24
            communication_groups {
              group_ids: 0
              group_ids: 2
              group_ids: 3
              group_ids: 1
            }
            inner_subroutines {
              name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_cw_unidir-ring"
              subroutine_root_id: 49
              execution_probability: 1
              execution_count: 1
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_cw_unidir-ring_sendrecv_1"
                opcode: "sendrecv"
                instruction_id: 47
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 0
                  group_ids: 3
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_cw_unidir-ring_sendrecv_2"
                opcode: "sendrecv"
                instruction_id: 48
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 0
                  group_ids: 3
                }
                operand_ids: 47
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_cw_unidir-ring_sendrecv_3"
                opcode: "sendrecv"
                instruction_id: 49
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 0
                  group_ids: 3
                }
                operand_ids: 48
              }
            }
          }
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_ccw"
            opcode: "all-gather"
            instruction_id: 50
            bytes_out: 24
            communication_groups {
              group_ids: 1
              group_ids: 3
              group_ids: 2
              group_ids: 0
            }
            inner_subroutines {
              name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_ccw_unidir-ring"
              subroutine_root_id: 53
              execution_probability: 1
              execution_count: 1
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_ccw_unidir-ring_sendrecv_1"
                opcode: "sendrecv"
                instruction_id: 51
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 3
                  group_ids: 0
                }
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_ccw_unidir-ring_sendrecv_2"
                opcode: "sendrecv"
                instruction_id: 52
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 3
                  group_ids: 0
                }
                operand_ids: 51
              }
              instructions {
                name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_ccw_unidir-ring_sendrecv_3"
                opcode: "sendrecv"
                instruction_id: 53
                bytes_in: 6
                bytes_out: 6
                communication_groups {
                  group_ids: 3
                  group_ids: 0
                }
                operand_ids: 52
              }
            }
          }
          instructions {
            name: "all-reduce_ring-2d-grid_bidir-ring_all-gather_bidir-ring_root_2"
            opcode: "null"
            instruction_id: 54
            operand_ids: 46
            operand_ids: 50
          }
        }
      }
    }
  }
}
      )proto";
  google::protobuf::TextFormat::ParseFromString(allreduce_str,
                                                &allreduce_proto);
  EXPECT_TRUE(google::protobuf::util::MessageDifferencer::Equals(
      allreduce->ToProto().value(), allreduce_proto));
}
