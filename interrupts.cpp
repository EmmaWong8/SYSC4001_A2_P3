/**
 *
 * @file interrupts.cpp
 * @author Sasisekhar Govind
 *
 */

#include<interrupts.hpp>

unsigned int next_pid = 1; // Global PID counter

std::tuple<std::string, std::string, int> simulate_trace(std::vector<std::string> trace_file, int time, std::vector<std::string> vectors, std::vector<int> delays, std::vector<external_file> external_files, PCB current, std::vector<PCB> wait_queue) {

    std::string trace;      //!< string to store single line of trace file
    std::string execution = "";  //!< string to accumulate the execution output
    std::string system_status = "";  //!< string to accumulate the system status output
    int current_time = time;

    //parse each line of the input trace file. 'for' loop to keep track of indices.
    for(size_t i = 0; i < trace_file.size(); i++) {
        auto trace = trace_file[i];

        auto [activity, duration_intr, program_name] = parse_trace(trace);

        if(activity == "CPU") { //As per Assignment 1
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", CPU Burst\n";
            current_time += duration_intr;
        } else if(activity == "SYSCALL") { //As per Assignment 1
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            execution += intr;
            current_time = time;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", SYSCALL ISR (saving registers, executing system call, updating PCB)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "END_IO") {
            auto [intr, time] = intr_boilerplate(current_time, duration_intr, 10, vectors);
            current_time = time;
            execution += intr;

            execution += std::to_string(current_time) + ", " + std::to_string(delays[duration_intr]) + ", ENDIO ISR(clearing interrupt flag, signaling process ready, updating ready queue)\n";
            current_time += delays[duration_intr];

            execution +=  std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
        } else if(activity == "FORK") {
            auto [intr, time] = intr_boilerplate(current_time, 2, 10, vectors);
            execution += intr;
            current_time = time;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your FORK output here

            // Clone the PCB
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", cloning the PCB\n";
            current_time += duration_intr;
            
            // Create child PCB
            PCB child(next_pid++, current.PID, current.program_name, current.size, current.partition_number);
            if (!allocate_memory(&child)) {
                std::cerr << "Error. Memory allocation failed for child PID " << child.PID << std::endl;
            }
            wait_queue.push_back(current); // Parent goes to wait queue
            
            // Call scheduler
            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
            
            // Take system status snapshot
            system_status += "time: " + std::to_string(current_time) + "; current trace: " + trace + "\n";
            system_status += print_PCB(child, wait_queue);


            ///////////////////////////////////////////////////////////////////////////////////////////

            //The following loop helps you do 2 things:
            // * Collect the trace of the chile (and only the child, skip parent)
            // * Get the index of where the parent is supposed to start executing from
            std::vector<std::string> child_trace;
            bool skip = true;
            bool exec_flag = false;
            int parent_index = 0;

            for(size_t j = i; j < trace_file.size(); j++) {
                auto [_activity, _duration, _pn] = parse_trace(trace_file[j]);
                if(skip && _activity == "IF_CHILD") {
                    skip = false;
                    continue;
                } else if(_activity == "IF_PARENT"){
                    skip = true;
                    parent_index = j;
                    if(exec_flag) {
                        break;
                    }
                } else if(skip && _activity == "ENDIF") {
                    skip = false;
                    continue;
                } else if(!skip && _activity == "EXEC") {
                    skip = true;
                    child_trace.push_back(trace_file[j]);
                    exec_flag = true;
                }

                if(!skip) {
                    child_trace.push_back(trace_file[j]);
                }
            }
            //i = parent_index;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the child's trace, run the child (HINT: think recursion)

            auto [child_exec, child_status, child_end_time] = simulate_trace(
                child_trace, 
                current_time, 
                vectors, 
                delays, 
                external_files, 
                child, 
                wait_queue
            );
            
            execution += child_exec;
            system_status += child_status;
            current_time = child_end_time;
            
            // After child finishes, restore parent from wait queue
            if(!wait_queue.empty()) {
                current = wait_queue.back();
                wait_queue.pop_back();
            }
            
            // Set i to parent_index to continue with parent execution
            i = parent_index;


            ///////////////////////////////////////////////////////////////////////////////////////////


        } else if(activity == "EXEC") {
            auto [intr, time] = intr_boilerplate(current_time, 3, 10, vectors);
            current_time = time;
            execution += intr;

            ///////////////////////////////////////////////////////////////////////////////////////////
            //Add your EXEC output here

            // Get program size from external files
            unsigned int prog_size = get_size(program_name, external_files);
            
            // Output program size
            execution += std::to_string(current_time) + ", " + std::to_string(duration_intr) + ", Program is " + std::to_string(prog_size) + " Mb large\n";
            current_time += duration_intr;
            
            // Free old memory partition if allocated
            if (current.partition_number != -1) {
                free_memory(&current);
            }
            
            // Load program into memory (15 ms per MB)
            int loader_time = 15 * prog_size;
            execution += std::to_string(current_time) + ", " + std::to_string(loader_time) + ", loading program into memory\n";
            current_time += loader_time;
            
            // Mark partition as occupied (random time = 3ms)
            execution += std::to_string(current_time) + ", 3, marking partition as occupied\n";
            current_time += 3;
            
            // Update PCB with new program information (random time = 6ms)
            execution += std::to_string(current_time) + ", 6, updating PCB\n";
            current_time += 6;
            current.program_name = program_name;
            current.size = prog_size;
            
            // Allocate memory partition for new program
            if (!allocate_memory(&current)) {
                std::cerr << "Error. Memory allocation failed for " << program_name << std::endl;
            }
            
            // Call scheduler and return from interrupt
            execution += std::to_string(current_time) + ", 0, scheduler called\n";
            execution += std::to_string(current_time) + ", 1, IRET\n";
            current_time += 1;
            
            // Take system status snapshot
            system_status += "time: " + std::to_string(current_time) + "; current trace: " + trace + "\n";
            system_status += print_PCB(current, wait_queue);

            ///////////////////////////////////////////////////////////////////////////////////////////


            std::ifstream exec_trace_file(program_name + ".txt");

            std::vector<std::string> exec_traces;
            std::string exec_trace;
            while(std::getline(exec_trace_file, exec_trace)) {
                exec_traces.push_back(exec_trace);
            }

            ///////////////////////////////////////////////////////////////////////////////////////////
            //With the exec's trace (i.e. trace of external program), run the exec (HINT: think recursion)

            auto [exec_exec, exec_status, exec_end_time] = simulate_trace(
                exec_traces,
                current_time,
                vectors,
                delays,
                external_files,
                current,
                wait_queue
            );
            
            execution += exec_exec;
            system_status += exec_status;
            current_time = exec_end_time;

            ///////////////////////////////////////////////////////////////////////////////////////////

            break; //Why is this important? (answer in report)

        } else if(activity == "IF_CHILD" || activity == "IF_PARENT" || activity == "ENDIF") {
            // Skip these lines as they are handled in FORK logic
            continue;
        }
    }

    return {execution, system_status, current_time};
}

