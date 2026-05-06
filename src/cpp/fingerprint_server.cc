#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <glib.h>
#include <sstream>
#include <cmath>

#include "base64.h"
#include <grpc/grpc.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>
#include <grpcpp/security/server_credentials.h>
#include "helper.h"
#include "fingerprint.grpc.pb.h"
#include "libfprint_adapter.h"

#define MAX_PORT "65535"
#define DEFAULT_PORT "4134"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using fingerprint::EnrolledFMD;
using fingerprint::EnrollmentRequest;
using fingerprint::VerificationRequest;
using fingerprint::VerificationResponse;
using fingerprint::CheckDuplicateResponse;
using fingerprint::FingerPrint;

using std::vector;
using std::string;
using std::cout;
using std::endl;
using std::unique_ptr;

// Parsear minucias
std::vector<std::pair<int,int>> parse_minutiae(const std::string& fmd) {
    std::vector<std::pair<int,int>> minutiae;
    std::stringstream ss(fmd);
    std::string minutia_str;
    while (std::getline(ss, minutia_str, ';')) {
        size_t comma = minutia_str.find(',');
        if (comma != std::string::npos) {
            int x = std::stoi(minutia_str.substr(0, comma));
            int y = std::stoi(minutia_str.substr(comma + 1));
            minutiae.push_back({x, y});
        }
    }
    return minutiae;
}

// Comparación con 2 pasos: distancia + diferencia de cantidad
double compare_minutiae(const std::vector<std::pair<int,int>>& m1, 
                        const std::vector<std::pair<int,int>>& m2) {
    if (m1.empty() || m2.empty()) return 0.0;
    
    const double DISTANCE_THRESHOLD = 30.0;
    int match_count = 0;
    
    for (const auto& min1 : m1) {
        for (const auto& min2 : m2) {
            double dist = sqrt(pow(min1.first - min2.first, 2) + 
                              pow(min1.second - min2.second, 2));
            if (dist <= DISTANCE_THRESHOLD) {
                match_count++;
                break;
            }
        }
    }
    
    double similitud = (double)match_count / std::min(m1.size(), m2.size()) * 100.0;
    return similitud;
}

class FingerPrintImpl final : public FingerPrint::Service {
    public:
        Status EnrollFingerprint(ServerContext* context, const EnrollmentRequest* enrollmentRequest, EnrolledFMD* enrolledFMD) {
            std::string fmd = capture_to_fmd();
            if (fmd.empty()) return Status(grpc::StatusCode::INTERNAL, "No se pudo capturar la huella");
            enrolledFMD->set_base64enrolledfmd(fmd);
            return Status::OK;
        }

        Status VerifyFingerprint(ServerContext* context, const VerificationRequest* verification_request, VerificationResponse* verification_response) {
            std::string current_fmd = capture_to_fmd();
            if (current_fmd.empty()) {
                verification_response->set_match(false);
                return Status(grpc::StatusCode::INTERNAL, "No se pudo capturar la huella");
            }

            auto current_min = parse_minutiae(current_fmd);
            if (current_min.empty()) {
                verification_response->set_match(false);
                return Status::OK;
            }

            for (const auto& candidate : verification_request->fmdcandidates()) {
                auto stored_min = parse_minutiae(candidate.base64enrolledfmd());
                
                if (!stored_min.empty()) {
                    double similitud = compare_minutiae(stored_min, current_min);
                    if (similitud > 100.0) similitud = 100.0;
                    
                    // Diferencia en cantidad de minucias
                    int diff = abs((int)stored_min.size() - (int)current_min.size());
                    
                    cout << "Similitud: " << similitud << "% | Minucias: " 
                         << stored_min.size() << " vs " << current_min.size() 
                         << " (diff=" << diff << ")" << endl;

                    // Mismo dedo: similitud > 80% Y diferencia de minucias < 15
                    if (similitud >= 80.0 && diff <= 15) {
                        cout << "✅ Match!" << endl;
                        verification_response->set_match(true);
                        return Status::OK;
                    }
                }
            }

            cout << "❌ No match" << endl;
            verification_response->set_match(false);
            return Status::OK;
        }

        Status CheckDuplicate(ServerContext* context, const VerificationRequest* verification_request, CheckDuplicateResponse* check_duplicate_response) {
            check_duplicate_response->set_isduplicate(false);
            return Status::OK;
        }
};

void RunServer() {
    const char * port = getenv("PORT");
    string  server_address("0.0.0.0:");
    server_address.append(port && strlen(port) <= strlen(MAX_PORT) ? port : DEFAULT_PORT);

    FingerPrintImpl service;
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    unique_ptr<Server> server(builder.BuildAndStart());
    cout << "Server started, listening on " << server_address << endl;
    server->Wait();
}   

int main() {
    if (!fp_init_device()) {
        std::cerr << "No se pudo inicializar el lector" << std::endl;
        return 1;
    }
    RunServer();
}