#include <iostream>
#include <string>
#include <fstream>
#include <chrono>
#include <limits>
#include <unordered_map>
#include <iomanip>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <thread>
#include "helpers/p2_quantile_sketch.h"
#include "helpers/thread_pool.h"

// partial result per thread (avoid resource sharing to prevent race conditions)
struct PartialResult {
    int events_processed = 0;
    int errors = 0;
    double total_latency = 0;
    double min_latency = std::numeric_limits<double>::infinity();
    double max_latency = 0;
    std::unordered_map<std::string, int> services_frequency;
    P2QuantileSketch p50, p95, p99;

    PartialResult() : p50(0.5), p95(0.95), p99(0.99) {}
};


int main(int argc, char* argv[]) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::cout << "Parralog\n";
    std::cout << "There are " << argc << " arguments\n";

    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <log_file>\n";
        return 1;
    }

    // for memory mapping
    int file_descriptor = open(argv[1], O_RDONLY);
    if (file_descriptor == -1) {
        std::cerr << "Error reading file descriptor.\n";
        return 1;
    }

    // gather file metadata
    struct stat metadata;
    if (fstat(file_descriptor, &metadata) == -1) {
        std::cerr << "Error retrieving metadata with fstat.\n";
        close(file_descriptor);
        return 1;
    }

    // create memory map
    char* map = static_cast<char*>(
        mmap(nullptr, metadata.st_size, PROT_READ, MAP_PRIVATE, file_descriptor, 0)
    );
    if (map == MAP_FAILED) {
        std::cerr << "Error memory mapping the file.\n";
        close(file_descriptor);
        return 1;
    }

    // get # of available hardware threads (logical CPU cores available for Parralog)
    unsigned int num_threads = std::thread::hardware_concurrency();

    // default back to 4
    if (num_threads == 0) {
        num_threads = 4;
    }

    auto start_time = std::chrono::system_clock::now();
    ThreadPool pool(num_threads); // instantiate a custom thread pool
    std::vector<std::future<PartialResult>> futures;

    size_t chunk_size = metadata.st_size / num_threads;
    std::vector<std::pair<size_t, size_t>> offsets;

    // set ranges - align chunks
    for (size_t i {0}; i < num_threads; i++) {
        size_t start = i * chunk_size;
        size_t end = (i == num_threads - 1) ? metadata.st_size : (i + 1) * chunk_size;

        if (i > 0) {
            while (start < metadata.st_size and map[start] != '\n') {
                start += 1;
            }
            start += 1;
        }

        if (i < num_threads - 1) {
            while (end < metadata.st_size and map[end] != '\n') {
                end += 1;
            }
            end += 1;
        }

        offsets.push_back({start, end});
    }

    // give a task to a thread
    for (auto& [start, end] : offsets) {
        futures.push_back(
            pool.enqueue([map, start, end]() {
                PartialResult result;
                size_t local_start = start;


                for (size_t i = start; i < end; i++) {
                    if (map[i] == '\n') {
                        size_t local_end = i;
                        std::string_view line(map + local_start, local_end - local_start);

                        size_t pos_status = line.find("\"status\":");
                        if (pos_status != std::string_view::npos) {
                            size_t value_start = pos_status + 9;
                            size_t value_end = line.find(',', value_start);
                            std::string_view status_str = line.substr(value_start, value_end - value_start);
                            int status = std::stoi(std::string(status_str));
                            if (status != 200) result.errors++;
                        }

                        size_t pos_latency = line.find("\"latency_ms\":");
                        if (pos_latency != std::string_view::npos) {
                            size_t value_start = pos_latency + 13;
                            size_t value_end = line.find(',', value_start);
                            std::string_view latency_str = line.substr(value_start, value_end - value_start);
                            double latency = std::stod(std::string(latency_str));

                            result.min_latency = std::min(result.min_latency, latency);
                            result.max_latency = std::max(result.max_latency, latency);
                            result.total_latency += latency;

                            result.p50.insert(latency);
                            result.p95.insert(latency);
                            result.p99.insert(latency);
                        }

                        size_t pos_service = line.find("\"service\":\"");
                        if (pos_service != std::string_view::npos) {
                            size_t value_start = pos_service + 11;
                            size_t value_end = line.find('"', value_start);
                            std::string service(line.substr(value_start, value_end - value_start));
                            result.services_frequency[service]++;
                        }

                        result.events_processed += 1;
                        local_start = i + 1;
                    }
                }

                return result;
            })
        );
    }

    // hold all partial results
    std::vector<PartialResult> results;
    for (auto& fut : futures) {
        results.push_back(fut.get());
    }

    int total_events_processed = 0;
    int total_errors = 0;
    double total_latency = 0;
    double min_latency = std::numeric_limits<double>::infinity();
    double max_latency = 0;
    double average_latency = 0;
    std::unordered_map<std::string, int> services_frequency;
    P2QuantileSketch p50(0.5), p95(0.95), p99(0.99);

    // aggregate partial results and merge
    for (auto& thread_result : results) {
        total_events_processed += thread_result.events_processed;
        total_errors += thread_result.errors;
        total_latency += thread_result.total_latency;

        min_latency = std::min(thread_result.min_latency, min_latency);
        max_latency = std::max(thread_result.max_latency, max_latency);

        for (auto& [service, count] : thread_result.services_frequency) {
            services_frequency[service] += count;
        }

        p50.merge(thread_result.p50);
        p95.merge(thread_result.p95);
        p99.merge(thread_result.p99);
    }
    auto end_time = std::chrono::system_clock::now();

    if (!total_events_processed) {
        std::cout << "There were no events to process\n";
        munmap(map, metadata.st_size);
        close(file_descriptor);
        return 1;
    }

    double est_p50 = p50.getQuantile();
    double est_p95 = p95.getQuantile();
    double est_p99 = p99.getQuantile();
    average_latency = total_latency / total_events_processed;

    auto total_time = end_time - start_time;
    double elapsed = std::chrono::duration<double>(total_time).count();
    double throughput = total_events_processed / elapsed;
    double error_percentage = (static_cast<double>(total_errors) / total_events_processed) * 100.0;

    // sort services to retrieve most frequent
    std::vector<std::pair<std::string, int>> sorted_services(
        services_frequency.begin(), services_frequency.end()
    );
    std::sort(sorted_services.begin(), sorted_services.end(),
        [](auto& a, auto& b) { return a.second > b.second; });

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
    std::cout << "   Time Taken  : " << elapsed << " sec\n\n";

    std::cout << "Top " << top_n_services << " Services:\n";
    for (int i = 0; i < top_n_services && i < (int)sorted_services.size(); i++) {
        std::cout << "   " << std::setw(12) << sorted_services[i].first
                  << " → " << sorted_services[i].second << " events\n";
    }

    std::cout << "==============================\n\n";

    // close and unmap to avoid resource leakage
    munmap(map, metadata.st_size);
    close(file_descriptor);
    return 0;
}