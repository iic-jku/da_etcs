## A tool for creating VSS layouts for train networks and for verifying and optimizing train schedules
Copyright (c) 2021
by Robert Wille (rober.wille@jku.at), Tom Peham (tom.peham@jku.at)
Johannes Kepler University Linz, Austria
Siemens Mobility GmbH, Braunschweig, Germany
https://iic.jku.at/eda/research/etcs/ 

The software is intellectual property of the above mentioned authors. You 
can freely redistribute this software for non-commercial purposes as long as 
it includes a reference to its origin.

Use at your own risk!
In no event shall the authors be liable for any 
damages whatsoever (including without limitation damages for loss of business 
profits, business interruption, loss of business information, or any other 
pecuniary loss) arising from the use of or inability to use the software, even 
if the authors have been advised of the possibility of such 
damages.

If you have any questions feel free to contact us using
rober.wille@jku.at, tom.peham@jku.at

## Overview

The European Train Control System (ETCS) strives to harmonize and improve train control systems throughout Europe. One particular aspect is the utilization of virtual subsections which allow for a much higher degree of freedom and provide significant potential for increasing the efficiency in today’s train schedules. However, exploiting this potential is a highly non-trivial task which, thus far, mainly relied on manual labor. In our work, we aim for introducing methods for design automation which aid designers of corresponding railway networks and train schedules.

In a first attempt, we developed an inital solution which utilizes satisfiability solvers to generate, verify, and optimize train layout and/or schedules. 

## Usage

### System Requirements 

The package has been tested under Linux (Ubuntu 18.04, 64-bit) and should be compatible with any current version of g++ supporting C++17 and a minimum CMake version of 3.13.
Note that in order for the bindings to work the SMT Solver Z3 has to be installed on the system and the dynamic linker has to be able to find the library. This can be accomplished in one of the following ways:
- Under Ubuntu 18.04 and newer: `sudo apt install z3`
- Alternatively: `pip install z3-solver` and then append the corresponding path to the library path (`LD_LIBRARY_PATH` under Linux, `DYLD_LIBRARY_PATH` under macOS), e.g. via
    ```bash
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$(python -c "import z3; print(z3.__path__[0]+'/lib')")
    ```
- Download pre-built binaries from https://github.com/Z3Prover/z3/releases and copy the files to the respective system directories
- Build Z3 from source and install it to the system

### Build and Run 

To build the tool go to the project folder and type 

     cmake CMakeLists.txt
    

To use the tool run 

    ./vss [option] <input file>

Option can be:
    
    -f ... verify given schedule, no layout generation
    -s ... verify schedule, layout generation
    -o ... optimize schedule, layout generation

If no options are given then all design tasks will be performed in that order.

#### Input File Format
 
The input file describes the track layout as an undirected graph and the train schedule. The BNF of the file format is given below. The tests folder contains some examples.
        
input_file ::== [integer] [integer] nl nl (track nl)* nl [fork nl]* nl (train)* (*number of nodes, max timesteps*, ...)

track ::== edge integer integer integer integer (*edge, length, speed limit, ttd*)

fork ::== node node node

train ::== integer integer integer edge nl (stop nl)* nl (*speed, length, departure time, start edge, ...*)

node ::== 0 | 1 | ... | number_of_nodes-1

edge ::== node node

stop ::== integer edge (*arrival time, stop edge*)

nl ::== newline

#### Output Format

output_file ::== ("SAT" nl (vss nl)* nl (route nl)* ) | "UNSAT""

vss ::== node* (*nodes belonging to same vss*)

route ::== 0 edge* nl | ... | maxTime-1 edge* nl (*each rout refers to route of a train. routes are in order of appearence in input file*)

## Reference

If you use our tool for your research, we would be thankful if you referred to it
by citing the following publication: 
````
@inproceedings{wille2021etcs,
   title={Towards Automatic Design and Verification for Level 3 of the European Train Control System},
   author={Robert Wille and Tom Peham and Judith Przigoda and Nils Przigoda},
   booktitle={Design, Automation and Test in Europe (DATE)},
   year={2021}
}
````
