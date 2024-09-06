import numpy as np
import paramiko
import tqdm
import re

from common import *

START_POW = 4
END_POW = 10

def build_ilp_state(start_pow, end_pow):
  states = []
  for i in range(start_pow, end_pow + 1):
    states.append([2 ** i, 2 ** (end_pow - (i - start_pow))])
  return states  
  

# Based on https://www.doc.ic.ac.uk/~phjk/AdvancedCompArchitecture/2001-02/Lectures/Ch03-Caches/node31.html
def generate_ilp_command(data_sets: int, inst_sets: int) -> str:
  cmd = f"srun sim-outorder -config ~/CS4223/assignment1/benchmarks/default.cfg " \
    f"-cache:dl1 dl1:{data_sets}:64:1:l  -cache:il1 il1:{inst_sets}:64:1:l " \
    "~/CS4223/assignment1/benchmarks/compress95.ss < ~/CS4223/assignment1/benchmarks/compress95.in > ~/CS4223/assignment1/benchmarks/compress95.out"
  return cmd

if __name__ == "__main__":
  ### Build all possible states ###
  states = build_ilp_state(START_POW, END_POW)
  print(f"Running for configurations: {states}.")

  ### Test the optimal values to obtain the IPC values ###
  ipcs = []
  # Open result log file
  with open("./res_3.log", "a") as res_file:
    # Establish ssh connection
    ssh = paramiko.SSHClient()
    ssh.load_system_host_keys()
    ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    ssh.connect(hostname = "xlogin2.comp.nus.edu.sg", username = SSH_USERNAME)
      
    for data_sets, inst_sets in (iter := tqdm.tqdm(states)):
      # alloc job on slurm if missing (CURRENTLY BUGGY)
      stdin, stdout, stderr = ssh.exec_command(f"squeue -u {SSH_USERNAME}")
      if len(stderr.read().decode()) > 0:
        stdin, stdout, stderr = ssh.exec_command(f"salloc")
      
      # Generate command
      cmd = generate_ilp_command(data_sets, inst_sets)
      # iter.write(f"State: {state}")
      # iter.write(f"Sending: {cmd}")

      # Send and process result
      stdin, stdout, stderr = ssh.exec_command(cmd)
      result = stderr.read().decode()
      match = re.search(SIM_IPC_PATTERN, result)
      
      # Extract the ipc value
      if match:
        matched_substring = match.group(0)
        
        splited = matched_substring.split()
        ipc = float(splited[1])
        res_file.write(f"Data Sets: {data_sets}; Instruction Sets: {inst_sets}; IPC: {ipc}\n")
        res_file.flush()
        ipcs.append(ipc)
      # Error occured need to debug
      else:
        iter.write(result)
        iter.write("No IPC data found")
        break
      
    ssh.close()
  
  ### Select the highest IPC as the best performance ###
  ipcs = np.asarray(ipcs)
  best_performing_idx = np.argmax(ipcs)
  print(f"Best performing state(idx {best_performing_idx}) is: {states[best_performing_idx[0]]} data sets, {states[best_performing_idx[1]]} instruction sets and IPC of {ipcs[best_performing_idx]}.")
  
  
  
      
      
    
  
  
  
  
  