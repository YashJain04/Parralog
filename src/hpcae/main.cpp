#include <iostream>
#include <string>
#include <fstream>
#include <chrono>

int main(int argc, char* argv[]) {
    std::cout << "HPC Analytics Engine\n";
    std::cout << "There are " << argc << " arguments\n";

    std::ifstream input_file { argv[1] };

    if (!input_file) {
        std::cout << "Error processing input file\n";
        return 1;
    }

    std::string line {};

    int total_events_processed = 0;
    int total_errors = 0;
    float total_latency = 0;
    float average_latency = 0;

    auto start_time = std::chrono::system_clock::now();
    
    while (std::getline(input_file, line)) {
        std::cout << line << "\n";

        std::size_t pos_ts = line.find("\"timestamp\":") + 12;
        std::size_t pos_comma = line.find(",", pos_ts);
        long long timestamp = std::stoll(line.substr(pos_ts, pos_comma - pos_ts));

        std::size_t pos_service = line.find("\"service\":\"") + 10;
        pos_comma = line.find(",", pos_service);
        std::string service = line.substr(pos_service, pos_comma - pos_service);

        std::size_t pos_status = line.find("\"status\":") + 9;
        pos_comma = line.find(",", pos_status);
        int status = std::stoi(line.substr(pos_status, pos_comma - pos_status));

        std::size_t pos_latency = line.find("\"latency_ms\":") + 13;
        pos_comma = line.find(",", pos_latency);
        float latency = static_cast<float>(std::stod(line.substr(pos_latency, pos_comma - pos_latency)));

        std::cout << "\n--CURRENT EVENT--\n";
        std::cout << "The timestamp is " << timestamp << "\n";
        std::cout << "The service is " << service << "\n";

        total_latency += latency;
        total_errors = (status == 200) ? total_errors : total_errors + 1;
        total_events_processed += 1;
    }

    auto end_time = std::chrono::system_clock::now();

    if (!total_events_processed) {
        std::cout << "There are no events to process\n";
        return 1;
    }

    average_latency = total_latency / total_events_processed;

    auto total_time = end_time - start_time;
    std::chrono::duration<double> seconds = total_time;
    double elapsed = seconds.count();
    double throughput = total_events_processed/elapsed;

    std::cout << "\n--METRICS SUMMARY REPORT--\n";
    std::cout << "Processed " << total_events_processed << " events\n";
    std::cout << "Average latency " << average_latency << " ms\n";
    std::cout << "Throughput " << throughput << " events/sec\n";

    double time_taken = total_events_processed / throughput;

    std::cout << "Time Taken " << time_taken << " seconds\n";


    return 0;
}