#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>

int main(int argc, char* argv[]) {
    std::cout << "Log Generator\n";
    std::cout << "There are " << argc << " arguments\n";
    
    std::ofstream output_file { argv[1] };
    int events { std::stoi(argv[2]) };

    std::ios::sync_with_stdio(false);

    if (!output_file) {
        std::cerr << "Error processing output file\n";
        return 1;
    }

    output_file.tie(nullptr);

    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::time_point_cast<std::chrono::milliseconds>(now);
    long long timestamp = ms.time_since_epoch().count();

    std::vector <std::string> services;

    for (int count { 1 }; count < 101; count++) {
        if (count < 10) {
            services.push_back("service_00" + std::to_string(count));
        }

        else if (count < 100) {
            services.push_back("service_0" + std::to_string(count));
        }

        else {
            services.push_back("service_" + std::to_string(count));
        }
    }

    std::random_device rd;
    std::mt19937 random(rd());
    std::uniform_int_distribution<int> services_dist(0, services.size()-1);
    std::uniform_real_distribution<float> latency_dist(5.0, 50);
    std::uniform_int_distribution<int> status_dist(1, 100);

    const int BUFFER_SIZE = 1024;
    std::string buffer[BUFFER_SIZE];
    int buffer_index = 0;

    for (int count { 0 }; count < events; count++) {
        int index = services_dist(random);
        std::string service = services[index];

        float latency = latency_dist(random);

        int roll = status_dist(random);
        int status = (roll <= 95) ? 200 : 500;

        std::string record = "{\"timestamp\":" + std::to_string(timestamp)
            + ",\"service\":\"" + service + "\""
            + ",\"status\":" + std::to_string(status)
            + ",\"latency_ms\":" + std::to_string(latency)
            + "}\n";

        if (buffer_index >= BUFFER_SIZE) {    
            for (int c { 0 }; c < BUFFER_SIZE; c++) {
                output_file.write(buffer[c].c_str(), buffer[c].size());
            }

            buffer_index = 0;
        }

        buffer[buffer_index] = record;

        if (count == events - 1) {
            for (int c { 0 }; c < buffer_index + 1; c++) {
                output_file.write(buffer[c].c_str(), buffer[c].size());
            }
        }

        timestamp += 1;
        buffer_index += 1;
    }

    auto end_time = std::chrono::system_clock::now();
    auto total_time = end_time - now;
    std::chrono::duration<double> seconds = total_time;
    double elapsed = seconds.count();

    std::cout << "Total time taken " << std::fixed << std::setprecision(2) << elapsed << " seconds.\n";

    std::cout << "Input file created\n";
    return 0;
}