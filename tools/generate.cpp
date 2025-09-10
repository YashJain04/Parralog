#include <iostream>
#include <fstream>
#include <string>

int main(int argc, char* argv[]) {
    std::cout << "Log Generator\n";
    std::cout << "There are " << argc << " arguments\n";
    
    std::ofstream output_file { argv[1] };
    int events { std::stoi(argv[2]) };

    if (!output_file) {
        std::cerr << "Error processing output file\n";
        return 1;
    }

    for (int count { 0 }; count < events; count++) {
        output_file << "{\"ts\":" << count << ",\"service\":\"service_001\"" << ",\"status\":200,\"latency_ms\":10.0}\n";
    }
    

    std::cout << "Input file created\n";
    return 0;
}