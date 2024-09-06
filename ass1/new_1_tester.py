import numpy as np
import paramiko
import tqdm
import re

PIPELINE_WIDTH_IDX = 0
INT_ALU_IDX = 1
INT_MULTIPLIER_IDX = 2
FP_ALU_IDX = 3
FP_MULTIPLIER_IDX = 4
OUT_OF_ORDER_ISSUE_IDX = 5
REORDER_BUFFER_IDX = 6
LOAD_STORE_QUEUE_IDX = 7

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

ACTUAL_VALUE_CONVERSION = [
  lambda state: (2 ** state[PIPELINE_WIDTH_IDX]),
  lambda state: state[INT_ALU_IDX],
  lambda state: state[INT_MULTIPLIER_IDX],
  lambda state: state[FP_ALU_IDX],
  lambda state: state[FP_MULTIPLIER_IDX],
  lambda state: state[OUT_OF_ORDER_ISSUE_IDX],
  lambda state: state[OUT_OF_ORDER_ISSUE_IDX] * (2 ** (state[REORDER_BUFFER_IDX])),
  lambda state: state[OUT_OF_ORDER_ISSUE_IDX] * (2 ** (state[LOAD_STORE_QUEUE_IDX])),
]

AREA_FORMULA = [
  lambda state: (2 ** state[PIPELINE_WIDTH_IDX]) * 1.8,
  lambda state: state[INT_ALU_IDX] * 2,
  lambda state: state[INT_MULTIPLIER_IDX] * 3,
  lambda state: state[FP_ALU_IDX] * 4,
  lambda state: state[FP_MULTIPLIER_IDX] * 5,
  lambda state: state[OUT_OF_ORDER_ISSUE_IDX] * 1.4 * (state[PIPELINE_WIDTH_IDX] + state[INT_ALU_IDX] + state[INT_MULTIPLIER_IDX] + state[FP_ALU_IDX] + state[FP_MULTIPLIER_IDX]),
  lambda state: state[OUT_OF_ORDER_ISSUE_IDX] * (2 ** (state[REORDER_BUFFER_IDX] - 1)),
  lambda state: state[OUT_OF_ORDER_ISSUE_IDX] * (2 ** (state[LOAD_STORE_QUEUE_IDX] - 1)),
]

MAX_AREA = 60.0

SSH_USERNAME = "zhehao"

def build_state(index: int):
  if index == (len(VALUE_RANGE) - 1):
    return [[i] for i in range(VALUE_RANGE[index][0], VALUE_RANGE[index][1] + 1)]

  ret_list = []
  for val in range(VALUE_RANGE[index][0], VALUE_RANGE[index][1] + 1):
    sub_list = build_state(index + 1)
    ret_list += map(lambda state : [val] + state, sub_list)
  
  return ret_list

if __name__ == "__main__":
  states = build_state(0)
  print(f"There are {len(states)} states total.")
  
  thresh_states = []
  thresh_areas = []
  for state in states:
    area = 0
    for idx in range(len(state)):
      area += AREA_FORMULA[idx](state)

    if area <= MAX_AREA:
      thresh_states.append(state)
      thresh_areas.append(area)
      
  thresh_states = np.asarray(thresh_states)
  thresh_areas = np.asarray(thresh_areas)
      
  print(f"There are {len(thresh_states)} states <= the max area.")
  
  is_max = np.asarray([True for i in thresh_states])
  
  for idx, state in enumerate(thresh_states):
    diff = thresh_states - state
    is_super = np.all((diff >= 0), axis = 1)
    is_super[idx] = False
    is_max[idx] = not np.any(is_super)

  max_states = thresh_states[is_max]
  max_areas = thresh_areas[is_max]
  
  print(f"There are {len(max_states)} max states.")
  
  for state, area in zip(max_states, max_areas):
    print(state)
   
    actual_values = []
    for idx in range(len(state)):
      actual_values.append(ACTUAL_VALUE_CONVERSION[idx](state).item())
    print(actual_values)
    print(area, "\n\n")
  
  
  
      
      
    
  
  
  
  
  