import numpy as np

PIPELINE_WIDTH_IDX = 0
INT_ALU_IDX = 1
INT_MULTIPLIER_IDX = 2
FP_ALU_IDX = 3
FP_MULTIPLIER_IDX = 4
OUT_OF_ORDER_ISSUE_IDX = 5
REORDER_BUFFER_IDX = 6
LOAD_STORE_QUEUE_IDX = 7

MAX_AREA = 60.0
SSH_USERNAME = "zhehao"

SIM_IPC_PATTERN = r'sim_IPC.*\d+\.\d+'

VALUE_RANGE = [
  (0, 2),
  (1, 4),
  (1, 1),
  (1, 1),
  (1, 4),
  (0, 1),
  (3, 5),
  (2, 4)
]

VALUE_TO_CONFIG = [
  lambda state: (2 ** state[PIPELINE_WIDTH_IDX]),
  lambda state: state[INT_ALU_IDX],
  lambda state: state[INT_MULTIPLIER_IDX],
  lambda state: state[FP_ALU_IDX],
  lambda state: state[FP_MULTIPLIER_IDX],
  lambda state: 0 if state[OUT_OF_ORDER_ISSUE_IDX] else 1,
  lambda state: 2 ** (state[REORDER_BUFFER_IDX]),
  lambda state: 2 ** (state[LOAD_STORE_QUEUE_IDX]),
]

VALUE_TO_AREA = [
  lambda state: (2 ** state[PIPELINE_WIDTH_IDX]) * 1.8,
  lambda state: state[INT_ALU_IDX] * 2,
  lambda state: state[INT_MULTIPLIER_IDX] * 3,
  lambda state: state[FP_ALU_IDX] * 4,
  lambda state: state[FP_MULTIPLIER_IDX] * 5,
  lambda state: state[OUT_OF_ORDER_ISSUE_IDX] * 1.4 * (
    VALUE_TO_AREA[PIPELINE_WIDTH_IDX](state) + VALUE_TO_AREA[INT_ALU_IDX](state)
    + VALUE_TO_AREA[INT_MULTIPLIER_IDX](state) + VALUE_TO_AREA[FP_ALU_IDX](state)
    + VALUE_TO_AREA[FP_MULTIPLIER_IDX](state)),
  lambda state: state[OUT_OF_ORDER_ISSUE_IDX] * (2 ** (state[REORDER_BUFFER_IDX] - 1)),
  lambda state: state[OUT_OF_ORDER_ISSUE_IDX] * (2 ** (state[LOAD_STORE_QUEUE_IDX] - 1)),
]

################## Functions ##################
def build_state(index: int):
  if index == (len(VALUE_RANGE) - 1):
    return [[i] for i in range(VALUE_RANGE[index][0], VALUE_RANGE[index][1] + 1)]

  ret_list = []
  for val in range(VALUE_RANGE[index][0], VALUE_RANGE[index][1] + 1):
    sub_list = build_state(index + 1)
    ret_list += map(lambda state : [val] + state, sub_list)
  
  return ret_list

def state_to_config_values(state: np.ndarray):
  list_state = state.tolist()
  config_values = []
  for idx in range(len(list_state)):
    config_values.append((VALUE_TO_CONFIG[idx](list_state)))
  
  return config_values
    
def generate_command(config_values) -> str:
  cmd = f"srun sim-outorder -config ~/CS4223/assignment1/benchmarks/default.cfg " \
    f"-decode:width {config_values[PIPELINE_WIDTH_IDX]} -issue:width {config_values[PIPELINE_WIDTH_IDX]} -commit:width {config_values[PIPELINE_WIDTH_IDX]} " \
    f"-res:ialu {config_values[INT_ALU_IDX]} " \
    f"-res:imult {config_values[INT_MULTIPLIER_IDX]} " \
    f"-res:fpalu {config_values[FP_ALU_IDX]} " \
    f"-res:fpmult {config_values[FP_MULTIPLIER_IDX]} " \
    f"-issue:inorder {config_values[OUT_OF_ORDER_ISSUE_IDX]} " \
    f"-ruu:size {config_values[REORDER_BUFFER_IDX]} " \
    f"-lsq:size {config_values[LOAD_STORE_QUEUE_IDX]} " \
    "~/CS4223/assignment1/benchmarks/compress95.ss < ~/CS4223/assignment1/benchmarks/compress95.in > ~/CS4223/assignment1/benchmarks/compress95.out"
  return cmd