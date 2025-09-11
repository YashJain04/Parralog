#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <iomanip>

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
    float min_latency = std::numeric_limits<float>::infinity();
    float max_latency = 0;
    float average_latency = 0;

    auto start_time = std::chrono::system_clock::now();

    std::unordered_map<std::string, int> services_frequency;

    std::vector<float> latencies;
    
    while (std::getline(input_file, line)) {
        // std::cout << line << "\n";

        std::size_t pos_ts = line.find("\"timestamp\":") + 12;
        std::size_t pos_comma = line.find(",", pos_ts);
        long long timestamp = std::stoll(line.substr(pos_ts, pos_comma - pos_ts));

        std::size_t pos_service = line.find("\"service\":\"") + 10;
        pos_comma = line.find(",", pos_service);
        std::string service = line.substr(pos_service, pos_comma - pos_service);

        services_frequency[service] += 1;

        std::size_t pos_status = line.find("\"status\":") + 9;
        pos_comma = line.find(",", pos_status);
        int status = std::stoi(line.substr(pos_status, pos_comma - pos_status));

        std::size_t pos_latency = line.find("\"latency_ms\":") + 13;
        pos_comma = line.find(",", pos_latency);
        float latency = static_cast<float>(std::stod(line.substr(pos_latency, pos_comma - pos_latency)));

        latencies.push_back(latency);

        // std::cout << "\n--CURRENT EVENT--\n";
        // std::cout << "The timestamp is " << timestamp << "\n";
        // std::cout << "The service is " << service << "\n";

        if (latency < min_latency) {
            min_latency = latency;
        }

        if (latency > max_latency) {
            max_latency = latency;
        }

        total_latency += latency;
        total_errors = (status == 200) ? total_errors : total_errors + 1;
        total_events_processed += 1;
    }

    auto end_time = std::chrono::system_clock::now();

    if (!total_events_processed) {
        std::cout << "There are no events to process\n";
        return 1;
    }

    std::sort(latencies.begin(), latencies.end());

    size_t p50_index = static_cast<size_t>(0.50 * latencies.size());
    float p50_latency = latencies[std::min(p50_index, latencies.size() - 1)];

    size_t p95_index = static_cast<size_t>(0.95 * latencies.size());
    float p95_latency = latencies[std::min(p95_index, latencies.size() - 1)];

    size_t p99_index = static_cast<size_t>(0.99 * latencies.size());
    float p99_latency = latencies[std::min(p99_index, latencies.size() - 1)];

    average_latency = total_latency / total_events_processed;

    auto total_time = end_time - start_time;
    std::chrono::duration<double> seconds = total_time;
    double elapsed = seconds.count();
    double throughput = total_events_processed/elapsed;
    double time_taken = total_events_processed / throughput;
    double error_percentage = (static_cast<double>(total_errors) / total_events_processed) * 100.0;
    
    std::vector<std::pair<std::string, int>> sorted_services(services_frequency.begin(), services_frequency.end());

    std::sort(sorted_services.begin(), sorted_services.end(),
        [](auto &a, auto &b) {
            return a.second > b.second;
        });

    int top_n_services = 3;

    std::cout << "\n==============================\n";
    std::cout << "     METRICS SUMMARY REPORT    \n";
    std::cout << "==============================\n";

    std::cout << "Events Processed : " << total_events_processed << "\n";
    std::cout << "Errors           : " << total_errors 
            << " (" << std::fixed << std::setprecision(2) << error_percentage << "%)\n\n";

    std::cout << "Latency (ms):\n";
    std::cout << "   Min        : " << std::fixed << std::setprecision(2) << min_latency << "\n";
    std::cout << "   Max        : " << max_latency << "\n";
    std::cout << "   Average    : " << average_latency << "\n";
    std::cout << "   P50        : " << p50_latency << "\n";
    std::cout << "   P95        : " << p95_latency << "\n";
    std::cout << "   P99        : " << p99_latency << "\n\n";

    std::cout << "Performance:\n";
    std::cout << "   Throughput  : " << throughput << " events/sec\n";
    std::cout << "   Time Taken  : " << time_taken << " sec\n\n";

    std::cout << "Top " << top_n_services << " Services:\n";
    for (int i = 0; i < top_n_services && i < sorted_services.size(); i++) {
        std::cout << "   " << std::setw(12) << sorted_services[i].first 
                << " → " << sorted_services[i].second << " events\n";
    }

    std::cout << "==============================\n\n";

    return 0;
}