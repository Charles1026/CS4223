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

MAX_AREA = 60.0
SSH_USERNAME = "zhehao"

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
  lambda state: state[OUT_OF_ORDER_ISSUE_IDX] * (2 ** (state[REORDER_BUFFER_IDX])),
  lambda state: state[OUT_OF_ORDER_ISSUE_IDX] * (2 ** (state[LOAD_STORE_QUEUE_IDX])),
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
  cmd = f"srun sim-outorder -config ~/CS4223/assignment1/min.cfg " \
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

if __name__ == "__main__":
  ### Build all possible states ###
  states = build_state(0)
  print(f"There are {len(states)} states total.")
  
  ### Filter the states and only keep those within the constraint ###
  constrained_states = []
  constrained_areas = []
  constrained_areas_by_part = []
  for state in states:
    areas = []
    for idx in range(len(state)):
      areas.append(VALUE_TO_AREA[idx](state))
    total_area = sum(areas)

    if total_area <= MAX_AREA:
      constrained_states.append(state)
      constrained_areas.append(total_area)
      constrained_areas_by_part.append(areas)
      
  constrained_states = np.asarray(constrained_states)
  constrained_areas = np.asarray(constrained_areas)
  constrained_areas_by_part = np.asarray(constrained_areas_by_part)
      
  print(f"There are {len(constrained_states)} states within the max area constraint of {MAX_AREA}.")
  
  ### Filter out the non optimal states ###
  is_max = np.asarray([True for i in constrained_states])
  for idx, state in enumerate(constrained_states):
    diff = constrained_states - state
    is_super = np.all((diff >= 0), axis = 1)
    is_super[idx] = False
    is_max[idx] = not np.any(is_super)

  optimal_states = constrained_states[is_max]
  optimal_areas = constrained_areas[is_max]
  optimal_areas_by_part = constrained_areas_by_part[is_max]
  
  print(f"There are {len(optimal_states)} optimal states amongst those constrained states.")
  
  ### Test the optimal values to obtain the IPC values ###
  optimal_config_values = []
  optimal_ipcs = []
  # Open result log file
  with open("./res.txt", "a") as res_file:
    # Establish ssh connection
    ssh = paramiko.SSHClient()
    ssh.load_system_host_keys()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(hostname = "xlogin2.comp.nus.edu.sg", username = SSH_USERNAME)
      
    for state, area, area_by_part in (iter := tqdm.tqdm(zip(optimal_states, optimal_areas, optimal_areas_by_part))):
      # alloc job on slurm if missing (CURRENTLY BUGGY)
      stdin, stdout, stderr = ssh.exec_command(f"squeue -u {SSH_USERNAME}")
      if len(stderr.read().decode()) > 0:
        stdin, stdout, stderr = ssh.exec_command(f"salloc")
      
      # Generate command
      config_values = state_to_config_values(state)
      optimal_config_values.append(config_values)
      cmd = generate_command(config_values)
      # iter.write(f"Sending: {cmd}")

      # Send and process result
      stdin, stdout, stderr = ssh.exec_command(cmd)
      result = stderr.read().decode()
      match = re.search(r'sim_IPC.*\d+\.\d+', result)
      
      # Extract the ipc value
      if match:
        matched_substring = match.group(0)
        
        splited = matched_substring.split()
        ipc = float(splited[1])
        res_file.write(f"Configuration: {config_values}; Area per resource: {area_by_part}; Total Area: {area}; IPC: {ipc}\n")
        res_file.flush()
        optimal_ipcs.append(ipc)
      # Error occured need to debug
      else:
        iter.write(result)
        iter.write("No IPC data found")
        break
      
    ssh.close()
  
  ### Select the highest IPC as the best performance ###
  optimal_ipcs = np.asarray(optimal_ipcs)
  best_performing_idx = np.argmax(optimal_ipcs)
  print(f"Best performing state(idx {best_performing_idx}) is: {optimal_config_values[best_performing_idx]}, with area {optimal_areas[best_performing_idx]} and IPC of {optimal_ipcs[best_performing_idx]}.")
  
  
  
      
      
    
  
  
  
  
  