int main(int argc, char** argv) {

    //vectors is a C++ std::vector of strings that contain the address of the ISR
    //delays  is a C++ std::vector of ints that contain the delays of each device
    //the index of these elemens is the device number, starting from 0
    //external_files is a C++ std::vector of the struct 'external_file'. Check the struct in 
    //interrupt.hpp to know more.
    auto [vectors, delays, external_files] = parse_args(argc, argv);
    std::ifstream input_file(argv[1]);

    //Just a sanity check to know what files you have
    print_external_files(external_files);

    //Make initial PCB (notice how partition is not assigned yet)
    PCB current(0, -1, "init", 1, -1);
    //Update memory (partition is assigned here, you must implement this function)
    if(!allocate_memory(&current)) {
        std::cerr << "Error. Memory allocation failed!" << std::endl;
    }

    std::vector<PCB> wait_queue;

    /******************ADD YOUR VARIABLES HERE*************************/


    /******************************************************************/

    //Converting the trace file into a vector of strings.
    std::vector<std::string> trace_file;
    std::string trace;
    while(std::getline(input_file, trace)) {
        trace_file.push_back(trace);
    }

    auto [execution, system_status, _] = simulate_trace(   trace_file, 
                                            0, 
                                            vectors, 
                                            delays,
                                            external_files, 
                                            current, 
                                            wait_queue);

    input_file.close();

    write_output(execution, "execution.txt");
    write_output(system_status, "system_status.txt");

    return 0;
}
