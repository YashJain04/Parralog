#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <iomanip>
#include <fcntl.h>
#include <unistd.h>
#include "tools/p2_quantile_sketch.h"
#include <sys/stat.h>
#include <sys/mman.h>


int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "HPC Analytics Engine\n";
    std::cout << "There are " << argc << " arguments\n";

    int file_descriptor = open(argv[1], O_RDONLY);

    if (file_descriptor == -1) {
        std::cout << "Error reading file descriptor.";
        return 1;
    }
    
    struct stat metadata;

    if (fstat(file_descriptor, &metadata) == -1) {
        std::cout << "Error retrieving metadata with fstat.";
        return 1;
    }

    char *map;

    map = static_cast<char*> (mmap (nullptr, metadata.st_size, PROT_READ, MAP_PRIVATE, file_descriptor, 0));

    if (map == MAP_FAILED) {
        std::cout << "Error memory mapping the file";
        return 1;
    }

    size_t start = 0;
    size_t end = 0;
    int total_events_processed = 0;
    int total_errors = 0;
    float total_latency = 0;
    float min_latency = std::numeric_limits<float>::infinity();
    float max_latency = 0;
    float average_latency = 0;
    std::unordered_map<std::string, int> services_frequency;

    auto start_time = std::chrono::system_clock::now();

    P2QuantileSketch p50(0.5), p95(0.95), p99(0.99);

    for (size_t i {0}; i < metadata.st_size; i++) {

        if(map[i] == '\n') {
            end = i;
            std::string_view line(map + start, end - start);

            size_t pos_status = line.find("\"status\":");
            int status = 0;
            if (pos_status != std::string_view::npos) {
                size_t value_start = pos_status + 9; // skip over "status":
                size_t value_end = line.find(',', value_start);
                std::string_view status_str = line.substr(value_start, value_end - value_start);
                status = std::stoi(std::string(status_str));
                total_errors = (status == 200) ? total_errors : total_errors + 1;
            }

            size_t pos_latency = line.find("\"latency_ms\":");
            float latency = 0.0f;
            if (pos_latency != std::string_view::npos) {
                size_t value_start = pos_latency + 13;
                size_t value_end = line.find(',', value_start);
                std::string_view latency_str = line.substr(value_start, value_end - value_start);
                latency = std::stof(std::string(latency_str));

                min_latency = std::min(min_latency, latency);
                max_latency = std::max(max_latency, latency);
                total_latency += latency;

                p50.insert(latency);
                p95.insert(latency);
                p99.insert(latency);

            }

            size_t pos_service = line.find("\"service\":\"");
            std::string service;
            if (pos_service != std::string_view::npos) {
                size_t value_start = pos_service + 11;
                size_t value_end = line.find('"', value_start);
                service = std::string(line.substr(value_start, value_end - value_start));
                services_frequency[service] += 1;
            }
            total_events_processed += 1;

            start = i + 1;
        }
    }

    auto end_time = std::chrono::system_clock::now();

    if (!total_events_processed) {
        std::cout << "There are no events to process\n";
        return 1;
    }

    float est_p50 = p50.getQuantile();
    float est_p95 = p95.getQuantile();
    float est_p99 = p99.getQuantile();
    average_latency = total_latency / total_events_processed;

    auto total_time = end_time - start_time;
    std::chrono::duration<double> seconds = total_time;
    double elapsed = seconds.count();
    double throughput = total_events_processed/elapsed;
    double time_taken = elapsed;
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
    std::cout << "   P50        : " << est_p50 << "\n";
    std::cout << "   P95        : " << est_p95 << "\n";
    std::cout << "   P99        : " << est_p99 << "\n\n";

    std::cout << "Performance:\n";
    std::cout << "   Throughput  : " << throughput << " events/sec\n";
    std::cout << "   Time Taken  : " << time_taken << " sec\n\n";

    std::cout << "Top " << top_n_services << " Services:\n";
    for (int i = 0; i < top_n_services && i < sorted_services.size(); i++) {
        std::cout << "   " << std::setw(12) << sorted_services[i].first 
                << " → " << sorted_services[i].second << " events\n";
    }

    std::cout << "==============================\n\n";

    munmap(map, metadata.st_size);
    close(file_descriptor);
    return 0;
}